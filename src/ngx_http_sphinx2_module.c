/*
 * Sphinx2 upstream module
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_sphinx2_sphx.h"

/* TYPES */

typedef enum {
    SPHX2_ARG_OFFSET = 0,
    SPHX2_ARG_NUM_RESULTS,
    SPHX2_ARG_MATCH_MODE,
    SPHX2_ARG_RANKER,
    SPHX2_ARG_RANK_EXPR,
    SPHX2_ARG_SORT_MODE,
    SPHX2_ARG_SORT_BY,
    SPHX2_ARG_KEYWORDS,
    SPHX2_ARG_INDEX,
    SPHX2_ARG_FILTERS,
    SPHX2_ARG_GROUP,
    SPHX2_ARG_MAX_MATCHES,
    SPHX2_ARG_GEO,
    SPHX2_ARG_INDEX_WEIGHTS,
    SPHX2_ARG_FIELD_WEIGHTS,
    SPHX2_ARG_OUTPUT_FORMAT,
    SPHX2_ARG_DOCID,
    SPHX2_ARG_COUNT
} sphx2_args_t;

typedef struct {
    ngx_http_upstream_conf_t       upstream;
    ngx_int_t                      cmd_idx;
    ngx_int_t                      arg_idx[SPHX2_ARG_COUNT];
} ngx_http_sphinx2_loc_conf_t;

typedef struct {
    ngx_http_request_t            *request;
    sphx2_response_ctx_t           repctx;
} ngx_http_sphinx2_ctx_t;


/* PROTOTYPES */

static void      * ngx_http_sphinx2_create_loc_conf(ngx_conf_t *cf);
static char      * ngx_http_sphinx2_merge_loc_conf(ngx_conf_t *cf, void 
                       *parent, void *child);

static ngx_int_t   ngx_http_sphinx2_handler(ngx_http_request_t *r);
static ngx_int_t   ngx_http_sphinx2_create_request(ngx_http_request_t *r);
static ngx_int_t   ngx_http_sphinx2_reinit_request(ngx_http_request_t *r);
static ngx_int_t   ngx_http_sphinx2_process_header(ngx_http_request_t *r);
#if 0
static ngx_int_t   ngx_http_sphinx2_filter_init(void *data);
static ngx_int_t   ngx_http_sphinx2_filter(void *data, ssize_t bytes);
#endif
static void        ngx_http_sphinx2_abort_request(ngx_http_request_t *r);
static void        ngx_http_sphinx2_finalize_request(ngx_http_request_t *r, 
ngx_int_t rc);

static char      * ngx_http_sphinx2_pass(ngx_conf_t *cf, ngx_command_t *cmd, 
                       void *conf);

/* LOCALS */

static ngx_str_t  ngx_http_sphinx2_command = ngx_string("sphinx2_command");

static ngx_str_t ngx_http_sphinx2_args[] = {
    ngx_string("sphx_offset"),       /* SPHX2_ARG_OFFSET */
    ngx_string("sphx_numresults"),   /* SPHX2_ARG_NUM_RESULTS */
    ngx_string("sphx_matchmode"),    /* SPHX2_ARG_MATCH_MODE */
    ngx_string("sphx_ranker"),       /* SPHX2_ARG_RANKER */
    ngx_string("sphx_rankexpr"),     /* SPHX2_ARG_RANK_EXPR */
    ngx_string("sphx_sortmode"),     /* SPHX2_ARG_SORT_MODE */
    ngx_string("sphx_sortby"),       /* SPHX2_ARG_SORT_BY */
    ngx_string("sphx_keywords"),     /* SPHX2_ARG_KEYWORDS */
    ngx_string("sphx_index"),        /* SPHX2_ARG_INDEX */
    ngx_string("sphx_filters"),      /* SPHX2_ARG_FILTERS */
    ngx_string("sphx_group"),        /* SPHX2_ARG_GROUP */
    ngx_string("sphx_maxmatches"),   /* SPHX2_ARG_MAX_MATCHES */
    ngx_string("sphx_geo"),          /* SPHX2_ARG_GEO */
    ngx_string("sphx_indexweights"), /* SPHX2_ARG_INDEX_WEIGHTS */
    ngx_string("sphx_fieldweights"), /* SPHX2_ARG_FIELD_WEIGHTS */
    ngx_string("sphx_outputtype"),   /* SPHX2_ARG_OUTPUT_FORMAT */
    ngx_string("sphx_docid"),        /* SPHX2_ARG_DOCID */
};

