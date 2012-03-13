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

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "cpr_memory.h"
#include "cpr_stdlib.h"
#include "ccapi.h"
#include "ccsip_task.h"
#include "debug.h"
#include "phone_debug.h"
#include "phntask.h"
#include "phone.h"
#include "text_strings.h"
#include "string_lib.h"
#include "gsm.h"
#include "vcm.h"
#include "subapi.h"
#include "misc_apps_task.h"


static void
sub_print_msg (char *pData, int len)
{
    int ix;
    int msg_id = *((int *)pData);

    buginf("\nCCAPI: cc_msg= %s, 0x=", cc_msg_name((cc_msgs_t)msg_id));
    for (ix = 0; ix < len; ix++) {
        if ((ix % 8 == 0) && ix) {
            buginf("  ");
        }
        if (ix % 24 == 0) {
            buginf("\n");
        }
        buginf("%02x ", *pData++);
    }
    buginf("\n");
}

cc_rcs_t
sub_send_msg (cprBuffer_t buf, uint32_t cmd, uint16_t len, cc_srcs_t dst_id)
{
    cpr_status_e rc;

    CC_DEBUG_MSG sub_print_msg((char *)buf, len);

    switch (dst_id) {
    case CC_SRC_GSM:
        rc = gsm_send_msg(cmd, buf, len);
        if (rc == CPR_FAILURE) {
            cprReleaseBuffer(buf);
        }
        break;
    case CC_SRC_SIP:
        rc = SIPTaskSendMsg(cmd, buf, len, NULL);
        if (rc == CPR_FAILURE) {
            cprReleaseBuffer(buf);
        }
        break;
    case CC_SRC_MISC_APP:
        rc = MiscAppTaskSendMsg(cmd, buf, len);
        if (rc == CPR_FAILURE) {
            cprReleaseBuffer(buf);
        }
        break;
    default:
        rc = CPR_FAILURE;
        break;
    }

    return (rc == CPR_SUCCESS) ? CC_RC_SUCCESS : CC_RC_ERROR;
}

cc_rcs_t
sub_int_subnot_register (cc_srcs_t src_id, cc_srcs_t dst_id,
                         cc_subscriptions_t evt_pkg, void *callback_fun,
                         cc_srcs_t dest_task, int msg_id, void *term_callback,
                         int term_msg_id, long min_duration, long max_duration)
{
    sipspi_msg_t *pmsg;

    pmsg = (sipspi_msg_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        return CC_RC_ERROR;
    }

    pmsg->msg.subs_reg.eventPackage = evt_pkg;
    pmsg->msg.subs_reg.max_duration = max_duration;
    pmsg->msg.subs_reg.min_duration = min_duration;
    pmsg->msg.subs_reg.subsIndCallback = (ccsipSubsIndCallbackFn_t)
        callback_fun;
    pmsg->msg.subs_reg.subsIndCallbackMsgID = msg_id;
    pmsg->msg.subs_reg.subsIndCallbackTask = dest_task;
    pmsg->msg.subs_reg.subsTermCallback = (ccsipSubsTerminateCallbackFn_t)
        term_callback;
    pmsg->msg.subs_reg.subsTermCallbackMsgID = term_msg_id;

    return sub_send_msg((cprBuffer_t)pmsg, SIPSPI_EV_CC_SUBSCRIBE_REGISTER,
                        sizeof(*pmsg), dst_id);
}

/*
 * Function: sub_int_subscribe()
 *
 * Parameters: msg_p - pointer to sipspi_msg_t (input parameter)
 *
 * Description: posts SIPSPI_EV_CC_SUBSCRIBE to SIP task message queue.
 *
 * Returns: CC_RC_ERROR - failed to post msg.
 *          CC_RC_SUCCESS - successful posted msg.
 */
cc_rcs_t
sub_int_subscribe (sipspi_msg_t *msg_p)
{
    sipspi_msg_t *pmsg;

    pmsg = (sipspi_msg_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        return CC_RC_ERROR;
    }

    memcpy(pmsg, msg_p, sizeof(sipspi_msg_t));
    return sub_send_msg((cprBuffer_t)pmsg, SIPSPI_EV_CC_SUBSCRIBE,
                        sizeof(*pmsg), CC_SRC_SIP);
}


cc_rcs_t
sub_int_subscribe_ack (cc_srcs_t src_id, cc_srcs_t dst_id, sub_id_t sub_id,
                       uint16_t response_code, int duration)
{
    sipspi_msg_t *pmsg;

    pmsg = (sipspi_msg_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        return CC_RC_ERROR;
    }

    pmsg->msg.subscribe_resp.response_code = response_code;
    pmsg->msg.subscribe_resp.sub_id = sub_id;
    pmsg->msg.subscribe_resp.duration = duration;

    return sub_send_msg((cprBuffer_t)pmsg, SIPSPI_EV_CC_SUBSCRIBE_RESPONSE,
                        sizeof(*pmsg), dst_id);
}


