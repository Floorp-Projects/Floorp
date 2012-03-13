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

#ifndef _CCAPI_H_
#define _CCAPI_H_

#include "cpr_types.h"
#include "cpr_memory.h"
#include "phone_types.h"
#include "string_lib.h"
#include "vcm.h"
#include "sdp.h"
#include "cc_constants.h"

typedef unsigned int cc_security_e;
typedef unsigned int cc_select_state_e;
typedef unsigned int cc_call_type_e;
typedef unsigned int cc_policy_e;
typedef int cc_causes_t;
#define  CC_CALL_OUTGOMING  CC_CALL_TYPE_OUTGOING
#define  CC_CALL_FORWARDED  CC_CALL_TYPE_FORWARDED
#define  CC_CALL_NONE       CC_CALL_TYPE_NONE
#define  CC_CALL_INCOMING   CC_CALL_TYPE_INCOMING

#include "sessionConstants.h"

typedef int  cc_features_t;
typedef unsigned int softkey_events;
typedef unsigned int cc_call_priority_e;
extern cc_reg_state_t ccapp_get_state();


//  global sdp structure
typedef struct cc_global_sdp_ {
	char			offerSDP[1020];
	char			answerSDP[1024];
	char			offerAddress[MAX_IPADDR_STR_LEN];
	int				audioPort;
	int				videoPort;
} cc_global_sdp_t;

extern cc_global_sdp_t  gROAPSDP;


typedef enum {
    CC_FEATURE_MIN = -1L,
    CC_FEATURE_NONE,
    CC_FEATURE_HOLD = 1L,
    CC_FEATURE_RESUME,
    CC_FEATURE_OFFHOOK,
    CC_FEATURE_NEW_CALL,
    CC_FEATURE_REDIAL,
    CC_FEATURE_ONHOOK,
    CC_FEATURE_KEYPRESS,
    CC_FEATURE_DIAL,
    CC_FEATURE_XFER,
    CC_FEATURE_CFWD_ALL,
    CC_FEATURE_END_CALL,
    CC_FEATURE_ANSWER,
    CC_FEATURE_INFO,
    CC_FEATURE_CONF,
    CC_FEATURE_JOIN,
    CC_FEATURE_DIRTRXFR,
    CC_FEATURE_SELECT,
    CC_FEATURE_SPEEDDIAL,
    CC_FEATURE_SWAP,
    CC_FEATURE_SPEEDDIAL_BLF,
    CC_FEATURE_BLIND_XFER_WITH_DIALSTRING,
    CC_FEATURE_BKSPACE,
    CC_FEATURE_CANCEL,
    CC_FEATURE_DIALSTR,
    CC_FEATURE_UPD_SESSION_MEDIA_CAP,
    /* Not used in the session API */
    CC_FEATURE_MEDIA,
    CC_FEATURE_UPDATE,
    CC_FEATURE_CALLINFO,
    CC_FEATURE_BLIND_XFER,
    CC_FEATURE_NOTIFY,
    CC_FEATURE_SUBSCRIBE,
    CC_FEATURE_B2BCONF,
    CC_FEATURE_B2B_JOIN,
    CC_FEATURE_HOLD_REVERSION,
    CC_FEATURE_BLF_ALERT_TONE,
    CC_FEATURE_REQ_PEND_TIMER_EXP,
    CC_FEATURE_NUMBER,
    CC_FEATURE_URL,
    CC_FEATURE_REDIRECT,
    CC_FEATURE_RINGBACK_DELAY_TIMER_EXP,
    CC_FEATURE_CALL_PRESERVATION,
    CC_FEATURE_UPD_MEDIA_CAP,
    CC_FEATURE_CAC_RESP_PASS,
    CC_FEATURE_CAC_RESP_FAIL,
    CC_FEATURE_FAST_PIC_UPD,
    CC_FEATURE_UNDEFINED,
    CC_FEATURE_MAX
} group_cc_feature_t;

/*
 * Constants
 */
#define CC_MAX_DIALSTRING_LEN (512)
#define CC_MAX_MEDIA_TYPES    (15)
#define CC_MAX_MEDIA_CAP      (4)
#define CC_NO_CALL_ID         (0)
#define CC_NO_LINE            (0)
#define CC_NO_DATA            (NULL)
#define CC_NO_MEDIA_REF_ID    (0)
#define CC_MAX_REDIRECTS      (1)
#define CC_MAX_BODY_PARTS     3
#define CC_NO_GROUP_ID        (0)
#define CC_NO_CALL_INST_ID    (0)
#define CC_SHORT_DIALSTRING_LEN (64)
#define CC_CISCO_PLAR_STRING  "x-cisco-serviceuri-offhook"
#define CISCO_BLFPICKUP_STRING  "x-cisco-serviceuri-blfpickup"
#define JOIN_ACROSS_LINES_DISABLED 0

/*
 *
 * Support type definintions
 *
 */
typedef enum cc_rcs_t_ {
    CC_RC_MIN = -1,
    CC_RC_SUCCESS,
    CC_RC_ERROR,
    CC_RC_MAX
} cc_rcs_t;

typedef enum cc_msgs_t_ {
    CC_MSG_MIN = -1,
    CC_MSG_SETUP,
    CC_MSG_SETUP_ACK,
    CC_MSG_PROCEEDING,
    CC_MSG_ALERTING,
    CC_MSG_CONNECTED,
    CC_MSG_CONNECTED_ACK,
    CC_MSG_RELEASE,
    CC_MSG_RELEASE_COMPLETE,
    CC_MSG_FEATURE,
    CC_MSG_FEATURE_ACK,
    CC_MSG_OFFHOOK,
    CC_MSG_ONHOOK,
    CC_MSG_LINE,
    CC_MSG_DIGIT_BEGIN,
    CC_MSG_DIGIT_END,
    CC_MSG_DIALSTRING,
    CC_MSG_MWI,
    CC_MSG_AUDIT,
    CC_MSG_AUDIT_ACK,
    CC_MSG_OPTIONS,
    CC_MSG_OPTIONS_ACK,
    CC_MSG_SUBSCRIBE,
    CC_MSG_NOTIFY,
    CC_MSG_FAILOVER_FALLBACK,
    CC_MSG_INFO,
    /* update the following strings table if this is changed */
    CC_MSG_MAX
} cc_msgs_t;

