/*
 * Sphinx upstream module URI query params parsing
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <assert.h>
#include "ngx_http_sphinx2_args_parser.h"

/* FUNCTION DEFINITIONS */

/* register the input, hints and delimiter to create a parsing ctx */
ngx_int_t
sphx2_arg_parse_register(
    sphx2_arg_parse_ctx_t    * ctxt,
    ngx_pool_t               * pool,
    char                     * input,
    sphx2_arg_parse_hint_t   * hints,
    const char               * delimiter)
{
    ctxt->input = input;
    ctxt->hints = hints;
    ctxt->delimiter = delimiter;
    ctxt->curr = NULL;
    ctxt->num_tokens = 0;
    ctxt->pool = pool;

    return NGX_OK;
}

/* register current 'token' of a ctx to another to parse it further */
ngx_int_t
sphx2_arg_parse_register_child(
    sphx2_arg_parse_ctx_t    * to,
    sphx2_arg_parse_ctx_t    * from,
    ngx_pool_t               * pool,
    sphx2_arg_parse_hint_t   * hints,
    const char               * delimiter)
{
    assert(NULL != from->curr);

    return sphx2_arg_parse_register(to, pool, from->curr, hints, delimiter);
}

/* take one tokenizing step, so curr and input pointers are modified */
ngx_int_t
sphx2_arg_step(sphx2_arg_parse_ctx_t * ctxt)
{
    if(NULL == (ctxt->curr = strsep(&ctxt->input, ctxt->delimiter))) {
        return NGX_ERROR;
    }

    ++ctxt->num_tokens;

    return NGX_OK;
}

/* get a string arg - internally does a 'step' and also a check using
 * the hints if available
*/
ngx_str_t*
sphx2_arg_parse_get_str_arg(sphx2_arg_parse_ctx_t * ctxt)
{
    ngx_str_t * str;

    if(NULL != ctxt->hints &&
            SPHX2_ARG_TYPE_STRING !=
                ctxt->hints[ctxt->num_tokens].param_type)
    {
        return(NULL);
    }

    if(NGX_ERROR == sphx2_arg_step(ctxt))
        return (NULL);

    if(NULL == (str = ngx_palloc(ctxt->pool, sizeof(ngx_str_t)))) {
        return(NULL);
    }

    str->len = strlen(ctxt->curr)+1;

    if(NULL == (str->data = ngx_palloc(ctxt->pool, str->len))) {
        return(NULL);
    }

    memcpy(str->data, ctxt->curr, str->len);

    return(str);
}

/* get an integer arg - internally does a 'step' and also a check using
 * the hints if available
*/
uint32_t
sphx2_arg_parse_get_int_arg(sphx2_arg_parse_ctx_t * ctxt)
{
    if(NULL != ctxt->hints &&
            SPHX2_ARG_TYPE_INTEGER !=
                ctxt->hints[ctxt->num_tokens].param_type)
    {
        return(NGX_ERROR);
    }

    if(NGX_ERROR == sphx2_arg_step(ctxt))
        return (NGX_ERROR);

    return(atoi(ctxt->curr));
}

/* get an int64 arg - internally does a 'step' and also a check using
 * the hints if available
*/
uint64_t
sphx2_arg_parse_get_int64_arg(sphx2_arg_parse_ctx_t * ctxt)
{
    if(NULL != ctxt->hints &&
            SPHX2_ARG_TYPE_INTEGER64 !=
                ctxt->hints[ctxt->num_tokens].param_type)
    {
        return(NGX_ERROR);
    }

    if(NGX_ERROR == sphx2_arg_step(ctxt))
        return (NGX_ERROR);

    return(strtoll(ctxt->curr, NULL, 10));
}

/* get a float arg - internally does a 'step' and also a check using
 * the hints if available
*/
float
sphx2_arg_parse_get_float_arg(sphx2_arg_parse_ctx_t * ctxt)
{
    if(NULL != ctxt->hints &&
            SPHX2_ARG_TYPE_FLOAT !=
                ctxt->hints[ctxt->num_tokens].param_type)
    {
        return((float)NGX_ERROR);
    }

    if(NGX_ERROR == sphx2_arg_step(ctxt))
        return ((float)NGX_ERROR);

    return(atof(ctxt->curr));
}

