/*
   SSSD

   NSS Responder, header file

   Copyright (C) Simo Sorce <ssorce@redhat.com>	2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __NSSSRV_H__
#define __NSSSRV_H__

#include <stdint.h>
#include <sys/un.h>
#include "config.h"
#include "talloc.h"
#include "tevent.h"
#include "ldb.h"
#include "../sss_client/sss_cli.h"
#include "dbus/dbus.h"
#include "sbus/sssd_dbus.h"
#include "responder/common/responder_packet.h"
#include "responder/common/responder.h"
#include "responder/nss/nsssrv_nc.h"

#define NSS_SBUS_SERVICE_VERSION 0x0001
#define NSS_SBUS_SERVICE_NAME "nss"

#define NSS_PACKET_MAX_RECV_SIZE 1024

#define NSS_ENUM_USERS 0x01
#define NSS_ENUM_GROUPS 0x02
#define NSS_ENUM_ALL 0x03

#define NSS_SRV_CONFIG "config/services/nss"

struct getent_ctx;

struct nss_ctx {
    struct resp_ctx *rctx;

    int cache_timeout;
    int neg_timeout;
    struct nss_nc_ctx *ncache;

    int enum_cache_timeout;
    time_t last_user_enum;
    time_t last_group_enum;

    struct getent_ctx *pctx;
    struct getent_ctx *gctx;
};

struct nss_packet;

int nss_cmd_execute(struct cli_ctx *cctx);

/* from nsssrv_dp.c */
struct sbus_method *get_nss_dp_methods(void);
struct sss_cmd_table *get_nss_cmds(void);

#endif /* __NSSSRV_H__ */
