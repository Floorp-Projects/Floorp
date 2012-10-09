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

#ifndef _CC_CONSTANTS_H_
#define _CC_CONSTANTS_H_
#include "cc_types.h"

/**
 * Max call servers 
 */
#define MAX_CALL_SERVERS 4

/**
 * Define notification priority
 */
#define CC_DEF_NOTIFY_PRI    20
#define CC_HR_NOTIFY_PRI     1

#define CC_ALL_LINES          255
#define SID_LINE_SHIFT        16
#define CC_NO_CALL_ID         (0)
#define CC_NO_LINE            (0)
#define CC_NO_CALL_INSTANCE   (0)

#define CC_MAX_LEN_REQ_SUPP_PARAM_CISCO_SISTAG 64

/**
 * Attrib bit in video available direction byte to indicate if the video is over CAST
 */
#define CC_ATTRIB_CAST 0x8 

/**
 * Define the line id. The value 0 (CC_NO_LINE) means not set, it will be set by pSipcc system.
 */
typedef unsigned short cc_lineid_t;

/**
 * Define the call id. The value 0 (CC_NO_CALL_ID)  means not set, it will be set by pSipcc system.
 */
typedef unsigned short cc_callid_t;

typedef unsigned short cc_streamid_t;
typedef unsigned short cc_mcapid_t;
typedef unsigned short cc_groupid_t;
typedef unsigned short cc_level_t;

/**
 * Define the call instance id
 */

typedef unsigned short cc_call_instance_t;

/**
 * Define string char
 */
typedef const char *cc_string_t;

/**
 * Define an empty call handle
 */
#define CC_EMPTY_CALL_HANDLE   (0)

/**
 * This will be returned by pSipcc system, a user/application should use the following two methods to get the selected or
 * assinged line id and call id.
 * When a user or an application doesn't select a line (user passes 0 to pSipcc),, the pSipcc will assign it with the first available
 * line to it based on the maximum call per line that is configured.
 */
typedef unsigned int cc_call_handle_t;
#define CC_SID_TYPE_SHIFT 28
#define CC_SID_LINE_SHIFT 16
#define CC_SESSIONTYPE_CALLCONTROL 1
#define GET_LINE_ID(call_handle) (cc_lineid_t)((call_handle & 0xFFF0000) >> CC_SID_LINE_SHIFT )
#define GET_CALL_ID(call_handle) (cc_callid_t)(call_handle & 0xFFFF)
#define CREATE_CALL_HANDLE(line, callid) (cc_call_handle_t)(((line & 0xFFF) << CC_SID_LINE_SHIFT) + (callid & 0xFFFF))
#define CREATE_CALL_HANDLE_FROM_SESSION_ID(session_id) (session_id & 0xFFFFFFF)
#define CREATE_SESSION_ID_FROM_CALL_HANDLE(call_handle) ((CC_SESSIONTYPE_CALLCONTROL << CC_SID_TYPE_SHIFT) + call_handle)

/**
 * Define return codes
 */
typedef enum {
	CC_FAILURE = -1,
	CC_SUCCESS
} cc_return_t;

/**
 * Define valid number digits for SendDigit method
 */
typedef enum {
	KEY_1 = '1',
	KEY_2 = '2',
	KEY_3 = '3',
	KEY_4 = '4',
	KEY_5 = '5',
	KEY_6 = '6',
	KEY_7 = '7',
	KEY_8 = '8',
	KEY_9 = '9',
	KEY_0 = '0',
	KEY_STAR = '*',
	KEY_POUND = '#',
	KEY_A = 'A',
	KEY_B = 'B',
	KEY_C = 'C',
	KEY_D = 'D',
        KEY_PLUS = '+'
} cc_digit_t;


/**
 * Defines cucm mode of the call manager to which device is connected.
 */
typedef enum {
	CC_MODE_INVALID = -1,
	CC_MODE_CCM,
	CC_MODE_NONCCM
} cc_cucm_mode_t;

// Line feature
typedef enum {
	CC_LINE_FEATURE_NONE = 0,
	CC_LINE_FEATURE_REDIAL = 1,
	CC_LINE_FEATURE_SPEEDDIAL = 2,
	CC_LINE_FEATURE_DN = 9,
	CC_LINE_FEATURE_SERVICE = 20,
	CC_LINE_FEATURE_SPEEDDIALBLF = 21,
	CC_LINE_FEATURE_MALICIOUSCALLID = 27,
	CC_LINE_FEATURE_CALLPICKUP = 127,
	CC_LINE_FEATURE_GROUPCALLPICKUP = 128,
	CC_LINE_FEATURE_QUALREPORTTOOL = 133,
	CC_LINE_FEATURE_OTHERPICKUP = 135,
	CC_LINE_FEATURE_ALLCALLS = 140,
	CC_LINE_FEATURE_ANSWEROLDEST = 141,
	CC_LINE_FEATURE_SERVICES = 192,
	CC_LINE_FEATURE_BLF = 255
} cc_line_feature_t;

