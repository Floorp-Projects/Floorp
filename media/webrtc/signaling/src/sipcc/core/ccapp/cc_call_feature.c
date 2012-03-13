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

#include "cc_call_feature.h"
#include "CCProvider.h"
#include "sessionConstants.h"
#include "sessionTypes.h"
#include "lsm.h"
#include "phone_debug.h"
#include "text_strings.h"
#include "ccapi.h"
#include "ccapp_task.h"

extern cpr_status_e ccappTaskPostMsg(unsigned int msgId, void * data, uint16_t len, int appId);

/**
 * Internal method: create call handle
 * @param line
 * @paran call_id
 */
cc_call_handle_t cc_createCallHandle(cc_lineid_t line, cc_callid_t call_id)
{
        return (CREATE_CALL_HANDLE(line, call_id));
}

/**
 * Internal method: assign a valid call id.
 * @param line_id line number
 * @param call_id call id
 * @return void
 */
void cc_getLineIdAndCallId (cc_lineid_t *line_id, cc_callid_t *call_id)
{
    // assign proper line_id and call_id if not already there
    if ((*line_id) == 0 || (*line_id) == CC_ALL_LINES) {
        /*
         * If the filter is the All Calls Complex Filter and the primary line
         * is at its configured call capacity, the next available line should
         * be used. In this scenario, sessionUI/Mgr send the line_id as zero.
         */
        (*line_id) = lsm_get_available_line(FALSE);
    }

    if ((*call_id) == 0) {
        (*call_id) = cc_get_new_call_id();
    }
}

/**
 * Invoke a call feature.
 */
cc_return_t cc_invokeFeature(cc_call_handle_t call_handle, group_cc_feature_t featureId, cc_sdp_direction_t video_pref, string_t data) {
	session_feature_t callFeature;
    callFeature.session_id = (SESSIONTYPE_CALLCONTROL << CC_SID_TYPE_SHIFT) + call_handle;
    callFeature.featureID = featureId;
    callFeature.featData.ccData.state = video_pref;
    CCAPP_DEBUG(DEB_F_PREFIX"cc_invokeFeature:sid=%d, line=%d, cid=%d, fid=%d, video_pref=%s data=%s\n",
                        DEB_F_PREFIX_ARGS("cc_call_feature", "cc_invokeFeature"),
                        callFeature.session_id,
                        GET_LINE_ID(call_handle),
                        GET_CALL_ID(call_handle),
                        featureId, SDP_DIRECTION_PRINT(video_pref),
                        ((featureId == CC_FEATURE_KEYPRESS) ? "...": data));

    switch (featureId) {
    case CC_FEATURE_KEYPRESS:
    case CC_FEATURE_DIALSTR:
    case CC_FEATURE_SPEEDDIAL:
    case CC_FEATURE_BLIND_XFER_WITH_DIALSTRING:
    case CC_FEATURE_END_CALL:
    case CC_FEATURE_B2BCONF:
    case CC_FEATURE_CONF:
    case CC_FEATURE_XFER:
    case CC_FEATURE_HOLD:
    	callFeature.featData.ccData.info = strlib_malloc(data, strlen(data));
        callFeature.featData.ccData.info1 = NULL;
    	break;

    default:
        callFeature.featData.ccData.info = NULL;
        callFeature.featData.ccData.info1 = NULL;
    	break;
    }

    if (ccappTaskPostMsg(CCAPP_INVOKE_FEATURE, &callFeature, sizeof(session_feature_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
            CCAPP_DEBUG(DEB_F_PREFIX"ccappTaskSendMsg failed\n",
            		DEB_F_PREFIX_ARGS("cc_call_feature", "cc_invokeFeature"));
            return CC_FAILURE;
	}
	return CC_SUCCESS;
}
/***********************************Basic Call Feature Control Methods************************************
 * This section defines all the call related methods that an upper layer can use to control
 * a call in progress.
 */

/**
 * Used to create any outgoing call regular call. The incoming/reverting/consultation call will be
 * created by the stack. It creates a call place holder and initialize the memory for a call. An user needs
 * other methods to start the call, such as the method OriginateCall, etc
 * @param line line number that is invoked and is assigned
 * @return call handle wich includes the assigned line and call id
 */
cc_call_handle_t CC_createCall(cc_lineid_t line) {
	static const char fname[] = "CC_CreateCall";
	//Create call handle to initialize the memory.
	//call handle
	cc_call_handle_t call_handle = CC_EMPTY_CALL_HANDLE;
	cc_lineid_t lineid = line;
	cc_callid_t callid = CC_NO_CALL_ID;


	//Assign line and call id.
	cc_getLineIdAndCallId(&lineid, &callid);
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, callid, lineid, fname));

	if (lineid == CC_NO_LINE) {
        lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);
        return call_handle;
    }

    call_handle = cc_createCallHandle(lineid, callid);

	return call_handle;
}