static const char* sphx2_command_strs[] = {
    "search",  /* SPHX2_COMMAND_SEARCH =      0, */
    "excerpt", /* SPHX2_COMMAND_EXCERPT =     1 */
    NULL
};

/* MODULE GLOBALS */

static ngx_conf_bitmask_t ngx_http_sphinx2_next_upstream_masks[] = {
    { ngx_string("error"),            NGX_HTTP_UPSTREAM_FT_ERROR },
    { ngx_string("timeout"),          NGX_HTTP_UPSTREAM_FT_TIMEOUT },
    { ngx_string("invalid_response"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
    { ngx_string("not_found"),        NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { ngx_string("off"),              NGX_HTTP_UPSTREAM_FT_OFF },
    { ngx_null_string, 0 }
};


static ngx_command_t ngx_http_sphinx2_commands[] = {

    /* module specific commands */
    { ngx_string("sphinx2_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
      ngx_http_sphinx2_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* standard ones for upstream module */
    { ngx_string("sphinx2_bind"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_upstream_bind_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sphinx2_loc_conf_t, upstream.local),
      NULL },

    { ngx_string("sphinx2_connect_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sphinx2_loc_conf_t, upstream.connect_timeout),
      NULL },

    { ngx_string("sphinx2_send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sphinx2_loc_conf_t, upstream.send_timeout),
      NULL },

    { ngx_string("sphinx2_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sphinx2_loc_conf_t, upstream.buffer_size),
      NULL },

    { ngx_string("sphinx2_read_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sphinx2_loc_conf_t, upstream.read_timeout),
      NULL },

    { ngx_string("sphinx2_next_upstream"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sphinx2_loc_conf_t, upstream.upstream),
      &ngx_http_sphinx2_next_upstream_masks },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_sphinx2_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_sphinx2_create_loc_conf,      /* create location configration */
    ngx_http_sphinx2_merge_loc_conf        /* merge location configration */
};


ngx_module_t ngx_http_sphinx2_module = {
    NGX_MODULE_V1,
    &ngx_http_sphinx2_module_ctx,          /* module context */
    ngx_http_sphinx2_commands,             /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


/* FUNCTION DEFINITIONS */

/* location conf creation */
static void*
ngx_http_sphinx2_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_sphinx2_loc_conf_t  *conf;
    size_t i;

    if(NULL == (conf = ngx_pcalloc(cf->pool, 
                           sizeof(ngx_http_sphinx2_loc_conf_t))))
    {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->upstream.bufs.num = 0;
     *     conf->upstream.upstream = 0;
     *     conf->upstream.temp_path = NULL;
     *     conf->upstream.uri = { 0, NULL };
     *     conf->upstream.location = NULL;
     */

    conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;

    conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;

    /* the hardcoded values */
    conf->upstream.cyclic_temp_file = 0;
    conf->upstream.buffering = 0;
    conf->upstream.ignore_client_abort = 0;
    conf->upstream.send_lowat = 0;
    conf->upstream.bufs.num = 0;
    conf->upstream.busy_buffers_size = 0;
    conf->upstream.max_temp_file_size = 0;
    conf->upstream.temp_file_write_size = 0;
    conf->upstream.intercept_errors = 1;
    conf->upstream.intercept_404 = 1;
    conf->upstream.pass_request_headers = 0;
    conf->upstream.pass_request_body = 0;

    /* initialize module specific elements of the context */
    conf->cmd_idx = NGX_CONF_UNSET;
    for(i = 0; i < SPHX2_ARG_COUNT; ++i) {
        conf->arg_idx[i] = NGX_CONF_UNSET;
    }

    return conf;
}

/* location conf merge */
static char*
ngx_http_sphinx2_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_sphinx2_loc_conf_t *prev = parent;
    ngx_http_sphinx2_loc_conf_t *conf = child;
    size_t i;

    ngx_conf_merge_msec_value(conf->upstream.connect_timeout, 
                              prev->upstream.connect_timeout, 60000);
    ngx_conf_merge_msec_value(conf->upstream.send_timeout, 
                              prev->upstream.send_timeout, 60000);
    ngx_conf_merge_msec_value(conf->upstream.read_timeout, 
                              prev->upstream.read_timeout, 60000);

    ngx_conf_merge_size_value(conf->upstream.buffer_size, 
                              prev->upstream.buffer_size, (size_t)ngx_pagesize);

    ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
                                 prev->upstream.next_upstream,
                                 (NGX_CONF_BITMASK_SET | 
                                      NGX_HTTP_UPSTREAM_FT_ERROR |
                                      NGX_HTTP_UPSTREAM_FT_TIMEOUT));

    if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.next_upstream =
            NGX_CONF_BITMASK_SET | NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (conf->upstream.upstream == NULL) {
        conf->upstream.upstream = prev->upstream.upstream;
    }

    if(conf->cmd_idx == NGX_CONF_UNSET) {
        conf->cmd_idx = prev->cmd_idx;
    }
    for(i = 0; i < SPHX2_ARG_COUNT; ++i) {
        if(conf->arg_idx[i] == NGX_CONF_UNSET) {
            conf->arg_idx[i] = prev->arg_idx[i];
        }
    }

    return NGX_CONF_OK;
}

/* pass */
static char*
ngx_http_sphinx2_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_sphinx2_loc_conf_t *slcf = conf;
    ngx_str_t                  *value;
    ngx_url_t                   url;
    ngx_http_core_loc_conf_t   *clcf;
    size_t                      i;

    if (slcf->upstream.upstream) {
        return "is duplicate";
    }

    value = cf->args->elts;

    ngx_memzero(&url, sizeof(ngx_url_t));

    url.url = value[1];
    url.no_resolve = 1;

    slcf->upstream.upstream = ngx_http_upstream_add(cf, &url, 0);
    if (slcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_sphinx2_handler;

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    if(NGX_ERROR == (slcf->cmd_idx = ngx_http_get_variable_index(
                                      cf, &ngx_http_sphinx2_command)))
    {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0,
             "Can't get variable index for 'sphinx2_command'");
        return NGX_CONF_ERROR;
    }

    for(i = 0; i < SPHX2_ARG_COUNT; ++i) {
        if(NGX_ERROR == (slcf->arg_idx[i] = ngx_http_get_variable_index(
                                      cf, &ngx_http_sphinx2_args[i])))
        {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                 "Can't get variable index for '%s' key",
                    ngx_http_sphinx2_args[i].data);
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

/* upstream handler to provide the callbacks */
ngx_int_t
ngx_http_sphinx2_handler(ngx_http_request_t *r)
{
    ngx_int_t                        rc;
    ngx_http_upstream_t             *u;
    ngx_http_sphinx2_ctx_t          *ctx;
    ngx_http_sphinx2_loc_conf_t     *slcf;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "sphinx2_handler: http method is not GET or HEAD");
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (ngx_http_set_content_type(r) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "sphinx2_handler: failed in set_content_type");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_upstream_create(r) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "sphinx2_handler: failed to create upstream");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    u = r->upstream;

    /*ngx_str_set(&u->schema, "sphinx2://"); */
    ngx_str_set(&u->schema, "");
    u->output.tag = (ngx_buf_tag_t) &ngx_http_sphinx2_module;

    slcf = ngx_http_get_module_loc_conf(r, ngx_http_sphinx2_module);

    u->conf = &slcf->upstream;

    u->create_request = ngx_http_sphinx2_create_request;
    u->reinit_request = ngx_http_sphinx2_reinit_request;
    u->process_header = ngx_http_sphinx2_process_header;
    u->abort_request = ngx_http_sphinx2_abort_request;
    u->finalize_request = ngx_http_sphinx2_finalize_request;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_sphinx2_ctx_t));
    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* TODO upstream context's sphinx specific members init here */

    ngx_http_set_ctx(r, ctx, ngx_http_sphinx2_module);

    /*u->input_filter_init = ngx_http_sphinx2_filter_init;
    u->input_filter = ngx_http_sphinx2_filter;
    u->input_filter_ctx = ctx;*/

    rc = ngx_http_read_client_request_body(r, ngx_http_upstream_init);

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}

/* parse search arguments */

static ngx_str_t s_empty_str = ngx_null_string;

ngx_int_t
ngx_http_sphinx2_parse_search_args(
    ngx_http_request_t                  * r,
    ngx_http_sphinx2_loc_conf_t         * slcf,
    sphx2_search_input_t                * srch_input)
{
#define GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no) \
    ngx_int_t i = slcf->arg_idx[arg_no]; \
    ngx_http_variable_value_t * vv = ngx_http_get_indexed_variable(r, i); \
    if (vv == NULL || vv->not_found) { \
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, \
            "'%s' variable is not set", ngx_http_sphinx2_args[arg_no].data); \
        return NGX_ERROR; \
    } \
    ngx_str_t* vvs = ngx_palloc(r->pool, sizeof(ngx_str_t)); \
    if(NULL == vvs) { return NGX_ERROR; } \
    if(vv->len != 0) { \
        if(NULL == (vvs->data = ngx_palloc(r->pool, vv->len))) { \
            return NGX_ERROR; \
        } \
        memcpy(vvs->data, vv->data, vv->len); \
    } else { vvs->data = NULL; } \
    vvs->len = vv->len;

#define MUST_HAVE_ARG(arg_no) \
do { \
    GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no); \
    if(0 == vvs->len) { \
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, \
            "No value for arg '%s' specified\n", \
            ngx_http_sphinx2_args[arg_no].data); \
        return NGX_ERROR; \
    } \
} while(0)

#define GET_ARG(arg_no, var) \
do { \
    GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no); \
    srch_input->var = vvs; \
} while(0)

#define PARSE_INT_ARG(arg_no, var, dflt)   \
do { \
    GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no); \
    if(vvs->len != 0) { \
        srch_input->var = atoi((const char*)(vvs->data)); \
    } else { srch_input->var = dflt; } \
} while(0)

#define PARSE_LIST_ARG(arg_no, key) \
do { \
    GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no); \
    if(vvs->len != 0 && NGX_OK != sphx2_parse_ ## key ## _str( \
                r->pool, vvs, &(srch_input->key), &(srch_input->num_ ## key))) \
    { return NGX_ERROR; } \
} while(0)

#define PARSE_ELEM_ARG(arg_no, key) \
do { \
    GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no); \
    if(vvs->len != 0 && NGX_OK != sphx2_parse_ ## key ## _str( \
                             r->pool, vvs, &srch_input->key)) \
    { return NGX_ERROR; } \
} while(0)