#ifdef __CC_MESSAGES_STRINGS__
static const char *cc_msg_names[] = {
    "SETUP",
    "SETUP_ACK",
    "PROCEEDING",
    "ALERTING",
    "CONNECTED",
    "CONNECTED_ACK",
    "RELEASE",
    "RELEASE_COMPLETE",
    "FEAT:",
    "FEATURE_ACK",
    "OFFHOOK",
    "ONHOOK",
    "LINE",
    "DIGIT_BEGIN",
    "DIGIT_END",
    "DIALSTRING",
    "MWI",
    "AUDIT",
    "AUDIT_ACK",
    "OPTIONS",
    "OPTIONS_ACK",
    "SUBSCRIBE",
    "NOTIFY",
    "FAILOVER_FALLBACK",
    "INFO",
    "INVALID",
};
#endif //__CC_MESSAGES_STRINGS__

typedef enum cc_srcs_t_ {
    CC_SRC_MIN = -1,
    CC_SRC_GSM,
    CC_SRC_UI,
    CC_SRC_SIP,
    CC_SRC_MISC_APP,
    CC_SRC_RCC,
    CC_SRC_CCAPP,
    CC_SRC_MAX
} cc_srcs_t;


typedef enum cc_transfer_mode_e_ {
    CC_XFR_MODE_MIN = -1,
    CC_XFR_MODE_NONE,
    CC_XFR_MODE_TRANSFEROR,
    CC_XFR_MODE_TRANSFEREE,
    CC_XFR_MODE_TARGET,
    CC_XFR_MODE_MAX
} cc_transfer_mode_e;

typedef enum cc_regmgr_rsp_type_e_ {
    CC_FAILOVER_RSP=0,
    CC_RSP_START =1,
    CC_RSP_COMPLETE=2
} cc_regmgr_rsp_type_e;

typedef enum cc_regmgr_rsp_e_ {
    CC_REG_FAILOVER_RSP,
    CC_REG_FALLBACK_RSP
} cc_regmgr_rsp_e;

/*
 * Identifies what was in the Alert-Info header
 */
typedef enum {
    ALERTING_NONE, /* No alert-info header present */
    ALERTING_OLD,  /* Unrecognized pattern or an error, mimic old behavior */
    ALERTING_TONE, /* Play tone */
    ALERTING_RING  /* Play ringing pattern */
} cc_alerting_type;

typedef enum {
    CC_MONITOR_NONE = 0,
    CC_MONITOR_SILENT = 1,
    CC_MONITOR_COACHING = 2
} monitor_mode_t;

typedef enum {
  CFWDALL_NONE = -1,
  CFWDALL_CLEAR,
  CFWDALL_SET
} cfwdall_mode_t;

/* Do not change enum values unless remote-cc xml has been changed */

typedef enum {
    CC_RING_DEFAULT = 0,
    CC_RING_ENABLE = 1,
    CC_RING_DISABLE = 2
} cc_rcc_ring_mode_e;

/* Do not change enum values unless remote-cc xml has been changed */

typedef enum {
    CC_SK_EVT_TYPE_DEF = 0,
    CC_SK_EVT_TYPE_EXPLI = CC_SK_EVT_TYPE_DEF,
    CC_SK_EVT_TYPE_IMPLI = 1
} cc_rcc_skey_evt_type_e;

/* media name with media capability table */
typedef enum {
    CC_AUDIO_1,
    CC_VIDEO_1
} cc_media_cap_name;

typedef struct cc_sdp_addr_t_ {
    cpr_ip_addr_t addr;
    unsigned int port;
} cc_sdp_addr_t;

typedef struct cc_sdp_data_t_ {
    cc_sdp_addr_t addr;
    int           media_types[CC_MAX_MEDIA_TYPES];
    int32_t       avt_payload_type;
} cc_sdp_data_t;

typedef struct cc_sdp_t_ {
    void          *src_sdp;     /* pointer to source SDP */
    void          *dest_sdp;    /* pointer to received SDP */
} cc_sdp_t;

typedef enum
{
    cc_disposition_unknown = 0,
    cc_disposition_render,
    cc_disposition_session,
    cc_dispostion_icon,
    cc_disposition_alert,
    cc_disposition_precondition
} cc_disposition_type_t;

typedef struct
{
    cc_disposition_type_t disposition;
    boolean               required_handling;
} cc_content_disposition_t;

typedef enum
{
    cc_content_type_unknown = 0,
    cc_content_type_SDP,
    cc_content_type_sipfrag,
    cc_content_type_CMXML
} cc_content_type_t;

typedef struct cc_msgbody_t_ {
    cc_content_type_t        content_type;
    cc_content_disposition_t content_disposition;
    uint32_t                 body_length;
    char                     *body;
    char                     *content_id;
} cc_msgbody_t;

typedef struct cc_msgbody_info_t_ {
        uint32_t          num_parts;            /* number of body parts   */
        cc_content_type_t content_type;         /* top level content type */
        cc_msgbody_t  parts[CC_MAX_BODY_PARTS]; /* parts                  */
} cc_msgbody_info_t;

/*
 * Message definitions
 */

typedef enum cc_redirect_reasons_t {
    CC_REDIRECT_REASON_MIN = -1,
    CC_REDIRECT_REASON_NONE,
    CC_REDIRECT_REASON_BUSY,
    CC_REDIRECT_REASON_NOANSWER,
    CC_REDIRECT_REASON_UNCONDITIONAL,
    CC_REDIRECT_REASON_DEFLECTION,
    CC_REDIRECT_REASON_UNAVAILABLE
} cc_redirect_reasons_t;

typedef struct cc_redirect_t_ {
    int count;
    struct {
        char                  number[CC_MAX_DIALSTRING_LEN];
        cc_redirect_reasons_t redirect_reason;
    } redirects[CC_MAX_REDIRECTS];
} cc_redirect_t;

typedef enum {
   CC_FEAT_NONE,
   CC_FEAT_HOLD,
   CC_FEAT_RESUME,
   CC_FEAT_BARGE,
   CC_FEAT_CBARGE,
   CC_FEAT_REPLACE,
   CC_FEAT_CALLINFO,
   CC_FEAT_INIT_CALL,
   CC_FEAT_MONITOR,
   CC_FEAT_TOGGLE_TO_SILENT_MONITORING,
   CC_FEAT_TOGGLE_TO_WHISPER_COACHING
} cc_call_info_e;

typedef enum {
   CC_REASON_NONE,
   CC_REASON_XFER,
   CC_REASON_CONF,
   CC_REASON_SWAP,
   CC_REASON_RCC,
   CC_REASON_INTERNAL,
   CC_REASON_MONITOR_UPDATE
} cc_hold_resume_reason_e;

typedef enum {
   CC_PURPOSE_NONE,
   CC_PURPOSE_INFO,
   CC_PURPOSE_ICON,
   CC_PURPOSE_CARD
} cc_purpose_e;