cc_rcs_t
sub_int_notify (cc_srcs_t src_id, cc_srcs_t dst_id, sub_id_t sub_id,
                ccsipNotifyResultCallbackFn_t notifyResultCallback,
                int subsNotResCallbackMsgID, ccsip_event_data_t * eventData,
                subscriptionState subState)
{
    sipspi_msg_t *pmsg;

    pmsg = (sipspi_msg_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        return CC_RC_ERROR;
    }

    pmsg->msg.notify.eventData = eventData;
    pmsg->msg.notify.notifyResultCallback = notifyResultCallback;
    pmsg->msg.notify.sub_id = sub_id;
    pmsg->msg.notify.subsNotResCallbackMsgID = subsNotResCallbackMsgID;
    pmsg->msg.notify.subState = subState;

    return sub_send_msg((cprBuffer_t) pmsg, SIPSPI_EV_CC_NOTIFY,
                        sizeof(*pmsg), dst_id);
}

/*
 * Function: sub_int_notify_ack()
 *
 * Parameters:  sub_id - subcription id for sip stack to track the subscription.
 *              response_code - response code to be sent in response to NOTIFY.
 *              cseq  : CSeq for which response is being sent.
 *
 * Description: posts SIPSPI_EV_CC_NOTIFY_RESPONSE to SIP task message queue.
 *
 * Returns: CC_RC_ERROR - failed to post msg.
 *          CC_RC_SUCCESS - successful posted msg.
 */
cc_rcs_t
sub_int_notify_ack (sub_id_t sub_id, uint16_t response_code, uint32_t cseq)
{
    sipspi_msg_t *pmsg;

    pmsg = (sipspi_msg_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        return CC_RC_ERROR;
    }

    pmsg->msg.notify_resp.sub_id = sub_id;
    pmsg->msg.notify_resp.response_code = response_code;
    pmsg->msg.notify_resp.cseq = cseq;

    return sub_send_msg((cprBuffer_t)pmsg, SIPSPI_EV_CC_NOTIFY_RESPONSE,
                        sizeof(*pmsg), CC_SRC_SIP);
}


/*
 * Function: sub_int_subscribe_term()
 *
 * Parameters:  sub_id - subcription id for sip stack to track the subscription.
 *              immediate - boolean flag to indicate if the termination be immediate.
 *              request_id - request id significant for out going subscriptions
 *              event_package - event package type
 *
 * Description: posts SIPSPI_EV_CC_SUBSCRIPTION_TERMINATED to SIP task message queue.
 *
 * Returns: CC_RC_ERROR - failed to post msg.
 *          CC_RC_SUCCESS - successful posted msg.
 */
cc_rcs_t
sub_int_subscribe_term (sub_id_t sub_id, boolean immediate, int request_id,
                        cc_subscriptions_t event_package)
{
    sipspi_msg_t *pmsg;

    pmsg = (sipspi_msg_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        return CC_RC_ERROR;
    }

    pmsg->msg.subs_term.immediate = immediate;
    pmsg->msg.subs_term.sub_id = sub_id;
    pmsg->msg.subs_term.request_id = request_id;
    pmsg->msg.subs_term.eventPackage = event_package;

    return sub_send_msg((cprBuffer_t)pmsg, SIPSPI_EV_CC_SUBSCRIPTION_TERMINATED,
                        sizeof(*pmsg), CC_SRC_SIP);
}

cc_rcs_t
sip_send_message (ccsip_sub_not_data_t * msg_data, cc_srcs_t dest_id, int msg_id)
{
    ccsip_sub_not_data_t *pmsg;

    pmsg = (ccsip_sub_not_data_t *) cc_get_msg_buf(sizeof(*pmsg));

    if (!pmsg) {
        return CC_RC_ERROR;
    }

    memcpy(pmsg, msg_data, sizeof(*pmsg));

    return sub_send_msg((cprBuffer_t)pmsg, msg_id, sizeof(*pmsg), dest_id);
}


cc_rcs_t
app_send_message (void *msg_data, int msg_len, cc_srcs_t dest_id, int msg_id)
{
    void *pmsg;

    pmsg = (void *) cc_get_msg_buf(msg_len);

    if (!pmsg) {
        return CC_RC_ERROR;
    }

    memcpy(pmsg, msg_data, msg_len);

    return sub_send_msg((cprBuffer_t)pmsg, msg_id, (uint16_t)msg_len, dest_id);
}