#define PARSE_ELEM_ARG_2(arg_no, key) \
do { \
    GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no); \
    if(NGX_OK != sphx2_parse_ ## key ## _str( \
                             r->pool, vvs, &srch_input->key)) \
    { return NGX_ERROR; } \
} while(0)

    MUST_HAVE_ARG(SPHX2_ARG_KEYWORDS); 

    MUST_HAVE_ARG(SPHX2_ARG_INDEX);

    /* offset */
    PARSE_INT_ARG(SPHX2_ARG_OFFSET, offset, 0);

    /* num-results */
    PARSE_INT_ARG(SPHX2_ARG_NUM_RESULTS, num_results,
                  sphx2_default_num_results);

    /* match mode */
    PARSE_ELEM_ARG(SPHX2_ARG_MATCH_MODE, match_mode);

    /* ranker */
    PARSE_ELEM_ARG(SPHX2_ARG_RANKER, ranker);

    /* rank expression */
    if(SPHX2_RANK_EXPR == srch_input->ranker) {
        GET_ARG(SPHX2_ARG_RANK_EXPR, rank_expr);
    }

    /* sort_mode */
    PARSE_ELEM_ARG(SPHX2_ARG_SORT_MODE, sort_mode);

    /* sort by */
    if(SPHX2_SORT_ATTR_ASC == srch_input->sort_mode ||
       SPHX2_SORT_ATTR_DESC == srch_input->sort_mode)
    {
        GET_ARG(SPHX2_ARG_SORT_BY, sort_by);
    } else {
        srch_input->sort_by = &s_empty_str;
    }

    /* keywords */
    GET_ARG(SPHX2_ARG_KEYWORDS, keywords);

    /* index */
    GET_ARG(SPHX2_ARG_INDEX, index);

    /* filters */
    PARSE_LIST_ARG(SPHX2_ARG_FILTERS, filters);

    /* group */
    PARSE_ELEM_ARG_2(SPHX2_ARG_GROUP, group);

    /* max matches */
    PARSE_INT_ARG(SPHX2_ARG_MAX_MATCHES, max_matches,
                  sphx2_default_max_matches);

    /* geo */
    PARSE_ELEM_ARG(SPHX2_ARG_GEO, geo);

    /* index weights */
    PARSE_LIST_ARG(SPHX2_ARG_INDEX_WEIGHTS, index_weights);

    /* field weights */
    PARSE_LIST_ARG(SPHX2_ARG_FIELD_WEIGHTS, field_weights);

    /* Output format */
    PARSE_ELEM_ARG(SPHX2_ARG_OUTPUT_FORMAT, output_type);

    return(NGX_OK);
}


