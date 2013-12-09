/*
 * Sphinx2 protocol functionality
 */

#include <assert.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_sphinx2_sphx.h"
#include "ngx_http_sphinx2_args_parser.h"
#include "ngx_http_sphinx2_stream.h"


/* MACROS */

#define LIST_ADD(l, n, sz) { (n)->next = (l); (l) = (n); ++(sz); }

/* GLOBALS */

sphx2_match_mode_t  sphx2_default_match_mode = SPHX2_MATCH_ALL;

sphx2_ranker_t      sphx2_default_ranker = SPHX2_RANK_PROXIMITY_BM25;

sphx2_sort_mode_t   sphx2_default_sort_mode = SPHX2_SORT_RELEVANCE;

sphx2_output_type_t sphx2_default_output_type = SPHX2_OUTPUT_RAW;

ngx_uint_t          sphx2_default_num_results = 50;

ngx_uint_t          sphx2_default_max_matches = 1000;

size_t              sphx2_min_search_header_len = 12; /* hs (4) + hdr(8) */

/* LOCAL GLOBALS */

static const char* s_match_mode_strs[] = {
    "all",      /* SPHX2_MATCH_ALL =           0, */
    "any",      /* SPHX2_MATCH_ANY =           1, */
    "phrase",   /* SPHX2_MATCH_PHRASE =        2, */
    "boolean",  /* SPHX2_MATCH_BOOLEAN =       3, */
    "extended", /* SPHX2_MATCH_EXTENDED =      4, */
    "fullscan", /* SPHX2_MATCH_FULLSCAN =      5, */
#if 0
    -- not supported for near future removal --
    SPHX2_MATCH_EXTENDED2 =     6
#endif
};

static const size_t sz_match_mode_strs =
    sizeof(s_match_mode_strs) / sizeof(const char*);

static const char* s_ranker_strs[] = {
    "proxbm25",  /* SPHX2_RANK_PROXIMITY_BM25 = 0, */
    "bm25",      /* SPHX2_RANK_BM25 =           1, */
    "none",      /* SPHX2_RANK_NONE =           2, */
    "wordcount", /* SPHX2_RANK_WORDCOUNT =      3, */
    "prox",      /* SPHX2_RANK_PROXIMITY =      4, */
    "matchany",  /* SPHX2_RANK_MATCHANY =       5, */
    "fieldmask", /* SPHX2_RANK_FIELDMASK =      6, */
    "sph04",     /* SPHX2_RANK_SPH04 =          7, */
    "expr",      /* SPHX2_RANK_EXPR =           8, */
    "total",     /* SPHX2_RANK_TOTAL =          9 */
};

static const size_t sz_ranker_strs =
    sizeof(s_ranker_strs) / sizeof(const char*);

static const char* s_sort_mode_strs[] = {
    "relevance", /* SPHX2_SORT_RELEVANCE =      0, */
    "attr_desc", /* SPHX2_SORT_ATTR_DESC =      1, */
    "attr_asc",  /* SPHX2_SORT_ATTR_ASC =       2, */
    "time_seg",  /* SPHX2_SORT_TIME_SEGMENTS =  3, */
    "extendex",  /* SPHX2_SORT_EXTENDED =       4, */
    "expr",      /* SPHX2_SORT_EXPR =           5 */
};

static const size_t sz_sort_mode_strs =
    sizeof(s_sort_mode_strs) / sizeof(const char*);

static const char* s_filter_type_strs[] = {
    "vals",     /* SPHX2_FILTER_VALUES =       0, */
    "range",    /* SPHX2_FILTER_RANGE =        1, */
    "frange",   /* SPHX2_FILTER_FLOATRANGE =   2, */
};

static const size_t sz_filter_type_strs =
    sizeof(s_filter_type_strs)/sizeof(const char*);

static const char* s_filter_exclude_strs[] = {
    "in",
    "ex"
};

static const size_t sz_filter_exclude_strs =
    sizeof(s_filter_exclude_strs)/sizeof(const char*);