typedef enum {
   CC_ORIENTATION_NONE = CC_CALL_TYPE_NONE,
   CC_ORIENTATION_FROM = CC_CALL_TYPE_INCOMING,
   CC_ORIENTATION_TO = CC_CALL_TYPE_OUTGOING
} cc_orientation_e;

typedef enum {
   CC_UI_STATE_NONE,
   CC_UI_STATE_RINGOUT,
   CC_UI_STATE_CONNECTED,
   CC_UI_STATE_BUSY
} cc_ui_state_e;

typedef enum {
   CC_REASON_NULL,
   CC_REASON_ACTIVECALL_LIST
} cc_onhook_reason_e;

typedef enum {
   CC_CALL_LOG_DISP_MISSED,
   CC_CALL_LOG_DISP_RCVD,
   CC_CALL_LOG_DISP_PLACED,
   CC_CALL_LOG_DISP_UNKNWN,
   CC_CALL_LOG_DISP_IGNORE
} cc_call_logdisp_e;

/*
 * The cc_caller_id_t structure contains caller IDs fields.
 * Sending these fields accross components needs special care.
 * There are CCAPI API that provide access or transport these fields
 * by the source or by the destination. Any new caller IDs added,
 * please update the APIs in ccapi.c to support the new fields.
 * See cc_cp_caller(), cc_mv_caller_id() and cc_free_caller_id() functions.
 */
// mostly overlap with sessionTypes.h:cc_callinfo_t
typedef struct cc_caller_id_t_ {
    string_t calling_name;
    string_t calling_number;
    string_t alt_calling_number;
    boolean  display_calling_number;
    string_t called_name;
    string_t called_number;
    boolean  display_called_number;
    string_t orig_called_name;
    string_t orig_called_number;
    string_t last_redirect_name;
    string_t last_redirect_number;
    string_t orig_rpid_number;
    cc_call_type_e call_type;
    uint16_t call_instance_id;
} cc_caller_id_t;

/*
 * The followings are definitions of bits in the feature_flag of the
 * cc_feature_data_call_info_t strucure.
 *
 * The CC_DELAY_UI_UPDATE flag is not related to call information but
 * it indicates that the call info event signaled by SIP stack
 * to GSM that UI update can be delayed due to media manipulation
 * event will follow. This allows GSM to defer UI update to after the
 * media manipulation event.
 */
#define CC_SECURITY         1
#define CC_ORIENTATION      (1<<1)
#define CC_UI_STATE         (1<<2)
#define CC_CALLER_ID        (1<<3)
#define CC_CALL_INSTANCE    (1<<4)
#define CC_DELAY_UI_UPDATE  (1<<5)
#define CC_POLICY  (1<<6)

typedef struct cc_feature_data_call_info_t_{
   uint16_t            feature_flag;
   cc_security_e       security;
   cc_policy_e         policy;
   cc_orientation_e    orientation;
   cc_ui_state_e       ui_state;
   cc_caller_id_t      caller_id;
   cc_call_priority_e  priority;
   boolean             swap; //Indicate if hold/resume is because of swap
   boolean             protect; //indicate if the call has to be protected
   boolean             dusting; //indicate if it is a dusting call
   uint32_t            callref;
   char                global_call_id[CC_GCID_LEN];
} cc_feature_data_call_info_t;

typedef struct cc_replace_info_t_ {
   callid_t           remote_call_id; /* remote call ID to replace */
} cc_replace_info_t;

typedef struct cc_join_info_t_ {
   callid_t           join_call_id;
} cc_join_info_t;

typedef struct cc_initcall_t {
   char                           gcid[CC_GCID_LEN];    // Global call id used for CTI
   monitor_mode_t                 monitor_mode;
} cc_initcall_t;

typedef union {
   cc_initcall_t                  initcall;
   cc_hold_resume_reason_e        hold_resume_reason;
   cc_feature_data_call_info_t    call_info_feat_data;
   cc_purpose_e                   purpose; // Used for Barge, CBARGE
   cc_replace_info_t              replace;
   cc_join_info_t                 join;
} cc_call_info_data_t;

typedef struct {
  cc_call_info_data_t    data;
  cc_call_info_e         type;
} cc_call_info_t;

typedef enum cc_xfer_methods_t_ {
    CC_XFER_METHOD_MIN = -1,
    CC_XFER_METHOD_NONE,
    CC_XFER_METHOD_BYE,
    CC_XFER_METHOD_REFER,
    CC_XFER_METHOD_DIRXFR,
    CC_RCC_METHOD_REFER,
    CC_XFER_METHOD_MAX
} cc_xfer_methods_t;

typedef enum cc_subscriptions_t_ {
    CC_SUBSCRIPTIONS_MIN = -1,
    CC_SUBSCRIPTIONS_NONE,
    CC_SUBSCRIPTIONS_XFER,
    CC_SUBSCRIPTIONS_DIALOG = CC_SUBSCRIPTIONS_DIALOG_EXT,
    CC_SUBSCRIPTIONS_CONFIG,
    CC_SUBSCRIPTIONS_KPML = CC_SUBSCRIPTIONS_KPML_EXT,
    CC_SUBSCRIPTIONS_PRESENCE = CC_SUBSCRIPTIONS_PRESENCE_EXT,
    CC_SUBSCRIPTIONS_REMOTECC = CC_SUBSCRIPTIONS_REMOTECC_EXT,
    CC_SUBSCRIPTIONS_REMOTECC_OPTIONSIND = CC_SUBSCRIPTIONS_REMOTECC_OPTIONSIND_EXT,
    CC_SUBSCRIPTIONS_CONFIGAPP = CC_SUBSCRIPTIONS_CONFIGAPP_EXT,
    CC_SUBSCRIPTIONS_MEDIA_INFO = CC_SUBSCRIPTIONS_MEDIA_INFO_EXT,
    CC_SUBSCRIPTIONS_MAX
} cc_subscriptions_t;

typedef struct cc_feature_data_newcall_t_ {
    char          dialstring[CC_MAX_DIALSTRING_LEN];
    cc_causes_t   cause;
    cc_redirect_t redirect;
    cc_replace_info_t replace;
    cc_join_info_t   join;
    char          global_call_id[CC_GCID_LEN];
    callid_t      prim_call_id;                /* For internal new call event
                                                * refer primary call's call_id
                                                */
    cc_hold_resume_reason_e     hold_resume_reason; /* Reason for new consult call */
} cc_feature_data_newcall_t;

