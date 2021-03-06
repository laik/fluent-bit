/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2019-2020 The Fluent Bit Authors
 *  Copyright (C) 2015-2018 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <fluent-bit/flb_output_plugin.h>
#include <fluent-bit/flb_utils.h>

#include "splunk.h"
#include "splunk_conf.h"

struct flb_splunk *flb_splunk_conf_create(struct flb_output_instance *ins,
                                          struct flb_config *config)
{
    int ret;
    int io_flags = 0;
    flb_sds_t t;
    const char *tmp;
    struct flb_upstream *upstream;
    struct flb_splunk *ctx;

    ctx = flb_calloc(1, sizeof(struct flb_splunk));
    if (!ctx) {
        flb_errno();
        return NULL;
    }
    ctx->ins = ins;

    ret = flb_output_config_map_set(ins, (void *) ctx);
    if (ret == -1) {
        flb_free(ctx);
        return NULL;
    }

    /* Set default network configuration */
    flb_output_net_default(FLB_SPLUNK_DEFAULT_HOST, FLB_SPLUNK_DEFAULT_PORT, ins);

    /* use TLS ? */
    if (ins->use_tls == FLB_TRUE) {
        io_flags = FLB_IO_TLS;
    }
    else {
        io_flags = FLB_IO_TCP;
    }

    if (ins->host.ipv6 == FLB_TRUE) {
        io_flags |= FLB_IO_IPV6;
    }

    /* Prepare an upstream handler */
    upstream = flb_upstream_create(config,
                                   ins->host.name,
                                   ins->host.port,
                                   io_flags,
                                   ins->tls);
    if (!upstream) {
        flb_plg_error(ctx->ins, "cannot create Upstream context");
        flb_splunk_conf_destroy(ctx);
        return NULL;
    }

    /* Set manual Index and Type */
    ctx->u = upstream;

    /* Splunk Auth Token */
    tmp = flb_output_get_property("splunk_token", ins);
    if (tmp) {
        ctx->auth_header = flb_sds_create("Splunk ");
        t = flb_sds_cat(ctx->auth_header, tmp, strlen(tmp));
        if (t) {
            ctx->auth_header = t;
        }
        else {
            flb_plg_error(ctx->ins, "error on token generation");
            flb_splunk_conf_destroy(ctx);
            return NULL;
        }
    }
    else {
        flb_plg_error(ctx->ins, "no splunk_token configuration key defined");
        flb_splunk_conf_destroy(ctx);
        return NULL;
    }

    /* HTTP Auth */
    tmp = flb_output_get_property("http_user", ins);
    if (tmp && ctx->auth_header) {
        flb_plg_error(ctx->ins, "splunk_token and http_user cannot be used at"
                      " the same time");
        flb_splunk_conf_destroy(ctx);
        return NULL;
    }
    if (tmp) {
        ctx->http_user = flb_strdup(tmp);
        tmp = flb_output_get_property("http_passwd", ins);
        if (tmp) {
            ctx->http_passwd = flb_strdup(tmp);
        }
        else {
            ctx->http_passwd = flb_strdup("");
        }
    }

    /* Set instance flags into upstream */
    flb_output_upstream_set(ctx->u, ins);

    return ctx;
}

int flb_splunk_conf_destroy(struct flb_splunk *ctx)
{
    if (!ctx) {
        return -1;
    }

    if (ctx->u) {
        flb_upstream_destroy(ctx->u);
    }
    flb_free(ctx);

    return 0;
}