/**
 * Start the call that was created.
 * @param the call handle
 * @return SUCCESS or FAILURE
 */
 /*move it up...*/
cc_return_t CC_CallFeature_originateCall(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref) {
	static const char fname[] = "CC_CallFeature_originateCall:";
	//CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
	//		GET_LINE_ID(call_handle), fname));
	CCAPP_DEBUG(DEB_F_PREFIX"CC_CallFeature_originateCall:cHandle=%d\n",
	                        DEB_F_PREFIX_ARGS("cc_call_feature", fname),
	                        call_handle);
    return cc_invokeFeature(call_handle, CC_FEATURE_OFFHOOK, video_pref, NULL);
}

/**
 * Terminate or end a normal call.
 * @param call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_terminateCall(cc_call_handle_t call_handle) {
	static const char fname[] = "CC_CallFeature_TerminateCall";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

    return cc_invokeFeature(call_handle, CC_FEATURE_ONHOOK, CC_SDP_MAX_QOS_DIRECTIONS, NULL);
}

/**
 * Answer an incoming or reverting call.
 * @param call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_answerCall(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref) {
	static const char fname[] = "CC_CallFeature_AnswerCall";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

    return cc_invokeFeature(call_handle, CC_FEATURE_ANSWER, video_pref, NULL);
}

/**
 * Send a keypress to a call, it could be a single digit.
 * @param call handle
 * @param cc_digit digit pressed
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_sendDigit(cc_call_handle_t call_handle, cc_digit_t cc_digit) {
	static const char fname[] = "CC_CallFeature_SendDigit";
    char digit;
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));
	//Demote to eliminate the endian issue
	digit = cc_digit;
    return cc_invokeFeature(call_handle, CC_FEATURE_KEYPRESS, CC_SDP_MAX_QOS_DIRECTIONS, (string_t)&digit);
}

/**
 * Send a backspace action.
 * @param call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_backSpace(cc_call_handle_t call_handle) {
	static const char fname[] = "CC_CallFeature_BackSpace";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

    return cc_invokeFeature(call_handle, CC_FEATURE_BKSPACE, CC_SDP_MAX_QOS_DIRECTIONS, NULL);
}

/**
 * Send a dial digit string on an active call, e.g."9191234567".
 * @param call handle
 * @param numbers dialed string
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_dial(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref, const string_t numbers) {
	static const char fname[] = "CC_CallFeature_Dial";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

    if (cpr_strcasecmp(numbers, "DIAL") == 0) {
	    return cc_invokeFeature(call_handle, CC_FEATURE_DIAL, video_pref, numbers);
    }

	return cc_invokeFeature(call_handle, CC_FEATURE_DIALSTR, video_pref, numbers);
}

/**
 * Initiate a speed dial.
 * @param call handle
 * @param callid call id
 * @param speed dial numbers.
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_speedDial(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref, const string_t speed_dial_number) {
	static const char fname[] = "CC_CallFeature_SpeedDial";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_SPEEDDIAL, video_pref, speed_dial_number);
}

/**
 * Initiate a BLF call pickup.
 * @param call handle
 * @param speed dial number configured.
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_blfCallPickup(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref, const string_t speed_dial_number) {
	static const char fname[] = "CC_CallFeature_BLFCallPickup";
	cc_return_t ret = CC_SUCCESS;
    string_t blf_sd = strlib_malloc(CISCO_BLFPICKUP_STRING, sizeof(CISCO_BLFPICKUP_STRING));
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

    blf_sd = strlib_append(blf_sd, "-");
    blf_sd = strlib_append(blf_sd, speed_dial_number);

	ret = cc_invokeFeature(call_handle, CC_FEATURE_SPEEDDIAL, video_pref, blf_sd);
	//free memory
	strlib_free(blf_sd);
	return ret;
}

/**
 * Redial the last dial numbers.
 * @param call handle
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 * @Notice: if there is no active dial made, this method should not be called.
 */
cc_return_t CC_CallFeature_redial(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref) {
	static const char fname[] = "CC_CallFeature_Redial";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_REDIAL, video_pref, NULL);
}