typedef struct cc_feature_data_xfer_t_ {
    cc_causes_t       cause;
    char              dialstring[CC_MAX_DIALSTRING_LEN];
    char              referred_by[CC_MAX_DIALSTRING_LEN];
    cc_xfer_methods_t method;
    callid_t          target_call_id;
    char          global_call_id[CC_GCID_LEN];
} cc_feature_data_xfer_t;

typedef enum cc_app_type_t_ {
    CC_APP_MIN = -1,
    CC_APP_NONE,
    CC_APP_CMXML,
    CC_APP_REMOTECC,
    CC_APP_MAX
} cc_app_type_t;

typedef struct cc_refer_body_t_ {
    struct cc_refer_body_t_  *next;
    char              refer_body[200];
    int                  refer_body_len;
    cc_app_type_t      app_type;
} cc_refer_body_t;

typedef struct cc_feature_data_ind_t__ {
    cc_causes_t       cause;
    char              to[CC_MAX_DIALSTRING_LEN];
    char              referred_by[CC_MAX_DIALSTRING_LEN];
    char              referred_to[CC_MAX_DIALSTRING_LEN];
    cc_refer_body_t   refer_body;
} cc_feature_data_ind_t;

typedef struct cc_feature_data_endcall_t_ {
    cc_causes_t cause;
    char        dialstring[CC_MAX_DIALSTRING_LEN];
} cc_feature_data_endcall_t;

typedef struct cc_kfact_t {
  char   rxstats[CC_KFACTOR_STAT_LEN];
  char   txstats[CC_KFACTOR_STAT_LEN];
} cc_kfact_t;

typedef struct cc_feature_data_hold_t_ {
    cc_msgbody_info_t msg_body;
    cc_call_info_t    call_info;
    cc_kfact_t        kfactor;
} cc_feature_data_hold_t;

typedef struct cc_feature_data_hold_reversion_t_ {
    int               alertInterval;
} cc_feature_data_hold_reversion_t;

typedef struct cc_feature_data_resume_t_ {
    cc_causes_t       cause;
    cc_msgbody_info_t msg_body;
    cc_call_info_t    call_info;
    cc_kfact_t        kfactor;
} cc_feature_data_resume_t;

typedef struct cc_feature_data_redirect_t_ {
    char          redirect_number[CC_MAX_DIALSTRING_LEN];
    cc_redirect_t redirect;
} cc_feature_data_redirect_t;

typedef struct cc_feature_data_subscribe_t_ {
    cc_subscriptions_t event_package;
    boolean            subscribe;
    char               subscribe_uri[CC_MAX_DIALSTRING_LEN];
    cc_srcs_t          component;
    int                component_id;
    int                *callBack;
} cc_feature_data_subscribe_t;

typedef enum cc_dialog_lock_e_ {
    CC_DIALOG_UNLOCKED,       /* unlocked                              */
    CC_DIALOG_LOCKED         /* local selected or locked               */
} cc_dialog_lock_e;

typedef struct cc_notify_data_config_t {
    boolean config_state;
} cc_notify_data_config_t;

typedef struct cc_notify_data_kpml_t {
    boolean kpml_state;
} cc_notify_data_kpml_t;


typedef struct cc_notify_data_rcc_t {
    cc_features_t     feature;
    cc_select_state_e select;
} cc_notify_data_rcc_t;


typedef union cc_notify_data_t {
    cc_notify_data_config_t config;
    cc_notify_data_kpml_t   kpml;
    cc_notify_data_rcc_t    rcc;
} cc_notify_data_t;

typedef struct cc_feature_data_notify_t_ {
    cc_subscriptions_t subscription;
    cc_xfer_methods_t method;
    cc_causes_t       cause;
    // The refer data above should have been in a cc_notify_data_refer_t
    int               cause_code;
    callid_t          blind_xferror_gsm_id;
    boolean           final;
    cc_notify_data_t  data;
} cc_feature_data_notify_t;

typedef struct cc_feature_data_update_t_ {
    cc_msgbody_info_t msg_body;
} cc_feature_data_update_t;

typedef struct cc_feature_data_b2bcnf_t_ {
    cc_causes_t cause;
    callid_t    call_id;
    callid_t    target_call_id;
    char        global_call_id[CC_GCID_LEN];
} cc_feature_data_b2bcnf_t;

typedef struct cc_feature_data_record_t_ {
    boolean subref_flag;
} cc_feature_data_record_t;

typedef struct cc_feature_data_select_t_ {
    boolean select;   /* TRUE when select, FALSE when unselect */
} cc_feature_data_select_t;

typedef struct cc_feature_data_b2b_join_t_ {
    callid_t    b2bjoin_callid;
    callid_t    b2bjoin_joincallid;
} cc_feature_data_b2b_join_t;


typedef struct cc_media_cap_t_ {
    cc_media_cap_name name;          /* media channel name designator     */
    sdp_media_e       type;          /* media type: audio, video etc      */
    boolean           enabled;       /* this media is enabled or disabled */
    boolean           support_security; /* security is supported          */
    sdp_direction_e   support_direction;/* supported direction            */
} cc_media_cap_t;

typedef struct cc_media_cap_table_t_ {
    uint32_t        id;
    cc_media_cap_t  cap[CC_MAX_MEDIA_CAP];/* capability table.             */
} cc_media_cap_table_t;

typedef struct cc_feature_data_generic_t {
    boolean subref_flag;
    uint32_t     eventid;
} cc_feature_data_generic_t;

typedef struct cc_feature_data_cnf_t_ {
    cc_causes_t cause;
    callid_t    call_id;
    callid_t    target_call_id;
} cc_feature_data_cnf_t;

typedef struct cc_feature_data_cancel_t_ {
    cc_rcc_skey_evt_type_e cause;
    callid_t    call_id;
    callid_t    target_call_id;
} cc_feature_data_cancel_t;

typedef union cc_feature_data_t {
    cc_feature_data_newcall_t   newcall;
    cc_feature_data_xfer_t      xfer;
    cc_feature_data_ind_t       indication;
    cc_feature_data_endcall_t   endcall;
    cc_feature_data_hold_t      hold;
    cc_feature_data_hold_reversion_t      hold_reversion;
    cc_feature_data_resume_t    resume;
    cc_feature_data_redirect_t  redirect;
    cc_feature_data_subscribe_t subscribe;
    cc_feature_data_notify_t    notify;
    cc_feature_data_update_t    update;
    cc_feature_data_b2bcnf_t    b2bconf;
    cc_feature_data_call_info_t call_info;
    cc_feature_data_record_t    record;
    cc_feature_data_select_t    select;
    cc_feature_data_b2b_join_t  b2bjoin;
    cc_feature_data_generic_t   generic;
    cc_feature_data_cnf_t       cnf;
    cc_feature_data_b2bcnf_t    cancel;
    cc_media_cap_t              caps;
} cc_feature_data_t;

