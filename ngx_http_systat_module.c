/*
 * Copyright (c) 2015 Eugene Mychlo
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include <ifaddrs.h>
#include <net/if.h>


#if (NGX_LINUX)
#include <linux/netdevice.h>

#define NGX_AF_LINK     AF_PACKET
#define ngx_get_ifa_tx_bytes(ifa) \
    ((uint64_t)((struct net_device_stats *)ifa->ifa_data)->tx_bytes)

#elif (NGX_FREEBSD) || (NGX_DARWIN)

#define NGX_AF_LINK     AF_LINK
#define ngx_get_ifa_tx_bytes(ifa) \
    ((uint64_t)((struct if_data *)ifa->ifa_data)->ifi_obytes)

#endif





#define ngx_getifaddrs(ifa) getifaddrs(ifa)
#define ngx_getifaddrs_n    "getifaddrs()"

#define ngx_freeifaddrs(ifa) freeifaddrs(ifa)



typedef struct {
    ngx_str_t  netif;
} ngx_http_systat_loc_conf_t;



static void *ngx_http_systat_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_systat_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static char *ngx_http_systat_set(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static ngx_int_t ngx_http_systat_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_systat_netif_tx_bytes(ngx_http_request_t *r,
    uint64_t *counter);

static ngx_command_t ngx_http_systat_commands[] = {
  { ngx_string("systat"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
    ngx_http_systat_set,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL },

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
    ngx_int_t           rc;
    ngx_chain_t         out;
    ngx_buf_t           *b;
    uint64_t            txb;

    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return rc;
    }

    ngx_str_set(&r->headers_out.content_type, "text/plain");
    r->headers_out.content_type_len = r->headers_out.content_type.len;
    r->headers_out.content_type_lowcase = NULL;
    r->headers_out.status = NGX_HTTP_OK;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
    }

    rc = ngx_http_systat_netif_tx_bytes(r, &txb);
    if (rc != NGX_OK) {
            return rc;
    }
    
    b = ngx_create_temp_buf(r->pool, NGX_INT64_LEN);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    b->last = ngx_sprintf(b->last, "%uL", txb);

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = b->last - b->pos;

    if (r == r->main) {
        b->last_buf = 1;
    }

    b->last_in_chain = 1;

    return ngx_http_output_filter(r, &out);
}


static ngx_int_t
ngx_http_systat_netif_tx_bytes(ngx_http_request_t *r, uint64_t *counter)
{
    ngx_http_systat_loc_conf_t     *stcf;
    struct ifaddrs *ifaddr, *ifa;

    stcf = ngx_http_get_module_loc_conf(r, ngx_http_systat_module);
    if (!stcf) {
        return NGX_ERROR;
    }

    
    if (ngx_getifaddrs(&ifaddr) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                      ngx_getifaddrs_n " failed");
        return NGX_ERROR;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != NGX_AF_LINK) {
            continue;
        }

        if (ngx_strcmp(ifa->ifa_name, stcf->netif.data) == 0) {
            *counter = ngx_get_ifa_tx_bytes(ifa);
            ngx_freeifaddrs(ifaddr);
            return NGX_OK;
        }
    }

    ngx_freeifaddrs(ifaddr);
    return NGX_HTTP_NOT_FOUND;
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
     *     conf->netif = { 0, NULL };
     */

    return conf;
}

static char *
ngx_http_systat_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_systat_loc_conf_t  *prev = parent;
    ngx_http_systat_loc_conf_t  *conf = child;

    ngx_conf_merge_str_value(conf->netif, prev->netif, "");


    return NGX_CONF_OK;
}


static char *
ngx_http_systat_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_systat_loc_conf_t *stcf = conf;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_str_t  *value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "netif_tx_bytes") != 0) {
        return "take an unsupported argument"; //XXX
    }

    if (stcf->netif.len) {
        return "is duplicate";
    }

    stcf->netif = value[2];

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_systat_handler;

    return NGX_CONF_OK;
}