static const char* s_group_type_strs[] = {
    "day",      /* SPHX2_GROUPBY_DAY =             0, */
    "week",     /* SPHX2_GROUPBY_WEEK =            1, */
    "month",    /* SPHX2_GROUPBY_MONTH =           2, */
    "year",     /* SPHX2_GROUPBY_YEAR =            3, */
    "attr",     /* SPHX2_GROUPBY_ATTR =            4, */
    "attrpair", /* SPHX2_GROUPBY_ATTRPAIR =        5, */
};

static const size_t sz_group_type_strs =
    sizeof(s_group_type_strs)/sizeof(const char*);

static const char* s_output_type_strs[] = {
    "raw",
    "text",
    "json",
    "xml"
};

static const size_t sz_output_type_strs = 
    sizeof(s_output_type_strs)/sizeof(const char*);

static sphx2_arg_parse_ctx_t s_main_ctxt, s_sec_ctxt;

static const char* s_no_delim = "$";
static const char* s_set_delim = ",";
static const char* s_key_val_delim = ":";
static const char* s_multi_delim = ";";

static const size_t sz16 = sizeof(uint16_t),
                    sz32 = sizeof(uint32_t),
                    sz64 = sizeof(uint64_t),
                    szf = sizeof(float);

static ngx_str_t empty_str = ngx_null_string;
static ngx_str_t default_select = ngx_string("*");

/* FUNCTION DEFINITIONS */

#include <ctype.h>

static void s_dump_buffer(u_char* ptr, size_t len) {
    size_t i; char c;
    fprintf(stderr, "buf [%lu]: ", len);
    for(i = 0; i < len; ++i) {
        c = ptr[i];
        if(isprint(c)) fprintf(stderr, "%c", c);
        else if(c < 0x10) fprintf(stderr, "0%x", c);
        else fprintf(stderr, "%x", c);
    }
    fprintf(stderr, "\n");
}

/* Functions to handle search request */

/*
 * Assumptions/Limitations in implementation of searchd protocol for Search
 *
 * -- which elements of search request are not supported
 * -- supported only with particular values
 *
 * 1  deprecated 'weights' field not supported. corresponding num-weights
 *    field is always 0.
 *
 * 2  'values' filter is not supported. only 'range' and 'float range'.
 *
 * 3  'overrides' not supported. field having number of overrides is 0 always.
 *
 * 4  For distributed search - 0 is default for cutoff, retrycount, retrydelay
 *
 * 5  'maxquerytime' is 0 (unlimited)
 */

static size_t
s_sphx2_search_request_len(sphx2_search_input_t * srch_input)
{
    /* Default part of the request */

    /* uint32_t vars:
     *     offset, limit, mode, ranker, sort mode, sort by len,
     *     keywords len, [depre] num weights (always 0), index len,
     *     id range marker, num filters, group type, group by len,
     *     max matches, group sort len, cutoff, retry count,
     *     retry delay, group distinct len, is geo there?,
     *     num index weights, max query time, num field weights,
     *     comment len, num overrides, select len
     *
     * uint64_t vars:
     *     [depre] idx min, [depre] idx max
     */

    static const size_t num_default_32s = 26,
                        num_default_64s = 2;

    size_t request_len = num_default_32s * sz32 + num_default_64s * sz64;

    sphx2_filter_t* f;
    sphx2_weight_t* w;

    size_t i;

    /* Calculate variable length of the request */
    request_len +=
        ((SPHX2_RANK_EXPR==srch_input->ranker) /* rank expr */
            ? (sz32 + srch_input->rank_expr->len) : 0)
        + srch_input->sort_by->len /* sort by */
        + srch_input->keywords->len /* keywords */
        + srch_input->index->len /* index */
        + srch_input->group->attr->len + /* group by attr */
        + srch_input->group->sort->len /* group sort type */
        + srch_input->group->distinct->len /* group distinct */
        + ((NULL != srch_input->geo) /* geo */
             ? (2 * szf + 2 * sz32 + srch_input->geo->lat_attr->len +
                srch_input->geo->lon_attr->len) : 0)
        + empty_str.len /* comment */
        + default_select.len;/* select */
        ;

    /* Filters */
    f = srch_input->filters;
    for(i = 0; i < srch_input->num_filters; ++i) {
        request_len +=
            (sz32 + f->attr->len) /* attr */ + 2 * sz32; /* type, exclude */
        switch(f->type) {
            /*case SPHX2_FILTER_VALUES:
                request_len += (sz32 + sz64 * f->spec.vals.n);
                break;*/
            case SPHX2_FILTER_RANGE: request_len += 2 * sz64; break;
            case SPHX2_FILTER_FLOATRANGE: request_len += 2 * szf; break;
            default: return (NGX_ERROR);
        }
        f = f->next;
    }

    /* Weights */
    w = srch_input->index_weights; /* index weights */
    for(i = 0; i < srch_input->num_index_weights; ++i) {
        request_len += (sz32 + w->entity->len + sz32); /* idx, weight */
        w = w->next;
    }
    w = srch_input->field_weights; /* field weights */
    for(i = 0; i < srch_input->num_field_weights; ++i) {
        request_len += (sz32 + w->entity->len + sz32); /* field, weight */
        w = w->next;
    }

    return(request_len);
}