typedef struct cc_setup_t_ {
    cc_msgs_t       msg_id;
    cc_srcs_t       src_id;
    callid_t        call_id;
    line_t          line;
    cc_alerting_type alert_info;
    vcm_ring_mode_t alerting_ring;
    vcm_tones_t     alerting_tone;
    cc_caller_id_t  caller_id;
    cc_redirect_t   redirect;
    cc_call_info_t  call_info;
    boolean         replaces;
    string_t        recv_info_list;
    cc_msgbody_info_t msg_body;
} cc_setup_t;

typedef struct cc_setup_ack_t_ {
    cc_msgs_t      msg_id;
    cc_srcs_t      src_id;
    callid_t       call_id;
    line_t         line;
    cc_caller_id_t caller_id;
    cc_msgbody_info_t    msg_body;
} cc_setup_ack_t;

typedef struct cc_proceeding_t_ {
    cc_msgs_t      msg_id;
    cc_srcs_t      src_id;
    callid_t       call_id;
    line_t         line;
    cc_caller_id_t caller_id;
} cc_proceeding_t;

typedef struct cc_alerting_t_ {
    cc_msgs_t      msg_id;
    cc_srcs_t      src_id;
    callid_t       call_id;
    line_t         line;
    cc_caller_id_t caller_id;
    cc_msgbody_info_t msg_body;
    boolean        inband;
} cc_alerting_t;

typedef struct cc_connected_t_ {
    cc_msgs_t      msg_id;
    cc_srcs_t      src_id;
    callid_t       call_id;
    line_t         line;
    cc_caller_id_t caller_id;
    string_t       recv_info_list;
    cc_msgbody_info_t msg_body;
} cc_connected_t;

typedef struct cc_connected_ack_t_ {
    cc_msgs_t      msg_id;
    cc_srcs_t      src_id;
    callid_t       call_id;
    line_t         line;
    cc_caller_id_t caller_id;
    cc_msgbody_info_t msg_body;
} cc_connected_ack_t;

typedef struct cc_release_t_ {
    cc_msgs_t   msg_id;
    cc_srcs_t   src_id;
    callid_t    call_id;
    line_t      line;
    cc_causes_t cause;
    char        dialstring[CC_MAX_DIALSTRING_LEN];
    cc_kfact_t  kfactor;
} cc_release_t;

typedef struct cc_release_complete_t_ {
    cc_msgs_t   msg_id;
    cc_srcs_t   src_id;
    callid_t    call_id;
    line_t      line;
    cc_causes_t cause;
    cc_kfact_t  kfactor;
} cc_release_complete_t;

typedef struct cc_feature_t_ {
    cc_msgs_t         msg_id;
    cc_srcs_t         src_id;
    callid_t          call_id;
    line_t            line;
    cc_features_t     feature_id;
    cc_feature_data_t data;
    boolean           data_valid;
} cc_feature_t;

typedef struct cc_feature_ack_t_ {
    cc_msgs_t         msg_id;
    cc_srcs_t         src_id;
    callid_t          call_id;
    line_t            line;
    cc_features_t     feature_id;
    cc_feature_data_t data;
    boolean           data_valid;
    cc_causes_t       cause;
} cc_feature_ack_t;

typedef struct cc_offhook_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t call_id;
    line_t   line;
    char     global_call_id[CC_GCID_LEN];
    callid_t prim_call_id;                /* For internal new call event
                                           * refer primary call's call_id
                                           */
    cc_hold_resume_reason_e hold_resume_reason; /* Reason for new consult call */
    monitor_mode_t monitor_mode;
    cfwdall_mode_t cfwdall_mode;
} cc_offhook_t;

typedef struct cc_onhook_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t call_id;
    line_t   line;
    boolean  softkey;
    callid_t prim_call_id;                /* For internal new call event
                                           * refer primary call's call_id
                                           */
    cc_hold_resume_reason_e hold_resume_reason; /* Reason for new consult call */
    cc_onhook_reason_e  active_list;                 /* onhook is because of active call
                                           * press
                                           */
} cc_onhook_t;

typedef struct cc_line_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t  call_id;
    line_t    line;
} cc_line_t;

typedef struct cc_digit_begin_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t  call_id;
    line_t    line;
    int       digit;
} cc_digit_begin_t;

typedef struct cc_digit_end_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t  call_id;
    line_t    line;
    int       digit;
} cc_digit_end_t;

typedef struct cc_dialstring_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t  call_id;
    line_t    line;
    char      dialstring[CC_MAX_DIALSTRING_LEN];
    char      g_call_id[CC_GCID_LEN];
    monitor_mode_t monitor_mode;
} cc_dialstring_t;

// mostly overlap with sessionTypes.h:cc_mwi_status_t
typedef struct cc_action_data_mwi_ {
    boolean on;
    int     type;
    int     newCount;
    int     oldCount;
    int     hpNewCount;
    int     hpOldCount;
} cc_action_data_mwi_t;

typedef struct cc_mwi_t_ {
    cc_msgs_t            msg_id;
    cc_srcs_t            src_id;
    callid_t             call_id;
    line_t               line;
    cc_action_data_mwi_t msgSummary;
} cc_mwi_t;

typedef struct cc_options_sdp_req_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t  call_id;
    line_t    line;
    void *    pMessage;
} cc_options_sdp_req_t;

typedef struct cc_options_sdp_ack_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t  call_id;
    line_t    line;
    void *    pMessage;
    cc_msgbody_info_t msg_body;
} cc_options_sdp_ack_t;

typedef struct cc_audit_sdp_req_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t call_id;
    line_t   line;
    boolean  apply_ringout;
} cc_audit_sdp_req_t;

typedef struct cc_audit_sdp_ack_t_ {
    cc_msgs_t msg_id;
    cc_srcs_t src_id;
    callid_t  call_id;
    line_t    line;
    cc_msgbody_info_t msg_body;
} cc_audit_sdp_ack_t;

typedef struct cc_feature_tmr_t_ {
    callid_t      call_id;
    line_t        line;
    cc_features_t feature_id;
} cc_feature_tmr_t;

typedef struct cc_regmgr_t_ {
    cc_msgs_t       msg_id;
    cc_srcs_t       src_id;
    int             rsp_type;
    cc_regmgr_rsp_e rsp_id;
    boolean         wait_flag;
} cc_regmgr_t;

