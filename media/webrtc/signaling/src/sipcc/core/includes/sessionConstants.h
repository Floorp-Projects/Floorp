/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SESSION_CONSTANTS_H_
#define _SESSION_CONSTANTS_H_

#include "cc_constants.h"

typedef enum {
    GROUP_TAG,
    GROUP_SESSION_TYPE,
    GROUP_CMD,
    GROUP_STATE,
    GROUP_CC_MODE,
    GROUP_CC_REG_CAUSE,
    GROUP_CC_FEATURE,
    GROUP_DEVICE_FEATURE,
    GROUP_RINGER_RESERVATION,
    GROUP_CALL_STATE,
    GROUP_CC_ATTR,
    GROUP_CC_CALL_TYPE,
    GROUP_CC_SECURITY,
    GROUP_INFO_PKG_ID,
    GROUP_CC_POLICY,
    GROUP_UI_PRIVACY,
    GROUP_SIP_BLF,
    GROUP_SESSION_EVENT,
    GROUP_CALL_EVENT,
    GROUP_FILEPLAYER,
    GROUP_MEDIA_EVENT,
    GROUP_MEDIA_DIRECTION,
    GROUP_PRIORITY,
    GROUP_SESSION,
    GROUP_CC_CAUSE
} group_t;

typedef enum {
    TAG_LINE = 1L,
    TAG_STATE,
    TAG_CCM_ADDR,
    TAG_STATUS,
    TAG_LCLCFWD,
    TAG_CFANUM,
    TAG_COUNT,
    TAG_INSTANCE,
    TAG_TIMEOUT,
    TAG_PRIORITY,
    TAG_NOTPROG,
    TAG_RESET_TYPE,
    TAG_ATTR,
    TAG_INST,
    TAG_SECURITY,
    TAG_CLD_NAME,
    TAG_CLD_NUMB,
    TAG_CLG_NAME,
    TAG_CLG_NUMB,
    TAG_PRIVACY,
    TAG_FEAT_SET,
    TAG_FEATURE,
    TAG_PROMPT,
    TAG_ORIG_NAME,
    TAG_ORIG_NUMB,
    TAG_REDIR_NAME,
    TAG_REDIR_NUMB,
    TAG_ALT_CLG,
    TAG_DISP_CLG,
    TAG_DISP_CLD,
    TAG_CALL_TYPE,
    TAG_MODE,
    TAG_CAUSE,
    TAG_CALL_SELECTED,
    TAG_BUTTON_NUMB,
    TAG_SPEED_DIAL,
    TAG_LABEL,
    TAG_GCID,
    TAG_LOGDISP,
    TAG_MWI_TYPE,
    TAG_NEW_COUNT,
    TAG_OLD_COUNT,
    TAG_HP_NEW_COUNT,
    TAG_HP_OLD_COUNT,
    TAG_MEDIA_TYPE,
    TAG_MEDIA_DIRECTION,
    TAG_MEDIA_MODE,
    TAG_DURATION,
    TAG_SESSION_HANDLE,
    TAG_MCAP_ID,
    TAG_GROUP_ID,
    TAG_STREAM_ID,
    TAG_REF_COUNT,
    TAG_SESSION_ID,
    TAG_RECV_INFO_LIST,
    TAG_INFO_PACKAGE,
    TAG_CONTENT_TYPE,
    TAG_MESSAGE_BODY,
    TAG_POLICY,
    TAG_CFG_VER,
    TAG_DP_VER,
    TAG_SK_VER,
    TAG_METHOD,
    TAG_SIS_VER_NAME,
    TAG_SIS_VER_MAJOR,
    TAG_SIS_VER_MINOR,
    TAG_SIS_VER_ADDTNL
} group_tag_t;


/* Session types supported */
/* SESSIONTYPE_* is encoded into the MSB of session_feature_t.session_id */
// XXX TODO figure out how to decouple this from the Java side constant
typedef enum {
    SESSIONTYPE_CALLCONTROL = 1L,
    SESSIONTYPE_RSTP,
    SESSIONTYPE_RTP,
    SESSIONTYPE_FILEPLAYER,
    SESSIONTYPE_TONE,
    SESSIONTYPE_CAPTURE
} group_session_type_t;

/* Session Provider Management Commands */
typedef enum {
    CMD_INIT = 1L,
    CMD_INSERVICE,
    CMD_RESTART,
    CMD_SHUTDOWN,
    CMD_UNLOAD,
    CMD_PRE_INIT,
    CMD_PRO_BASE,
    CMD_UNREGISTER_ALL_LINES = 10L,
    CMD_REGISTER_ALL_LINES,
    CMD_BLF_INIT
} group_cmd_t;

