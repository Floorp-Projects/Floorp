/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PUBLISH_INT_H
#define PUBLISH_INT_H

#include "ccsip_subsmanager.h"
#include "cpr_types.h"
#include "ccsip_callinfo.h"


typedef uint32_t pub_handle_t;
#define NULL_PUBLISH_HANDLE  0 /* this is to indicate that handle is not assigned  yet */


typedef struct  {
    pub_handle_t         pub_handle;  // handle assigned by the SIP stack
    pub_handle_t         app_handle;  // handle assigned by the Application
    char                 ruri[MAX_URI_LENGTH];  // Address of the resource
    char                 esc[MAX_URI_LENGTH];   // Event State Compositor
    uint32_t             expires;    // suggested Expires in seconds
    cc_subscriptions_t   event_type; // EventType, for example, prescence
    ccsip_event_data_t  *event_data_p;  // Event Data
    cc_srcs_t            callback_task;  // CallBack Task ID
    int                  resp_msg_id; //Response Msg ID
} pub_req_t;

typedef struct {
     unsigned int       resp_code;  // response code
     pub_handle_t       pub_handle; // handle assigned by the SIP stack
     pub_handle_t       app_handle; // handle assigned by the Application
} pub_rsp_t;

extern
void publish_init(pub_handle_t             app_handle,
                  char                    *ruri,
                  char                    *esc,
                  unsigned int             expires,
                  cc_subscriptions_t       event_type,
                  ccsip_event_data_t      *event_data_p,
                  cc_srcs_t                callback_task,
                  int                      message_id
                 );

extern
void publish_update(pub_handle_t          pub_handle,
                    cc_subscriptions_t    event_type,
                    ccsip_event_data_t   *event_data_p,
                    cc_srcs_t             callback_task,
                    int                   message_id
                   );

extern
void publish_terminate(pub_handle_t          pub_handle,
                       cc_subscriptions_t    event_type,
                       cc_srcs_t             callback_task,
                       int                   message_id
                      );

extern
cc_rcs_t publish_int_response(pub_rsp_t               *pub_rsp_p,
                              cc_srcs_t                callback_task,
                              int                      message_id
                             );

#endif  /*PUBLISH_INT_H*/