// mostly overlap with sessionTypes.h:session_send_info_t
typedef struct cc_info_t {
    cc_msgs_t msg_id;
    cc_srcs_t not_used; // why not share a common struct??  why cast everything to cc_setup_t??
    callid_t  call_id;
    line_t    line;
    string_t  info_package;
    string_t  content_type;
    string_t  message_body;
} cc_info_t;

typedef struct cc_msg_t_ {
    union {
        cc_setup_t            setup;
        cc_setup_ack_t        setup_ack;
        cc_proceeding_t       proceeding;
        cc_alerting_t         alerting;
        cc_connected_t        connected;
        cc_connected_ack_t    connected_ack;
        cc_release_t          release;
        cc_release_complete_t release_complete;
        cc_feature_t          feature;
        cc_feature_ack_t      feature_ack;
        cc_offhook_t          offhook;
        cc_onhook_t           onhook;
        cc_line_t             line;
        cc_digit_begin_t      digit_begin;
        cc_digit_end_t        digit_end;
        cc_dialstring_t       dialstring;
        cc_mwi_t              mwi;
        cc_options_sdp_ack_t  options_ack;
        cc_audit_sdp_ack_t    audit_ack;
        cc_info_t             info;
    } msg;
} cc_msg_t;


callid_t    cc_get_new_call_id(void);
const char *cc_msg_name(cc_msgs_t id);
const char *cc_src_name(cc_srcs_t id);
const char *cc_cause_name(cc_causes_t id);
const char *cc_feature_name(cc_features_t id);

void cc_int_setup(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                  line_t line, cc_caller_id_t *caller_id,
                  cc_alerting_type alert_info, vcm_ring_mode_t alerting_ring,
                  vcm_tones_t alerting_tone, cc_redirect_t *redirect,
                  cc_call_info_t *call_info_p, boolean replaces,
                  string_t recv_info_list, cc_msgbody_info_t *msg_body);

void cc_int_setup_ack(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                      line_t line, cc_caller_id_t *caller_id,
                      cc_msgbody_info_t *msg_body);

void cc_int_proceeding(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                       line_t line, cc_caller_id_t *caller_id);

void cc_int_alerting(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                     line_t line, cc_caller_id_t *caller_id,
                     cc_msgbody_info_t *msg_body, boolean inband);

void cc_int_connected(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                      line_t line, cc_caller_id_t *caller_id,
                      string_t recv_info_list, cc_msgbody_info_t *msg_body);

void cc_int_connected_ack(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                          line_t line, cc_caller_id_t *caller_id,
                          cc_msgbody_info_t *msg_body);

void cc_int_release(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                    line_t line, cc_causes_t cause, const char *dialstring,
                    cc_kfact_t *kfactor);

void cc_int_release_complete(cc_srcs_t src_id, cc_srcs_t dst_id,
                             callid_t call_id, line_t line, cc_causes_t cause,
                             cc_kfact_t *kfactor);

void cc_int_feature(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                    line_t line, cc_features_t feature_id,
                    cc_feature_data_t *data);

void cc_int_feature_ack(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                        line_t line, cc_features_t feature_id,
                        cc_feature_data_t *data, cc_causes_t cause);

void cc_int_offhook(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t prim_call_id,
                    cc_hold_resume_reason_e consult_reason, callid_t call_id,
                    line_t line, char *global_call_id, 
                    monitor_mode_t monitor_mode,
                    cfwdall_mode_t cfwdall_mode);

void cc_int_onhook(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t prim_call_id,
                   cc_hold_resume_reason_e consult_reason, callid_t call_id,
                   line_t line, boolean softkey, cc_onhook_reason_e active_list);

void cc_int_line(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                 line_t line);

void cc_int_digit_begin(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                        line_t line, int digit);

void cc_int_digit_end(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                      line_t line, int digit);

void cc_int_dialstring(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                       line_t line, const char *dialstring,
                       const char *g_call_id, monitor_mode_t monitor_mode);

void cc_int_mwi(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                line_t line, boolean on, int type, int newCount,
                int oldCount, int hpNewCount, int hpOldCount);

void cc_int_options_sdp_req(cc_srcs_t src_id, cc_srcs_t dst_id,
                            callid_t call_id, line_t line, void *pMessage);

void cc_int_options_sdp_ack(cc_srcs_t src_id, cc_srcs_t dst_id,
                            callid_t call_id, line_t line, void *pMessage,
                            cc_msgbody_info_t *msg_body);

void cc_int_audit_sdp_req(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                          line_t line, boolean apply_ringout);

void cc_int_audit_sdp_ack(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                          line_t line, cc_msgbody_info_t *msg_body);

void cc_int_info(cc_srcs_t src_id, cc_srcs_t dst_id, callid_t call_id,
                      line_t line, string_t info_package, string_t content_type,
                      string_t message_body);

void cc_int_fail_fallback(cc_srcs_t src_id, cc_srcs_t dst_id, int rsp_type,
                          cc_regmgr_rsp_e rsp_id, boolean waited);
#define cc_fail_fallback_sip(a, b, c, d)     cc_int_fail_fallback(a, CC_SRC_SIP, b, c, d)
#define cc_fail_fallback_gsm(a, b, c)     cc_int_fail_fallback(a, CC_SRC_GSM, b, c, FALSE)

#define cc_setup(a, b, c, d, e, f, g, h, i, j, k, l) cc_int_setup(a, CC_SRC_GSM, b, c, d, e, f, g, h, i, j, k, l)
#define cc_setup_ack(a, b, c, d, e) \
        cc_int_setup_ack(a, CC_SRC_GSM, b, c, d, e)
#define cc_proceeding(a, b, c, d)     cc_int_proceeding(a, CC_SRC_GSM, b, c, d)
#define cc_alerting(a, b, c, d, e, f) \
        cc_int_alerting(a, CC_SRC_GSM, b, c, d, e, f)
#define cc_connected(a, b, c, d, e, f) \
        cc_int_connected(a, CC_SRC_GSM, b, c, d, e, f)
#define cc_connected_ack(a, b, c, d, e) \
        cc_int_connected_ack(a, CC_SRC_GSM, b, c, d, e)
#define cc_release(a, b, c, d, e, f)     cc_int_release(a, CC_SRC_GSM, b, c, d, e, f)
#define cc_release_complete(a, b, c, d, e) \
        cc_int_release_complete(a, CC_SRC_GSM, b, c, d, e)
#define cc_feature(a, b, c, d, e)     cc_int_feature(a, CC_SRC_GSM, b, c, d, e)
#define cc_feature_ack(a, b, c, d, e, f) \
        cc_int_feature_ack(a, CC_SRC_GSM, b, c, d, e, f)
