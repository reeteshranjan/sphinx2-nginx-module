/*
 * Sphinx2 types to work with search daemon
 */

#ifndef NGX_HTTP_SPHINX2_SPHX_H
#define NGX_HTTP_SPHINX2_SPHX_H


/* TYPES */

#define SPHX2_CLI_VERSION       1
#define SPHX2_SEARCHD_PROTO     1

/* Codes of commands to be sent to Sphinx search daemon */
typedef enum {
    SPHX2_COMMAND_NONE =       -1,
    SPHX2_COMMAND_SEARCH =      0,
    SPHX2_COMMAND_EXCERPT =     1,
#if 0
    -- not supported as of this release --

    SPHX2_COMMAND_UPDATE =      2,
    SPHX2_COMMAND_KEYWORDS =    3,
    SPHX2_COMMAND_PERSIST =     4,
    SPHX2_COMMAND_STATUS =      5,
    SPHX2_COMMAND_FLUSHATTRS =  7
#endif
    SPHX2_COMMAND_COUNT
} sphx2_command_t;

/* Versions of commands to be sent to Sphinx search daemon */
typedef enum {
    SPHX2_VER_COMMAND_SEARCH =      0x119,
    SPHX2_VER_COMMAND_EXCERPT =     0x104
#if 0
    -- not supported as of this release --
    SPHX2_VER_COMMAND_UPDATE =      0x102,
    SPHX2_VER_COMMAND_KEYWORDS =    0x100,
    SPHX2_VER_COMMAND_STATUS =      0x100,
    SPHX2_VER_COMMAND_QUERY =       0x100,
    SPHX2_VER_COMMAND_FLUSHATTRS =  0x100
#endif
} sphx2_version_no_t;

/* Return codes from Sphinx search daemon */
typedef enum {
    SPHX2_STATUS_OK =           0,
    SPHX2_STATUS_ERROR =        1,
    SPHX2_STATUS_RETRY =        2,
    SPHX2_STATUS_WARNING =      3
} sphx2_status_t;

/* Match mode to use while searching */
typedef enum {
    SPHX2_MATCH_ALL =           0,
    SPHX2_MATCH_ANY =           1,
    SPHX2_MATCH_PHRASE =        2,
    SPHX2_MATCH_BOOLEAN =       3,
    SPHX2_MATCH_EXTENDED =      4,
    SPHX2_MATCH_FULLSCAN =      5,
    SPHX2_MATCH_EXTENDED2 =     6
} sphx2_match_mode_t;

/* Ranker to use while searching */
typedef enum {
    SPHX2_RANK_PROXIMITY_BM25 = 0,
    SPHX2_RANK_BM25 =           1,
    SPHX2_RANK_NONE =           2,
    SPHX2_RANK_WORDCOUNT =      3,
    SPHX2_RANK_PROXIMITY =      4,
    SPHX2_RANK_MATCHANY =       5,
    SPHX2_RANK_FIELDMASK =      6,
    SPHX2_RANK_SPH04 =          7,
    SPHX2_RANK_EXPR =           8,
    SPHX2_RANK_TOTAL =          9
} sphx2_ranker_t;

/* Mode of sorting results obtained */
typedef enum {
    SPHX2_SORT_RELEVANCE =      0,
    SPHX2_SORT_ATTR_DESC =      1,
    SPHX2_SORT_ATTR_ASC =       2,
    SPHX2_SORT_TIME_SEGMENTS =  3,
    SPHX2_SORT_EXTENDED =       4,
    SPHX2_SORT_EXPR =           5
} sphx2_sort_mode_t;

/* Filter type */
typedef enum {
    SPHX2_FILTER_VALUES =       0,
    SPHX2_FILTER_RANGE =        1,
    SPHX2_FILTER_FLOATRANGE =   2,
} sphx2_filter_type_t;

/* Group by type */
typedef enum {
    SPHX2_GROUPBY_DAY =         0,
    SPHX2_GROUPBY_WEEK =        1,
    SPHX2_GROUPBY_MONTH =       2,
    SPHX2_GROUPBY_YEAR =        3,
    SPHX2_GROUPBY_ATTR =        4,
    SPHX2_GROUPBY_ATTRPAIR =    5,
} sphx2_group_type_t;

/* Output format type */
typedef enum {
    SPHX2_OUTPUT_RAW =          0,
    SPHX2_OUTPUT_TEXT =         1,
    SPHX2_OUTPUT_JSON =         2,
    SPHX2_OUTPUT_XML =          3,
} sphx2_output_type_t;

/* Search daemon response status */
typedef enum {
    SPHX2_SEARCHD_OK =          0,
    SPHX2_SEARCHD_ERROR =       1,
    SPHX2_SEARCHD_RETRY =       2,
    SPHX2_SEARCHD_WARNING =     3,
} sphx2_searchd_status_t;

/* Specify weight of a field */
typedef struct _sphx2_weight sphx2_weight_t;

struct _sphx2_weight {
    ngx_str_t            * entity;
    uint32_t               weight;
    sphx2_weight_t       * next;
};

