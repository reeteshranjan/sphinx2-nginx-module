/*
 * Stream abstraction for ngx_buf_t
 */

#ifndef NGX_HTTP_SPHINX2_STREAM_H
#define NGX_HTTP_SPHINX2_STREAM_H

/* TYPES */

typedef struct sphx2_stream_s sphx2_stream_t;

/* PROTOTYPES */

/* create stream */
sphx2_stream_t*
sphx2_stream_create(ngx_pool_t*);

/* allocate buffer (for writes) */
ngx_int_t
sphx2_stream_alloc(sphx2_stream_t * strm, size_t len);

/* set given buffer (for reads) */
ngx_int_t
sphx2_stream_set_buf(sphx2_stream_t * strm, ngx_buf_t * b);

/* get the buffer */
ngx_buf_t*
sphx2_stream_get_buf(sphx2_stream_t * strm);

/* get current offset in stream */
ngx_uint_t
sphx2_stream_offset(sphx2_stream_t * strm);

/* get max size of stream */
ngx_uint_t
sphx2_stream_maxsize(sphx2_stream_t * strm);

/* writes */
ngx_int_t
sphx2_stream_write_int16(sphx2_stream_t * strm, uint16_t val);

ngx_int_t
sphx2_stream_write_int32(sphx2_stream_t * strm, uint32_t val);

ngx_int_t
sphx2_stream_write_int64(sphx2_stream_t * strm, uint64_t val);

ngx_int_t
sphx2_stream_write_float(sphx2_stream_t * strm, float val);

ngx_int_t
sphx2_stream_write_string(sphx2_stream_t * strm, ngx_str_t * val);

/* reads */
ngx_int_t
sphx2_stream_read_int16(sphx2_stream_t * strm, uint16_t * val);

ngx_int_t
sphx2_stream_read_int32(sphx2_stream_t * strm, uint32_t * val);

ngx_int_t
sphx2_stream_read_int64(sphx2_stream_t * strm, uint64_t * val);

ngx_int_t
sphx2_stream_read_float(sphx2_stream_t * strm, float * val);

ngx_int_t
sphx2_stream_read_string(sphx2_stream_t * strm, ngx_str_t ** val);

#endif /* NGX_HTTP_SPHINX2_STREAM_H */