#define cc_offhook(a, b, c)           cc_int_offhook(a, CC_SRC_GSM, CC_NO_CALL_ID, CC_REASON_NONE, b, c, NULL, CC_MONITOR_NONE,CFWDALL_NONE)
#define cc_offhook_ext(a, b, c, d, e) cc_int_offhook(a, CC_SRC_GSM, CC_NO_CALL_ID, CC_REASON_NONE, b, c, d, e,CFWDALL_NONE)
#define cc_onhook(a, b, c, d)         cc_int_onhook(a, CC_SRC_GSM, CC_NO_CALL_ID, CC_REASON_NONE, b, c, d, CC_REASON_NULL)
#define cc_onhook_ext(a, b, c, d, e)  cc_int_onhook(a, CC_SRC_GSM, CC_NO_CALL_ID, CC_REASON_NONE, b, c, d, e)
#define cc_line(a, b, c)              cc_int_line(a, CC_SRC_GSM, b, c)
#define cc_digit_begin(a, b, c, d)    cc_int_digit_begin(a, CC_SRC_GSM, b, c, d)
#define cc_dialstring(a, b, c, d)     cc_int_dialstring(a, CC_SRC_GSM, b, c, d, NULL, CC_MONITOR_NONE)
#define cc_dialstring_ext(a, b, c, d, e, f)     cc_int_dialstring(a, CC_SRC_GSM, b, c, d, e, f)
#define cc_mwi(a, b, c, d, e, f, g, h, i)            cc_int_mwi(a, CC_SRC_GSM, b, c, d, e, f, g, h, i )
#define cc_options_sdp_req(a, b, c, d)   cc_int_options_sdp_req(a, CC_SRC_GSM, b, c, d)
#define cc_audit_sdp_req(a, b, c, d)     cc_int_audit_sdp_req(a, CC_SRC_GSM, b, c, d)

typedef enum cc_types_t_ {
    CC_TYPE_INVALID,
    CC_TYPE_CCM,
    CC_TYPE_OTHER
} cc_types_t;

typedef enum cc_states_t_ {
    CC_STATE_MIN = -1,
    CC_STATE_OFFHOOK,
    CC_STATE_DIALING,
    CC_STATE_DIALING_COMPLETED,
    CC_STATE_CALL_SENT,
    CC_STATE_FAR_END_PROCEEDING,
    CC_STATE_FAR_END_ALERTING,
    CC_STATE_CALL_RECEIVED,
    CC_STATE_ALERTING,
    CC_STATE_ANSWERED,
    CC_STATE_CONNECTED,
    CC_STATE_HOLD,
    CC_STATE_RESUME,
    CC_STATE_ONHOOK,
    CC_STATE_CALL_FAILED,
    CC_STATE_HOLD_REVERT,
    CC_STATE_UNKNOWN,
    CC_STATE_MAX
} cc_states_t;

/* Update cc_action_names structure in lsm.c with the
 * corresponding change for the following structure.
 */
typedef enum cc_actions_t_ {
    CC_ACTION_MIN = -1,
    CC_ACTION_SPEAKER,
    CC_ACTION_DIAL_MODE,
    CC_ACTION_MWI,
    CC_ACTION_MWI_LAMP_ONLY,
    CC_ACTION_OPEN_RCV,
    CC_ACTION_UPDATE_UI,
    CC_ACTION_MEDIA,
    CC_ACTION_RINGER,
    CC_ACTION_SET_LINE_RINGER,
    CC_ACTION_PLAY_TONE,
    CC_ACTION_STOP_TONE,
    CC_ACTION_STOP_MEDIA,
    CC_ACTION_START_RCV,
    CC_ACTION_ANSWER_PENDING,
    CC_ACTION_PLAY_BLF_ALERTING_TONE,
    CC_ACTION_MAX
} cc_actions_t;

typedef enum cc_services_t_ {
    CC_SERVICE_MIN = -1,
    CC_SERVICE_MAX
} cc_services_t;

typedef enum cc_update_ui_actions_t_ {
    CC_UPDATE_MIN = -1,
    CC_UPDATE_CONF_ACTIVE,
    CC_UPDATE_CONF_RELEASE,
    CC_UPDATE_XFER_PRIMARY,
    CC_UPDATE_CALLER_INFO,
    CC_UPDATE_SECURITY_STATUS,
    CC_UPDATE_SET_CALL_STATUS,
    CC_UPDATE_CLEAR_CALL_STATUS,
    CC_UPDATE_SET_NOTIFICATION,
    CC_UPDATE_CLEAR_NOTIFICATION,
    CC_UPDATE_CALL_PRESERVATION,
    CC_UPDATE_CALL_CONNECTED,
    CC_UPDATE_MAX
} cc_update_ui_actions_t;

typedef struct cc_state_data_offhook_t_ {
    cc_caller_id_t caller_id;
    int dial_mode;
} cc_state_data_offhook_t;

/*
 * This structure is passed in CC_STATE_DIALING to lsm to indicate
 * wether a dialtone needs to be played or not
 * should the stutter dial tone be suppressed or not
 */
typedef struct cc_state_data_dialing_t_ {
    boolean play_dt;
    boolean suppress_stutter;
} cc_state_data_dialing_t;

typedef struct cc_state_data_dialing_completedt_ {
    cc_caller_id_t caller_id;
} cc_state_data_dialing_completed_t;

typedef struct cc_state_data_call_sent_t_ {
    cc_caller_id_t caller_id;
} cc_state_data_call_sent_t;

typedef struct cc_state_data_far_end_proceeding_t_ {
    cc_caller_id_t caller_id;
} cc_state_data_far_end_proceeding_t;

typedef struct cc_state_data_far_end_alerting_t_ {
    cc_caller_id_t caller_id;
} cc_state_data_far_end_alerting_t;

typedef struct cc_state_data_call_received_t_ {
    cc_caller_id_t caller_id;
} cc_state_data_call_received_t;

typedef struct cc_state_data_alerting_t_ {
    cc_caller_id_t caller_id;
} cc_state_data_alerting_t;

typedef struct cc_state_data_answered_t_ {
    cc_caller_id_t caller_id;
} cc_state_data_answered_t;

typedef struct cc_state_data_connected_t_ {
    cc_caller_id_t caller_id;
} cc_state_data_connected_t;

typedef struct cc_state_data_hold_t_ {
    cc_caller_id_t          caller_id;
    boolean                 local;
    cc_hold_resume_reason_e reason;
} cc_state_data_hold_t;

typedef struct cc_state_data_resume_t_ {
    cc_caller_id_t caller_id;
    boolean        local;
} cc_state_data_resume_t;