/* Filter values (int64)
typedef struct _sphx2_filter_value sphx2_filter_value_t;

struct _sphx2_filter_value {
    uint64_t                           value;
    sphx2_filter_value_t  * next;
};*/

/* Filter range (int64) */
typedef struct {
    uint64_t  min;
    uint64_t  max;
} sphx2_filter_int_range_t;

/* Filter range (float) */
typedef struct {
    double     min;
    double     max;
} sphx2_filter_float_range_t;

/* Filter spec */
typedef union {
    /*struct {
        sphx2_filter_value_t  * v;
        ngx_uint_t              n;
    } vals;*/
    sphx2_filter_int_range_t   ir;
    sphx2_filter_float_range_t fr;
} sphx2_filter_spec_t;

/* Filter */
typedef struct _sphx2_filter sphx2_filter_t;

struct _sphx2_filter {
    ngx_str_t            * attr;
    int32_t                exclude;
    sphx2_filter_type_t    type;
    sphx2_filter_spec_t    spec;
    sphx2_filter_t       * next;
};

/* Grouping */
typedef struct {
    sphx2_group_type_t     type;
    ngx_str_t            * attr;
    ngx_str_t            * sort;
    ngx_str_t            * distinct;
} sphx2_group_t;

/* Geo */
typedef struct {
    ngx_str_t            * lat_attr;
    ngx_str_t            * lon_attr;
    float                  lat;
    float                  lon;
} sphx2_geo_t;

/* Input to search query */
typedef struct {
    uint32_t               offset;
    uint32_t               num_results;
    sphx2_match_mode_t     match_mode;
    sphx2_ranker_t         ranker;
    ngx_str_t            * rank_expr;
    sphx2_sort_mode_t      sort_mode;
    ngx_str_t            * sort_by;
    ngx_str_t            * keywords;
    ngx_str_t            * index;
    uint32_t               num_filters;
    sphx2_filter_t       * filters;
    sphx2_group_t        * group;
    uint32_t               max_matches;
    sphx2_geo_t          * geo;
    uint32_t               num_index_weights;
    sphx2_weight_t       * index_weights;
    uint32_t               num_field_weights;
    sphx2_weight_t       * field_weights;
    sphx2_output_type_t    output_type;
} sphx2_search_input_t;

/* Input to excerpt command */
typedef struct {
    ngx_str_t            * keywords;
    ngx_str_t            * index;
    uint32_t               doc_id;
} sphx2_excerpt_input_t;

/* Input - union */
typedef union {
    sphx2_search_input_t   srch;
    sphx2_excerpt_input_t  exrp;
} sphx2_input_t;

/* Search response context */
typedef struct {
    uint32_t               len;
} sphx2_search_response_ctx_t;

/* Excerpt command response */
typedef struct {
    uint32_t               len;
} sphx2_excerpt_response_ctx_t;

/* Response context */
typedef union {
    sphx2_search_response_ctx_t     srch;
    sphx2_excerpt_response_ctx_t    excrpt;
} sphx2_response_ctx_t;


/* FUNCTION PROTOTYPES */

/* Parse URL arguments */
ngx_int_t
sphx2_parse_match_mode_str(ngx_pool_t*, ngx_str_t*, sphx2_match_mode_t*);

ngx_int_t
sphx2_parse_ranker_str(ngx_pool_t*, ngx_str_t*, sphx2_ranker_t*);

ngx_int_t
sphx2_parse_sort_mode_str(ngx_pool_t*, ngx_str_t*, sphx2_sort_mode_t*);

ngx_int_t
sphx2_parse_weights_str(ngx_pool_t*, ngx_str_t*, sphx2_weight_t**, uint32_t*);

#define sphx2_parse_index_weights_str sphx2_parse_weights_str
#define sphx2_parse_field_weights_str sphx2_parse_weights_str

ngx_int_t
sphx2_parse_filters_str(ngx_pool_t*, ngx_str_t*, sphx2_filter_t**, uint32_t*);

ngx_int_t
sphx2_parse_group_str(ngx_pool_t*, ngx_str_t*, sphx2_group_t**);

ngx_int_t
sphx2_parse_output_type_str(ngx_pool_t*, ngx_str_t*, sphx2_output_type_t*);

ngx_int_t
sphx2_parse_geo_str(ngx_pool_t*, ngx_str_t*, sphx2_geo_t**);


/* Requests & Responses */
ngx_int_t  
sphx2_create_search_request(ngx_pool_t*, sphx2_search_input_t*, ngx_buf_t**);

ngx_int_t
sphx2_parse_search_response_header(ngx_pool_t*, ngx_buf_t*,
    sphx2_search_response_ctx_t*);

/* GLOBALS */

extern sphx2_match_mode_t  sphx2_default_match_mode;

extern sphx2_ranker_t      sphx2_default_ranker;

extern sphx2_sort_mode_t   sphx2_default_sort_mode;

extern sphx2_output_type_t sphx2_default_output_type;

extern ngx_uint_t          sphx2_default_num_results;

extern ngx_uint_t          sphx2_default_max_matches;

extern size_t              sphx2_min_search_header_len;

#endif /* NGX_HTTP_SPHINX2_SPHX_H */
