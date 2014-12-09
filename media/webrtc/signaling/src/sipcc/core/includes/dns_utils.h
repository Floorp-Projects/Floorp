/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _DNS_UTILS_INCLUDED_H
#define _DNS_UTILS_INCLUDED_H

#include "dns_util.h"

#define MAX_DNS_CON 10
#define DNS_TO_CNT 400
#define DNS_RSP_FLG 0x8000
#define DNS_RSP_CODE 0xf

#define DNS_HOST_TYPE  0x01
#define DNS_CNAME_TYPE 0x05
#define DNS_SRV_TYPE   0x21
#define DNS_PTR_TYPE   0xc0

#define DNS_IN_CLASS 1

#define DNS_INUSE_FREE   0
#define DNS_INUSE_WAIT   1
#define DNS_INUSE_CACHED 2

/* per suggestion RFC in about sanity checking */
#define DNS_MIN_TTL      (5*60)     /* 5 minutes */
#define DNS_MAX_TTL      (24*60*60) /* 1 day */

#define KAZOO_DNS_DEBUG 1

#define DNS_OK                 0
#define DNS_ERR_NOBUF          1
#define DNS_ERR_INUSE          2
#define DNS_ERR_TIMEOUT        3
#define DNS_ERR_NOHOST         4
#define DNS_ERR_HOST_UNAVAIL   5
#define DNS_ERR_BAD_DATA       6
#define DNS_ERR_LINK_DOWN      7
#define DNS_ENTRY_VALID        8
#define DNS_ENTRY_INVALID      9
#define DNS_ERR_TTL_EXPIRED    10

#define DNS_MAX_SRV_REQ        20

#define DNS_HNAME_PAD          10
#define DOMAIN_NAME_LENGTH     256

typedef struct rr_reply_rec_ {
    uint8_t ordered;
    uint32_t priority;
    uint32_t weight;
    uint32_t rweight;
    uint32_t ttl;
    uint32_t port;
    uint32_t rcvd_order;
    int8_t *name;
} rr_reply_rec_t;

typedef struct master_rr_list_ {
    uint8_t valid;
    uint16_t ref_count;
    uint8_t num_recs;
    int8_t domain_name[DOMAIN_NAME_LENGTH + 1];
    rr_reply_rec_t **rr_recs_order;
} master_rr_list_t;

typedef struct call_rr_list_ {
    uint8_t current_index;
    uint8_t max_index;
    uint8_t master_index;
    int8_t *domain_name;
    rr_reply_rec_t **rr_recs_order;
} call_rr_list_t;


#endif /* _DNS_UTILS_INCLUDED_H */
