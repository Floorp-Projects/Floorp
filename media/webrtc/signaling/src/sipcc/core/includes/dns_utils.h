/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