/**
 * Define feature option mask
 */
typedef enum {
	CC_FEATUREOPTIONMASK_NONE,
	CC_FEATUREOPTIONMASK_BLF_PICKUP
} cc_feature_option_mask_t;

/**
 * Defines cucm secure levels
 */
typedef enum {
    CC_CUCM_NONSECURE,
    CC_CUCM_AUTHENTICATED,
    CC_CUCM_ENCRYPTED,
    CC_CUCM_NOT_IN_CTL
} cc_cucm_sec_level_t;

/**
 * Defines cc events causing registration state change
 */
typedef enum {
	CC_CAUSE_NONE,
	CC_CAUSE_FAILOVER,
	CC_CAUSE_FALLBACK,
	CC_CAUSE_REG_ALL_FAILED,
	CC_CAUSE_SHUTDOWN,
	CC_CAUSE_UNREG_ALL,
	CC_CAUSE_LOGOUT_RESET
} cc_service_cause_t;

/**
 * Defines cc service state
 */
typedef enum {
    CC_STATE_IDLE = 0,
    CC_STATE_INS,
    CC_STATE_OOS,
    CC_STATE_PRO_BASE
} cc_service_state_t;

/**
 * Define cucm connection status.
 */
typedef enum {
    CC_CCM_STATUS_NONE = 0,
    CC_CCM_STATUS_STANDBY,
    CC_CCM_STATUS_ACTIVE
} cc_ccm_status_t;

/**
 * Define line registration state
 */
typedef enum {
    CC_UNREGISTERED,
    CC_REGISTERED
}cc_line_reg_state_t;

/**
 * Defines pSipcc shutdown reason code
 */
typedef enum {
    CC_SHUTDOWN_NORMAL,
    CC_SHUTDOWN_UNSPECIFIED,
    CC_SHUTDOWN_VERMISMATCH
} cc_shutdown_reason_t;

/**
 * Defines kpml value
 */
typedef enum {
    CC_KPML_NONE = 0x0,
    CC_KPML_SIGNAL_ONLY = 0x1,
    CC_KPML_DTMF_ONLY = 0x2,
    CC_KPML_BOTH = 0x3
} cc_kpml_config_t;

/**
 * Defines whether to upgrade now or later to recently download firmware image
 */
typedef enum {
	CC_UPGRADE_NONE = 0,
	CC_UPGRADE_NOW,
	CC_UPGRADE_LATER
} cc_upgrade_t;

/* Media flow direction */
typedef enum {
    CC_SDP_DIRECTION_INACTIVE,
    CC_SDP_DIRECTION_SENDONLY,
    CC_SDP_DIRECTION_RECVONLY,
    CC_SDP_DIRECTION_SENDRECV,
    CC_SDP_MAX_QOS_DIRECTIONS
} cc_sdp_direction_t;

/**
 * Defines BLF state
 */
typedef enum {
	CC_SIP_BLF_UNKNOWN,
	CC_SIP_BLF_IDLE,
	CC_SIP_BLF_INUSE,
	CC_SIP_BLF_EXPIRED,
	CC_SIP_BLF_REJECTED,
	CC_SIP_BLF_ALERTING
} cc_blf_state_t;

/**
 * Defines BLF feature mask
 */
typedef enum {
    CC_BLF_FEATURE_MASK_NONE = 0,
    CC_BLF_FEATURE_MASK_PICKUP
} cc_blf_feature_mask_t;

/**
 * Defines call state
 */
typedef enum {
	OFFHOOK,
	ONHOOK,
	RINGOUT,
	RINGIN,
	PROCEED,
	CONNECTED,
	HOLD,
	REMHOLD,
	RESUME,
	BUSY,
	REORDER,
	CONFERENCE,
	DIALING,
	REMINUSE,
	HOLDREVERT,
	WHISPER,
    PRESERVATION,
	WAITINGFORDIGITS = 21,
	CREATEOFFER,
	CREATEANSWER,
	CREATEOFFERERROR,
	CREATEANSWERERROR,
	SETLOCALDESC,
	SETREMOTEDESC,	
	SETLOCALDESCERROR,
	SETREMOTEDESCERROR,
	REMOTESTREAMADD,
    MAX_CALL_STATES
} cc_call_state_t;