static ngx_int_t
s_write_filters_to_stream(
    sphx2_search_input_t   * input,
    sphx2_stream_t         * st)
{
    size_t           i;
    ngx_int_t        status;
    sphx2_filter_t * f;

    f = input->filters;

    for(i = 0; i < input->num_filters; ++i) {
        status =
               sphx2_stream_write_string(st, f->attr)
            || sphx2_stream_write_int32(st, (uint32_t)f->type)
            || ((SPHX2_FILTER_RANGE == f->type)
                 ? (   sphx2_stream_write_int64(st, f->spec.ir.min)
                    || sphx2_stream_write_int64(st, f->spec.ir.max))
                 : (   sphx2_stream_write_float(st, (float)f->spec.fr.min)
                    || sphx2_stream_write_float(st, (float)f->spec.fr.max)))
            || sphx2_stream_write_int32(st, (uint32_t)f->exclude);

        if(NGX_OK != status) return(status);

        f = f->next;
    }

    return(NGX_OK);
}

static ngx_int_t
s_write_weights_to_stream(
    uint32_t           num_weights,
    sphx2_weight_t   * weights,
    sphx2_stream_t   * st)
{
    size_t           i;
    ngx_int_t        status;
    sphx2_weight_t * w;

    w = weights;

    for(i = 0; i < num_weights; ++i) {
        status =
               sphx2_stream_write_string(st, w->entity)
            || sphx2_stream_write_int32(st, (uint32_t)w->weight);

        if(NGX_OK != status) return(status);

        w = w->next;
    }

    return(NGX_OK);
}

