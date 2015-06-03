#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>


#define NGX_HTTP_SYSTAT_FORMAT_PLAIN    0
#define NGX_HTTP_SYSTAT_FORMAT_XML      1

#define NGX_HTTP_SYSTAT_IFS_TXBYTES     0x0001
#define NGX_HTTP_SYSTAT_IFS_RXBYTES     0x0002


typedef struct {
    ngx_uint_t  param;
    ngx_uint_t  format;
} ngx_http_systat_loc_conf_t;



static char *ngx_http_systat_set(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static void *ngx_http_systat_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_systat_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);



static ngx_conf_enum_t  ngx_http_systat_format[] = {
  { ngx_string("plain"), NGX_HTTP_SYSTAT_FORMAT_PLAIN },
  { ngx_string("xml"),   NGX_HTTP_SYSTAT_FORMAT_XML   },
  { ngx_null_string, 0 }
};

static ngx_conf_bitmask_t  ngx_http_systat_param_mask[] = {
  { ngx_string("ifstat"), NGX_HTTP_SYSTAT_IFSTAT },
  { ngx_null_string, 0 }
};



static ngx_command_t ngx_http_systat_commands[] = {
  { ngx_string("systat"),
    NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
    ngx_http_systat_set,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL },

  { ngx_string("systat_format"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
    ngx_conf_set_enum_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_systat_loc_conf_t, format),
    &ngx_http_systat_format },


  { ngx_string("systat_param"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
    ngx_conf_set_bitmask_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_systat_loc_conf_t, param),
    &ngx_http_systat_param_mask },

    ngx_null_command
};



static ngx_http_module_t ngx_http_systat_module_ctx = {
    NULL,                               /* preconfiguration */
    NULL,                               /* postconfiguration */
    NULL,                               /* create main configuration */
    NULL,                               /* init main configuration */
    NULL,                               /* create server configuration */
    NULL,                               /* merge server configuration */
    ngx_http_systat_create_loc_conf,    /* create location configration */
    ngx_http_systat_merge_loc_conf      /* merge location configration */
};



ngx_module_t ngx_http_systat_module = {
    NGX_MODULE_V1,
    &ngx_http_systat_module_ctx,        /* module context */
    ngx_http_systat_commands,           /* module directives */
    NGX_HTTP_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                               /* init process */
    NULL,                               /* init thread */
    NULL,                               /* exit thread */
    NULL,                               /* exit process */
    NULL,                               /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_systat_handler(ngx_http_request_t *r)
{
    ngx_int_t                       rc;
    ngx_chain_t                     out;
    ngx_buf_t                      *b;
    //ngx_uint_t                      format;
    ngx_http_systat_loc_conf_t     *stcf;


    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return rc;
    }

    stcf = ngx_http_get_module_loc_conf(r, ngx_http_systat_module);

    switch (stcf->format) {

        case NGX_HTTP_SYSTAT_FORMAT_XML: 
            ngx_str_set(&r->headers_out.content_type, "text/xml");
            ngx_str_set(&r->headers_out.charset, "utf-8");
            break;

        default: /* NGX_HTTP_SYSTAT_FORMAT_PLAIN */
            ngx_str_set(&r->headers_out.content_type, "text/plain");
            break;
    }

    r->headers_out.content_type_len = r->headers_out.content_type.len;
    r->headers_out.content_type_lowcase = NULL;

    if (r->method == NGX_HTTP_HEAD) {
        r->headers_out.status = NGX_HTTP_OK;

        rc = ngx_http_send_header(r);

        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
        }
    }

    b = ngx_create_temp_buf(r->pool, 2);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    b->last = ngx_sprintf(b->last, "OK");

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = b->last - b->pos;

    if (r == r->main) {
        b->last_buf = 1;
    }

    b->last_in_chain = 1;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}


static void *
ngx_http_systat_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_systat_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_systat_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->param = 0;
     */

    conf->format = NGX_CONF_UNSET_UINT;


    return conf;
}

static char *
ngx_http_systat_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_systat_loc_conf_t  *prev = parent;
    ngx_http_systat_loc_conf_t  *conf = child;

    ngx_conf_merge_bitmask_value(conf->param, prev->param,
                                 NGX_CONF_BITMASK_SET);

    ngx_conf_merge_uint_value(conf->format, prev->format,
                              NGX_HTTP_SYSTAT_FORMAT_PLAIN);

    return NGX_CONF_OK;
}


static char *
ngx_http_systat_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t   *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_systat_handler;

    return NGX_CONF_OK;
}