/**
 * Update a media capability for a call.
 * @param call_handle
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_updateCallMediaCapability(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref) {
	static const char fname[] = "CC_CallFeature_updateCallMediaCapability";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_UPD_SESSION_MEDIA_CAP, video_pref, NULL);
}

/**
 * Make a call forward all on particular line
 * @param call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_callForwardAll(cc_call_handle_t call_handle) {
	static const char fname[] = "CC_CallFeature_CallForwardAll";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_CFWD_ALL, CC_SDP_MAX_QOS_DIRECTIONS, NULL);
}

/**
 * Resume a held call.
 * @param call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_resume(cc_call_handle_t call_handle, cc_sdp_direction_t video_pref) {
	static const char fname[] = "CC_CallFeature_Resume";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_RESUME, video_pref, NULL);
}

/**
 * End a consultation call.
 * @param call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_endConsultativeCall(cc_call_handle_t call_handle) {
	static const char fname[] = "CC_CallFeature_EndConsultativeCall";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_END_CALL, CC_SDP_MAX_QOS_DIRECTIONS, "ACTIVECALLS");
}

/**
 * Initiate a conference. Steps to make a conference or transfer:
 * 1. Create a call handle, e.g. chandle1.
 * 2. Start the call on this call handle.
 * 3. When the call is answered, invoke:
 *        CC_CallFeature_Conference(chandle1, FALSE, CC_EMPTY_CALL_HANDLE) to start a conference operation.
 * 4. Upon receiving the consultative call (cHandle2) created from pSipcc system,
 *    invoke:
 *        CC_CallFeature_Dial(cHandle2)
 * 	  to dial the consultative call.
 * 5. When the consultative call is in ringout or connected state, invoke:
 *        CC_CallFeature_Conference(cHandle2, FALSE, CC_EMPTY_CALL_HANDLE) to
 *    finish the conference.
 * Note: 1. in the step 4, a user could kill the consultative call and pickup a hold call (not the parent call that
 *    initiated the conference). In this scenario, a parent call handle should be supplied.
 *    For instance,
 *        CC_CallFeature_Conference(cHandle2, FALSE, cHandle1)
 *       2. If it's a B2bConf, substitute the "FALSE" with "TRUE"
 *
 * @param call_handle the call handle for
 * 	1. the original connected call.
 * 	2. the consultative call or a held call besides the parent call initiated the transfer. This is used
 * 	   on the second time to finish the transfer.
 * @param is locall conference or not. If it's a local conference, it's a b2bconf.
 * @param parent_call_handle if supplied, it will be the targeted parent call handle, which initiated the conference.
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_conference(cc_call_handle_t call_handle,
		boolean is_local,
		cc_call_handle_t parent_call_handle, cc_sdp_direction_t video_pref) {
	static const char fname[] = "CC_CallFeature_Conference";
	char call_handle_str[10];
	cc_return_t ret = CC_SUCCESS;
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));
	if (parent_call_handle == CC_EMPTY_CALL_HANDLE) {
		if (is_local == FALSE) {
		    return cc_invokeFeature(call_handle, CC_FEATURE_B2BCONF, video_pref, "");
		} else {
		    return cc_invokeFeature(call_handle, CC_FEATURE_CONF, video_pref, "");
		}
	} else {
		cc_call_handle_t parent = (SESSIONTYPE_CALLCONTROL << CC_SID_TYPE_SHIFT) + parent_call_handle;
        string_t parent_call_handle_str;
		snprintf(call_handle_str, sizeof(call_handle_str), "%d", parent);
		parent_call_handle_str = strlib_malloc(call_handle_str, strlen(call_handle_str));

		if (is_local == FALSE) {
		    ret = cc_invokeFeature(call_handle, CC_FEATURE_B2BCONF, video_pref, parent_call_handle_str);
		} else {
		    ret = cc_invokeFeature(call_handle, CC_FEATURE_CONF, video_pref, parent_call_handle_str);
		}
		strlib_free(parent_call_handle_str);
		return ret;
	}
}

/**
 * Initiate a call transfer. Please refer to Conference feature.
 * @param call_handle the call handle for
 * 	1. the original connected call.
 * 	2. the consultative call or a held call besides the parent call initiated the transfer. This is used
 * 	   on the second time to finish the transfer.
 * @param parent_call_handle if supplied, it will be the parent call handle, which initiated the transfer.
 * @param video_pref the sdp direction
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_transfer(cc_call_handle_t call_handle, cc_call_handle_t parent_call_handle, cc_sdp_direction_t video_pref) {
	static const char fname[] = "CC_CallFeature_transfer";
	char call_handle_str[10];
	cc_return_t ret = CC_SUCCESS;
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));
	if (parent_call_handle == CC_EMPTY_CALL_HANDLE) {
		return cc_invokeFeature(call_handle, CC_FEATURE_XFER, video_pref, "");
	} else {
		cc_call_handle_t parent = (SESSIONTYPE_CALLCONTROL << CC_SID_TYPE_SHIFT) + parent_call_handle;
        string_t parent_call_handle_str;
		snprintf(call_handle_str, sizeof(call_handle_str), "%d", parent);
		parent_call_handle_str = strlib_malloc(call_handle_str, strlen(call_handle_str));

		ret = cc_invokeFeature(call_handle, CC_FEATURE_XFER, video_pref, parent_call_handle_str);
		strlib_free(parent_call_handle_str);
		return ret;
	}
}

/**
 * Put a connected call on hold.
 * @param call handle
 * @param reason the reason to hold. The following values should be used.
 * 	  CC_HOLD_REASON_NONE,
 *    CC_HOLD_REASON_XFER, //Hold for transfer
 *    CC_HOLD_REASON_CONF, //Hold for conference
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_holdCall(cc_call_handle_t call_handle, cc_hold_reason_t reason) {
	static const char fname[] = "CC_CallFeature_HoldCall";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));
	switch (reason) {
	case CC_HOLD_REASON_XFER:
		return cc_invokeFeature(call_handle, CC_FEATURE_HOLD, CC_SDP_MAX_QOS_DIRECTIONS, "TRANSFER");
	case CC_HOLD_REASON_CONF:
		return cc_invokeFeature(call_handle, CC_FEATURE_HOLD, CC_SDP_MAX_QOS_DIRECTIONS, "CONFERENCE");
	case CC_HOLD_REASON_SWAP:
		return cc_invokeFeature(call_handle, CC_FEATURE_HOLD, CC_SDP_MAX_QOS_DIRECTIONS, "SWAP");
	default:
		break;
	}

	return cc_invokeFeature(call_handle, CC_FEATURE_HOLD, CC_SDP_MAX_QOS_DIRECTIONS, "");
}

/********************************End of basic call feature methods******************************************/

