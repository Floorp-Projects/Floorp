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
#include "ccapi.h"
#include "string_lib.h"
#include "ccsip_pmh.h"
#include "ccsip_messaging.h"

/*
 *  Function: sip_cc_mv_msg_body_to_cc_msg
 *
 *  Parameters: cc_msg  - pointer to cc_msgbody_info structure to
 *                        move the content from the SIP body msg. to.
 *              sip_msg - pointer to sipMessage_t structure of the source
 *                        body.
 *
 *  Description: This routine moves the body parts from sipMessage_t to
 *               CCAPI body msg. Once the content is moved,
 *               the all pointers from the sipMessage_t structure,
 *               will be NULL so that they are not own by SIP stack.
 *               The destination of the msg. needs to free the
 *               memory for of the parts.
 *
 *  Returns: N/A
 *
 */
void sip_cc_mv_msg_body_to_cc_msg (cc_msgbody_info_t *cc_msg,
                                   sipMessage_t *sip_msg)
{
    int i;
    uint32_t num_parts = 0;
    cc_msgbody_t *part;

    if (cc_msg == NULL) {
        /* destination to move msg. to */
        return;
    }
    if (sip_msg == NULL) {
        /* No SIP message to move from, set number of part to zero */
        cc_msg->num_parts = 0;
        return;
    }

    part = &cc_msg->parts[0];
    for (i = 0; (i < sip_msg->num_body_parts) &&
                (i < CC_MAX_BODY_PARTS); i++) {
        if ((sip_msg->mesg_body[i].msgBody != NULL) &&
            (sip_msg->mesg_body[i].msgLength)) {
            /* Body */
            part->body        = sip_msg->mesg_body[i].msgBody;
            part->body_length = sip_msg->mesg_body[i].msgLength;
            sip_msg->mesg_body[i].msgBody = NULL;

            /* Content type */
            part->content_type =
                    sip2cctype(sip_msg->mesg_body[i].msgContentTypeValue);
            /* Disposition */
            part->content_disposition.disposition =
                    sip2ccdisp(sip_msg->mesg_body[i].msgContentDisp);
            part->content_disposition.required_handling =
                    sip_msg->mesg_body[i].msgRequiredHandling;
            /* Content ID */
            part->content_id = sip_msg->mesg_body[i].msgContentId;
            sip_msg->mesg_body[i].msgContentId = NULL;

            /* Next part */
            part++;
            num_parts++;
        }
    }
    /* Set the number of parts */
    cc_msg->num_parts = num_parts;
}

/*
 *  Function: sip_cc_create_cc_msg_body_from_sip_msg
 *
 *  Parameters: cc_msg  - pointer to cc_msgbody_info structure to
 *                        store the content of the SIP body msg.
 *              sip_msg - pointer to sipMessage_t structure of the source
 *                        body.
 *
 *  Description: This routine creates the cc_msgbody_info_t content
 *               from the SIP message. The original msg. body in the
 *               SIP message remains in the SIP message.
 *
 *  Returns: TRUE  - success
 *           FALSE - fail.
 *
 */
boolean sip_cc_create_cc_msg_body_from_sip_msg (cc_msgbody_info_t *cc_msg,
                                                sipMessage_t *sip_msg)
{
    int i, len;
    uint32_t num_parts = 0;
    cc_msgbody_t *part;
    boolean  status = TRUE;

    if (cc_msg == NULL) {
        /* destination to move msg. to */
        return (FALSE);
    }

    if (sip_msg == NULL) {
        /* No SIP message to move from, set number of part to zero */
        cc_msg->num_parts = 0;
        return (FALSE);
    }

    memset(cc_msg, 0, sizeof(cc_msgbody_info_t));
    part = &cc_msg->parts[0];
    for (i = 0; (i < sip_msg->num_body_parts) &&
                (i < CC_MAX_BODY_PARTS) ; i++) {
        if ((sip_msg->mesg_body[i].msgBody != NULL) &&
            (sip_msg->mesg_body[i].msgLength)) {
            /* Body */
            part->body = (char *) cpr_malloc(sip_msg->mesg_body[i].msgLength);
            if (part->body != NULL) {
                part->body_length = sip_msg->mesg_body[i].msgLength;
                memcpy(part->body,
                       sip_msg->mesg_body[i].msgBody,
                       sip_msg->mesg_body[i].msgLength);
            } else {
                /* Unable to allocate memory for msg. body */
                status = FALSE;
                break;
            }

            /* Content type */
            part->content_type =
                    sip2cctype(sip_msg->mesg_body[i].msgContentTypeValue);
            /* Disposition */
            part->content_disposition.disposition =
                    sip2ccdisp(sip_msg->mesg_body[i].msgContentDisp);
            part->content_disposition.required_handling =
                    sip_msg->mesg_body[i].msgRequiredHandling;

            /* Content ID */
            if (sip_msg->mesg_body[i].msgContentId != NULL) {
                /* Get length of the msgContentID with NULL */
                len = strlen(sip_msg->mesg_body[i].msgContentId) + 1;
                part->content_id = (char *) cpr_malloc(len);
                if (part->content_id != NULL) {
                    memcpy(part->content_id,
                           sip_msg->mesg_body[i].msgContentId,
                           len);
                } else {
                    /* Unable to allocate allocate memory for content ID */
                    status = FALSE;
                    break;
                }
            } else {
                /* No content ID */
                part->content_id = NULL;
            }
            /* Next part */
            part++;
            num_parts++;
        }
    }
    /* Set the number of parts */
    cc_msg->num_parts = num_parts;

    if (!status) {
        /*
         * Faied for some reason, free the resources that mighe be
         * created before failure
         */
        cc_free_msg_body_parts(cc_msg);
    }
    return (status);
}