/* Other provider specific cmds can be defined beginning with CMD_PRO_BASE */
/* TBD from JNI */
#define CC_CMD_UPDATELINES  CMD_PRO_BASE

/**
 * Defines registration state
 */
typedef enum {
    CC_CREATED_IDLE,
    CC_OOS_FAILOVER,
    CC_OOS_REGISTERING,
    CC_OOS_AWAIT_CFG_SYNC,
    CC_OOS_AWAIT_RESTART,
    CC_INSERVICE,
    CC_OOS_IDLE
} cc_reg_state_t;

/* Other provider specific cmds can be defined beginning with STATE_PRO_BASE */



/* Device specific feature update IDs */
typedef enum {
    DEVICE_FEATURE_CFWD = 1L,
    DEVICE_FEATURE_MWI,
    DEVICE_FEATURE_MWILAMP,
    DEVICE_FEATURE_MNC_REACHED,
    DEVICE_SERVICE_CONTROL_REQ,
    DEVICE_NOTIFICATION,
    DEVICE_LABEL_N_SPEED,
    DEVICE_REG_STATE,
    DEVICE_CCM_CONN_STATUS,
    DEVICE_CONDITIONAL_RESTART = 14L,
    DEVICE_SYNC_CONFIG_VERSION,
    DEVICE_ENABLE_VIDEO,
    DEVICE_ENABLE_CAMERA,
    DEVICE_FEATURE_BLF,
    DEVICE_SUPPORTS_NATIVE_VIDEO
} group_device_feature_t;

/* Ringer Reservation feature update IDs */
typedef enum {
    RINGER_RESERVATION_CREATED = 100L,
    RINGER_RESERVATION_UPDATE
} group_ringer_reservation_t;

/* Info Package */
typedef enum {
    INFO_PKG_ID_GENERIC_RAW = 0L
} group_info_pkg_id_t;

/* Session Events */
typedef enum {
    SESSION_CREATED = 1L,
    SESSION_CLOSED
} group_session_event_t;

/* Call Session Events */
typedef enum {
    CALL_SESSION_CREATED = SESSION_CREATED,
    CALL_SESSION_CLOSED = SESSION_CLOSED,
    CALL_STATE = 3L,
    CALL_NEWCALL,
    CALL_INFORMATION,
    CALL_ATTR,
    CALL_SECURITY,
    CALL_LOGDISP,
    CALL_PLACED_INFO,
    CALL_STATUS,
    CALL_DELETE_LAST_DIGIT,
    CALL_ENABLE_BKSP,
    CALL_SELECT_FEATURE_SET,
    CALL_SELECTED,
    CALL_PRESERVATION_ACTIVE,
    CALL_GCID,
    CALL_FEATURE_CANCEL,
    VIDEO_AVAIL = 20L,
    CALL_RECV_INFO_LIST,
    VIDEO_OFFERED,
    RINGER_STATE,
    CALL_CALLREF,
    MEDIA_INTERFACE_UPDATE_BEGIN,
    MEDIA_INTERFACE_UPDATE_SUCCESSFUL,
    MEDIA_INTERFACE_UPDATE_FAIL,
    CREATE_OFFER,
    CREATE_ANSWER,
    SET_LOCAL_DESC,
    SET_REMOTE_DESC,
    UPDATE_LOCAL_DESC,
    UPDATE_REMOTE_DESC,
    REMOTE_STREAM_ADD,
    ICE_CANDIDATE_ADD
} group_call_event_t;

/* File Player Session Events */
typedef enum {
    FILEPLAYER_PLAYED = 300L,
    FILEPLAYER_ALLOCATED
} group_fileplayer_t;

typedef enum {
    TONE_STARTED = 101L,
    TONE_STOPPED,
    MEDIA_INFO,
    MEDIA_UPDATE
} group_media_event_t;

//#include "com_cisco_sessionapi_MediaDirection.h"
typedef enum {
    RX_DIRECTION = 0L,
    TX_DIRECTION,
    BI_DIRECTION
} group_media_direction_t;

typedef enum {
    PROMPTSTATUS_PROMPT = 10L,
    PROMPTSTATUS_HIGH = 11L,
    PROMPTSTATUS_NORMAL = 15L,
    PROMPTSTATUS_MEDIA_MANAGER = 16L,
    PROMPTSTATUS_NOTIFICATION = 20L,
    PROMPTSTATUS_STATUS = 30L,
    PROMPTSTATUS_LOW = 31L,
    SOFTKEYBAR_APPLICATION_MANAGER = 100L,
    SOFTKEYBAR_MEDIA_MANAGER = 50L
} group_priority_t;

/* Session Features that can be invoked TBD should come from JNI */
#define FEATURE_NONE        0
#define FEATURE_VOLUME_CTRL 1
#define FEATURE_PRO_BASE    0