/* get an enum arg - internally does a 'step' and also a check using
 * the hints if available. the matching str-arr if specified overrides
 * the hints (if specified). one of the arg here or hint must be
 * available.
*/
int32_t
sphx2_arg_parse_get_enum_arg(
    sphx2_arg_parse_ctx_t    * ctxt,
    const char               * str_arr[],
    size_t                     sz_str_arr)
{
    size_t i;

    if(NULL != ctxt->hints &&
            SPHX2_ARG_TYPE_ENUM !=
                ctxt->hints[ctxt->num_tokens].param_type)
    {
        return(NGX_ERROR);
    }

    assert(!(NULL == str_arr && NULL == ctxt->hints));

    if(NULL == str_arr) {
        str_arr = ctxt->hints[ctxt->num_tokens].str_arr;
        sz_str_arr = ctxt->hints[ctxt->num_tokens].sz_str_arr;
    }

    if(NGX_ERROR == sphx2_arg_step(ctxt))
        return (NGX_ERROR);

    for(i = 0; i < sz_str_arr; ++i) {
        if(!strncmp(ctxt->curr, str_arr[i], strlen(str_arr[i]))) {
            return (i);
        }
    }

    return(NGX_ERROR);
}

/* parse the whole input using the hints and copy the results in the
 * order specified by hints array into the target pointer arg assuming 
 * it is a struct pointer with elements of types specified in hints
 * occurring in that order 
 */
ngx_int_t
sphx2_arg_parse_whole_using_hints(
    sphx2_arg_parse_ctx_t    * ctxt,
    void                     * ptr)
{
#define APPEND_4_BYTES(p, v, a)  \
do { \
    memcpy(p, v, sizeof(uint32_t)); \
    p = (void*)((uint64_t)p + sizeof(uint32_t)); \
    a = 1 - a; \
} while(0)

#define APPEND_8_BYTES(p, v, a) \
do { \
    if(0==a) { p = (void*)((uint64_t)p + sizeof(uint32_t)); a = 1;}; \
    memcpy(p, v, sizeof(uint64_t)); \
    p = (void*)((uint64_t)p + sizeof(uint64_t)); \
} while(0)

    size_t i = 0;
    int32_t is_ptr_aligned = 1;
    union { uint32_t i; uint64_t i64; float f; ngx_str_t* s; int32_t e; } v;

    assert(NULL != ctxt->hints);

    while(SPHX2_ARG_TYPE_NONE != ctxt->hints[i].param_type) {

        switch(ctxt->hints[i].param_type) {
        case SPHX2_ARG_TYPE_INTEGER:
            if((uint32_t)NGX_ERROR == (v.i = sphx2_arg_parse_get_int_arg(ctxt)))
            {
                return(NGX_ERROR);
            }
            APPEND_4_BYTES(ptr, &v.i, is_ptr_aligned);
            break;
        case SPHX2_ARG_TYPE_INTEGER64:
            if((uint64_t)NGX_ERROR == (v.i64 = sphx2_arg_parse_get_int64_arg(ctxt)))
            {
                return(NGX_ERROR);
            }
            APPEND_8_BYTES(ptr, &v.i64, is_ptr_aligned);
            break;
        case SPHX2_ARG_TYPE_FLOAT:
            if((float)NGX_ERROR == (v.f = sphx2_arg_parse_get_float_arg(ctxt)))
            {
                return(NGX_ERROR);
            }
            APPEND_4_BYTES(ptr, &v.i, is_ptr_aligned);
            break;
        case SPHX2_ARG_TYPE_STRING:
            if(NULL == (v.s = sphx2_arg_parse_get_str_arg(ctxt)))
            {
                return(NGX_ERROR);
            }
            APPEND_8_BYTES(ptr, &v.s, is_ptr_aligned);
            break;
        case SPHX2_ARG_TYPE_ENUM:
            if((int32_t)NGX_ERROR == (v.e = sphx2_arg_parse_get_enum_arg(ctxt,
                    ctxt->hints[i].str_arr, ctxt->hints[i].sz_str_arr)))
            {
                return(NGX_ERROR);
            }
            APPEND_4_BYTES(ptr, &v.e, is_ptr_aligned);
            break;
        default:
            assert(0);
        }

        ++i;
    }

    return (NGX_OK);
}
