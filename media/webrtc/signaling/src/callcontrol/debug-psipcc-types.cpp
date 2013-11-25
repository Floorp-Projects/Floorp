/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CC_Common.h"
#include <stdlib.h>

#include "debug-psipcc-types.h"

struct ename {
    int v;
    cc_string_t name;
};

#define ENAME_DECL(v) \
    { v, #v }

/*
 * Get a symbolic representation of a numeric identifier.
 */
static cc_string_t
egetname(const struct ename *index, int event)
{
    const struct ename *ename;

    for (ename = index; !(ename->v == 0 && ename->name == nullptr); ename++) {
        if (event == ename->v)
            return ename->name;
    }

    return "UNKNOWN";
}

#define CC_EVENT _e
#define CC_TYPE _t

#define DEFINE_NO_PREPEND_TYPE_NAME_FUNCTION(typeName, eventOrType, ...)\
  DEFINE_TYPE_NAME_FUNCTION_HELPER(, typeName, eventOrType, __VA_ARGS__)

#define DEFINE_CCAPI_TYPE_NAME_FUNCTION(typeName, eventOrType, ...)\
  DEFINE_TYPE_NAME_FUNCTION_HELPER(ccapi_, typeName, eventOrType, __VA_ARGS__)

#define DEFINE_CC_TYPE_NAME_FUNCTION(typeName, eventOrType, ...)\
  DEFINE_TYPE_NAME_FUNCTION_HELPER(cc_, typeName, eventOrType, __VA_ARGS__)

#define DEFINE_TYPE_NAME_FUNCTION_HELPER(tokenPrepend, typeName, eventOrType, ...)\
  static struct ename tokenPrepend##typeName##_names[] = {\
    __VA_ARGS__, \
    { 0, nullptr } \
  };\
  \
  ECC_API cc_string_t typeName##_getname(tokenPrepend##typeName##eventOrType ev) \
  {\
      return egetname(&tokenPrepend##typeName##_names[0], (int) ev);\
  }

//stringizing enums from ccapi_types.h

//define device_event_getname(ccapi_device_event_e);
DEFINE_CCAPI_TYPE_NAME_FUNCTION(device_event, CC_EVENT,
                                ENAME_DECL(CCAPI_DEVICE_EV_CONFIG_CHANGED),
                                ENAME_DECL(CCAPI_DEVICE_EV_STATE),
                                ENAME_DECL(CCAPI_DEVICE_EV_IDLE_SET),
                                ENAME_DECL(CCAPI_DEVICE_EV_MWI_LAMP),
                                ENAME_DECL(CCAPI_DEVICE_EV_NOTIFYPROMPT),
                                ENAME_DECL(CCAPI_DEVICE_EV_SERVER_STATUS),
                                ENAME_DECL(CCAPI_DEVICE_EV_BLF),
                                ENAME_DECL(CCAPI_DEVICE_EV_CAMERA_ADMIN_CONFIG_CHANGED),
                                ENAME_DECL(CCAPI_DEVICE_EV_VIDEO_CAP_ADMIN_CONFIG_CHANGED)
                                );

//define call_event_getname(ccapi_call_event_e);
DEFINE_CCAPI_TYPE_NAME_FUNCTION(call_event, CC_EVENT,
                                ENAME_DECL(CCAPI_CALL_EV_CREATED),
                                ENAME_DECL(CCAPI_CALL_EV_STATE),
                                ENAME_DECL(CCAPI_CALL_EV_CALLINFO),
                                ENAME_DECL(CCAPI_CALL_EV_ATTR),
                                ENAME_DECL(CCAPI_CALL_EV_SECURITY),
                                ENAME_DECL(CCAPI_CALL_EV_LOG_DISP),
                                ENAME_DECL(CCAPI_CALL_EV_PLACED_CALLINFO),
                                ENAME_DECL(CCAPI_CALL_EV_STATUS),
                                ENAME_DECL(CCAPI_CALL_EV_SELECT),
                                ENAME_DECL(CCAPI_CALL_EV_LAST_DIGIT_DELETED),
                                ENAME_DECL(CCAPI_CALL_EV_GCID),
                                ENAME_DECL(CCAPI_CALL_EV_XFR_OR_CNF_CANCELLED),
                                ENAME_DECL(CCAPI_CALL_EV_PRESERVATION),
                                ENAME_DECL(CCAPI_CALL_EV_CAPABILITY),
                                ENAME_DECL(CCAPI_CALL_EV_VIDEO_AVAIL),
                                ENAME_DECL(CCAPI_CALL_EV_VIDEO_OFFERED),
                                ENAME_DECL(CCAPI_CALL_EV_RECEIVED_INFO),
                                ENAME_DECL(CCAPI_CALL_EV_RINGER_STATE),
                                ENAME_DECL(CCAPI_CALL_EV_CONF_PARTICIPANT_INFO),
                                ENAME_DECL(CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_BEGIN),
                                ENAME_DECL(CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_SUCCESSFUL),
                                ENAME_DECL(CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_FAIL)
                                );

//define line_event_getname(ccapi_line_event_e);
DEFINE_CCAPI_TYPE_NAME_FUNCTION(line_event, CC_EVENT,
                                ENAME_DECL(CCAPI_LINE_EV_CONFIG_CHANGED),
                                ENAME_DECL(CCAPI_LINE_EV_REG_STATE),
                                ENAME_DECL(CCAPI_LINE_EV_CAPSET_CHANGED),
                                ENAME_DECL(CCAPI_LINE_EV_CFWDALL),
                                ENAME_DECL(CCAPI_LINE_EV_MWI)
                                );

//
//
//stringizing enums from cc_constants.h
//
//


//define digit_getname(cc_digit_t);
DEFINE_CC_TYPE_NAME_FUNCTION(digit, CC_TYPE,
                             ENAME_DECL(KEY_1),
                             ENAME_DECL(KEY_2),
                             ENAME_DECL(KEY_3),
                             ENAME_DECL(KEY_4),
                             ENAME_DECL(KEY_5),
                             ENAME_DECL(KEY_6),
                             ENAME_DECL(KEY_7),
                             ENAME_DECL(KEY_8),
                             ENAME_DECL(KEY_9),
                             ENAME_DECL(KEY_0),
                             ENAME_DECL(KEY_STAR),
                             ENAME_DECL(KEY_POUND),
                             ENAME_DECL(KEY_A),
                             ENAME_DECL(KEY_B),
                             ENAME_DECL(KEY_C),
                             ENAME_DECL(KEY_D)
                             );

//define cucm_mode_getname(cc_cucm_mode_t);
DEFINE_CC_TYPE_NAME_FUNCTION(cucm_mode, CC_TYPE,
                             ENAME_DECL(CC_MODE_INVALID),
                             ENAME_DECL(CC_MODE_CCM),
                             ENAME_DECL(CC_MODE_NONCCM)
                             );

//define line_feature_getname(cc_line_feature_t);
DEFINE_CC_TYPE_NAME_FUNCTION(line_feature, CC_TYPE,
                             ENAME_DECL(CC_LINE_FEATURE_NONE),
	                         ENAME_DECL(CC_LINE_FEATURE_REDIAL),
	                         ENAME_DECL(CC_LINE_FEATURE_SPEEDDIAL),
	                         ENAME_DECL(CC_LINE_FEATURE_DN),
	                         ENAME_DECL(CC_LINE_FEATURE_SERVICE),
	                         ENAME_DECL(CC_LINE_FEATURE_SPEEDDIALBLF),
	                         ENAME_DECL(CC_LINE_FEATURE_MALICIOUSCALLID),
	                         ENAME_DECL(CC_LINE_FEATURE_QUALREPORTTOOL),
	                         ENAME_DECL(CC_LINE_FEATURE_ALLCALLS),
	                         ENAME_DECL(CC_LINE_FEATURE_ANSWEROLDEST),
	                         ENAME_DECL(CC_LINE_FEATURE_SERVICES),
	                         ENAME_DECL(CC_LINE_FEATURE_BLF)
                             );

//define feature_option_mask_getname(cc_feature_option_mask_t);
DEFINE_CC_TYPE_NAME_FUNCTION(feature_option_mask, CC_TYPE,
                             ENAME_DECL(CC_FEATUREOPTIONMASK_NONE),
  	                         ENAME_DECL(CC_FEATUREOPTIONMASK_BLF_PICKUP)
                             );

//define service_cause_getname(cc_service_cause_t);
DEFINE_CC_TYPE_NAME_FUNCTION(service_cause, CC_TYPE,
                             ENAME_DECL(CC_CAUSE_NONE),
 	                         ENAME_DECL(CC_CAUSE_FAILOVER),
	                         ENAME_DECL(CC_CAUSE_FALLBACK),
	                         ENAME_DECL(CC_CAUSE_REG_ALL_FAILED),
	                         ENAME_DECL(CC_CAUSE_SHUTDOWN),
	                         ENAME_DECL(CC_CAUSE_UNREG_ALL),
	                         ENAME_DECL(CC_CAUSE_LOGOUT_RESET)
                             );

//define service_state_getname(cc_service_state_t);
DEFINE_CC_TYPE_NAME_FUNCTION(service_state, CC_TYPE,
                             ENAME_DECL(CC_STATE_IDLE),
                             ENAME_DECL(CC_STATE_INS),
                             ENAME_DECL(CC_STATE_OOS),
                             ENAME_DECL(CC_STATE_PRO_BASE)
                             );

//define ccm_status_getname(cc_ccm_status_t);
DEFINE_CC_TYPE_NAME_FUNCTION(ccm_status, CC_TYPE,
                             ENAME_DECL(CC_CCM_STATUS_NONE),
                             ENAME_DECL(CC_CCM_STATUS_STANDBY),
                             ENAME_DECL(CC_CCM_STATUS_ACTIVE)
                             );

//define reg_state_getname(cc_line_reg_state_t);
DEFINE_CC_TYPE_NAME_FUNCTION(line_reg_state, CC_TYPE,
	                         ENAME_DECL(CC_UNREGISTERED),
	                         ENAME_DECL(CC_REGISTERED)
                             );

//define shutdown_reason_getname(cc_shutdown_reason_t);
DEFINE_CC_TYPE_NAME_FUNCTION(shutdown_reason, CC_TYPE,
                             ENAME_DECL(CC_SHUTDOWN_NORMAL),
                             ENAME_DECL(CC_SHUTDOWN_UNSPECIFIED),
                             ENAME_DECL(CC_SHUTDOWN_VERMISMATCH)
                             );

//define kpml_config_getname(cc_kpml_config_t);
DEFINE_CC_TYPE_NAME_FUNCTION(kpml_config, CC_TYPE,
                             ENAME_DECL(CC_KPML_NONE),
                             ENAME_DECL(CC_KPML_SIGNAL_ONLY),
                             ENAME_DECL(CC_KPML_DTMF_ONLY),
                             ENAME_DECL(CC_KPML_BOTH)
                             );

//define upgrade_getname(cc_upgrade_t);
DEFINE_CC_TYPE_NAME_FUNCTION(upgrade, CC_TYPE,
                             ENAME_DECL(CC_UPGRADE_NONE),
	                         ENAME_DECL(CC_UPGRADE_NOW),
	                         ENAME_DECL(CC_UPGRADE_LATER)
                             );

//define sdp_direction_getname(cc_sdp_direction_t);
DEFINE_CC_TYPE_NAME_FUNCTION(sdp_direction, CC_TYPE,
                             ENAME_DECL(CC_SDP_DIRECTION_INACTIVE),
                             ENAME_DECL(CC_SDP_DIRECTION_SENDONLY),
                             ENAME_DECL(CC_SDP_DIRECTION_RECVONLY),
                             ENAME_DECL(CC_SDP_DIRECTION_SENDRECV),
                             ENAME_DECL(CC_SDP_MAX_QOS_DIRECTIONS)
                             );

//define blf_state_getname(cc_blf_state_t);
DEFINE_CC_TYPE_NAME_FUNCTION(blf_state, CC_TYPE,
	                         ENAME_DECL(CC_SIP_BLF_UNKNOWN),
	                         ENAME_DECL(CC_SIP_BLF_IDLE),
	                         ENAME_DECL(CC_SIP_BLF_INUSE),
                             ENAME_DECL(CC_SIP_BLF_EXPIRED),
	                         ENAME_DECL(CC_SIP_BLF_REJECTED),
	                         ENAME_DECL(CC_SIP_BLF_ALERTING)
                             );

//define blf_feature_mask_getname(cc_blf_feature_mask_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (blf_feature_mask, CC_TYPE,
                                ENAME_DECL(CC_BLF_FEATURE_MASK_NONE),
                                ENAME_DECL(CC_BLF_FEATURE_MASK_PICKUP)
                                );