/* Call Priority TBD should come from JNI */
#define CC_CALL_PRIORITY_NORMAL  0
#define CC_CALL_PRIORITY_URGENT  1

/* Session Commands  TBD should come from JNI */
typedef enum {
    SESSION_REALIZE = 1,
    SESSION_PREFETCH,
    SESSION_START,
    SESSION_STOP,
    SESSION_DEALLOCATE,
    SESSION_CLOSE,
    SESSION_ALLOCATE
} group_session_t;

/*
 * CC Provider specific constants.
 * These do not come from JNI Files
 */

#include "phone_types.h"

#define CC_ALL_LINES       255
#define CC_SESSION_INVALID 0x01FFFFFF
#define CC_MAX_GCID        CC_GCID_LEN

/* 1-9 * # A B C D are number 1 thru 16 */
#define BKSP_KEY   90

#ifdef __CC_CAUSE_STRINGS__
static const char *cc_cause_names[] = {
    "OK",
    "ERR",
    "UNASSIGNED_NUM",
    "NO_RESOURCE",
    "NO_ROUTE",
    "NORMAL",
    "BUSY",
    "NO_USER_RESP",
    "NO_USER_ANS",
    "REJECT",
    "INVALID_NUMBER",
    "FACILITY_REJECTED",
    "CALL_ID_IN_USE",
    "XFER_LOCAL",
    "XFER_REMOTE",
    "XFER_BY_REMOTE",
    "XFER_CONFERENCE",
    "CONGESTION",
    "ANONYMOUS",
    "REDIRECT",
    "PAYLOAD_MISMATCH",
    "CONF",
    "REPLACE",
    "NO_REPLACE_CALL",
    "NO_RESUME",
    "NO_MEDIA",
    "REQUEST_PENDING",
    "INVALID_PARTICIPANT",
    "NO_CONF_BRIDGE",
    "MAX_PARTICIPANT",
    "KEY_NOT_ACTIVE",
    "TEMP_NOT_AVAILABLE",
    "REMOTE_SERVER_ERROR",
    "BARGE",
    "CBARGE",
    "NOT_FOUND",
    "SECURITY_FAILURE",
    "MONITOR",
    "UI_STATE_BUSY",
    "SIP_CAUSE_ANSWERED_ELSEWHERE",
    "RETRIEVED",
    "FORWARDED",
    "ABANDONED",
    "XFER_LOCAL_WITH_DIALSTRING",
    "CAC_BW_OK",
    "ONHOOK_FEAT_COMP",
    "RESP_TIMEOUT",
    "SERV_ERR_UNAVAIL",
    "REMOTE_DISCONN_REQ_PLAYTONE",
    "OUT_OF_MEM",
    "VALUE_NOT_FOUND",
    "BAD_ICE_ATTRIBUTE",
    "DTLS_ATTRIBUTE_ERROR",
    "DTLS_DIGEST_ALGORITHM_EMPTY",
    "DTLS_DIGEST_ALGORITHM_TOO_LONG",
    "DTLS_DIGEST_EMPTY",
    "DTLS_DIGEST_TOO_LONG",
    "DTLS_FINGERPRINT_PARSE_ERROR",
    "DTLS_FINGERPRINT_TOO_LONG",
    "INVALID_SDP_POINTER",
    "NO_AUDIO",
    "NO_DTLS_FINGERPRINT",
    "MISSING_ICE_ATTRIBUTES",
    "NO_MEDIA_CAPABILITY",
    "NO_M_LINE",
    "NO_PEERCONNECTION",
    "NO_SDP",
    "NULL_POINTER",
    "SDP_CREATE_FAILED",
    "SDP_ENCODE_FAILED",
    "SDP_PARSE_FAILED",
    "SETTING_ICE_SESSION_PARAMETERS_FAILED",
    "MAX_CAUSE"
};
#endif //__CC_CAUSE_STRINGS__

#define MAX_SOFT_KEYS    16


// eventually these should come from the Java side
typedef enum {
    SESSION_MGMT_APPLY_CONFIG,
    SESSION_MGMT_SET_TIME,
    SESSION_MGMT_GET_PHRASE_TEXT,
    SESSION_MGMT_SET_UNREG_REASON,
    SESSION_MGMT_GET_UNREG_REASON,
    SESSION_MGMT_UPDATE_KPMLCONFIG,
    SESSION_MGMT_GET_AUDIO_DEVICE_STATUS,
    SESSION_MGMT_CHECK_SPEAKER_HEADSET_MODE,
    SESSION_MGMT_LINE_HAS_MWI_ACTIVE,
    SESSION_MGMT_EXECUTE_URI
} session_mgmt_func_e;

#endif