/* create request callback */
static ngx_int_t
ngx_http_sphinx2_create_request(ngx_http_request_t *r)
{
    ngx_buf_t                      * b;
    ngx_chain_t                    * cl;
    ngx_http_sphinx2_loc_conf_t    * slcf;
    ngx_http_sphinx2_ctx_t         * ctx;
    ngx_http_variable_value_t      * vv;
    sphx2_input_t                    input;
    sphx2_command_t                  cmd;
    ngx_str_t                        dbg;
 
    slcf = ngx_http_get_module_loc_conf(r, ngx_http_sphinx2_module);

    /* find the sphinx2 command */
    vv = ngx_http_get_indexed_variable(r, slcf->cmd_idx);

    if (vv == NULL || vv->not_found || vv->len == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "the \"sphinx2_command\" variable is not set");
        return NGX_ERROR;
    }

    for(cmd = SPHX2_COMMAND_SEARCH; cmd < SPHX2_COMMAND_COUNT; ++cmd) {
        if(!strncmp(sphx2_command_strs[cmd],
                    (const char*)vv->data, vv->len)) {
            break;
        }
    }

    if(cmd == SPHX2_COMMAND_COUNT) {
        dbg.data = vv->data; dbg.len = vv->len;
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
             "Sphinx command \"%V\" is not recognized", &dbg);
        return(NGX_ERROR);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_sphinx2_module);

    switch(cmd) {
        case SPHX2_COMMAND_SEARCH:
            memset(&input, 0, sizeof(input));
            if(NGX_OK != ngx_http_sphinx2_parse_search_args(
                             r, slcf, &input.srch)) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "Sphinx2 query args parse error");
                return(NGX_ERROR);
            }
            if(NGX_ERROR == sphx2_create_search_request(r->pool,
                                                        &input.srch, &b))
            {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "Sphinx2 upstream search req creation failed");
                return(NGX_ERROR);
            }
            break;
        case SPHX2_COMMAND_EXCERPT:
        default:
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "Sphinx2 upstream unsupported req type - %u", cmd);
            return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;
    cl->next = NULL;

    r->upstream->request_bufs = cl;

    ctx->request = r;

    dbg.data = b->pos;
    dbg.len = b->last - b->pos;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "sphinx2 request: \"%V\"", &dbg);

    return NGX_OK;
}


