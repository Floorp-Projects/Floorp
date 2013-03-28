/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "sip_common_regmgr.h"
#include "util_string.h"

static const char *cc_src_names[] = {
    "GSM",
    "UI",
    "SIP",
    "MISC_APP",
    "CCAPP"
};

#define CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, msg) \
    DEF_DEBUG(DEB_L_C_F_PREFIX"%s -> %s: %-20s\n",\
			DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__),\
            cc_src_name(src_id), cc_src_name(dst_id), msg)


callid_t cc_get_new_call_id (void)
{
    static callid_t call_id = CC_NO_CALL_ID;

    if (++call_id == 0) {
        call_id = 1;
    }

    return call_id;
}


const char *
cc_src_name (cc_srcs_t id)
{
    if ((id <= CC_SRC_MIN) || (id >= CC_SRC_MAX)) {
        return get_debug_string(GSM_UNDEFINED);
    }

    return cc_src_names[id];
}


static void
cc_print_msg (char *pData, int len)
{
    int ix;
    int msg_id = *((int *) pData);

    buginf("\n" CCA_F_PREFIX "cc_msg= %s, 0x=", __FUNCTION__,
                            cc_msg_name((cc_msgs_t) msg_id));
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


/*
 * Return a SysBuf and initialize
 * Parameters supplied by application:
 *  - MinSize: size of buffer requested
 */
cprBuffer_t
cc_get_msg_buf (int min_size)
{
    cprBuffer_t buf;

    if (min_size > CPR_MAX_MSG_SIZE) {
        /* Size requested exceeds maximum ethernet buffer */
        GSM_ERR_MSG(get_debug_string(DEBUG_MSG_BUFFER_TOO_BIG),
                    __FUNCTION__, min_size);
        return (cprBuffer_t)NULL;
    }

    buf = gsm_get_buffer((uint16_t) min_size);
    if (!buf) {
        GSM_ERR_MSG(get_debug_string(DEBUG_SYSBUF_UNAVAILABLE), __FUNCTION__);
        return (cprBuffer_t)NULL;
    }

    /* Clean out the data region of the message */
    memset(buf, 0, min_size);

    CC_DEBUG(DEB_F_PREFIX "Msg id = 0x%0x\n", DEB_F_PREFIX_ARGS(CC_API, __FUNCTION__), buf);

    return buf;
}

static cc_rcs_t
cc_send_cmd_msg (uint32_t cmd, cprBuffer_t buf, uint16_t len, cc_srcs_t dst_id)
{
    cpr_status_e rc;

    CC_DEBUG_MSG cc_print_msg((char *) buf, len);

    switch (dst_id) {
    case CC_SRC_GSM:
        rc = gsm_send_msg(cmd, buf, len);
        if (rc == CPR_FAILURE) {
            cc_free_msg_data((cc_msg_t *) buf);
            cpr_free(buf);
        }
        break;
    case CC_SRC_SIP:
        rc = SIPTaskSendMsg(cmd, buf, len, NULL);
        if (rc == CPR_FAILURE) {
            cc_free_msg_data((cc_msg_t *) buf);
            cpr_free(buf);
        }
        break;
    default:
        rc = CPR_FAILURE;
        break;
    }

    return (rc == CPR_SUCCESS) ? CC_RC_SUCCESS : CC_RC_ERROR;
}

static cc_rcs_t
cc_send_msg (cprBuffer_t buf, uint16_t len, cc_srcs_t dst_id)
{
    cpr_status_e rc;

    CC_DEBUG_MSG cc_print_msg((char *) buf, len);

    switch (dst_id) {
    case CC_SRC_GSM:
        rc = gsm_send_msg(GSM_SIP, buf, len);
        if (rc == CPR_FAILURE) {
            cc_free_msg_data((cc_msg_t *) buf);
            cpr_free(buf);
        }
        break;
    case CC_SRC_SIP:
        rc = SIPTaskSendMsg(SIP_GSM, buf, len, NULL);
        if (rc == CPR_FAILURE) {
            cc_free_msg_data((cc_msg_t *) buf);
            cpr_free(buf);
        }
        break;
    default:
        rc = CPR_FAILURE;
        break;
    }

    return (rc == CPR_SUCCESS) ? CC_RC_SUCCESS : CC_RC_ERROR;
}


/*
 *  ROUTINE:     cc_initialize_msg_body_parts_info
 *
 *  DESCRIPTION: Initializes the msg body part.
 *
 *  PARAMETERS:
 *      msg_body - pointer to cc_msgbody_info_t to be initialized.
 *
 *  RETURNS:
 *      None.
 *
 *  NOTES:       None
 */
void
cc_initialize_msg_body_parts_info (cc_msgbody_info_t *msg_body)
{
    if (msg_body == NULL) {
        return;
    }
    msg_body->num_parts = 0;
    memset(&msg_body->parts[0], 0, sizeof(msg_body->parts));
}

/*
 *  ROUTINE:     cc_mv_msg_body_parts
 *
 *  DESCRIPTION: Move the body parts from source to destination.
 *
 *  PARAMETERS:
 *      dst_msg - pointer to destination cc_msgbody_info_t
 *      src_msg - pointer to source cc_msgbody_info_t
 *
 *  RETURNS:
 *      None
 *
 *  NOTES:       The function attempts to free the message bodies
 *               that might be in the destination to prevent
 *               memory leak from overridden the destination by
 *               moving the new message source.
 *
 *               The dst_msg must be pointed to the initialized
 *               message block either with all zero or at least
 *               number of parts is set to zero to prevent
 *               freeing an invalid address.
 */
void
cc_mv_msg_body_parts (cc_msgbody_info_t *dst_msg, cc_msgbody_info_t *src_msg)
{
    if (dst_msg == NULL) {
        GSM_ERR_MSG(CCA_F_PREFIX "dst is NULL\n", __FUNCTION__);
        return;
    }

    /* Free the msg. bodies that might be in the dest first */
    cc_free_msg_body_parts(dst_msg);

    if (src_msg != NULL) {
        /* copy all of the parts */
        *dst_msg = *src_msg;
        src_msg->num_parts = 0;
    }
}

/*
 *  ROUTINE:     cc_free_msg_body_parts
 *
 *  DESCRIPTION: Free elements in the msg body part structure.
 *
 *  PARAMETERS:
 *      msg_body - pointer to cc_msgbody_info_t for freeing.
 *
 *  RETURNS:
 *      None
 *
 *  NOTES:       None
 */
void
cc_free_msg_body_parts (cc_msgbody_info_t *msg_body)
{
    cc_msgbody_t *part;

    if ((msg_body == NULL) || (msg_body->num_parts == 0)) {
        /* Nothing to be freed */
        return;
    }

    /* Free all of the bodies and their contents */
    part = &msg_body->parts[0];
    for (; msg_body->num_parts; msg_body->num_parts--, part++) {
        if (part->body != NULL) {
            cpr_free(part->body);
            part->body = NULL;
        }
        if (part->content_id != NULL) {
            cpr_free(part->content_id);
            part->content_id = NULL;
        }
    }
}

/*
 *  ROUTINE:     cc_get_msg_body_info_ptr_from_feature_data
 *
 *  DESCRIPTION: The function gets msg pointer from a cc_feature_data_t
 *               structure if there is one. The msg body
 *               info does not aways attach to all features
 *               i.e. only certain feature IDs have msg body
 *               in the feature data. The function returns
 *               pointer to the msg body if the feature has
 *               valid data and for those features that contain
 *               msg body.
 *
 *  PARAMETERS:
 *      id   - cc_feature_t.
 *      data - pointer to cc_feature_data_t of the feature data to
 *             whose embedded resources need to be freed.
 *
 *  RETURNS:
 *      msg_body - pointer to msg_body_info_t if there is
 *                 a msg.
 *
 *  NOTES:       None
 */
static cc_msgbody_info_t *
cc_get_msg_body_info_ptr_from_feature_data (cc_features_t id,
                                            cc_feature_data_t *data)
{
    cc_msgbody_info_t *msg_body = NULL;

    if (data == NULL) {
        return (NULL);
    }

    switch (id) {
    case CC_FEATURE_HOLD:
        msg_body = &data->hold.msg_body;
        break;
    case CC_FEATURE_RESUME:
    case CC_FEATURE_MEDIA:
        msg_body = &data->resume.msg_body;
        break;
    case CC_FEATURE_UPDATE:
        msg_body = &data->update.msg_body;
        break;
    default:
        /*
         * Other ones do not have msg body info yet, add the handling here
         * when adding feature that needs msg body info.
         */
        break;
    }
    return (msg_body);
}

/*
 *  ROUTINE:     cc_cp_caller
 *
 *  DESCRIPTION: Copy caller ID and other fields from source to destination.
 *
 *  PARAMETERS:
 *      dst_caller - pointer to destination cc_caller_id_t
 *      src_caller - pointer to source cc_caller_id_t
 *
 *  RETURNS:
 *      None
 *
 *  Note: See also cc_mv_caller_id() cc_free_caller().
 */
static void
cc_cp_caller (cc_caller_id_t *dst_caller, cc_caller_id_t *src_caller)
{
    if ((src_caller == NULL) || (dst_caller == NULL)) {
        return;
    }

    dst_caller->calling_name = strlib_empty();
    if (src_caller->calling_name != NULL) {
        dst_caller->calling_name = strlib_update(dst_caller->calling_name,
                                                 src_caller->calling_name);
    }

    dst_caller->calling_number = strlib_empty();
    if (src_caller->calling_number != NULL) {
        dst_caller->calling_number = strlib_update(dst_caller->calling_number,
                                                   src_caller->calling_number);
    }

    dst_caller->alt_calling_number = strlib_empty();
    if (src_caller->alt_calling_number != NULL) {
        dst_caller->alt_calling_number = strlib_update(dst_caller->alt_calling_number,
                                                   src_caller->alt_calling_number);
    }

    dst_caller->called_name = strlib_empty();
    if (src_caller->called_name != NULL) {
        dst_caller->called_name = strlib_update(dst_caller->called_name,
                                                src_caller->called_name);
    }

    dst_caller->called_number = strlib_empty();
    if (src_caller->called_number != NULL) {
        dst_caller->called_number = strlib_update(dst_caller->called_number,
                                                  src_caller->called_number);
    }

    dst_caller->orig_called_name = strlib_empty();
    if (src_caller->orig_called_name != NULL) {
        dst_caller->orig_called_name =
            strlib_update(dst_caller->orig_called_name,
                          src_caller->orig_called_name);
    }

    dst_caller->orig_called_number = strlib_empty();
    if (src_caller->orig_called_number != NULL) {
        dst_caller->orig_called_number =
            strlib_update(dst_caller->orig_called_number,
                          src_caller->orig_called_number);
    }

    dst_caller->last_redirect_name = strlib_empty();
    if (src_caller->last_redirect_name != NULL) {
        dst_caller->last_redirect_name =
            strlib_update(dst_caller->last_redirect_name,
                          src_caller->last_redirect_name);
    }

    dst_caller->last_redirect_number = strlib_empty();
    if (src_caller->last_redirect_number) {
        dst_caller->last_redirect_number =
            strlib_update(dst_caller->last_redirect_number,
                          src_caller->last_redirect_number);
    }

    dst_caller->orig_rpid_number = strlib_empty();
    if (src_caller->orig_rpid_number != NULL) {
        dst_caller->orig_rpid_number =
            strlib_update(dst_caller->orig_rpid_number,
                          src_caller->orig_rpid_number);
    }
    dst_caller->display_calling_number = src_caller->display_calling_number;
    dst_caller->display_called_number = src_caller->display_called_number;
    dst_caller->call_type = src_caller->call_type;
    dst_caller->call_instance_id = src_caller->call_instance_id;
}

/*
 *  ROUTINE:     cc_mv_caller_id
 *
 *  DESCRIPTION: Move caller ID fields from source to destination.
 *               The caller ID fields from the destination will be
 *               freed if necessary prior to the move. This allows
 *               caller to replace the older IDs with the new IDs
 *               without data duplication.
 *
 *  PARAMETERS:
 *      dst_caller - pointer to destination cc_caller_id_t
 *      src_caller - pointer to source cc_caller_id_t
 *
 *  RETURNS:
 *      None
 *
 *  Note: See also cc_cp_caller() cc_free_caller().
 */
void
cc_mv_caller_id (cc_caller_id_t *dst_caller, cc_caller_id_t *src_caller)
{
    if ((src_caller == NULL) || (dst_caller == NULL)) {
        return;
    }

    /*
     * Move the IDs from the source to the destination.
     * After moved the IDs from the source to the destination then
     * the source needs to be NULL.
     */
    if (src_caller->calling_name != NULL) {
        if (dst_caller->calling_name != NULL) {
            strlib_free(dst_caller->calling_name);
        }
        dst_caller->calling_name = src_caller->calling_name;
        src_caller->calling_name = NULL;
    }

    if (src_caller->calling_number != NULL) {
        if (dst_caller->calling_number != NULL) {
            strlib_free(dst_caller->calling_number);
        }
        dst_caller->calling_number = src_caller->calling_number;
        src_caller->calling_number = NULL;
    }

    if (src_caller->alt_calling_number != NULL) {
        if (dst_caller->alt_calling_number != NULL) {
            strlib_free(dst_caller->alt_calling_number);
        }
        dst_caller->alt_calling_number = src_caller->alt_calling_number;
        src_caller->alt_calling_number = NULL;
    }

    if (src_caller->called_name != NULL) {
        if (dst_caller->called_name != NULL) {
            strlib_free(dst_caller->called_name);
        }
        dst_caller->called_name = src_caller->called_name;
        src_caller->called_name = NULL;
    }

    if (src_caller->called_number != NULL) {
        if (dst_caller->called_number != NULL) {
            strlib_free(dst_caller->called_number);
        }
        dst_caller->called_number = src_caller->called_number;
        src_caller->called_number = NULL;
    }

    if (src_caller->orig_called_name != NULL) {
        if (dst_caller->orig_called_name != NULL) {
            strlib_free(dst_caller->orig_called_name);
        }
        dst_caller->orig_called_name = src_caller->orig_called_name;
        src_caller->orig_called_name = NULL;
    }

    if (src_caller->orig_called_number != NULL) {
        if (dst_caller->orig_called_number != NULL) {
            strlib_free(dst_caller->orig_called_number);
        }
        dst_caller->orig_called_number = src_caller->orig_called_number;
        src_caller->orig_called_number = NULL;
    }

    if (src_caller->last_redirect_name != NULL) {
        if (dst_caller->last_redirect_name != NULL) {
            strlib_free(dst_caller->last_redirect_name);
        }
        dst_caller->last_redirect_name = src_caller->last_redirect_name;
        src_caller->last_redirect_name = NULL;
    }

    if (src_caller->last_redirect_number != NULL) {
        if (dst_caller->last_redirect_number != NULL) {
            strlib_free(dst_caller->last_redirect_number);
        }
        dst_caller->last_redirect_number = src_caller->last_redirect_number;
        src_caller->last_redirect_number = NULL;
    }

    if (src_caller->orig_rpid_number != NULL) {
        if (dst_caller->orig_rpid_number != NULL) {
            strlib_free(dst_caller->orig_rpid_number);
        }
        dst_caller->orig_rpid_number = src_caller->orig_rpid_number;
        src_caller->orig_rpid_number = NULL;
    }
    dst_caller->display_calling_number = src_caller->display_calling_number;
    dst_caller->display_called_number = src_caller->display_called_number;
}

/*
 *  ROUTINE:     cc_free_caller_id
 *
 *  DESCRIPTION: Free caller ID fields. The IDs that are not NULL IDs will be
 *               freed because others might have been moved already.
 *
 *  PARAMETERS:
 *      caller - pointer to destination cc_caller_id_t
 *
 *  RETURNS:
 *      None
 *
 *  Note: See also cc_cp_caller() cc_mv_caller().
 */
static void
cc_free_caller_id (cc_caller_id_t *caller)
{
    if (caller == NULL) {
        return;
    }

    if (caller->calling_name != NULL) {
        strlib_free(caller->calling_name);
    }

    if (caller->calling_number != NULL) {
        strlib_free(caller->calling_number);
    }

    if (caller->alt_calling_number != NULL) {
        strlib_free(caller->alt_calling_number);
    }

    if (caller->called_name != NULL) {
        strlib_free(caller->called_name);
    }

    if (caller->called_number != NULL) {
        strlib_free(caller->called_number);
    }

    if (caller->orig_called_name != NULL) {
        strlib_free(caller->orig_called_name);
    }

    if (caller->orig_called_number != NULL) {
        strlib_free(caller->orig_called_number);
    }

    if (caller->last_redirect_name != NULL) {
        strlib_free(caller->last_redirect_name);
    }

    if (caller->last_redirect_number) {
        strlib_free(caller->last_redirect_number);
    }

    if (caller->orig_rpid_number != NULL) {
        strlib_free(caller->orig_rpid_number);
    }
}

/*
 *  ROUTINE:     cc_free_msg_data
 *
 *  DESCRIPTION: Free resources embedded in CCAPI msg. data
 *               structure.
 *
 *  PARAMETERS:
 *      data - pointer to cc_msg_t whose embedded resources
 *             need to be freed.
 *
 *  RETURNS:
 *      None
 */
void
cc_free_msg_data (cc_msg_t *msg)
{
    cc_msgbody_info_t *msg_body = NULL;
    cc_caller_id_t *caller_id = NULL;

    if (msg == NULL) {
        return;
    }
    switch (msg->msg.setup.msg_id) {
    case CC_MSG_SETUP:
        msg_body = &msg->msg.setup.msg_body;
        caller_id = &msg->msg.setup.caller_id;
        strlib_free(msg->msg.setup.recv_info_list);
        break;
    case CC_MSG_SETUP_ACK:
        msg_body = &msg->msg.setup_ack.msg_body;
        caller_id = &msg->msg.setup_ack.caller_id;
        break;
    case CC_MSG_PROCEEDING:
        caller_id = &msg->msg.proceeding.caller_id;
        break;
    case CC_MSG_ALERTING:
        msg_body = &msg->msg.alerting.msg_body;
        caller_id = &msg->msg.alerting.caller_id;
        break;
    case CC_MSG_CONNECTED:
        msg_body = &msg->msg.connected.msg_body;
        caller_id = &msg->msg.connected.caller_id;
        strlib_free(msg->msg.connected.recv_info_list);
        break;
    case CC_MSG_CONNECTED_ACK:
        msg_body = &msg->msg.connected_ack.msg_body;
        caller_id = &msg->msg.connected_ack.caller_id;
        break;
    case CC_MSG_FEATURE:
        if (msg->msg.feature.data_valid) {
            msg_body = cc_get_msg_body_info_ptr_from_feature_data(
                               msg->msg.feature.feature_id,
                               &msg->msg.feature.data);

            if (msg->msg.feature.feature_id == CC_FEATURE_CALLINFO) {
                caller_id = &msg->msg.feature.data.call_info.caller_id;
            }
        }
        break;
    case CC_MSG_FEATURE_ACK:
        if (msg->msg.feature_ack.data_valid) {
            msg_body = cc_get_msg_body_info_ptr_from_feature_data(
                               msg->msg.feature_ack.feature_id,
                               &msg->msg.feature_ack.data);
        }
        break;
    case CC_MSG_OPTIONS_ACK:
        msg_body = &msg->msg.options_ack.msg_body;
        break;
    case CC_MSG_AUDIT_ACK:
        msg_body = &msg->msg.audit_ack.msg_body;
        break;
    case CC_MSG_INFO:
        strlib_free(msg->msg.info.message_body);
        strlib_free(msg->msg.info.content_type);
        strlib_free(msg->msg.info.info_package);
        break;
    default:
        return;
    }
    cc_free_msg_body_parts(msg_body);
    cc_free_caller_id(caller_id);
}

/*
 *  ROUTINE:     cc_cp_msg_body_parts
 *
 *  DESCRIPTION: Copy elements in the msg body part structure.
 *
 *  PARAMETERS:
 *      dst_msg - pointer to destination cc_msgbody_info_t
 *      src_msg - pointer to source cc_msgbody_info_t
 *
 *  RETURNS:
 *      CC_RC_ERROR
 *      CC_RC_SUCCESS
 *
 *  NOTES:       The function attempts to free the message bodies
 *               that might be in the destination to prevent
 *               memory leak from overridden the destination by
 *               moving the new message source.
 *
 *               The dst_msg must be pointed to the initialized
 *               message block either with all zero or at least
 *               number of parts is set to zero to prevent
 *               freeing an invalid address.
 */
cc_rcs_t
cc_cp_msg_body_parts (cc_msgbody_info_t *dst_msg, cc_msgbody_info_t *src_msg)
{
    uint32_t        i, body_length;
    cc_msgbody_t   *src_part, *dst_part;
    cc_rcs_t        status;
    int             len;

    if ((dst_msg == NULL) || (src_msg == NULL)) {
        return CC_RC_ERROR;
    }
    /* Free the msg. bodies that might be in the dest first */
    cc_free_msg_body_parts(dst_msg);

    src_part = &src_msg->parts[0];
    dst_part = &dst_msg->parts[0];

    /* Copy all of the fields of all body parts */
    status = CC_RC_SUCCESS;
    for (i = 0; i < src_msg->num_parts; i++, src_part++, dst_part++) {
        dst_part->content_type = src_part->content_type;
        dst_part->content_disposition = src_part->content_disposition;

        /* Copy body */
        dst_part->body = NULL;
        dst_part->body_length = 0;
        if ((src_part->body != NULL) && (src_part->body_length > 0)) {
            body_length = src_part->body_length;
            dst_part->body = (char *) cpr_malloc(body_length);

            if (dst_part->body == NULL) {
                /* Unable to allocate memory for body */
                status = CC_RC_ERROR;
                break;
            } else {
                /*
                 * Copy body including NULL char but the length field
                 * does not include the extra NULL char.
                 */
                memcpy(dst_part->body, src_part->body, body_length);
                dst_part->body_length = src_part->body_length;
            }
        }

        dst_part->content_id = NULL;
        /* Copy content ID */
        if (src_part->content_id != NULL) {
            len = strlen(src_part->content_id) + 1;
            dst_part->content_id = (char *) cpr_malloc(len);
            if (dst_part->content_id != NULL) {
                memcpy(dst_part->content_id, src_part->content_id, len);
            } else {
                /* Unable to allocate memory for content ID */
                status = CC_RC_ERROR;
                break;
            }
        }
    }
    dst_msg->num_parts = i;     /* number of part copied */
    dst_msg->content_type = src_msg->content_type;

    if (status != CC_RC_SUCCESS) {
        /*
         * Unable to duplicate the body parts, free all of the allocated
         * resources
         */
        cc_free_msg_body_parts(dst_msg);
    }
    return status;
}

void
cc_int_setup (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
              line_t line, cc_caller_id_t *caller_id,
              cc_alerting_type alert_info, vcm_ring_mode_t alerting_ring,
              vcm_tones_t alerting_tone, cc_redirect_t *redirect,
              cc_call_info_t *call_info_p, boolean replaces,
              string_t recv_info_list, cc_msgbody_info_t *msg_body)
{
    cc_setup_t *pmsg;

    if (caller_id == NULL) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG("%s: caller id is NULL\n", __FUNCTION__);
        return;
    }

    CC_DEBUG(DEB_L_C_F_PREFIX "    CGPD= %s, CGPN= %s,\n    CDPD= %s, CDPN= %s\n",
			 DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__),
             caller_id->calling_name, caller_id->calling_number,
             caller_id->called_name, caller_id->called_number);

    pmsg = (cc_setup_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id        = CC_MSG_SETUP;
    pmsg->src_id        = src_id;
    pmsg->call_id       = call_id;
    pmsg->line          = line;
    pmsg->alert_info    = alert_info;
    pmsg->alerting_ring = alerting_ring;
    pmsg->alerting_tone = alerting_tone;
    cc_cp_caller(&pmsg->caller_id, caller_id);

    pmsg->call_info.type = CC_FEAT_NONE;
    if (call_info_p) {
        pmsg->call_info = *call_info_p;
    }

    pmsg->replaces = replaces;

    if (redirect != NULL) {
        pmsg->redirect = *redirect;
    }

    /* Info Package */
    if (recv_info_list && (*recv_info_list != '\0')) {
        pmsg->recv_info_list = strlib_copy(recv_info_list);
    } else {
        pmsg->recv_info_list = strlib_empty();
    }

    /* Move body parts if there are any */
    pmsg->msg_body.num_parts = 0;
    cc_mv_msg_body_parts(&pmsg->msg_body, msg_body);

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_setup_ack (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                  line_t line, cc_caller_id_t *caller_id,
                  cc_msgbody_info_t *msg_body)
{
    cc_setup_ack_t *pmsg;

    pmsg = (cc_setup_ack_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_SETUP_ACK;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    if (caller_id != NULL) {
        cc_cp_caller(&pmsg->caller_id, caller_id);
    }
    /* Move body parts if there are any */
    pmsg->msg_body.num_parts = 0;
    cc_mv_msg_body_parts(&pmsg->msg_body, msg_body);

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
}


void
cc_int_proceeding (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                   line_t line, cc_caller_id_t *caller_id)
{
    cc_proceeding_t *pmsg;

    pmsg = (cc_proceeding_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_PROCEEDING;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    if (caller_id != NULL) {
        cc_cp_caller(&pmsg->caller_id, caller_id);
    }

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg(pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_alerting (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                 line_t line, cc_caller_id_t *caller_id,
                 cc_msgbody_info_t *msg_body, boolean inband)
{
    cc_alerting_t *pmsg;


    pmsg = (cc_alerting_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_ALERTING;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    if (caller_id != NULL) {
        cc_cp_caller(&pmsg->caller_id, caller_id);
    }
    /* Move body parts if there are any */
    pmsg->msg_body.num_parts = 0;
    cc_mv_msg_body_parts(&pmsg->msg_body, msg_body);

    pmsg->inband = inband;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));
    CC_DEBUG(DEB_L_C_F_PREFIX "    inband= %d\n",
		DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), inband);

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_connected (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                  line_t line, cc_caller_id_t *caller_id,
                  string_t recv_info_list, cc_msgbody_info_t *msg_body)
{
    cc_connected_t *pmsg;

    pmsg = (cc_connected_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_CONNECTED;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    if (caller_id != NULL) {
        cc_cp_caller(&pmsg->caller_id, caller_id);
    }

    /* Info Package */
    if (recv_info_list && (*recv_info_list != '\0')) {
        pmsg->recv_info_list = strlib_copy(recv_info_list);
    } else {
        pmsg->recv_info_list = strlib_empty();
    }

    /* Move body parts if there are any */
    pmsg->msg_body.num_parts = 0;
    cc_mv_msg_body_parts(&pmsg->msg_body, msg_body);

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_connected_ack (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                      line_t line, cc_caller_id_t *caller_id,
                      cc_msgbody_info_t *msg_body)
{
    cc_connected_ack_t *pmsg;

    pmsg = (cc_connected_ack_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_CONNECTED_ACK;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    if (caller_id != NULL) {
        cc_cp_caller(&pmsg->caller_id, caller_id);
    }
    /* Move body parts if there are any */
    pmsg->msg_body.num_parts = 0;
    cc_mv_msg_body_parts(&pmsg->msg_body, msg_body);

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_release (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                line_t line, cc_causes_t cause, const char *dialstring,
                cc_kfact_t *kfactor)
{
    cc_release_t *pmsg;

    if (dialstring == NULL) {
        CC_DEBUG(DEB_L_C_F_PREFIX "    cause= %s\n",
			DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), cc_cause_name(cause));
    } else {
        CC_DEBUG(DEB_L_C_F_PREFIX "    cause= %s, dialstring= %s\n",
			DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), cc_cause_name(cause), dialstring);
    }

    pmsg = (cc_release_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_RELEASE;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    pmsg->cause   = cause;

    if (dialstring) {
        sstrncpy(pmsg->dialstring, dialstring, CC_MAX_DIALSTRING_LEN);
    }
    if (kfactor != NULL) {
        sstrncpy(pmsg->kfactor.rxstats, kfactor->rxstats, CC_KFACTOR_STAT_LEN);
        sstrncpy(pmsg->kfactor.txstats, kfactor->txstats, CC_KFACTOR_STAT_LEN);
    }

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_release_complete (cc_srcs_t src_id, cc_srcs_t dst_id,
                         callid_t call_id, line_t line, cc_causes_t cause,
                         cc_kfact_t *kfactor)
{
    cc_release_complete_t *pmsg;


    pmsg = (cc_release_complete_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_RELEASE_COMPLETE;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    pmsg->cause   = cause;
    if (kfactor != NULL) {
        sstrncpy(pmsg->kfactor.rxstats, kfactor->rxstats, CC_KFACTOR_STAT_LEN);
        sstrncpy(pmsg->kfactor.txstats, kfactor->txstats, CC_KFACTOR_STAT_LEN);
    }

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));
    CC_DEBUG(DEB_L_C_F_PREFIX "    cause= %s\n",
		DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), cc_cause_name(cause));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_feature2 (cc_msgs_t msg_id, cc_srcs_t src_id, cc_srcs_t dst_id,
                 callid_t call_id, line_t line, cc_features_t feature_id,
                 cc_feature_data_t *data)
{
    cc_feature_t *pmsg;
    cc_msgbody_info_t *msg_body;

    pmsg = (cc_feature_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG("%s: no buffer available for feat=%s\n", __FUNCTION__,
                    cc_feature_name(feature_id));
        return;
    }

    pmsg->msg_id     = msg_id;
    pmsg->src_id     = src_id;
    pmsg->call_id    = call_id;
    pmsg->line       = line;
    pmsg->feature_id = feature_id;
    pmsg->data_valid = (data == NULL) ? (FALSE) : (TRUE);

    if (pmsg->data_valid == TRUE) {
        pmsg->data = *data;
        /*
         * For call Info feature, need to copy the caller ID
         */
        if (feature_id == CC_FEATURE_CALLINFO) {
            /* Copy the caller ID */
            cc_cp_caller(&pmsg->data.call_info.caller_id,
                         &data->call_info.caller_id);
        }
        /*
         * Clear the msg body from the source now since the msg. bodies
         * has been transferred to to the CCAPI msg.
         */
        msg_body = cc_get_msg_body_info_ptr_from_feature_data(feature_id, data);
        cc_initialize_msg_body_parts_info(msg_body);
    }

    if ((feature_id == CC_FEATURE_XFER) ||
        (feature_id == CC_FEATURE_BLIND_XFER)) {
        if (data != NULL) {
            CC_DEBUG(DEB_L_C_F_PREFIX
                     "method= %d, call_id= %d, cause= %s dialstring= %s\n",
                     DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__),
					 data->xfer.method, data->xfer.target_call_id,
                     cc_cause_name(data->xfer.cause), data->xfer.dialstring);
        }
    }
    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_feature_name(feature_id));
    CC_DEBUG(DEB_L_C_F_PREFIX "feature= %s, data= %p\n",
		DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), cc_feature_name(feature_id), data);
    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG("%s: unable to send msg for feat=%s\n", __FUNCTION__,
                    cc_feature_name(feature_id));
    }
    return;
}

/*
 *  Helper function for the next six functions that populates and sends
 *  a feature message
 *
 */
static void send_message_helper(
    cc_msgs_t msg_id,
    cc_srcs_t src_id,
    cc_srcs_t dst_id,
    callid_t call_id,
    line_t line,
    cc_features_t feature_id,
    cc_feature_data_t *data,
    string_t sdp,
    cc_jsep_action_t action)
{
    cc_feature_t *pmsg;
    cc_msgbody_info_t *msg_body;

    pmsg = (cc_feature_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        GSM_ERR_MSG("%s: no buffer available for feat=%s\n", __FUNCTION__,
                    cc_feature_name(feature_id));
        return;
    }

    pmsg->msg_id     = msg_id;
    pmsg->src_id     = src_id;
    pmsg->call_id    = call_id;
    pmsg->line       = line;
    pmsg->feature_id = feature_id;
    pmsg->data_valid = (data == NULL) ? (FALSE) : (TRUE);

    if (msg_id == CC_MSG_SETLOCALDESC || msg_id == CC_MSG_SETREMOTEDESC) {
        pmsg->action = action;
    }

    if (msg_id == CC_MSG_CREATEANSWER || msg_id == CC_MSG_SETLOCALDESC || msg_id == CC_MSG_SETREMOTEDESC) {
        sstrncpy(pmsg->sdp, sdp, sizeof(pmsg->sdp));
    }

    if (pmsg->data_valid == TRUE) {
        pmsg->data = *data;
        /*
         * For call Info feature, need to copy the caller ID
         */
        if (feature_id == CC_FEATURE_CALLINFO) {
            /* Copy the caller ID */
            cc_cp_caller(&pmsg->data.call_info.caller_id,
                         &data->call_info.caller_id);
        }
        /*
         * Clear the msg body from the source now since the msg. bodies
         * has been transferred to to the CCAPI msg.
         */
        msg_body = cc_get_msg_body_info_ptr_from_feature_data(feature_id, data);
        cc_initialize_msg_body_parts_info(msg_body);
    }

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_feature_name(feature_id));
    CC_DEBUG(DEB_L_C_F_PREFIX "feature= %s, data= %p\n",
        DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), cc_feature_name(feature_id), data);
    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG("%s: unable to send msg for feat=%s\n", __FUNCTION__,
                    cc_feature_name(feature_id));
    }

    return;
}