//define call_state_getname(cc_call_state_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (call_state, CC_TYPE,
	                            ENAME_DECL(OFFHOOK),
	                            ENAME_DECL(ONHOOK),
	                            ENAME_DECL(RINGOUT),
	                            ENAME_DECL(RINGIN),
	                            ENAME_DECL(PROCEED),
	                            ENAME_DECL(CONNECTED),
	                            ENAME_DECL(HOLD),
	                            ENAME_DECL(REMHOLD),
	                            ENAME_DECL(RESUME),
	                            ENAME_DECL(BUSY),
	                            ENAME_DECL(REORDER),
	                            ENAME_DECL(CONFERENCE),
	                            ENAME_DECL(DIALING),
	                            ENAME_DECL(REMINUSE),
	                            ENAME_DECL(HOLDREVERT),
	                            ENAME_DECL(WHISPER),
                                ENAME_DECL(PRESERVATION),
	                            ENAME_DECL(WAITINGFORDIGITS),
                                ENAME_DECL(MAX_CALL_STATES)
                                );

//define call_attr_getname(cc_call_attr_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (call_attr, CC_TYPE,
	                            ENAME_DECL(CC_ATTR_NOT_DEFINED),
	                            ENAME_DECL(CC_ATTR_NORMAL),
	                            ENAME_DECL(CC_ATTR_XFR_CONSULT),
	                            ENAME_DECL(CC_ATTR_CONF_CONSULT),
	                            ENAME_DECL(CC_ATTR_BARGING),
	                            ENAME_DECL(CC_ATTR_RIUHELD_LOCKED),
	                            ENAME_DECL(CC_ATTR_LOCAL_CONF_CONSULT),
	                            ENAME_DECL(CC_ATTR_LOCAL_XFER_CONSULT),
	                            ENAME_DECL(CC_ATTR_CFWDALL),
	                            ENAME_DECL(CC_ATTR_CFWD_ALL),
	                            ENAME_DECL(CC_ATTR_MAX)
                                );