ngx_int_t
sphx2_create_search_request(
    ngx_pool_t             * pool,
    sphx2_search_input_t   * input,
    ngx_buf_t             ** b)
{
    size_t request_len = s_sphx2_search_request_len(input);

    /* data to send =
     *   handshake = version [4]
     * . header = command [2] . command_version [2] . bytes following [4]
     *            . 0 [4] . num_queries [4]
     * . request [request_len]
     */
    size_t buf_len =
        (2 * sz16 + 4 * sz32) + request_len;

    sphx2_stream_t* st = sphx2_stream_create(pool);

    ngx_int_t status;

    if(NGX_ERROR == sphx2_stream_alloc(st, buf_len)) {
        return(NGX_ERROR);
    }

    status =
           /* handshake */
           sphx2_stream_write_int32(st, (uint32_t)SPHX2_CLI_VERSION)
           /* header - command */
        || sphx2_stream_write_int16(st, (uint16_t)SPHX2_COMMAND_SEARCH)
           /* header - command ver */
        || sphx2_stream_write_int16(st, (uint16_t)SPHX2_VER_COMMAND_SEARCH)
           /* bytes after this variable in the request */
        || sphx2_stream_write_int32(st, (uint32_t)(request_len + 2 * sz32))
        || sphx2_stream_write_int32(st, (uint32_t)0) /* 0 in 2.x */
        || sphx2_stream_write_int32(st, (uint32_t)1) /* query count */
           /* the single search query request here onwards ... */
        || sphx2_stream_write_int32(st, (uint32_t)input->offset)
        || sphx2_stream_write_int32(st, (uint32_t)input->num_results)
        || sphx2_stream_write_int32(st, (uint32_t)input->match_mode)
        || sphx2_stream_write_int32(st, (uint32_t)input->ranker)
        || ((SPHX2_RANK_EXPR == input->ranker)
              ? sphx2_stream_write_string(st, input->rank_expr)
              : NGX_OK)
        || sphx2_stream_write_int32(st, (uint32_t)input->sort_mode)
        || sphx2_stream_write_string(st, input->sort_by)
        || sphx2_stream_write_string(st, input->keywords)
        || sphx2_stream_write_int32(st, (uint32_t)0) /* [d] weights count */
        || sphx2_stream_write_string(st, input->index)
        || sphx2_stream_write_int32(st, (uint32_t)1) /* [d] range marker */
        || sphx2_stream_write_int64(st, (uint64_t)0) /* [d] min */
        || sphx2_stream_write_int64(st, (uint64_t)0) /* [d] max */
        || sphx2_stream_write_int32(st, (uint32_t)input->num_filters)
        || ((0 != input->num_filters)
              ? s_write_filters_to_stream(input, st)
              : NGX_OK)
        || sphx2_stream_write_int32(st, (uint32_t)input->group->type)
        || sphx2_stream_write_string(st, input->group->attr)
        || sphx2_stream_write_int32(st, (uint32_t)input->max_matches)
        || sphx2_stream_write_string(st, input->group->sort)
        || sphx2_stream_write_int32(st, (uint32_t)0) /* cut off */
        || sphx2_stream_write_int32(st, (uint32_t)0) /* retry count */
        || sphx2_stream_write_int32(st, (uint32_t)0) /* retry delay */
        || sphx2_stream_write_string(st, input->group->distinct)
        || ((NULL != input->geo)
              ? (    sphx2_stream_write_int32(st, (uint32_t)1) /* have geo */
                  || sphx2_stream_write_string(st, input->geo->lat_attr)
                  || sphx2_stream_write_string(st, input->geo->lon_attr)
                  || sphx2_stream_write_float(st, input->geo->lat)
                  || sphx2_stream_write_float(st, input->geo->lon))
              : sphx2_stream_write_int32(st, (uint32_t)0)) /* no geo */
        || sphx2_stream_write_int32(st, (uint32_t)input->num_index_weights)
        || ((0 != input->num_index_weights)
              ? s_write_weights_to_stream(input->num_index_weights,
                                          input->index_weights, st)
              : NGX_OK)
        || sphx2_stream_write_int32(st, (uint32_t)0) /* maxqtime - no limit */
        || sphx2_stream_write_int32(st, (uint32_t)input->num_field_weights)
        || ((0 != input->num_field_weights)
              ? s_write_weights_to_stream(input->num_field_weights,
                                          input->field_weights, st)
              : NGX_OK)
        || sphx2_stream_write_string(st, &empty_str) /* empty comments */
        || sphx2_stream_write_int32(st, (uint32_t)0) /* [us] num overrides */
        || sphx2_stream_write_string(st, &default_select) /* select all attrs */
        ;

    *b = sphx2_stream_get_buf(st);

    s_dump_buffer((*b)->pos, buf_len);

    return status;
}

/* Functions to handle excerpt request */

static size_t
s_sphx2_excerpt_request_len(sphx2_excerpt_input_t * input)
{
    /* constant part of the request */

    /* uint32 elements
     *
     * mode, flags, index len, keywords len, before match len,
     * after match len, chunk separator len, limit, around,
     * limit passages, limit words, start passage id, html strip mode len,
     * passage boundary len, docs count
     */

    static const size_t num_default_32s = 15;

    size_t request_len = sz32 * num_default_32s, i;

    sphx2_doc_t * doc;

    /* variable part */
    request_len += input->index->len
        + input->keywords->len
        + input->excerpt_opts->before_match->len
        + input->excerpt_opts->after_match->len
        + input->excerpt_opts->chunk_separator->len
        + input->excerpt_opts->html_strip_mode->len
        + input->excerpt_opts->passage_boundary->len;

    doc = input->docs;
    for(i = 0; i < input->num_docs; ++i) {
        request_len += (sz32 + doc->doc->len);
        doc = doc->next;
    }

    return(request_len);
}