void
cc_createoffer (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                line_t line, cc_features_t feature_id, cc_feature_data_t *data)
{
    send_message_helper(CC_MSG_CREATEOFFER, src_id, dst_id, call_id, line,
        feature_id, data, NULL, 0);

    return;
}


void
cc_createanswer (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                line_t line, cc_features_t feature_id, string_t sdp, cc_feature_data_t *data)
{
    send_message_helper(CC_MSG_CREATEANSWER, src_id, dst_id, call_id, line,
        feature_id, data, sdp, 0);

    return;
}


void cc_setlocaldesc (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id, line_t line,
                    cc_features_t feature_id, cc_jsep_action_t action, string_t sdp,  cc_feature_data_t *data)
{
    send_message_helper(CC_MSG_SETLOCALDESC, src_id, dst_id, call_id, line,
        feature_id, data, sdp, action);

    return;
}

void cc_setremotedesc (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id, line_t line,
                    cc_features_t feature_id, cc_jsep_action_t action, string_t sdp, cc_feature_data_t *data)
{
    send_message_helper(CC_MSG_SETREMOTEDESC, src_id, dst_id, call_id, line,
        feature_id, data, sdp, action);

    return;
}

void
cc_int_feature_ack (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                    line_t line, cc_features_t feature_id,
                    cc_feature_data_t *data, cc_causes_t cause)
{
    cc_feature_ack_t *pmsg;
    cc_msgbody_info_t *msg_body;

    pmsg = (cc_feature_ack_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id     = CC_MSG_FEATURE_ACK;
    pmsg->src_id     = src_id;
    pmsg->call_id    = call_id;
    pmsg->line       = line;
    pmsg->feature_id = feature_id;
    pmsg->data_valid = (data == NULL) ? (FALSE) : (TRUE);

    if (pmsg->data_valid == TRUE) {
        pmsg->data = *data;
        /*
         * Clear the msg body from the source now since the msg. bodies
         * has been transferred to to the CCAPI msg.
         */
        msg_body = cc_get_msg_body_info_ptr_from_feature_data(feature_id, data);
        cc_initialize_msg_body_parts_info(msg_body);
    }
    pmsg->cause = cause;

    if ((feature_id == CC_FEATURE_XFER) ||
        (feature_id == CC_FEATURE_BLIND_XFER)) {
        if (data != NULL) {
            CC_DEBUG(DEB_L_C_F_PREFIX
                     "method= %d, call_id= %d, cause= %s dialstring= %s\n",
                     DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__),
					 data->xfer.method, data->xfer.target_call_id,
                     cc_cause_name(data->xfer.cause), data->xfer.dialstring);
        }
    }

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));
    CC_DEBUG(DEB_L_C_F_PREFIX "feature= %s, data= %p, cause= %s\n",
			DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), cc_feature_name(feature_id), data,
             cc_cause_name(cause));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_offhook (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t prim_call_id,
                cc_hold_resume_reason_e consult_reason, callid_t call_id,
                line_t line, char *global_call_id, monitor_mode_t monitor_mode,
                cfwdall_mode_t cfwdall_mode)
{
    cc_offhook_t *pmsg;

    pmsg = (cc_offhook_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_OFFHOOK;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    if (global_call_id != NULL) {
        sstrncpy(pmsg->global_call_id, global_call_id, CC_GCID_LEN);
    }
    pmsg->prim_call_id = prim_call_id;
    pmsg->hold_resume_reason = consult_reason;
    pmsg->monitor_mode = monitor_mode;
    pmsg->cfwdall_mode = cfwdall_mode;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_line (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id, line_t line)
{
    cc_line_t *pmsg;

    pmsg = (cc_line_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_LINE;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_onhook (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t prim_call_id,
               cc_hold_resume_reason_e consult_reason, callid_t call_id,
               line_t line, boolean softkey, cc_onhook_reason_e active_list,
               const char *filename, int fileline)
{
    cc_onhook_t *pmsg;

    pmsg = (cc_onhook_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id       = CC_MSG_ONHOOK;
    pmsg->src_id       = src_id;
    pmsg->call_id      = call_id;
    pmsg->line         = line;
    pmsg->softkey      = softkey;
    pmsg->prim_call_id = prim_call_id;
    pmsg->hold_resume_reason = consult_reason;
    pmsg->active_list  = active_list;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));
    DEF_DEBUG("(%u/%u) On-hook called from %s:%d",
              line, call_id, filename, fileline);

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_digit_begin (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                    line_t line, int digit)
{
    cc_digit_begin_t *pmsg;

    pmsg = (cc_digit_begin_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_DIGIT_BEGIN;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->digit   = digit;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_dialstring (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                   line_t line, const char *dialstring, const char *g_call_id,
                   monitor_mode_t monitor_mode)
{
    cc_dialstring_t *pmsg;

    if (dialstring == NULL) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG("%s: no dialstring\n", __FUNCTION__);
        return;
    }

    CC_DEBUG(DEB_L_C_F_PREFIX "dialstring= %s\n",
		DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__),dialstring);

    pmsg = (cc_dialstring_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_DIALSTRING;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    sstrncpy(pmsg->dialstring, dialstring, CC_MAX_DIALSTRING_LEN);
    sstrncpy(pmsg->g_call_id, g_call_id, CC_GCID_LEN);
    pmsg->monitor_mode    = monitor_mode;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}


void
cc_int_mwi (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id, line_t line,
            boolean on, int type, int newCount, int oldCount, int hpNewCount, int hpOldCount)
{
    cc_mwi_t *pmsg;

    pmsg = (cc_mwi_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_MWI;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    pmsg->msgSummary.on = on;
    pmsg->msgSummary.type = type;
    pmsg->msgSummary.newCount = newCount;
    pmsg->msgSummary.oldCount = oldCount;
    pmsg->msgSummary.hpNewCount = hpNewCount;
    pmsg->msgSummary.hpOldCount = hpOldCount;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));
    CC_DEBUG(DEB_L_C_F_PREFIX "    mwi status= %d\n new count= %d old count= %d",
		DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), on, newCount, oldCount);

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}

void
cc_int_options_sdp_req (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                        line_t line, void *pMessage)
{
    cc_options_sdp_req_t *pmsg;

    pmsg = (cc_options_sdp_req_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id   = CC_MSG_OPTIONS;
    pmsg->src_id   = src_id;
    pmsg->call_id  = call_id;
    pmsg->line     = line;
    pmsg->pMessage = pMessage;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));
    CC_DEBUG(DEB_L_C_F_PREFIX " message ptr=%p\n",
		DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), pMessage);

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}

void
cc_int_options_sdp_ack (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                        line_t line, void *pMessage, cc_msgbody_info_t *msg_body)
{
    cc_options_sdp_ack_t *pmsg;

    pmsg = (cc_options_sdp_ack_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id   = CC_MSG_OPTIONS_ACK;
    pmsg->src_id   = src_id;
    pmsg->call_id  = call_id;
    pmsg->line     = line;
    pmsg->pMessage = pMessage;
    /* Move body parts if there are any */
    pmsg->msg_body.num_parts = 0;
    cc_mv_msg_body_parts(&pmsg->msg_body, msg_body);

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));
    CC_DEBUG(DEB_L_C_F_PREFIX " message ptr=%p\n",
		DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__), pMessage);

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}

