/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCSIP_COMMON_CB_H_
#define _CCSIP_COMMON_CB_H_

#include "cpr_types.h"
#include "phone_types.h"
#include "ccapi.h"
#include "dns_utils.h"
#include "ccsip_platform.h"
#include "ccsip_pmh.h"
#include "ccsip_core.h"
#include "xml_parser_defines.h"


#define HOOK_STATUS_STR "hook status"
#define BLF_SPEEDDIAL_STR "blf speed dial"

typedef enum {
    SUBNOT_CB,
    PUBLISH_CB,
    UNSOLICIT_NOTIFY_CB
} ccsip_cb_type_e;


typedef struct {
    ccsip_cb_type_e         cb_type;
    line_t                  dn_line;
    cpr_ip_addr_t           dest_sip_addr;
    uint32_t                dest_sip_port;  /* Destination port */
    cpr_ip_addr_t           src_addr;       /* Source address */
    uint32_t                local_port;     /* Source port */
    srv_handle_t             SRVhandle;      /* handle for dns_gethostbysrv() */
    unsigned int            retx_counter;
    boolean                 retx_flag;
    char                    sipCallID[MAX_SIP_CALL_ID];
    sipAuthenticate_t       authen;
    long                    orig_expiration;  /* Original time to expire */
    long                    expires;          /* Running expiry timer */
    ccsip_event_data_t     *event_data_p;
    cc_subscriptions_t      event_type;     /* event type, such as presence */
    cc_subscriptions_t      accept_type;     /* accept type, such as presence */
} ccsip_common_cb_t;


extern void ccsip_common_util_set_dest_ipaddr_port(ccsip_common_cb_t *cb_p);
extern void ccsip_common_util_set_src_ipaddr(ccsip_common_cb_t *cb_p);
extern void ccsip_common_util_set_retry_settings(ccsip_common_cb_t *cb_p, int *timeout_p);
extern boolean ccsip_common_util_generate_auth(sipMessage_t *pSipMessage, ccsip_common_cb_t *cb_p,
                                         const char *rsp_method, int response_code, char *uri);
extern void ccsip_util_extract_user(char *url, char *user);
extern void ccsip_util_get_from_entity(sipMessage_t *pSipMessage, char *entity);

#endif //_CCSIP_COMMON_CB_H_