//define hold_reason_getname(cc_hold_reason_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (hold_reason, CC_TYPE,
                                ENAME_DECL(CC_HOLD_REASON_NONE),
                                ENAME_DECL(CC_HOLD_REASON_XFER),
                                ENAME_DECL(CC_HOLD_REASON_CONF),
                                ENAME_DECL(CC_HOLD_REASON_SWAP),
                                ENAME_DECL(CC_HOLD_REASON_INTERNAL)
                                );

//define call_type_getname(cc_call_type_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (call_type, CC_TYPE,
                                ENAME_DECL(CC_CALL_TYPE_NONE),
                                ENAME_DECL(CC_CALL_TYPE_INCOMING),
                                ENAME_DECL(CC_CALL_TYPE_OUTGOING),
                                ENAME_DECL(CC_CALL_TYPE_FORWARDED)
                                );

//define call_security_getname(cc_call_security_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (call_security, CC_TYPE,
	                            ENAME_DECL(CC_SECURITY_NONE),
	                            ENAME_DECL(CC_SECURITY_UNKNOWN),
	                            ENAME_DECL(CC_SECURITY_AUTHENTICATED),
	                            ENAME_DECL(CC_SECURITY_NOT_AUTHENTICATED),
	                            ENAME_DECL(CC_SECURITY_ENCRYPTED)
                                );