static ngx_int_t
s_write_docs_to_stream(
    uint32_t           num_docs,
    sphx2_doc_t      * docs,
    sphx2_stream_t   * st)
{
    size_t           i;
    ngx_int_t        status;
    sphx2_doc_t    * d;

    d = docs;

    for(i = 0; i < num_docs; ++i) {
        status = sphx2_stream_write_string(st, d->doc);

        if(NGX_OK != status) return(status);

        d = d->next;
    }

    return(NGX_OK);
}

ngx_int_t
sphx2_create_excerpt_request(
    ngx_pool_t             * pool,
    sphx2_excerpt_input_t  * input,
    ngx_buf_t             ** b)
{
    size_t request_len = s_sphx2_excerpt_request_len(input);

    /* data to send =
     *   handshake = version [4]
     * . header = command [2] . command_version [2] . bytes following [4]
     * . request [request_len]
     */
    size_t buf_len = (2 * sz16 + 2 * sz32) + request_len;

    sphx2_stream_t* st = sphx2_stream_create(pool);

    ngx_int_t status;

    if(NGX_ERROR == sphx2_stream_alloc(st, buf_len)) {
        return(NGX_ERROR);
    }

    status =
           /* handshake */
           sphx2_stream_write_int32(st, (uint32_t)SPHX2_CLI_VERSION)
           /* header - command */
        || sphx2_stream_write_int16(st, (uint16_t)SPHX2_COMMAND_EXCERPT)
           /* header - command ver */
        || sphx2_stream_write_int16(st, (uint16_t)SPHX2_VER_COMMAND_EXCERPT)
           /* bytes after this variable in the request */
        || sphx2_stream_write_int32(st, (uint32_t)request_len)
           /* the excerpt request here onwards ... */
        || sphx2_stream_write_int32(st, (uint32_t)0) /* mode = 0 */
        || sphx2_stream_write_int32(st, input->excerpt_opts->opts_flag)
        || sphx2_stream_write_string(st, input->index)
        || sphx2_stream_write_string(st, input->keywords)
        || sphx2_stream_write_string(st, input->excerpt_opts->before_match)
        || sphx2_stream_write_string(st, input->excerpt_opts->after_match)
        || sphx2_stream_write_string(st, input->excerpt_opts->chunk_separator)
        || sphx2_stream_write_int32(st, input->excerpt_opts->limit)
        || sphx2_stream_write_int32(st, input->excerpt_opts->around)
        || sphx2_stream_write_int32(st, input->excerpt_opts->limit_passages)
        || sphx2_stream_write_int32(st, input->excerpt_opts->limit_words)
        || sphx2_stream_write_int32(st, input->excerpt_opts->start_passage_id)
        || sphx2_stream_write_string(st, input->excerpt_opts->html_strip_mode)
        || sphx2_stream_write_string(st, input->excerpt_opts->passage_boundary)
        || sphx2_stream_write_int32(st, input->num_docs)
        || ((0 != input->num_docs)
             ? s_write_docs_to_stream(input->num_docs, input->docs, st)
             : NGX_OK)
        ;

    *b = sphx2_stream_get_buf(st);

    s_dump_buffer((*b)->pos, buf_len);

    return status;
}

/* Functions to work with searchd response */