void sip_cc_setup (callid_t call_id, line_t line,
                   string_t calling_name, string_t calling_number, string_t alt_calling_number,
                   boolean display_calling_number,
                   string_t called_name,  string_t called_number,
                   boolean display_called_number,
                   string_t orig_called_name, string_t orig_called_number,
                   string_t last_redirect_name, string_t last_redirect_number,
                   cc_call_type_e   call_type,
                   cc_alerting_type alert_info,
                   vcm_ring_mode_t alerting_ring,
                   vcm_tones_t alerting_tone, cc_call_info_t *call_info_p,
                   boolean replaces, string_t recv_info_list, sipMessage_t *sip_msg)
{
    cc_caller_id_t caller_id;
    cc_msgbody_info_t cc_body_info;

    caller_id.calling_name = calling_name;
    caller_id.calling_number = calling_number;
    caller_id.alt_calling_number = alt_calling_number;
    caller_id.display_calling_number = display_calling_number;
    caller_id.called_name = called_name;
    caller_id.called_number = called_number;
    caller_id.display_called_number = display_called_number;
    caller_id.last_redirect_name = last_redirect_name;
    caller_id.last_redirect_number = last_redirect_number;
    caller_id.orig_called_name = orig_called_name;
    caller_id.orig_called_number = orig_called_number;
    caller_id.orig_rpid_number = strlib_empty();
    caller_id.call_type = call_type;

    /* Move the SIP body parts to the CCAPI msg. body information block */
    sip_cc_mv_msg_body_to_cc_msg(&cc_body_info, sip_msg);

// Check with CraigB
    cc_setup(CC_SRC_SIP, call_id, line, &caller_id, alert_info,
             alerting_ring, alerting_tone, NULL, call_info_p, replaces,
             recv_info_list, &cc_body_info);
}


#ifdef REMOVED_UNUSED_FUNCTION
void sip_cc_setup_ack (callid_t call_id, line_t line,
                       cc_msgbody_info_t *msg_body)
{
    cc_setup_ack(CC_SRC_SIP, call_id, line, NULL, msg_body);
}
#endif

void sip_cc_proceeding (callid_t call_id, line_t line)
{
    cc_proceeding(CC_SRC_SIP, call_id, line, NULL);
}

void sip_cc_alerting (callid_t call_id, line_t line,
                      sipMessage_t *sip_msg, int inband)
{
    cc_msgbody_info_t cc_body_info;

    /* Move the SIP body parts to the CCAPI msg. body information block */
    sip_cc_mv_msg_body_to_cc_msg(&cc_body_info, sip_msg);

    cc_alerting(CC_SRC_SIP, call_id, line, NULL, &cc_body_info,
                (boolean)inband);
}


void sip_cc_connected (callid_t call_id, line_t line, string_t recv_info_list, sipMessage_t *sip_msg)
{
    cc_msgbody_info_t cc_body_info;

    /* Move the SIP body parts to the CCAPI msg. body information block */
    sip_cc_mv_msg_body_to_cc_msg(&cc_body_info, sip_msg);

    cc_connected(CC_SRC_SIP, call_id, line, NULL, recv_info_list, &cc_body_info);
}


void sip_cc_connected_ack (callid_t call_id, line_t line,
                           sipMessage_t *sip_msg)
{
    cc_msgbody_info_t cc_body_info;

    /* Move the SIP body parts to the CCAPI msg. body information block */
    sip_cc_mv_msg_body_to_cc_msg(&cc_body_info, sip_msg);

    cc_connected_ack(CC_SRC_SIP, call_id, line, NULL, &cc_body_info);
}


void sip_cc_release (callid_t call_id, line_t line, cc_causes_t cause,
                     const char *dialstring)
{
    cc_release(CC_SRC_SIP, call_id, line, cause, dialstring, NULL);
}


void sip_cc_release_complete (callid_t call_id, line_t line, cc_causes_t cause)
{
    cc_release_complete(CC_SRC_SIP, call_id, line, cause, NULL);
}


void sip_cc_feature (callid_t call_id, line_t line, cc_features_t feature, void *data)
{
    cc_feature(CC_SRC_SIP, call_id, line, feature, (cc_feature_data_t *)data);
}


void sip_cc_feature_ack (callid_t call_id, line_t line, cc_features_t feature,
                         void *data, cc_causes_t cause)
{
    cc_feature_ack(CC_SRC_SIP, call_id, line, feature, (cc_feature_data_t *)data, cause);
}


void sip_cc_mwi (callid_t call_id, line_t line, boolean on, int type,
                 int newCount, int oldCount, int hpNewCount, int hpOldCount)
{
    cc_mwi(CC_SRC_SIP, call_id, line, on, type, newCount, oldCount, hpNewCount, hpOldCount);
}

void sip_cc_options (callid_t call_id, line_t line, sipMessage_t *pSipMessage)
{
    cc_options_sdp_req(CC_SRC_SIP, call_id, line, pSipMessage);
}

void sip_cc_audit (callid_t call_id, line_t line, boolean apply_ringout)
{
    cc_audit_sdp_req(CC_SRC_SIP, call_id, line, apply_ringout);
}