//define call_policy_getname(cc_call_policy_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (call_policy, CC_TYPE,
	                            ENAME_DECL(CC_POLICY_NONE),
	                            ENAME_DECL(CC_POLICY_UNKNOWN),
	                            ENAME_DECL(CC_POLICY_CHAPERONE)
                                );

//define log_disposition_getname(cc_log_disposition_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (log_disposition, CC_TYPE,
	                            ENAME_DECL(CC_LOGD_MISSED),
	                            ENAME_DECL(CC_LOGD_RCVD),
	                            ENAME_DECL(CC_LOGD_SENT),
	                            ENAME_DECL(CC_LOGD_UNKNWN),
	                            ENAME_DECL(CC_LOGD_DELETE)
                                );

//define call_priority_getname(cc_call_priority_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (call_priority, CC_TYPE,
	                            ENAME_DECL(CC_PRIORITY_NORMAL),
	                            ENAME_DECL(CC_PRIORITY_URGENT)
                                );

//define call_selection_getname(cc_call_selection_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (call_selection, CC_TYPE,
	                            ENAME_DECL(CC_CALL_SELECT_NONE),
	                            ENAME_DECL(CC_CALL_SELECT_LOCKED),
	                            ENAME_DECL(CC_CALL_SELECT_UNLOCKED),
	                            ENAME_DECL(CC_CALL_SELECT_REMOTE_LOCKED)
                                );