static ngx_int_t
ngx_http_sphinx2_reinit_request(ngx_http_request_t *r)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_sphinx2_process_header(ngx_http_request_t *r)
{
    ngx_http_upstream_t        * u;
    ngx_http_sphinx2_ctx_t     * ctx;
    ngx_buf_t                  * b;
    ngx_int_t                    status;

    u = r->upstream;
    b = &u->buffer;

    /* not enough bytes read to parse status */
    if((b->last - b->pos) < (ssize_t)sphx2_min_search_header_len) {
        return(NGX_AGAIN);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_sphinx2_module);

    if(NGX_OK != (status =
            sphx2_parse_search_response_header(r->pool, b, &ctx->repctx.srch)))
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "Sphinx2 upstream error processing header");
        return status;
    }

    u->headers_in.status_n = NGX_HTTP_OK;
    u->state->status = NGX_HTTP_OK;

    return NGX_OK;
}

#if 0
static ngx_int_t
ngx_http_sphinx2_filter_init(void *data)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_sphinx2_filter(void *data, ssize_t bytes)
{
    /*ngx_http_sphinx2_ctx_t  *ctx = data;*/

    /* TODO filter impl here */
    return 0; /*ctx->filter(ctx, bytes);*/
}
#endif

static void
ngx_http_sphinx2_abort_request(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "abort http sphinx2 request");
    return;
}


static void
ngx_http_sphinx2_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http sphinx2 request");
    return;
}
