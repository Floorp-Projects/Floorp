/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
