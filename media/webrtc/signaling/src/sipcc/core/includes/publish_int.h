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