typedef struct cc_state_data_onhook_t_ {
    cc_caller_id_t caller_id;
    boolean        local;
    cc_causes_t    cause;
} cc_state_data_onhook_t;

typedef struct cc_state_data_call_failed_t_ {
    cc_caller_id_t caller_id;
    cc_causes_t    cause;
} cc_state_data_call_failed_t;

typedef union cc_state_data_t_ {
    cc_state_data_offhook_t            offhook;
    cc_state_data_dialing_t            dialing;
    cc_state_data_dialing_completed_t  dialing_completed;
    cc_state_data_call_sent_t          call_sent;
    cc_state_data_far_end_proceeding_t far_end_proceeding;
    cc_state_data_far_end_alerting_t   far_end_alerting;
    cc_state_data_call_received_t      call_received;
    cc_state_data_alerting_t           alerting;
    cc_state_data_answered_t           answered;
    cc_state_data_connected_t          connected;
    cc_state_data_hold_t               hold;
    cc_state_data_resume_t             resume;
    cc_state_data_onhook_t             onhook;
    cc_state_data_call_failed_t        call_failed;
} cc_state_data_t;

typedef struct cc_action_data_digit_begin_ {
    int tone;
} cc_action_data_digit_begin_t;

typedef struct cc_action_data_tone_ {
    vcm_tones_t tone;
} cc_action_data_tone_t;

typedef struct cc_action_data_speaker_ {
    boolean on;
} cc_action_data_speaker_t;

typedef struct cc_action_data_dial_mode_ {
    int mode;
    int digit_cnt;
} cc_action_data_dial_mode_t;

/*
typedef struct cc_action_data_mwi_ {
    boolean on;
    int32_t type;
    int32_t newCount;
    int32_t oldCount;
    int32_t hpNewCount;
    int32_t hpOldCount;
} cc_action_data_mwi_t;
*/

typedef struct cc_action_data_open_rcv_ {
    boolean  is_multicast;
    cpr_ip_addr_t listen_ip;
    uint16_t port;
    boolean  rcv_chan;
    boolean  keep;
    media_refid_t media_refid; /* the ID of the media to reference to */
    sdp_media_e media_type;
} cc_action_data_open_rcv_t;

typedef struct cc_set_call_status_data_ {
    char *phrase_str_p;
    int timeout;
    callid_t call_id;
    line_t line;
} cc_set_call_status_data_t;

typedef struct cc_clear_call_status_data_ {
    callid_t call_id;
    line_t line;
} cc_clear_call_status_data_t;

// mostly overlap with sessionTypes.h:cc_notification_data_t
typedef struct cc_set_notification_data_ {
    char *phrase_str_p;
    unsigned long timeout;
    unsigned long priority;
} cc_set_notification_data_t;

typedef union cc_update_ui_data_ {
    cc_feature_data_call_info_t caller_info;
    cc_set_call_status_data_t   set_call_status_parms;
    cc_clear_call_status_data_t clear_call_status_parms;
    cc_set_notification_data_t  set_notification_parms;
    string_t security_status;
} cc_update_ui_data_t;

typedef struct cc_action_data_update_ui_ {
    cc_update_ui_actions_t action;
    cc_update_ui_data_t    data;
} cc_action_data_update_ui_t;

typedef struct cc_action_data_ringer_ {
    boolean on;
} cc_action_data_ringer_t;

typedef struct cc_action_data_stop_media_ {
     media_refid_t  media_refid; /* the ID of the media to reference to */
} cc_action_data_stop_media_t;

typedef union cc_action_data_t_ {
    cc_action_data_digit_begin_t digit_begin;
    cc_action_data_tone_t      tone;
    cc_action_data_speaker_t   speaker;
    cc_action_data_dial_mode_t dial_mode;
    cc_action_data_mwi_t       mwi;
    cc_action_data_open_rcv_t  open_rcv;
    cc_action_data_update_ui_t update_ui;
    cc_action_data_ringer_t    ringer;
    cc_action_data_stop_media_t stop_media;
} cc_action_data_t;

typedef struct cc_service_data_get_facility_by_line_ {
    line_t line;
} cc_service_data_get_facility_by_line_t;

typedef union cc_service_data_t_ {
    cc_service_data_get_facility_by_line_t get_facility_by_line;
} cc_service_data_t;


typedef enum {
    MEDIA_INTERFACE_UPDATE_NOT_REQUIRED,
    MEDIA_INTERFACE_UPDATE_STARTED,
    MEDIA_INTERFACE_UPDATE_IN_PROCESS
} dock_undock_event_t;

extern dock_undock_event_t  g_dock_undock_event;

void cc_call_state(callid_t call_id, line_t line, cc_states_t state,
                   cc_state_data_t *data);
cc_rcs_t cc_call_action(callid_t call_id, line_t line, cc_actions_t action,
                    cc_action_data_t *data);
cc_rcs_t cc_call_service(callid_t call_id, line_t line, cc_services_t service,
                         cc_service_data_t *data);
void cc_call_attribute(callid_t call_id, line_t line, call_attr_t attr);
void cc_init(void);
void cc_free_msg_body_parts(cc_msgbody_info_t *msg_body);
void cc_free_msg_data(cc_msg_t *msg);
void cc_initialize_msg_body_parts_info(cc_msgbody_info_t *msg_body);
void cc_mv_msg_body_parts(cc_msgbody_info_t *dst_msg,
                          cc_msgbody_info_t *src_msg);
cc_rcs_t cc_cp_msg_body_parts(cc_msgbody_info_t *dst_msg,
                              cc_msgbody_info_t *src_msg);
void cc_mv_caller_id(cc_caller_id_t *dst_caller, cc_caller_id_t *src_caller);
  //function that will be invoked by modules external to gsm:

int cc_is_cnf_call(callid_t call_id);
cc_transfer_mode_e cc_is_xfr_call(callid_t call_id);

extern cprBuffer_t cc_get_msg_buf(int min_size);
extern const char *cc_feature_name(cc_features_t id);
extern const char *cc_msg_name(cc_msgs_t id);
extern const char *cc_cause_name(cc_causes_t id);
extern void cc_media_update_native_video_support(boolean val);
extern void cc_media_update_video_cap(boolean val);
extern void cc_media_update_native_video_txcap(boolean val);
extern cc_boolean cc_media_isTxCapEnabled();
extern cc_boolean cc_media_isVideoCapEnabled();
extern void cc_media_setVideoAutoTxPref(cc_boolean txPref);
extern cc_boolean cc_media_getVideoAutoTxPref();
#endif