/**
 * Defines call attribute
 */
typedef enum {
	CC_ATTR_NOT_DEFINED = -1,
	CC_ATTR_NORMAL,
	CC_ATTR_XFR_CONSULT,
	CC_ATTR_CONF_CONSULT,
	CC_ATTR_BARGING,
	CC_ATTR_RIUHELD_LOCKED,
	CC_ATTR_LOCAL_CONF_CONSULT,
	CC_ATTR_LOCAL_XFER_CONSULT,
	CC_ATTR_CFWDALL,
	CC_ATTR_GRP_CALL_PICKUP,
	CC_ATTR_CFWD_ALL,
	CC_ATTR_MAX
} cc_call_attr_t;

/**
 * Defines the hold reason
 */
typedef enum {
   CC_HOLD_REASON_NONE,
   CC_HOLD_REASON_XFER,
   CC_HOLD_REASON_CONF,
   CC_HOLD_REASON_SWAP,
   CC_HOLD_REASON_INTERNAL
} cc_hold_reason_t;

/**
 * Defines call type
 */
typedef enum {
	CC_CALL_TYPE_NONE,
	CC_CALL_TYPE_INCOMING,
	CC_CALL_TYPE_OUTGOING,
	CC_CALL_TYPE_FORWARDED
} cc_call_type_t;

/**
 * Defines call security
 */
typedef enum {
	CC_SECURITY_NONE,
	CC_SECURITY_UNKNOWN,
	CC_SECURITY_AUTHENTICATED,
	CC_SECURITY_NOT_AUTHENTICATED,
	CC_SECURITY_ENCRYPTED
} cc_call_security_t;

/**
 * Defines call policy
 */
typedef enum {
	/* Call Policy */
	CC_POLICY_NONE,
	CC_POLICY_UNKNOWN,
	CC_POLICY_CHAPERONE
} cc_call_policy_t;

/**
 * Defines Log disposition
 */
typedef enum {
	CC_LOGD_MISSED,
	CC_LOGD_RCVD,
	CC_LOGD_SENT,
	CC_LOGD_UNKNWN,
	CC_LOGD_DELETE
} cc_log_disposition_t;

/**
 * Defines call priority
 */
typedef enum{
	CC_PRIORITY_NORMAL,
	CC_PRIORITY_URGENT
} cc_call_priority_t;

/**
 * Defines call selection
 */
typedef enum {
	CC_CALL_SELECT_NONE,
	CC_CALL_SELECT_LOCKED,
	CC_CALL_SELECT_UNLOCKED,
	CC_CALL_SELECT_REMOTE_LOCKED
} cc_call_selection_t;

/**
 * Defines service control request
 */
typedef enum {
	CC_DEVICE_RESET = 1,
	CC_DEVICE_RESTART,
    CC_DEVICE_ICMP_UNREACHABLE = 5
} cc_srv_ctrl_req_t;

/**
 * Defines service control command
 */
typedef enum {
	CC_ACTION_RESET = 1,
	CC_ACTION_RESTART
} cc_srv_ctrl_cmd_t;

/**
 * Defines messaging type
 */
typedef enum {
	CC_VOICE_MESSAGE = 1,
	CC_TEXT_MESSAGE
} cc_message_type_t;

/**
 * Defines handset lamp state
 */
typedef enum {
	CC_LAMP_NONE = 0,
	CC_LAMP_ON,
	CC_LAMP_BLINK,
	CC_LAMP_FRESH
} cc_lamp_state_t;

/**
 * defines call cause
 * Important: when update this enum, please update the cc_cause_name accordingly.
 */
