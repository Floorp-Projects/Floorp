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

#include "publish_int.h"
#include "subapi.h"
#include "ccsip_subsmanager.h"
#include "phntask.h"

/**
 * This function will post an event - SIPSPI_EV_CC_PUBLISH_REQ - to SIP stack
 * which will be handled by PUBLISH Handler in sip stack.
 *
 * @param[in] pub_req_p - pointer to a pub_req_t struct.
 *
 * @return  CC_RC_SUCCESS if the event is successfully posted.
 *          Otherwise, CC_RC_ERROR is returned.
 *
 * @pre (pub_req_p != NULL)
 */
static cc_rcs_t pub_int_req (pub_req_t *pub_req_p)
{
    cc_rcs_t ret_code;

    ret_code = app_send_message(pub_req_p, sizeof(pub_req_t), CC_SRC_SIP,  SIPSPI_EV_CC_PUBLISH_REQ);

    if (ret_code != CC_RC_SUCCESS) {
        free_event_data(pub_req_p->event_data_p);
    }
    return ret_code;
}

/**
 * This function will trigger initial PUBLISH. This is invoked by
 * applications that intend to PUBLISH an event state.
 *
 * @param[in] app_handle - a unique application handle.
 * @param[in] ruri - Request URI or user part of it.
 * @param[in] esc - event state compositor.
 * @param[in] expires - event state expiration value.
 * @param[in] event_type - event package name.
 * @param[in] event_data_p -  pointer to event body. caller does not have to free it.
 * @param[in] callback_task -  task that is interested in response.
 * @param[in] message_id -  message id of the response.
 *
 * @note application MUST not free event_data_p
 *
 * @return  none.
 */
void publish_init (pub_handle_t             app_handle,
                   char                    *ruri,
                   char                    *esc,
                   unsigned int             expires,
                   cc_subscriptions_t       event_type,
                   ccsip_event_data_t      *event_data_p,
                   cc_srcs_t                callback_task,
                   int                      message_id 
                  )
{

    pub_req_t pub_req;

    /*
     * Populate pub_req for initial PUBLISH
     */
    pub_req.pub_handle = NULL_PUBLISH_HANDLE; //because it is initial request.
    pub_req.app_handle = app_handle;
    sstrncpy(pub_req.ruri, ruri, MAX_URI_LENGTH);
    sstrncpy(pub_req.esc, esc, MAX_URI_LENGTH);
    pub_req.expires = expires;
    pub_req.event_type = event_type;
    pub_req.event_data_p = event_data_p;
    pub_req.callback_task = callback_task;
    pub_req.resp_msg_id = message_id;

    (void)pub_int_req(&pub_req);
}


/**
 * This function will trigger modification PUBLISH. This is invoked by
 * applications that intend to PUBLISH to modify an event state.
 *
 * @param[in] pub_handle - a unique publish handle.
 * @param[in] event_type - event package name.
 * @param[in] event_data_p -  pointer to event body. caller does not have to free it.
 * @param[in] callback_task -  task that is interested in response.
 * @param[in] message_id -  message id of the response.
 *
 * @return  none.
 */
void publish_update (pub_handle_t          pub_handle,
                     cc_subscriptions_t    event_type,
                     ccsip_event_data_t   *event_data_p,
                     cc_srcs_t             callback_task,
                     int                   message_id 
                    ) 
{
    pub_req_t pub_req;

    /*
     * Populate pub_req for update PUBLISH
     */
    memset(&pub_req, 0, sizeof(pub_req));
    pub_req.pub_handle = pub_handle;
    pub_req.event_type = event_type;
    pub_req.event_data_p = event_data_p;
    pub_req.callback_task = callback_task;
    pub_req.resp_msg_id = message_id;

    (void)pub_int_req(&pub_req);
}


/**
 * This function will trigger terminating PUBLISH. This is invoked by
 * applications that intend to PUBLISH to remove an event state.
 *
 * @param[in] pub_handle - a unique publish handle.
 * @param[in] event_type - event package name.
 * @param[in] callback_task -  task that is interested in response.
 * @param[in] message_id -  message id of the response.
 *
 * @return  none.
 */
void publish_terminate (pub_handle_t          pub_handle,
                        cc_subscriptions_t    event_type,
                        cc_srcs_t             callback_task,
                        int                   message_id 
                       )
{
   pub_req_t pub_req;

    /*
     * Populate pub_req for terminating PUBLISH
     */
    memset(&pub_req, 0, sizeof(pub_req));
    pub_req.pub_handle = pub_handle;
    pub_req.event_type = event_type;
    pub_req.callback_task = callback_task;
    pub_req.resp_msg_id = message_id;

    (void)pub_int_req(&pub_req);
}


/**
 * This function will post a response event to applications.
 * this is invoked by SIP stack.
 *
 * @param[in] pub_rsp_p - response struct.
 * @param[in] callback_task -  task that is interested in response.
 * @param[in] message_id -  message id of the response.
 *
 * @return  CC_RC_SUCCESS if the event is successfully posted.
 *          Otherwise, CC_RC_ERROR is returned.
 */
cc_rcs_t publish_int_response (pub_rsp_t               *pub_rsp_p,
                               cc_srcs_t               callback_task,
                               int                     message_id    
                              )
{
    pub_rsp_t *pmsg;

    pmsg = (pub_rsp_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        return CC_RC_ERROR;
    }

    memcpy(pmsg, pub_rsp_p, sizeof(*pmsg));

    return sub_send_msg((cprBuffer_t)pmsg, message_id, sizeof(*pmsg), callback_task);
}

