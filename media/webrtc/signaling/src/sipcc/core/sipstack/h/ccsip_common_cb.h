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