typedef enum {
	CC_CAUSE_MIN = -1,
	CC_CAUSE_BASE = -1,
	CC_CAUSE_OK,
	CC_CAUSE_ERROR,
	CC_CAUSE_UNASSIGNED_NUM,
	CC_CAUSE_NO_RESOURCE,
	CC_CAUSE_NO_ROUTE,
	CC_CAUSE_NORMAL,
	CC_CAUSE_BUSY,
	CC_CAUSE_NO_USER_RESP,
	CC_CAUSE_NO_USER_ANS,
	CC_CAUSE_REJECT,
	CC_CAUSE_INVALID_NUMBER,
	CC_CAUSE_FACILITY_REJECTED,
	CC_CAUSE_CALL_ID_IN_USE,
	CC_CAUSE_XFER_LOCAL,
	CC_CAUSE_XFER_REMOTE,
	CC_CAUSE_XFER_BY_REMOTE,
	CC_CAUSE_XFER_CNF,
	CC_CAUSE_CONGESTION,
	CC_CAUSE_ANONYMOUS,
	CC_CAUSE_REDIRECT,
	CC_CAUSE_PAYLOAD_MISMATCH,
	CC_CAUSE_CONF,
	CC_CAUSE_REPLACE,
	CC_CAUSE_NO_REPLACE_CALL,
	CC_CAUSE_NO_RESUME,
	CC_CAUSE_NO_MEDIA,
	CC_CAUSE_REQUEST_PENDING,
	CC_CAUSE_INVALID_PARTICIPANT,
	CC_CAUSE_NO_CNF_BRIDE,
	CC_MAXIMUM_PARTICIPANT,
	CC_KEY_NOT_ACTIVE,
	CC_TEMP_NOT_AVAILABLE,
	CC_CAUSE_REMOTE_SERVER_ERROR,
	CC_CAUSE_BARGE,
	CC_CAUSE_CBARGE,
	CC_CAUSE_NOT_FOUND,
	CC_CAUSE_SECURITY_FAILURE,
	CC_CAUSE_MONITOR,
	CC_CAUSE_UI_STATE_BUSY,
	CC_SIP_CAUSE_ANSWERED_ELSEWHERE,
	CC_CAUSE_RETRIEVED,
	CC_CAUSE_FORWARDED,
	CC_CAUSE_ABANDONED,
	CC_CAUSE_XFER_LOCAL_WITH_DIALSTRING,
	CC_CAUSE_BW_OK,
	CC_CAUSE_XFER_COMPLETE,
	CC_CAUSE_RESP_TIMEOUT,
	CC_CAUSE_SERV_ERR_UNAVAIL,
    CC_CAUSE_REMOTE_DISCONN_REQ_PLAYTONE,
    CC_CAUSE_OUT_OF_MEM,
    CC_CAUSE_VALUE_NOT_FOUND,
	CC_CAUSE_MAX
} cc_cause_t;

/**
 * defines subscription event type
 */
typedef enum {
	CC_SUBSCRIPTIONS_DIALOG_EXT = 2,
    CC_SUBSCRIPTIONS_KPML_EXT = 4,
    CC_SUBSCRIPTIONS_PRESENCE_EXT = 5,
    CC_SUBSCRIPTIONS_REMOTECC_EXT = 6,
    CC_SUBSCRIPTIONS_REMOTECC_OPTIONSIND_EXT = 7,
    CC_SUBSCRIPTIONS_CONFIGAPP_EXT = 8,
    CC_SUBSCRIPTIONS_MEDIA_INFO_EXT = 10
} cc_subscriptions_ext_t;

typedef enum {
    APPLY_DYNAMICALLY=0,
    RESTART_NEEDED,
    APPLY_CONFIG_NONE
} cc_apply_config_result_t;


/**
 * defines ID of cucm
 */

typedef enum {
    PRIMARY_CCM = 0,
    SECONDARY_CCM,
    TERTIARY_CCM,
    MAX_CCM,
    UNUSED_PARAM
} CCM_ID;

typedef enum {
    CC_SIS_B2B_CONF = 0,
    CC_SIS_SWAP,
    CC_SIS_CFWD_ANY_LINE
} cc_sis_feature_id_e;
  
/**
 * enum for conference participant status
 */  

typedef enum {
   CCAPI_CONFPARTICIPANT_UNKNOWN,
   CCAPI_CONFPARTICIPANT_DIALING_OUT,
   CCAPI_CONFPARTICIPANT_ALERTING,
   CCAPI_CONFPARTICIPANT_CONNECTED,
   CCAPI_CONFPARTICIPANT_ON_HOLD,
   CCAPI_CONFPARTICIPANT_DISCONNECTED
} cc_conf_participant_status_t;  


typedef enum {
  JSEP_NO_ACTION = -1,
  JSEP_OFFER,
  JSEP_ANSWER,
  JSEP_PRANSWER
} cc_jsep_action_t;


typedef cc_string_t cc_peerconnection_t;

typedef unsigned int cc_media_stream_id_t;

typedef unsigned int cc_media_track_id_t;


typedef enum {
  NO_STREAM = -1,
  AUDIO,
  VIDEO,
  TYPE_MAX
} cc_media_type_t;



#endif /* _CC_CONSTANTS_H_ */

