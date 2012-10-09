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

#ifndef _CCSIP_PUBLISH_H_
#define _CCSIP_PUBLISH_H_

#include "publish_int.h"
#include "ccsip_common_cb.h"
#include "ccsip_subsmanager.h"

/* the following is to take advantage of SUB/NOT periodic timer */
#define TMR_PERIODIC_PUBLISH_INTERVAL TMR_PERIODIC_SUBNOT_INTERVAL

#define PUBLISH_FAILED_START        (1000)
#define PUBLISH_FAILED_NOCONTEXT    (PUBLISH_FAILED_START + 1)
#define PUBLISH_FAILED_NORESOURCE   (PUBLISH_FAILED_START + 2)
#define PUBLISH_FAILED_SEND         (PUBLISH_FAILED_START + 3)
#define PUBLISH_FAILED_RESET        (PUBLISH_FAILED_START + 4)

typedef struct {
    ccsip_common_cb_t       hb;  /* this MUST be the first member */

    cc_srcs_t               callback_task;  // CallBack Task ID
    int                     resp_msg_id;    //Response Msg ID
    char                   *entity_tag;
    pub_handle_t            pub_handle;
    pub_handle_t            app_handle;
    char                    ruri[MAX_URI_LENGTH];  // Address of the resource
    char                    full_ruri[MAX_SIP_URL_LENGTH];
    char                    esc[MAX_URI_LENGTH];   // Event State Compositor
    boolean                 outstanding_trxn;
    sipPlatformUITimer_t    retry_timer;
    sll_handle_t            pending_reqs; //holds pending requests
} ccsip_publish_cb_t; //PUBLISH Control Block data structure

extern int publish_handle_ev_app_publish(cprBuffer_t buf);
extern int publish_handle_retry_timer_expire(uint32_t handle);
extern void publish_handle_periodic_timer_expire(void);
extern int publish_handle_ev_sip_response(sipMessage_t *pSipMessage);
extern void publish_reset(void);
extern cc_int32_t show_publish_stats(cc_int32_t argc, const char *argv[]);

#endif // _CCSIP_PUBLISH_H_