void
cc_int_audit_sdp_req (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                      line_t line, boolean apply_ringout)
{
    cc_audit_sdp_req_t *pmsg;

    pmsg = (cc_audit_sdp_req_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_AUDIT;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    pmsg->apply_ringout = apply_ringout;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));
    CC_DEBUG(DEB_L_C_F_PREFIX "\n", DEB_L_C_F_PREFIX_ARGS(CC_API, line, call_id, __FUNCTION__));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}

void
cc_int_audit_sdp_ack (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                      line_t line, cc_msgbody_info_t *msg_body)
{
    cc_audit_sdp_ack_t *pmsg;

    pmsg = (cc_audit_sdp_ack_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id  = CC_MSG_AUDIT_ACK;
    pmsg->src_id  = src_id;
    pmsg->call_id = call_id;
    pmsg->line    = line;
    /* Move body parts if there are any */
    pmsg->msg_body.num_parts = 0;
    cc_mv_msg_body_parts(&pmsg->msg_body, msg_body);

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}

void
cc_int_fail_fallback (cc_srcs_t src_id, cc_srcs_t dst_id, int rsp_type,
                      cc_regmgr_rsp_e rsp_id, boolean waited)
{
    cc_regmgr_t *pmsg;


    pmsg = (cc_regmgr_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id    = CC_MSG_FAILOVER_FALLBACK;
    pmsg->src_id    = src_id;
    pmsg->rsp_type  = rsp_type;
    pmsg->rsp_id    = rsp_id;
    pmsg->wait_flag = waited;

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, 0, 0, cc_msg_name(pmsg->msg_id));
    CC_DEBUG(DEB_F_PREFIX "rsp_type= %s rsp_id= %s waited = %d\n",
             DEB_F_PREFIX_ARGS(CC_API, __FUNCTION__),
             rsp_type == RSP_START ? "RSP_START" : "RSP_COMPLETE",
             rsp_id == CC_REG_FAILOVER_RSP  ? "REG_FAILOVER_RSP" : "REG_FALLBACK_RSP",
             waited);

    if (cc_send_cmd_msg(REG_MGR_STATE_CHANGE, (cprBuffer_t) pmsg,
            sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}

void
cc_int_info (cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
             line_t line, string_t info_package, string_t content_type,
             string_t message_body)
{
    cc_info_t *pmsg;


    pmsg = (cc_info_t *) cc_get_msg_buf(sizeof(*pmsg));
    if (!pmsg) {
        // nobody checks, CC_RC_ERROR, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_NO_MSG_BUFFER), __FUNCTION__);
        return;
    }

    pmsg->msg_id       = CC_MSG_INFO;
    pmsg->call_id      = call_id;
    pmsg->line         = line;
    pmsg->info_package = strlib_copy(info_package);
    pmsg->content_type = strlib_copy(content_type);
    pmsg->message_body = strlib_copy(message_body);

    CC_DEBUG_ENTRY(__FUNCTION__, src_id, dst_id, call_id, line, cc_msg_name(pmsg->msg_id));

    if (cc_send_msg((cprBuffer_t) pmsg, sizeof(*pmsg), dst_id) != CC_RC_SUCCESS) {
        // nobody checks the return code, so generate error message
        GSM_ERR_MSG(get_debug_string(CC_SEND_FAILURE), __FUNCTION__);
    }
    return;
}

void
cc_init (void)
{
    /* Placeholder function for any initialization tasks */
}

