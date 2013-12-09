/*
 * Search query string parameters parsing
 */

#ifndef SPHX2_QUERY_STRING_PARAMS_PARSING_H
#define SPHX2_QUERY_STRING_PARAMS_PARSING_H


/* TYPES  */

typedef enum {
    SPHX2_ARG_TYPE_NONE =  0,
    SPHX2_ARG_TYPE_INTEGER = 0x01,
    SPHX2_ARG_TYPE_INTEGER64 = 0x02,
    SPHX2_ARG_TYPE_FLOAT = 0x04,
    SPHX2_ARG_TYPE_STRING = 0x08,
    SPHX2_ARG_TYPE_ENUM = 0x10,
    SPHX2_ARG_TYPE_KEYVAL = 0x1000
} sphx2_arg_type_t;

#define SPHX2_ARG_TYPE_MASK  0x1F

typedef struct {
    sphx2_arg_type_t     param_type;
    const char        ** str_arr;
    size_t               sz_str_arr;
} sphx2_arg_parse_hint_t;

typedef struct {
    char                     * input;
    sphx2_arg_parse_hint_t   * hints;
    const char               * delimiter;
    char                     * curr;
    size_t                     num_tokens;
    ngx_pool_t               * pool;
} sphx2_arg_parse_ctx_t;


/* PROTOTYPES */

/* register the input, hints and delimiter to create a parsing context */
ngx_int_t
sphx2_arg_parse_register(
    sphx2_arg_parse_ctx_t    * ctxt,
    ngx_pool_t               * pool,
    char                     * input,
    sphx2_arg_parse_hint_t   * hints,
    const char               * delimiter);

/* register current 'token' of a context to another to parse it further */
ngx_int_t
sphx2_arg_parse_register_child(
    sphx2_arg_parse_ctx_t    * to,
    sphx2_arg_parse_ctx_t    * from,
    ngx_pool_t               * pool,
    sphx2_arg_parse_hint_t   * hints,
    const char               * delimiter);

/* take one tokenizing step, so curr and input pointers are modified */
ngx_int_t
sphx2_arg_step(sphx2_arg_parse_ctx_t * ctxt);

/* parse the whole input using the hints and copy the results in the
 * order specified by hints array into the target pointer arg assuming 
 * it is a struct pointer with elements of types specified in hints
 * occurring in that order 
 */
ngx_int_t
sphx2_arg_parse_whole_using_hints(
    sphx2_arg_parse_ctx_t    * ctxt,
    void                     * ptr);

/* get a string arg - internally does a 'step' and also a check using
 * the hints if available
*/
ngx_str_t*
sphx2_arg_parse_get_str_arg(sphx2_arg_parse_ctx_t * ctxt);

/* get an integer arg - internally does a 'step' and also a check using
 * the hints if available
*/
uint32_t
sphx2_arg_parse_get_int_arg(sphx2_arg_parse_ctx_t * ctxt);

/* get an int64 arg - internally does a 'step' and also a check using
 * the hints if available
*/
uint64_t
sphx2_arg_parse_get_int64_arg(sphx2_arg_parse_ctx_t * ctxt);

/* get a float arg - internally does a 'step' and also a check using
 * the hints if available
*/
float
sphx2_arg_parse_get_float_arg(sphx2_arg_parse_ctx_t * ctxt);

/* get an enum arg - internally does a 'step' and also a check using
 * the hints if available. the matching str-arr if specified overrides
 * the hints (if specified). one of the arg here or hint must be
 * available.
*/
int32_t
sphx2_arg_parse_get_enum_arg(
    sphx2_arg_parse_ctx_t    * ctxt,
    const char               * strarr[],
    size_t                     sz_strarr);

#endif /* SPHX2_QUERY_STRING_PARAMS_PARSING_H */