//cc_string_t call_capability_getname(ccapi_call_capability_e);
DEFINE_CCAPI_TYPE_NAME_FUNCTION(call_capability, CC_EVENT,
	                            ENAME_DECL(CCAPI_CALL_CAP_NEWCALL),
                                ENAME_DECL(CCAPI_CALL_CAP_ANSWER),
                                ENAME_DECL(CCAPI_CALL_CAP_ENDCALL),
                                ENAME_DECL(CCAPI_CALL_CAP_HOLD),
                                ENAME_DECL(CCAPI_CALL_CAP_RESUME),
                                ENAME_DECL(CCAPI_CALL_CAP_CALLFWD),
                                ENAME_DECL(CCAPI_CALL_CAP_DIAL),
                                ENAME_DECL(CCAPI_CALL_CAP_BACKSPACE),
                                ENAME_DECL(CCAPI_CALL_CAP_SENDDIGIT),
                                ENAME_DECL(CCAPI_CALL_CAP_TRANSFER),
                                ENAME_DECL(CCAPI_CALL_CAP_CONFERENCE),
                                ENAME_DECL(CCAPI_CALL_CAP_SWAP),
                                ENAME_DECL(CCAPI_CALL_CAP_REDIAL),
                                ENAME_DECL(CCAPI_CALL_CAP_JOIN),
                                ENAME_DECL(CCAPI_CALL_CAP_SELECT),
                                ENAME_DECL(CCAPI_CALL_CAP_RMVLASTPARTICIPANT),
                                ENAME_DECL(CCAPI_CALL_CAP_MAX)
                                );

//define srv_ctrl_req_getname(cc_srv_ctrl_req_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (srv_ctrl_req, CC_TYPE,
	                            ENAME_DECL(CC_DEVICE_RESET),
	                            ENAME_DECL(CC_DEVICE_RESTART),
	                            ENAME_DECL(CC_DEVICE_ICMP_UNREACHABLE)
                                );

//define srv_ctrl_cmd_getname(cc_srv_ctrl_cmd_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (srv_ctrl_cmd, CC_TYPE,
	                            ENAME_DECL(CC_ACTION_RESET),
	                            ENAME_DECL(CC_ACTION_RESTART)
                                );