static ngx_int_t
s_sphx2_parse_response_header(
    ngx_pool_t      * pool,
    ngx_buf_t       * b,
    uint32_t        * len)
{
    ngx_int_t       status;
    sphx2_stream_t* st;
    uint32_t        searchd_proto;
    uint16_t        sphx_status;
    uint16_t        version;

    if(NULL == (st = sphx2_stream_create(pool))) {
        return(NGX_ERROR);
    }

    if(NGX_ERROR == sphx2_stream_set_buf(st, b)) {
        return(NGX_ERROR);
    }

    if(NGX_ERROR == (status =
                         sphx2_stream_read_int32(st, &searchd_proto)
                      || sphx2_stream_read_int16(st, &sphx_status)
                      || sphx2_stream_read_int16(st, &version)))
    {
        return(NGX_HTTP_UPSTREAM_INVALID_HEADER);
    }

    if(SPHX2_SEARCHD_PROTO != searchd_proto) {
        return(NGX_HTTP_UPSTREAM_INVALID_HEADER);
    }

    switch(sphx_status) {
        case SPHX2_SEARCHD_OK:
        case SPHX2_SEARCHD_ERROR:
        case SPHX2_SEARCHD_RETRY:
        case SPHX2_SEARCHD_WARNING:
            if(NGX_ERROR == sphx2_stream_read_int32(st, len)) {
                return(NGX_HTTP_UPSTREAM_INVALID_HEADER);
            }
            s_dump_buffer(b->pos, *len);
            break;
        default:
            return(NGX_HTTP_UPSTREAM_INVALID_HEADER);
    }

    return(NGX_OK);
}

ngx_int_t
sphx2_parse_search_response_header(
    ngx_pool_t                     * pool,
    ngx_buf_t                      * b,
    sphx2_search_response_ctx_t    * ctx)
{
    return(s_sphx2_parse_response_header(pool, b, &ctx->len));
}

ngx_int_t
sphx2_parse_excerpt_response_header(
    ngx_pool_t                     * pool,
    ngx_buf_t                      * b,
    sphx2_excerpt_response_ctx_t    * ctx)
{
    return(s_sphx2_parse_response_header(pool, b, &ctx->len));
}

/* Functions to work with URL query param arg parsing */