/*************************************Additional call feature methods***************************************
 *
 */

/**
 * Join a call.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_b2bJoin(cc_call_handle_t call_handle) {
	static const char fname[] = "CC_CallFeature_b2bJoin";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_B2B_JOIN, CC_SDP_MAX_QOS_DIRECTIONS, NULL);
}

/**
 * Initiate a direct transfer
 * @param call_handle the call handle for the call to initialize a transfer
 * @param target_call_handle the call handle for the target transfer call.
 * @retrun SUCCESS or FAILURE. If the target call handle is empty, a FAILURE will be returned.
 */
cc_return_t CC_CallFeature_directTransfer(cc_call_handle_t call_handle,
        cc_call_handle_t target_call_handle) {
    static const char fname[] = "CC_CallFeature_directTransfer";
    CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
                GET_LINE_ID(call_handle), fname));
    if (target_call_handle == CC_EMPTY_CALL_HANDLE) {
        CCAPP_DEBUG(DEB_L_C_F_PREFIX"target call handle is empty.\n", DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
                    GET_LINE_ID(call_handle), fname));
        return CC_FAILURE;
    }
    return CC_CallFeature_transfer(call_handle, target_call_handle, CC_SDP_MAX_QOS_DIRECTIONS);
}

/**
 * Initiate a join across line
 * @param call_handle the call handle for the call that initializes a join across line (conference).
 * @param target_call_handle the call handle for the call will be joined.
 */
cc_return_t CC_CallFeature_joinAcrossLine(cc_call_handle_t call_handle, cc_call_handle_t target_call_handle) {
    static const char fname[] = "CC_CallFeature_joinAcrossLine";
    CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
                GET_LINE_ID(call_handle), fname));
    if (target_call_handle == CC_EMPTY_CALL_HANDLE) {
        CCAPP_DEBUG(DEB_L_C_F_PREFIX"target call handle is empty.\n", DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
                    GET_LINE_ID(call_handle), fname));
        return CC_FAILURE;
    }
    return CC_CallFeature_conference(call_handle, TRUE, target_call_handle, CC_SDP_MAX_QOS_DIRECTIONS);
}

/**
 * Select or locked a call.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_select(cc_call_handle_t call_handle) {
	static const char fname[] = "CC_CallFeature_select";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_SELECT, CC_SDP_MAX_QOS_DIRECTIONS, NULL);
}

/**
 * Cancel a call feature, e.g. when the consultative call is connected and the
 * user wishes not to make the conference, thie method can be invoked.
 * @param call_handle call handle
 * @return SUCCESS or FAILURE
 */
cc_return_t CC_CallFeature_cancelXfrerCnf(cc_call_handle_t call_handle) {
	static const char fname[] = "CC_CallFeature_cancelXfrerCnf";
	CCAPP_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, GET_CALL_ID(call_handle),
			GET_LINE_ID(call_handle), fname));

	return cc_invokeFeature(call_handle, CC_FEATURE_CANCEL, CC_SDP_MAX_QOS_DIRECTIONS, NULL);
}

void CC_CallFeature_mute(boolean mute) {
}

void CC_CallFeature_speaker(boolean mute) {
}

cc_call_handle_t CC_CallFeature_getConnectedCall() {
    return ccappGetConnectedCall();
}