//define message_type_getname(cc_message_type_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (message_type, CC_TYPE,
	                            ENAME_DECL(CC_VOICE_MESSAGE),
	                            ENAME_DECL(CC_TEXT_MESSAGE)
                                );

//define lamp_state_getname(cc_lamp_state_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (lamp_state, CC_TYPE,
	                            ENAME_DECL(CC_LAMP_NONE),
	                            ENAME_DECL(CC_LAMP_ON),
	                            ENAME_DECL(CC_LAMP_BLINK),
	                            ENAME_DECL(CC_LAMP_FRESH)
                                );

//define cause_getname(cc_cause_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (cause, CC_TYPE,
                                ENAME_DECL(CC_CAUSE_MIN),
                                ENAME_DECL(CC_CAUSE_BASE),
                                ENAME_DECL(CC_CAUSE_OK),
                                ENAME_DECL(CC_CAUSE_ERROR),
                                ENAME_DECL(CC_CAUSE_UNASSIGNED_NUM),
                                ENAME_DECL(CC_CAUSE_NO_RESOURCE),
                                ENAME_DECL(CC_CAUSE_NO_ROUTE),
                                ENAME_DECL(CC_CAUSE_NORMAL),
                                ENAME_DECL(CC_CAUSE_BUSY),
                                ENAME_DECL(CC_CAUSE_NO_USER_RESP),
                                ENAME_DECL(CC_CAUSE_NO_USER_ANS),
                                ENAME_DECL(CC_CAUSE_REJECT),
                                ENAME_DECL(CC_CAUSE_INVALID_NUMBER),
                                ENAME_DECL(CC_CAUSE_FACILITY_REJECTED),
                                ENAME_DECL(CC_CAUSE_CALL_ID_IN_USE),
                                ENAME_DECL(CC_CAUSE_XFER_LOCAL),
                                ENAME_DECL(CC_CAUSE_XFER_REMOTE),
                                ENAME_DECL(CC_CAUSE_XFER_BY_REMOTE),
                                ENAME_DECL(CC_CAUSE_XFER_CNF),
                                ENAME_DECL(CC_CAUSE_CONGESTION),
                                ENAME_DECL(CC_CAUSE_ANONYMOUS),
                                ENAME_DECL(CC_CAUSE_REDIRECT),
                                ENAME_DECL(CC_CAUSE_PAYLOAD_MISMATCH),
                                ENAME_DECL(CC_CAUSE_CONF),
                                ENAME_DECL(CC_CAUSE_REPLACE),
                                ENAME_DECL(CC_CAUSE_NO_REPLACE_CALL),
                                ENAME_DECL(CC_CAUSE_NO_RESUME),
                                ENAME_DECL(CC_CAUSE_NO_MEDIA),
                                ENAME_DECL(CC_CAUSE_REQUEST_PENDING),
                                ENAME_DECL(CC_CAUSE_INVALID_PARTICIPANT),
                                ENAME_DECL(CC_CAUSE_NO_CNF_BRIDE),
                                ENAME_DECL(CC_MAXIMUM_PARTICIPANT),
                                ENAME_DECL(CC_KEY_NOT_ACTIVE),
                                ENAME_DECL(CC_TEMP_NOT_AVAILABLE),
                                ENAME_DECL(CC_CAUSE_REMOTE_SERVER_ERROR),
                                ENAME_DECL(CC_CAUSE_NOT_FOUND),
                                ENAME_DECL(CC_CAUSE_SECURITY_FAILURE),
                                ENAME_DECL(CC_CAUSE_MONITOR),
                                ENAME_DECL(CC_CAUSE_UI_STATE_BUSY),
                                ENAME_DECL(CC_SIP_CAUSE_ANSWERED_ELSEWHERE),
                                ENAME_DECL(CC_CAUSE_RETRIEVED),
                                ENAME_DECL(CC_CAUSE_FORWARDED),
                                ENAME_DECL(CC_CAUSE_ABANDONED),
                                ENAME_DECL(CC_CAUSE_XFER_LOCAL_WITH_DIALSTRING),
                                ENAME_DECL(CC_CAUSE_BW_OK),
                                ENAME_DECL(CC_CAUSE_XFER_COMPLETE),
                                ENAME_DECL(CC_CAUSE_RESP_TIMEOUT),
                                ENAME_DECL(CC_CAUSE_SERV_ERR_UNAVAIL),
                                ENAME_DECL(CC_CAUSE_REMOTE_DISCONN_REQ_PLAYTONE),
                                ENAME_DECL(CC_CAUSE_OUT_OF_MEM),
                                ENAME_DECL(CC_CAUSE_VALUE_NOT_FOUND),
                                ENAME_DECL(CC_CAUSE_BAD_ICE_ATTRIBUTE),
                                ENAME_DECL(CC_CAUSE_DTLS_ATTRIBUTE_ERROR),
                                ENAME_DECL(CC_CAUSE_DTLS_DIGEST_ALGORITHM_EMPTY),
                                ENAME_DECL(CC_CAUSE_DTLS_DIGEST_ALGORITHM_TOO_LONG),
                                ENAME_DECL(CC_CAUSE_DTLS_DIGEST_EMPTY),
                                ENAME_DECL(CC_CAUSE_DTLS_DIGEST_TOO_LONG),
                                ENAME_DECL(CC_CAUSE_DTLS_FINGERPRINT_PARSE_ERROR),
                                ENAME_DECL(CC_CAUSE_DTLS_FINGERPRINT_TOO_LONG),
                                ENAME_DECL(CC_CAUSE_INVALID_SDP_POINTER),
                                ENAME_DECL(CC_CAUSE_NO_AUDIO),
                                ENAME_DECL(CC_CAUSE_NO_DTLS_FINGERPRINT),
                                ENAME_DECL(CC_CAUSE_MISSING_ICE_ATTRIBUTES),
                                ENAME_DECL(CC_CAUSE_NO_MEDIA_CAPABILITY),
                                ENAME_DECL(CC_CAUSE_NO_M_LINE),
                                ENAME_DECL(CC_CAUSE_NO_PEERCONNECTION),
                                ENAME_DECL(CC_CAUSE_NO_SDP),
                                ENAME_DECL(CC_CAUSE_NULL_POINTER),
                                ENAME_DECL(CC_CAUSE_SDP_CREATE_FAILED),
                                ENAME_DECL(CC_CAUSE_SDP_ENCODE_FAILED),
                                ENAME_DECL(CC_CAUSE_SDP_PARSE_FAILED),
                                ENAME_DECL(CC_CAUSE_SETTING_ICE_SESSION_PARAMETERS_FAILED),
                                ENAME_DECL(CC_CAUSE_MAX)
                                );

//define subscriptions_ext_getname(cc_subscriptions_ext_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (subscriptions_ext, CC_TYPE,
                                ENAME_DECL(CC_SUBSCRIPTIONS_KPML_EXT),
                                ENAME_DECL(CC_SUBSCRIPTIONS_PRESENCE_EXT),
                                ENAME_DECL(CC_SUBSCRIPTIONS_REMOTECC_EXT),
                                ENAME_DECL(CC_SUBSCRIPTIONS_REMOTECC_OPTIONSIND_EXT),
                                ENAME_DECL(CC_SUBSCRIPTIONS_CONFIGAPP_EXT),
                                ENAME_DECL(CC_SUBSCRIPTIONS_MEDIA_INFO_EXT)
                                );

//define apply_config_result_getname(cc_apply_config_result_t);
DEFINE_CC_TYPE_NAME_FUNCTION   (apply_config_result, CC_TYPE,
                                ENAME_DECL(APPLY_CONFIG_NONE),
                                ENAME_DECL(APPLY_DYNAMICALLY),
                                ENAME_DECL(RESTART_NEEDED)
                                );