#define DEFINE_ENUM_ARG_PARSE_FUNCTION(key)    \
ngx_int_t \
sphx2_parse_ ## key ## _str( \
    ngx_pool_t          * pool, \
    ngx_str_t           * key ##_str, \
    sphx2_## key ##_t   * key) \
{ \
    int32_t  i = 0; \
 \
    sphx2_arg_parse_hint_t s_ ## key ## _hints[] = \
    { \
        { SPHX2_ARG_TYPE_ENUM, s_ ## key ## _strs, sz_ ## key ## _strs }, \
        { SPHX2_ARG_TYPE_NONE, NULL, 0 } \
    }; \
 \
    if(NULL == key ## _str || 0 == key ## _str->len) { \
        *key = sphx2_default_ ## key; \
        return(NGX_OK); \
    } \
 \
    if(NGX_ERROR == sphx2_arg_parse_register(&s_main_ctxt, \
        pool, (char*)key ## _str->data, s_ ## key ## _hints, s_no_delim)) \
    { \
        return(NGX_ERROR); \
    } \
 \
    if(NGX_ERROR == (i = \
        sphx2_arg_parse_get_enum_arg(&s_main_ctxt, NULL, 0))) \
    { \
        return(NGX_ERROR); \
    } \
 \
    *key = (sphx2_ ## key ## _t)i; \
    return NGX_OK; \
}

#define MULTI_ARG_PARSE_FUNCTION_SIGNATURE(key) \
ngx_int_t \
sphx2_parse_ ## key ## s_str( \
    ngx_pool_t        * pool, \
    ngx_str_t         * key ## s_str, \
    sphx2_## key ## _t   ** key ## s, \
    uint32_t          * num_ ## key ## s)

#define MULTI_ARG_PARSE_FUNCTION_BODY(key, elem_delim) \
    sphx2_ ## key ## _t *key; \
 \
    assert(NULL != key ## s_str && 0 != key ## s_str->len); \
 \
    *key ## s = NULL; \
    *num_ ## key ## s = 0; \
 \
    if(NGX_ERROR == sphx2_arg_parse_register(&s_main_ctxt, \
        pool, (char*)key ## s_str->data, NULL, s_multi_delim)) \
    { \
        return(NGX_ERROR); \
    } \
 \
    while(NGX_ERROR != sphx2_arg_step(&s_main_ctxt)) { \
 \
        if(NGX_ERROR == sphx2_arg_parse_register_child( \
                           &s_sec_ctxt, &s_main_ctxt, pool, \
                           s_ ## key ## _hints, elem_delim)) \
        { \
            return(NGX_ERROR); \
        } \
 \
        if(NULL == (key = \
                ngx_pcalloc(pool, sizeof(sphx2_ ## key ## _t)))) \
        { \
            return NGX_ERROR; \
        } \
 \
        if(NGX_ERROR == sphx2_arg_parse_whole_using_hints(&s_sec_ctxt, key)) \
        { \
            return NGX_ERROR; \
        } \
 \
        LIST_ADD(*key ## s, key, *num_ ## key ## s); \
    } \
 \
    return NGX_OK;

#define SET_ARG_PARSE_FUNCTION_SIGNATURE(key) \
ngx_int_t \
sphx2_parse_ ## key ## _str( \
    ngx_pool_t          * pool, \
    ngx_str_t           * key ## _str, \
    sphx2_ ## key ## _t ** key) \

#define SET_ARG_PARSE_FUNCTION_BODY(key) \
    if(NGX_ERROR == sphx2_arg_parse_register(&s_main_ctxt, \
        pool, (char*)key ## _str->data, s_ ## key ## _hints, s_set_delim)) \
    { \
        return(NGX_ERROR); \
    } \
 \
    if(NULL == (*key = ngx_pcalloc(pool, sizeof(sphx2_ ## key ## _t)))) { \
        return NGX_ERROR; \
    } \
 \
    return(sphx2_arg_parse_whole_using_hints(&s_main_ctxt, *key));


DEFINE_ENUM_ARG_PARSE_FUNCTION(match_mode)
DEFINE_ENUM_ARG_PARSE_FUNCTION(ranker)
DEFINE_ENUM_ARG_PARSE_FUNCTION(sort_mode)
DEFINE_ENUM_ARG_PARSE_FUNCTION(output_type)

MULTI_ARG_PARSE_FUNCTION_SIGNATURE(weight)
{
    sphx2_arg_parse_hint_t s_weight_hints[] =
    {
        { SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_NONE, NULL, 0 }
    };

    MULTI_ARG_PARSE_FUNCTION_BODY(weight, s_key_val_delim)
}

MULTI_ARG_PARSE_FUNCTION_SIGNATURE(filter)
{
    sphx2_arg_parse_hint_t s_filter_hints[] =
    {
        { SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_ENUM, s_filter_exclude_strs, sz_filter_exclude_strs },
        { SPHX2_ARG_TYPE_ENUM, s_filter_type_strs, sz_filter_type_strs },
        { SPHX2_ARG_TYPE_INTEGER64, NULL, 0 },
        { SPHX2_ARG_TYPE_INTEGER64, NULL, 0 },
        { SPHX2_ARG_TYPE_NONE, NULL, 0 }
    };

    MULTI_ARG_PARSE_FUNCTION_BODY(filter, s_set_delim)
}

static ngx_str_t s_dflt_sort = ngx_string("@group desc");
static ngx_str_t s_empty_str = ngx_null_string;

SET_ARG_PARSE_FUNCTION_SIGNATURE(group)
{
    sphx2_arg_parse_hint_t s_group_hints[] =
    {
        { SPHX2_ARG_TYPE_ENUM, s_group_type_strs, sz_group_type_strs },
        { SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_NONE, NULL, 0 }
    };

    /* there must be a default group specification as per Sphinx 2.0 protocol */
    if(NULL == group_str || 0 == group_str->len) {

        if(NULL == (*group = ngx_pcalloc(pool, sizeof(sphx2_group_t)))) {
            return NGX_ERROR;
        }

        (*group)->type = SPHX2_GROUPBY_DAY;
        (*group)->sort = &s_dflt_sort;
        (*group)->attr = &s_empty_str;
        (*group)->distinct = &s_empty_str;

        return(NGX_OK);
    }

    SET_ARG_PARSE_FUNCTION_BODY(group)
}

SET_ARG_PARSE_FUNCTION_SIGNATURE(geo)
{
    sphx2_arg_parse_hint_t s_geo_hints[] =
    {
        { SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_FLOAT, NULL, 0 },
        { SPHX2_ARG_TYPE_FLOAT, NULL, 0 },
        { SPHX2_ARG_TYPE_NONE, NULL, 0 }
    };

    assert(NULL != geo_str && 0 != geo_str->len);

    SET_ARG_PARSE_FUNCTION_BODY(geo)
}

MULTI_ARG_PARSE_FUNCTION_SIGNATURE(doc)
{
    sphx2_arg_parse_hint_t s_doc_hints[] =
    {
        { SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_NONE, NULL, 0 }
    };

    MULTI_ARG_PARSE_FUNCTION_BODY(doc, s_no_delim)
}

SET_ARG_PARSE_FUNCTION_SIGNATURE(excerpt_opts)
{
    sphx2_arg_parse_hint_t s_excerpt_opts_hints[] =
    {
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_STRING, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_KEYVAL | SPHX2_ARG_TYPE_INTEGER, NULL, 0 },
        { SPHX2_ARG_TYPE_NONE, NULL, 0 }
    };

    static ngx_str_t s_default_before_match = ngx_string("<b>");
    static ngx_str_t s_default_after_match = ngx_string("</b>");
    static ngx_str_t s_default_chunk_separator = ngx_string(" ... ");
    static uint32_t s_default_limit = 256;
    static uint32_t s_default_around = 5;
    static uint32_t s_default_start_passage_id = 1;
    static ngx_str_t s_default_html_strip_mode = ngx_string("index");
    static ngx_str_t s_default_passage_boundary = ngx_string("none");

    if(NULL == excerpt_opts_str || 0 == excerpt_opts_str->len) {

        if(NULL == (*excerpt_opts = ngx_pcalloc(pool,
                                       sizeof(sphx2_excerpt_opts_t))))
        {
            return NGX_ERROR;
        }

        (*excerpt_opts)->before_match = &s_default_before_match;
        (*excerpt_opts)->after_match = &s_default_after_match;
        (*excerpt_opts)->chunk_separator = &s_default_chunk_separator;
        (*excerpt_opts)->limit = s_default_limit;
        (*excerpt_opts)->limit_passages = 0;
        (*excerpt_opts)->limit_words = 0;
        (*excerpt_opts)->around = s_default_around;
        (*excerpt_opts)->exact_phrase = 0;
        (*excerpt_opts)->single_passage = 0;
        (*excerpt_opts)->use_boundaries = 0;
        (*excerpt_opts)->weight_order = 0;
        (*excerpt_opts)->query_mode = 0;
        (*excerpt_opts)->force_all_words = 0;
        (*excerpt_opts)->start_passage_id = s_default_start_passage_id;
        (*excerpt_opts)->load_files = 0;
        (*excerpt_opts)->html_strip_mode = &s_default_html_strip_mode;
        (*excerpt_opts)->allow_empty = 0;
        (*excerpt_opts)->passage_boundary = &s_default_passage_boundary;
        (*excerpt_opts)->emit_zones = 0;
        (*excerpt_opts)->load_files_scattered = 0;

        return(NGX_OK);
    }

    SET_ARG_PARSE_FUNCTION_BODY(excerpt_opts)
}

void
sphx2_create_opts_flag(sphx2_excerpt_opts_t * excerpt_opts)
{
    excerpt_opts->opts_flag = 1;

    if ( excerpt_opts->exact_phrase != 0 )    excerpt_opts->opts_flag |= 2;
    if ( excerpt_opts->single_passage != 0 )  excerpt_opts->opts_flag |= 4;
    if ( excerpt_opts->use_boundaries != 0 )  excerpt_opts->opts_flag |= 8;
    if ( excerpt_opts->weight_order != 0 )    excerpt_opts->opts_flag |= 16;
    if ( excerpt_opts->query_mode != 0 )      excerpt_opts->opts_flag |= 32;
    if ( excerpt_opts->force_all_words != 0 ) excerpt_opts->opts_flag |= 64;
    if ( excerpt_opts->load_files != 0 )      excerpt_opts->opts_flag |= 128;
    if ( excerpt_opts->allow_empty != 0 )     excerpt_opts->opts_flag |= 256;
    if ( excerpt_opts->emit_zones != 0 )      excerpt_opts->opts_flag |= 512;
    if ( excerpt_opts->load_files_scattered != 0 )    excerpt_opts->opts_flag |= 1024;
}
