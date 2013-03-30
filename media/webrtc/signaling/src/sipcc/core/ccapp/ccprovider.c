/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>

#include "CCProvider.h"
#include "ccSession.h"
#include "ccsip_task.h"
#include "cpr.h"
#include "cpr_stdlib.h"
#include "cpr_ipc.h"
#include "phone.h"
#include "phntask.h"
#include "ccapi.h"
#include "debug.h"
#include "phone_debug.h"
#include "dialplanint.h"
#include "configmgr.h"
#include "prot_configmgr.h"
#include "lsm.h"
#include "sip_common_regmgr.h"
#include "sessionHash.h"
#include "pres_sub_not_handler.h"
#include "cc_call_listener.h"
#include "cc_service_listener.h"
#include "cc_config.h"
#include "cc_device_listener.h"
#include "cc_service_listener.h"
#include "plat_api.h"
#include "cc_info_listener.h"
#include "cc_blf_listener.h"
#include "platform_api.h"
#include "ccapp_task.h"
#include "ccapi.h"
#include "capability_set.h"
#include "plat_debug.h"

#include "config_api.h"
#include "ccapi_call_info.h"
#include "ccapi_call_listener.h"
#include "ccapi_snapshot.h"
#include "ccapi_device.h"
#include "ccapi_line_listener.h"
#include "ccapi_device_listener.h"
#include "cc_device_manager.h"
#include "call_logger.h"
#include "subscription_handler.h"
#include "ccapi_device_info.h"
#include "conf_roster.h"
#include "reset_api.h"
#include "prlog.h"
#include "prlock.h"
#include "prcvar.h"

/*---------------------------------------------------------
 *
 * Definitions
 *
 */
#define NOTIFY_CALL_STATUS (data->state == OFFHOOK? "OFFHOOK" : \
        data->state == ONHOOK? "ONHOOK" : \
        data->state == RINGOUT? "RINGOUT" : \
        data->state == RINGIN? "RINGIN" : \
        data->state == PROCEED ? "PROCEED" : \
        data->state == CONNECTED ? "CONNECTED" : \
        data->state == HOLD ? "HOLD" : \
        data->state == REMHOLD ? "REMHOLD" : \
        data->state == RESUME ? "RESUME" : \
        data->state == BUSY ? "BUSY" : \
        data->state == REORDER ? "REORDER" : \
        data->state == CONFERENCE ? "CONFERENCE" : \
        data->state == DIALING ? "DIALING" : \
        data->state == REMINUSE ? "REMINUSE" : \
        data->state == HOLDREVERT ? "HOLDREVERT" :\
        data->state == WHISPER ? "WHISPER" : \
        data->state == PRESERVATION ? "PRESERVATION" : \
        data->state == WAITINGFORDIGITS ? "WAITINGFORDIGITS" : \
        "Unknown state")

#define CCAPP_TASK_CMD_PRINT(arg) ((arg) == CCAPP_SERVICE_CMD ?  "CCAPP_SERVICE_CMD" : \
                (arg) == CCAPP_CREATE_SESSION ? "CCAPP_CREATE_SESSION" : \
                (arg) == CCAPP_CLOSE_SESSION ? "CCAPP_CLOSE_SESSION" : \
                (arg) == CCAPP_INVOKE_FEATURE ? "CCAPP_INVOKE_FEATURE" : \
                (arg) == CCAPP_SESSION_UPDATE ? "CCAPP_SESSION_UPDATE" : \
                (arg) == CCAPP_FEATURE_UPDATE ? "CCAPP_FEATURE_UPDATE" : \
                (arg) == CCAPP_UPDATELINES ? "CCAPP_UPDATELINES" : \
                (arg) == CCAPP_FAILOVER_IND ? "CCAPP_FAILOVER_IND" : \
                (arg) == CCAPP_FALLBACK_IND ? "CCAPP_FALLBACK_IND" : \
                (arg) == CCAPP_MODE_NOTIFY ? "CCAPP_MODE_NOTIFY" : \
                (arg) == CCAPP_SHUTDOWN_ACK ? "CCAPP_SHUTDOWN_ACK" : \
                (arg) == CCAPP_REG_ALL_FAIL ? "CCAPP_REG_ALL_FAIL" : \
                (arg) == CCAPP_INVOKEPROVIDER_FEATURE ?  "CCAPP_INVOKEPROVIDER_FEATURE" : \
                (arg) == CCAPP_SEND_INFO ?  "CCAPP_SEND_INFO" : \
                (arg) == CCAPP_RCVD_INFO ?  "CCAPP_RCVD_INFO" : \
                (arg) == CCAPP_LOGOUT_RESET ?  "CCAPP_LOGOUT_RESET" : \
                (arg) == CCAPP_SESSION_MGMT ?  "CCAPP_SESSION_MGMT" : \
                (arg) == CCAPP_THREAD_UNLOAD ?  "CCAPP_THREAD_UNLOAD" : \
                (arg) == CCAPP_SESSION_MGMT ?  "CCAPP_SESSION_MGMT" : \
                "Unknown Cmd")

#define APP_ERR_MSG err_msg

#define MAX_REASON_LENGTH      MAX_SIP_REASON_LENGTH
#define MAX_SESSION_PARAM_LEN  128

/* UNKNOWN_PHRASE_STR should equal the value returned
 *  if the phrase code is not found in the dictionary.
 *  For CIUS, this is found in LocalizedStrings.java */
#define UNKNOWN_PHRASE_STR "???"
#define UNKNOWN_PHRASE_STR_SIZE 3

/*---------------------------------------------------------
 *
 * Local Variables
 *
 */

/*---------------------------------------------------------
 *
 * Global Variables
 *
 */
static CCAppGlobal_t gCCApp;
cc_int32_t g_CCAppDebug=TRUE;
cc_int32_t g_CCLogDebug=TRUE;
cc_int32_t g_NotifyCallDebug=TRUE;
cc_int32_t g_NotifyLineDebug=TRUE;

cc_srv_ctrl_req_t reset_type;

extern cprMsgQueue_t ccapp_msgq;
extern cprThread_t ccapp_thread;
extern boolean platform_initialized;
extern void resetReady();
extern void resetNotReady();
extern int sendResetUpdates;

extern boolean apply_config;

extern  char g_new_signaling_ip[];

extern int configFileDownloadNeeded;
cc_action_t pending_action_type = NO_ACTION;
/*
 * Bug 845357. Guard to avoid races related to gCCApp.state
 */
static PRLock *gAppStateLock = NULL;
static PRCondVar *gAppStateCondVar = NULL;
static int canReadCCAppState = 0;

/*--------------------------------------------------------------------------
 * External function prototypes
 *--------------------------------------------------------------------------
 */
extern void send_protocol_config_msg(void);
extern void gsm_shutdown(void);
extern void sip_shutdown(void);
extern void dp_shutdown(void);
extern void MiscAppTaskShutdown(void);
extern void lsm_init(void);
extern void fsm_init(void);
extern void fim_init(void);
extern void SIPTaskPostRestart(boolean restart);
extern void ccsip_register_cancel(boolean cancel_reg, boolean backup_proxy);
extern void notify_register_update(int last_available_line);
extern void getLineIdAndCallId (line_t *line_id, callid_t *call_id);
extern void set_default_video_pref(int pref);
extern void set_next_sess_video_pref(int pref);
extern void cc_media_update_native_video_support(boolean val);
extern void cc_media_update_video_cap(boolean val);
extern void cc_media_update_video_txcap(boolean val);

session_data_t * getDeepCopyOfSessionData(session_data_t *data);
static void ccappUpdateSessionData(session_update_t *sessUpd);
static void ccappFeatureUpdated (feature_update_t *featUpd);
void destroy_ccapp_thread();
void ccpro_handleserviceControlNotify();
/* Sets up mutex needed for protecting state variables. */
static cc_int32_t InitInternal();
static void ccapp_set_state(cc_reg_state_t state);

/* Handy macro for executing memory barrier on CCApp State updates. */
#define ENTER_CCAPPSTATE_PROTECT \
  PR_Lock(gAppStateLock); \
  canReadCCAppState = 0;

#define EXIT_CCAPPSTATE_PROTECT \
  canReadCCAppState = 1; \
  PR_NotifyAllCondVar(gAppStateCondVar); \
  PR_Unlock(gAppStateLock);

/*---------------------------------------------------------
 *
 * Function declarations
 *
 */

cc_service_state_t mapProviderState(cc_reg_state_t state)
{
   switch(state) {
     case CC_INSERVICE:
        return CC_STATE_INS;
     case CC_CREATED_IDLE:
        return CC_STATE_IDLE;
     case CC_OOS_REGISTERING:
     case CC_OOS_FAILOVER:
     case CC_OOS_IDLE:
	 default:
        return CC_STATE_OOS;

   }
}

static void ccapp_hlapi_update_device_reg_state() {
        g_deviceInfo.ins_state = mapProviderState(gCCApp.state);
        g_deviceInfo.ins_cause = gCCApp.cause;
        g_deviceInfo.cucm_mode = gCCApp.mode;
	ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_STATE, CC_DEVICE_ID);
}

void ccpro_handleINS() {
      registration_processEvent(EV_CC_INSERVICE);
}

void ccpro_handleOOS() {
      switch(gCCApp.cause){
         case CC_CAUSE_REG_ALL_FAILED:
            registration_processEvent(EV_CC_OOS_REG_ALL_FAILED);
            break;
         case CC_CAUSE_FAILOVER:
            registration_processEvent(EV_CC_OOS_FAILOVER);
            break;
         case CC_CAUSE_FALLBACK:
            registration_processEvent(EV_CC_OOS_FALLBACK);
            break;
         case CC_CAUSE_SHUTDOWN:
            registration_processEvent(EV_CC_OOS_SHUTDOWN_ACK);
            break;
      }
}

cc_boolean is_action_to_be_deferred(cc_action_t action) {
    if (CCAPI_DeviceInfo_isPhoneIdle(CC_DEVICE_ID) == FALSE) {
        pending_action_type = action;
        DEF_DEBUG("Action deferred=%d", action);
        return TRUE;
    } else {
        return FALSE;
    }
}

void perform_deferred_action() {
    cc_action_t  temp_action = pending_action_type;

    if (is_action_to_be_deferred(pending_action_type) == TRUE) {
        return;
    }

    pending_action_type = NO_ACTION;
    DEF_DEBUG("Perform deferred action=%d", temp_action);

    if (temp_action == RESET_ACTION || temp_action == RESTART_ACTION) {
        ccpro_handleserviceControlNotify();
    } else if (temp_action == RE_REGISTER_ACTION) {
        CCAPI_Service_reregister(g_dev_hdl, g_dev_name, g_cfg_p, g_compl_cfg);
    } else if (temp_action == STOP_ACTION) {
        CCAPI_Service_stop();
    } else if (temp_action == DESTROY_ACTION) {
        CCAPI_Service_destroy();
    }
}

void ccpro_handleserviceControlNotify() {
    cc_action_t  temp_action  = NO_ACTION;
    if (reset_type == CC_DEVICE_RESET) {
        temp_action = RESET_ACTION;
    } else if (reset_type == CC_DEVICE_RESTART) {
        temp_action = RESTART_ACTION;
    }

    if ((reset_type != CC_DEVICE_ICMP_UNREACHABLE) &&
        is_action_to_be_deferred(temp_action) == TRUE) {
        return;
    }


    if (reset_type == CC_DEVICE_RESET) {
        resetRequest();
    } else if (reset_type == CC_DEVICE_RESTART) {
        registration_processEvent(EV_CC_DO_SOFT_RESET);
    }
}

/**
 * Initializes guard to protect gCCApp.state
 */

static cc_int32_t InitInternal() {
  gAppStateLock = PR_NewLock();
  if(!gAppStateLock) {
   return FALSE;
  }
  gAppStateCondVar = PR_NewCondVar(gAppStateLock);
  if(!gAppStateCondVar) {
    return FALSE;
  }
  return TRUE;
}

/**
 * Setter for the CCApp state
 */

static void ccapp_set_state(cc_reg_state_t state) {
  ENTER_CCAPPSTATE_PROTECT;
  gCCApp.state = state;
  EXIT_CCAPPSTATE_PROTECT;
}

/**
 * Returns the registration state
 */

cc_reg_state_t ccapp_get_state() {
   /* wait till the ongoing modification is completed */
  cc_reg_state_t state;
  if (gAppStateCondVar) {
    PR_Lock(gAppStateLock);
    while (!canReadCCAppState) {
      PR_WaitCondVar(gAppStateCondVar, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(gAppStateLock);
  }
  state = gCCApp.state;
  return state;
}

/**
 *
 * CCApp Provider init routine.
 *
 * @param   none
 *
 * @return  void
 *
 * @pre     None
 */
void CCAppInit()
{
  ccProvider_state_t srvcState;
  if(InitInternal() == FALSE) {
    return;
  }
  ccapp_set_state(CC_CREATED_IDLE);
  gCCApp.cause = CC_CAUSE_NONE;
  gCCApp.mode = CC_MODE_INVALID;
  gCCApp.cucm_mode = NONE_AVAIL;
  if (platThreadInit("CCApp_Task") != 0) {
    return;
  }

  /*
   * Adjust relative priority of CCApp thread.
   */
  (void) cprAdjustRelativeThreadPriority(CCPROVIDER_THREAD_RELATIVE_PRIORITY);

  debug_bind_keyword("cclog", &g_CCLogDebug);
  srvcState.cause = gCCApp.cause;  // XXX set but not used
  srvcState.mode = gCCApp.mode;  // XXX set but not used
  (void) srvcState;

  //Add listeners
  //CCProvider lister
  DEF_DEBUG(DEB_F_PREFIX"Add ccp listener: type%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAppInit"),
          CCAPP_CCPROVIER);

  addCcappListener((appListener *)ccp_handler, CCAPP_CCPROVIER);
}

/*
 *  Function:    processProviderEvent
 *
 *  Description: inform session events to CCApp
 *
 *  Parameters:
 *    call_id - call identifier
 *    line_id - line identifier
 *
 *  Returns:     none
 */
void
processProviderEvent (line_t line_id, unsigned int event, int data)
{
  static const char fname[] = "processProviderEvent";
  callid_t call_id = 0;

    DEF_DEBUG(DEB_F_PREFIX"line=%d event=%d data=%d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                line_id, event, data);
    switch(event) {
         case DEVICE_FEATURE_CFWD:
             getLineIdAndCallId(&line_id, &call_id);
             if (lsm_is_line_available(line_id, FALSE) == FALSE) {
                 //Send onhook to indicate session closure.
		 ccsnap_gen_callEvent(CCAPI_CALL_EV_STATE, CREATE_CALL_HANDLE(line_id, call_id));
                 lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);
                 break;
             }

             cc_feature(CC_SRC_UI, call_id, line_id, CC_FEATURE_CFWD_ALL, NULL);

             break;
         case DEVICE_SUPPORTS_NATIVE_VIDEO:
             cc_media_update_native_video_support(data);
             break;
         case DEVICE_ENABLE_VIDEO:
             cc_media_update_video_cap(data);
             break;
         case DEVICE_ENABLE_CAMERA:
             cc_media_update_native_video_txcap(data);
             break;
    }
}


/*
 *  Function:    CheckAndGetAvailableLine
 *
 *  Parameters:
 *    line - line identifier
 *    call - call identifier
 *
 *  Returns:  TRUE if a line is available.
 */
static boolean CheckAndGetAvailableLine(line_t *line, callid_t *call)
{
    /*
     * case 1: line>0 & call>0: invoking this in an offhook session.
     * case 2: line>0 & call=0: invoking this onhook with simple filter active
     * case 3: line=0 & call=0: invoking this in onhook with all calls filter active
     */
    if ((*line > 0) && (*call == 0)) {
        if (lsm_is_line_available(*line, FALSE) == FALSE) {
            // since the session is not created yet, no need to close the session.
            lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);
            return FALSE;
        }
    }
    else if ((*line == 0) && (*call == 0)) {
        *line = lsm_get_available_line(FALSE);
        if (*line == 0 ) {
            // since the session is not created yet, no need to close the session.
            lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);
            return FALSE;
        }
    }
    *call = (*call == 0) ? cc_get_new_call_id() : *call;
    return TRUE;
}

/*
 *  Function:    getVideoPref
 *
 *  Description: get video direction if specified
 *
 *  Parameters:
 *    data - urn param
 *
 *  Returns:     sdp_direction_e - requested video direction
 */
sdp_direction_e getVideoPref( string_t data)
{
    if ( data == NULL ) {
      return SDP_MAX_QOS_DIRECTIONS;
    }

    if ( strstr(data, "video=sendrecv")) {
         return SDP_DIRECTION_SENDRECV;
    } else if (strstr(data, "video=sendonly")) {
         return SDP_DIRECTION_SENDONLY;
    } else if (strstr(data, "video=recvonly")){
         return SDP_DIRECTION_RECVONLY;
    } else if (strstr(data, "video=none")){
         return SDP_DIRECTION_INACTIVE;
    } else {
         return SDP_MAX_QOS_DIRECTIONS;
    }
}

/*
 *  Function: updateSessionVideoPref
 *
 *  Description: update video direction for the session only
 *
 *  Parameters:
 *    line_id - line at which the session needs the video pref updated
 *    call_id - callid for which the session needs the video pref updated
 *    dir - video pref
 *
 *  Returns:
 */
static void updateSessionVideoPref( line_t line_id, callid_t call_id, sdp_direction_e video_pref)
{
    static const char fname[] = "updateSessionVideoPref";
    cc_feature_data_t featdata;

    DEF_DEBUG(DEB_L_C_F_PREFIX"updating to %d video capapbility %s",
            DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, call_id, line_id, fname), video_pref,
            (video_pref == SDP_DIRECTION_INACTIVE ? "disabled" : "enabled"));
    featdata.caps.support_direction = video_pref;
    cc_feature(CC_SRC_UI, call_id, line_id, CC_FEATURE_UPD_SESSION_MEDIA_CAP, &featdata);
}

/*
 *  Function:    updateVideoPref
 *
 *  Description: update video direction if specified
 *
 *  Parameters:
 *    line_id - line at which the session needs the video pref updated
 *    call_id - callid for which the session needs the video pref updated
 *    data - urn param
 *
 *  Returns:
 */

static void updateVideoPref( unsigned int event, line_t line_id, callid_t call_id, sdp_direction_e video_pref/*string_t data*/)
{
    static const char fname[] = "updateVideoPref";

    if ( video_pref == SDP_MAX_QOS_DIRECTIONS ) {
        // we are done no updates needed
        return;
    }

    switch (event) {
        case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
            if ( call_id == CC_NO_CALL_ID ) {
                // This is the only means to update the default behavior
                set_default_video_pref(video_pref);
            } else {
                updateSessionVideoPref(line_id, call_id, video_pref);
            }
            break;
        case CC_FEATURE_DIALSTR:
        case CC_FEATURE_DIAL:
        case CC_FEATURE_SPEEDDIAL:
        case CC_FEATURE_REDIAL:
            if ( call_id == CC_NO_CALL_ID ) {
                set_next_sess_video_pref(video_pref);
            } else {
                updateSessionVideoPref(line_id, call_id, video_pref);
            }
            break;
        case CC_FEATURE_NEW_CALL:
        case CC_FEATURE_OFFHOOK:
        case CC_FEATURE_CONF:
        case CC_FEATURE_B2BCONF:
        case CC_FEATURE_XFER:
            set_next_sess_video_pref(video_pref);
            break;
        case CC_FEATURE_RESUME:
        case CC_FEATURE_ANSWER:
            updateSessionVideoPref(line_id, call_id, video_pref);
            break;
        default:
            CCAPP_DEBUG(DEB_L_C_F_PREFIX"event=%d. update to %d not supported",
                DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, call_id, line_id, fname), event, video_pref);

    }
}

/*
 *  Function:    getDigits
 *
 *  Description: returns the first param before the ;
 *
 *  Parameters:
 *    data - urn param
 *    digits - memory to return the first param
 *  Returns:
 */
void getDigits(string_t data, char *digits) {
   char *endptr;
   int len=0;

   digits[0]=0;

   if ( data !=NULL ) {
       endptr = strchr(data,';');
       if ( endptr ) {
           len = endptr - data;
       } else {
           len = strlen(data);
       }

       if ( len) {
           memcpy(digits, data, len);
           digits[len] = 0;
       }
    }
}

/*
 *  Function:    processSessionEvent
 *
 *  Description: inform session events to CCApp
 *
 *  Parameters:
 *    call_id - call identifier
 *    line_id - line identifier
 *
 *  Returns:     none
 */
void
processSessionEvent (line_t line_id, callid_t call_id, unsigned int event, sdp_direction_e video_pref, ccSession_feature_t ccData)
{
    static const char fname[] = "processSessionEvent";
    cc_feature_data_t featdata;
    int instance = line_id;
    boolean incoming = FALSE;
    session_data_t * sess_data_p;
    char digits[CC_MAX_DIALSTRING_LEN];
    char* data = (char*)ccData.info;
    char* data1 =(char*)ccData.info1;
    long strtol_result;
    char *strtol_end;

    CCAPP_DEBUG(DEB_L_C_F_PREFIX"event=%d data=%s",
                DEB_L_C_F_PREFIX_ARGS(SIP_CC_PROV, call_id, line_id, fname), event,
                ((event == CC_FEATURE_KEYPRESS) ? "..." : data));

    memset(&featdata, 0, sizeof(cc_feature_data_t));
    updateVideoPref(event, line_id, call_id, video_pref);
    switch(event) {
         case CC_FEATURE_ONHOOK:
             getLineIdAndCallId(&line_id, &call_id);
             CCAPP_DEBUG(DEB_F_PREFIX"CC_FEATURE_ONHOOK = %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data);
             cc_onhook_ext(CC_SRC_UI, call_id, line_id, FALSE,
                         (data && (strncasecmp(data, "ACTIVECALLS", sizeof("ACTIVECALLS")) == 0))?
                             CC_REASON_ACTIVECALL_LIST: CC_REASON_NULL );
	     break;
         case CC_FEATURE_KEYPRESS:
             dp_int_update_keypress(line_id, call_id, (unsigned char)*data);
             break;
         case CC_FEATURE_BKSPACE:
             dp_int_update_keypress(line_id, call_id, BKSP_KEY);
             break;
         case CC_FEATURE_CREATEOFFER:
             featdata.session.constraints = ccData.constraints;
             cc_createoffer (CC_SRC_UI, CC_SRC_GSM, call_id, (line_t)instance, CC_FEATURE_CREATEOFFER, &featdata);
             break;
         case CC_FEATURE_CREATEANSWER:
             featdata.session.constraints = ccData.constraints;
             cc_createanswer (CC_SRC_UI, CC_SRC_GSM, call_id, (line_t)instance, CC_FEATURE_CREATEANSWER, data, &featdata);
             break;
         case CC_FEATURE_SETLOCALDESC:
             cc_setlocaldesc (CC_SRC_UI, CC_SRC_GSM, call_id, (line_t)instance, CC_FEATURE_SETLOCALDESC, ccData.action, data, &featdata);
             break;
         case CC_FEATURE_SETREMOTEDESC:
             cc_setremotedesc (CC_SRC_UI, CC_SRC_GSM, call_id, (line_t)instance, CC_FEATURE_SETREMOTEDESC, ccData.action, data, &featdata);
             break;
         case CC_FEATURE_SETPEERCONNECTION:
           PR_ASSERT(strlen(data) < PC_HANDLE_SIZE);
           if (strlen(data) >= PC_HANDLE_SIZE)
             return;

           sstrncpy(featdata.pc.pc_handle, data, sizeof(featdata.pc.pc_handle));

           cc_int_feature2(CC_MSG_SETPEERCONNECTION, CC_SRC_UI, CC_SRC_GSM,
             call_id, (line_t)instance,
             CC_FEATURE_SETPEERCONNECTION, &featdata);
           break;
         case CC_FEATURE_ADDSTREAM:
           featdata.track.stream_id = ccData.stream_id;
           featdata.track.track_id = ccData.track_id;
           featdata.track.media_type = ccData.media_type;
           cc_int_feature2(CC_MSG_ADDSTREAM, CC_SRC_UI, CC_SRC_GSM, call_id, (line_t)instance, CC_FEATURE_ADDSTREAM, &featdata);
           break;
         case CC_FEATURE_REMOVESTREAM:
           featdata.track.stream_id = ccData.stream_id;
           featdata.track.track_id = ccData.track_id;
           featdata.track.media_type = ccData.media_type;
           cc_int_feature2(CC_MSG_REMOVESTREAM, CC_SRC_UI, CC_SRC_GSM, call_id, (line_t)instance, CC_FEATURE_REMOVESTREAM, &featdata);
           break;
         case CC_FEATURE_ADDICECANDIDATE:
           featdata.candidate.level = ccData.level;
           sstrncpy(featdata.candidate.candidate, data, sizeof(featdata.candidate.candidate)-1);
           sstrncpy(featdata.candidate.mid, data1, sizeof(featdata.candidate.mid)-1);
           cc_int_feature2(CC_MSG_ADDCANDIDATE, CC_SRC_UI, CC_SRC_GSM, call_id, (line_t)instance, CC_FEATURE_ADDICECANDIDATE, &featdata);
           break;
         case CC_FEATURE_DIALSTR:
             if (CheckAndGetAvailableLine(&line_id, &call_id) == TRUE) {
                 getDigits(data, digits);
                 if (strlen(digits) == 0) {
                    //if dial string is empty then go offhook
                    cc_offhook(CC_SRC_UI, call_id, line_id);
                 } else {
                 dp_int_init_dialing_data(line_id, call_id);
                 dp_int_dial_immediate(line_id, call_id, TRUE,
                                       (char *)digits, NULL, CC_MONITOR_NONE);
                 }
             }
             break;

         case CC_FEATURE_DIAL:
             dp_int_dial_immediate(line_id, call_id, FALSE, NULL, NULL, CC_MONITOR_NONE);
             break;
         case CC_FEATURE_SPEEDDIAL:
             /*
              * BLF Pickup Line Selection:
              * -------------------------
              *
              * case 1: line_id>0 & call_id>0 : BLF key press after going offhook on a given line.
              * Check if the given line has available call capacity. If not, end the session
              * and display STR_INDEX_NO_LINE_FOR_PICKUP toast.
              *
              * case 2: line_id>0 & call_id=0 : BLF key press w/o offhook when a simple filter is active.
              * Check if the given line has available call capacity. If not, find the next available line.
              * If no available line is found, display STR_INDEX_NO_LINE_FOR_PICKUP toast.
              *
              * case 3: line_id=0 & call_id=0 : BLF key press w/o offhook when a AllCalls filter is active.
              * Find an available line. If no available line is found,
              * display STR_INDEX_NO_LINE_FOR_PICKUP toast.
              */
             if (strncmp(data, CISCO_BLFPICKUP_STRING, (sizeof(CISCO_BLFPICKUP_STRING) - 1)) == 0) {
                 if ((instance > 0) && (call_id > 0)) {
                     if (lsm_is_line_available(instance, incoming) == FALSE) {
                         session_update_t sessUpd;
                         sessUpd.eventID = CALL_STATE;
                         sessUpd.sessionID = createSessionId(line_id, call_id);
                         sessUpd.sessType = SESSIONTYPE_CALLCONTROL;
                         sessUpd.update.ccSessionUpd.data.state_data.state = evOnHook;
                         sessUpd.update.ccSessionUpd.data.state_data.line_id = line_id;
                         sessUpd.update.ccSessionUpd.data.state_data.cause = CC_CAUSE_NORMAL;
                         ccappUpdateSessionData(&sessUpd);
                         // Pass this update to Session Manager
                         lsm_ui_display_notify_str_index(STR_INDEX_NO_LINE_FOR_PICKUP);
                         break;
                     }
                 }
                 else if ((instance > 0) && (call_id == 0)) {
                     if (lsm_is_line_available(instance, incoming) == FALSE) {
                         line_id = lsm_get_available_line(incoming);
                         if (line_id == 0 ) {
                             // since the session is not created yet, no need to close the session.
                             lsm_ui_display_notify_str_index(STR_INDEX_NO_LINE_FOR_PICKUP);
                             break;
                         }
                     }
                 }
                 else if ((instance == 0) && (call_id == 0)) {
                     line_id = lsm_get_available_line(incoming);
                     if (line_id == 0 ) {
                         // since the session is not created yet, no need to close the session.
                         lsm_ui_display_notify_str_index(STR_INDEX_NO_LINE_FOR_PICKUP);
                         break;
                      }
                 }
                 call_id = (call_id == 0) ? cc_get_new_call_id() : call_id;
             }
             /*
              * SpeedDial Line Selection:
              * ------------------------
              *
              * case 1: line_id>0 & call_id>0 : speeddial key press after going offhook on a given line.
              * Simply use this line to make an outgoing call.
              *
              * case 2: line_id>0 & call_id=0 : SD key press w/o offhook when a simple filter is active.
              * Check if the given line has available call capacity. If not,
              * display STR_INDEX_ERROR_PASS_LIMIT toast.
              *
              * case 3: line_id=0 & call_id=0 : SD key press w/o offhook when a AllCalls filter is active.
              * Find an available line. If no available line is found,
              * display STR_INDEX_ERROR_PASS_LIMIT toast.
              */
             else if (CheckAndGetAvailableLine(&line_id, &call_id) == FALSE) {
                 break;
             }

             getDigits(data,digits);

             dp_int_init_dialing_data(line_id, call_id);
             dp_int_dial_immediate(line_id, call_id, TRUE,
                                   digits, NULL, CC_MONITOR_NONE);
             break;
         case CC_FEATURE_BLIND_XFER_WITH_DIALSTRING:
             featdata.xfer.cause = CC_CAUSE_XFER_LOCAL_WITH_DIALSTRING;
             sstrncpy(featdata.xfer.dialstring, (char *)data, CC_MAX_DIALSTRING_LEN);
             cc_feature(CC_SRC_UI, call_id, (line_t)instance, CC_FEATURE_BLIND_XFER,
                        &featdata);
        break;
         case CC_FEATURE_REDIAL:
             if (line_id == 0) {
                 /*
                  * If line_id is zero, get the last dialed line.
                  */
                 line_id = dp_get_redial_line();
                 if (line_id == 0) {
                     break;
                 }
             }

             /* if not an existing call, check capacity on the line */
             if (call_id == CC_NO_CALL_ID) {
                 if (lsm_is_line_available(line_id, FALSE) == FALSE) {
                     /*
                      * since the session is not created yet, no
                      * need to close the session.
                      */
                     lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);
                     break;
                     } else {
                         /* we have capacity and call does not exist */
                     call_id = cc_get_new_call_id();
                 }
             }
             dp_int_do_redial(line_id, call_id);
             break;

         case CC_FEATURE_UPD_SESSION_MEDIA_CAP:
             /* handled in updateVideoPref() just outside of the switch */
             break;

         case CC_FEATURE_NEW_CALL:
         case CC_FEATURE_OFFHOOK:
             if (lsm_is_line_available(line_id, FALSE) == FALSE) {
                 //close the session
                 CCAPP_DEBUG(DEB_F_PREFIX"CC_FEATURE_OFFHOOK: no available line.\n",
                                             DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
		 ccsnap_gen_callEvent(CCAPI_CALL_EV_STATE, CREATE_CALL_HANDLE(line_id, call_id));
                 lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);
                 break;
             }

             if (call_id == 0) {
               call_id = cc_get_new_call_id();
             } else {
               if (event == CC_FEATURE_NEW_CALL) {
                   CCAPP_DEBUG(DEB_F_PREFIX"CC_FEATURE_NEW_CALL called with callid=%d\n",
                           DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), call_id);
               } else {
                   CCAPP_DEBUG(DEB_F_PREFIX"CC_FEATURE_OFFHOOK called with callid=%d\n",
                           DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), call_id);
               }
             }
             cc_offhook(CC_SRC_UI, call_id, line_id);
             break;

         case CC_FEATURE_CFWD_ALL:
             getLineIdAndCallId(&line_id, &call_id);

             cc_feature(CC_SRC_UI, call_id, line_id, CC_FEATURE_CFWD_ALL, NULL);

             break;

         case CC_FEATURE_RESUME:
         case CC_FEATURE_ANSWER:

             sess_data_p = (session_data_t *)findhash(createSessionId(line_id, call_id));
             if ( sess_data_p == NULL ) {
               break;
             }
             if (sess_data_p->state == HOLD || sess_data_p->state == HOLDREVERT) {
                 cc_feature(CC_SRC_UI, call_id, line_id, CC_FEATURE_RESUME, NULL);
             } else {
                 cc_feature(CC_SRC_UI, call_id, line_id, event, NULL);
             }
             break;

         case CC_FEATURE_END_CALL:
		     CCAPP_DEBUG(DEB_F_PREFIX"CC_FEATURE_ONHOOK = %s\n",
				 DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data);
		     if (data && (strcmp(data, "ACTIVECALLS") == 0)) {
		         getLineIdAndCallId(&line_id, &call_id);
		         cc_onhook_ext(CC_SRC_UI, call_id, line_id, TRUE, CC_REASON_ACTIVECALL_LIST);
		     } else {
		         cc_feature(CC_SRC_UI, call_id, line_id, event, NULL);
		     }
		     break;

	 case CC_FEATURE_CONF:
		     // for CUCM mode we will translate CONF to B2BCONF
		     if ( gCCApp.mode == CC_MODE_CCM ) {
                 if ( gCCApp.mode == CC_MODE_CCM && platGetFeatureAllowed(CC_SIS_B2B_CONF) ) {
                     //CUCME can't support B2BCONF currently.
		         event = CC_FEATURE_B2BCONF;
  	         }
		     }// DON'T ADD BREAK HERE. EVENT IS PASSED BELOW
	 case CC_FEATURE_B2BCONF:
	 case CC_FEATURE_XFER:
		     getDigits(data,digits);
		     if ( strlen(digits)) {
			cc_feature_data_t ftr_data;
			CCAPP_DEBUG(DEB_F_PREFIX"conf: sid=%s.\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),data);
			// if data is received we need to extract the call id
			// and send target_call_id in feature_data
			// Post the event on the original call ( received call id as string in data).
			if ( event == CC_FEATURE_B2BCONF ) {
			    ftr_data.b2bconf.target_call_id = call_id;
			} else if ( event == CC_FEATURE_XFER ) {
			    ftr_data.xfer.target_call_id = call_id;
			} else if ( event == CC_FEATURE_CONF) {
                            //Legacy local conference.
			    ftr_data.cnf.target_call_id = call_id;
			}

			errno = 0;
			strtol_result = strtol(digits, &strtol_end, 10);

			if (errno || digits == strtol_end || strtol_result < INT_MIN || strtol_result > INT_MAX) {
				CCAPP_ERROR(DEB_F_PREFIX"digits parse error %s.\n",DEB_F_PREFIX_ARGS(SIP_CC_PROV, __FUNCTION__), digits);
			} else {
				cc_feature(CC_SRC_UI, GET_CALLID((int) strtol_result), line_id, event, &ftr_data);
			}
			break;
		     } else {
			CCAPP_DEBUG(DEB_F_PREFIX"conf: no sid.\n",DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
             	     }// DON'T ADD BREAK HERE. EVENT IS PASSED BELOW

         case CC_FEATURE_B2B_JOIN:
         case CC_FEATURE_DIRTRXFR:
         case CC_FEATURE_SELECT:
         case CC_FEATURE_CANCEL:
             cc_feature(CC_SRC_UI, call_id, line_id, event, NULL);
             break;

         case CC_FEATURE_HOLD:
            if (data && ((strcmp(data, "TRANSFER") == 0) ||
                (strcmp(data, "CONFERENCE") == 0) || (strcmp(data, "SWAP") == 0))) {
                featdata.hold.call_info.data.hold_resume_reason = CC_REASON_SWAP;
            } else {

                featdata.hold.call_info.data.hold_resume_reason = CC_REASON_NONE;
            }
            featdata.hold.call_info.type = CC_FEAT_HOLD;
            featdata.hold.msg_body.num_parts = 0;
            cc_feature(CC_SRC_UI, call_id, line_id, event, &featdata);
            break;

         default:
             break;
    }
}

void CCAppShutdown()
{
}


static char * ccapp_cmd_to_str(unsigned int cmd) {
   switch (cmd) {
     case CMD_INSERVICE:
         return "CMD_INSERVICE";
     case CMD_SHUTDOWN:
         return "CMD_SHUTDOWN";
     case CMD_RESTART:
         return "CMD_RESTART";
     case CMD_UNREGISTER_ALL_LINES:
         return "CMD_UNREGISTER_ALL_LINES";
     case CMD_REGISTER_ALL_LINES:
         return "CMD_REGISTER_ALL_LINES";
     default:
         return "CMD_UNKNOWN";
   }
}
/**
 *
 * CC Provider process service Cmds
 *
 * @param   cmd - Command
 * @param   reason - reason
 * @param   reasonStr - reason string
 *
 * @return  void
 *
 * @pre     None
 */
void CCApp_processCmds(unsigned int cmd, unsigned int reason, string_t reasonStr)
{
    static const char fname[] = "CCApp_processCmds";
    CCAPP_DEBUG(DEB_F_PREFIX" Received Cmd %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV,
           fname), ccapp_cmd_to_str(cmd));
       switch (cmd) {
         case CMD_INSERVICE:
            ccsnap_device_init();
            ccsnap_line_init();
            ccapp_set_state(CC_OOS_REGISTERING);
            send_protocol_config_msg();
            break;
         case CMD_SHUTDOWN:
         case CMD_UNREGISTER_ALL_LINES:
            /* send a shutdown message to the SIP Task */
            SIPTaskPostShutdown(SIP_EXTERNAL, reason, reasonStr);
            break;
         case CMD_RESTART:
            SIPTaskPostRestart(TRUE);
            break;
         case CMD_BLF_INIT:
            pres_sub_handler_initialized();
            break;
         default:
            APP_ERR_MSG("CCApp_processCmds: Error: Unknown message %d\n", cmd);
            break;
       }
}

boolean isNoCallExist()
{
  hashItr_t itr;

  hashItrInit(&itr);

  return (hashItrNext(&itr)?FALSE:TRUE);
}

/**
 * Make deep copy of session_data_t
 */
session_data_t * getDeepCopyOfSessionData(session_data_t *data)
{
   session_data_t *newData = (session_data_t *) cpr_malloc(sizeof(session_data_t));

   if ( newData != NULL ) {
       memset(newData, 0, sizeof(session_data_t));

       if ( data != NULL ) {
           *newData = *data;
           newData->ref_count = 1;
           newData->clg_name =  strlib_copy(data->clg_name);
           newData->clg_number =  strlib_copy(data->clg_number);
           newData->cld_name =  strlib_copy(data->cld_name);
           newData->cld_number =  strlib_copy(data->cld_number);
           newData->alt_number =  strlib_copy(data->alt_number);
           newData->orig_called_name =  strlib_copy(data->orig_called_name);
           newData->orig_called_number =  strlib_copy(data->orig_called_number);
           newData->last_redir_name =  strlib_copy(data->last_redir_name);
           newData->last_redir_number =  strlib_copy(data->last_redir_number);
           newData->plcd_name =  strlib_copy(data->plcd_name);
           newData->plcd_number =  strlib_copy(data->plcd_number);
           newData->status =  strlib_copy(data->status);
           newData->sdp = strlib_copy(data->sdp);
           calllogger_copy_call_log(&newData->call_log, &data->call_log);
       } else {
           newData->ref_count = 1;
           newData->state = ONHOOK;
           newData->security = CC_SECURITY_NONE;
           newData->policy = CC_POLICY_NONE;
           newData->clg_name =  strlib_empty();
           newData->clg_number = strlib_empty();
           newData->cld_name =  strlib_empty();
           newData->cld_number = strlib_empty();
           newData->alt_number = strlib_empty();
           newData->orig_called_name = strlib_empty();
           newData->orig_called_number = strlib_empty();
           newData->last_redir_name = strlib_empty();
           newData->last_redir_number = strlib_empty();
           newData->plcd_name =  strlib_empty();
           newData->plcd_number =  strlib_empty();
           newData->status = strlib_empty();
           newData->sdp = strlib_empty();
           calllogger_init_call_log(&newData->call_log);
       }

   }
   return newData;
}


/**
 *
 * CCApp Provider search for session and deepfree data
 *
 * @return  ptr to session data
 *
 * @pre     None
 */

void cleanSessionData(session_data_t *data)
{
   if ( data != NULL ) {
	strlib_free(data->clg_name);
        data->clg_name = strlib_empty();
	strlib_free(data->clg_number);
        data->clg_number = strlib_empty();
	strlib_free(data->alt_number);
        data->alt_number = strlib_empty();
	strlib_free(data->cld_name);
        data->cld_name = strlib_empty();
	strlib_free(data->cld_number);
        data->cld_number = strlib_empty();
	strlib_free(data->orig_called_name);
        data->orig_called_name = strlib_empty();
	strlib_free(data->orig_called_number);
        data->orig_called_number = strlib_empty();
	strlib_free(data->last_redir_name);
        data->last_redir_name = strlib_empty();
	strlib_free(data->last_redir_number);
        data->last_redir_number = strlib_empty();
	strlib_free(data->plcd_name);
        data->plcd_name = strlib_empty();
	strlib_free(data->plcd_number);
        data->plcd_number = strlib_empty();
	strlib_free(data->status);
        data->status = strlib_empty();
	strlib_free(data->sdp);
        data->sdp = strlib_empty();
        calllogger_free_call_log(&data->call_log);
    }
}

/**
 *
 * CCApp Provider check for CONNECTED calls for preservation
 *
 * @return  boolean - do we need to move in call preservation phase
 *
 * @pre     None
 */

boolean ccappPreserveCall()
{
    static const char fname[] = "ccappPreserveCall";
    hashItr_t itr;
    session_data_t *data;
    boolean retVal = FALSE;

    CCAPP_DEBUG(DEB_F_PREFIX"called\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
    hashItrInit(&itr);
    while ( (data = (session_data_t*)hashItrNext(&itr)) != NULL ) {
      if ( data->state == CONNECTED || data->state == PRESERVATION ) {
	// need to wait for this call to end.
        CCAPP_DEBUG(DEB_F_PREFIX"inPreservation = true\n",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
        gCCApp.inPreservation = TRUE;
        gCCApp.preservID = data->sess_id;
        capset_get_allowed_features(gCCApp.mode, PRESERVATION, data->allowed_features);
	ccsnap_gen_callEvent(CCAPI_CALL_EV_PRESERVATION, CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id));
        retVal = TRUE;
      } else {
        // End this call now
        CCAPP_DEBUG(DEB_F_PREFIX"ending call %x\n",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),  data->sess_id);
        /*
         * Note that for calls in state such as RIU, HOLD etc.
         * we must send ON hook event (cc_onhook) instead of
         * CC_FEATURE_END_CALL because corrsponding state machines
         * handle only onhook event to clean up calls.
         */
        cc_onhook(CC_SRC_UI, GET_CALLID(data->sess_id),
                  GET_LINEID(data->sess_id), TRUE);

      }
    }

    return retVal;
}

/**
 *
 * CCApp Provider check if its safe to proceed with failover/fallback
 *
 * @return  void - do we need to move in call preservation phase
 *
 * @pre     None
 */

void proceedWithFOFB()
{
  static const char fname[] = "proceedWithFOFB";

  CCAPP_DEBUG(DEB_F_PREFIX"called. preservation=%d in cucm mode=%s \n",
        DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
        gCCApp.inPreservation,
        gCCApp.cucm_mode == FAILOVER ? "FAILOVER":
        gCCApp.cucm_mode == FALLBACK ? "FALLBACK":
        gCCApp.cucm_mode == NO_CUCM_SRST_AVAILABLE ?
            "NO_CUCM_SRST_AVAILABLE": "NONE");
        ccapp_set_state(CC_OOS_REGISTERING);

  switch(gCCApp.cucm_mode)
  {
    case FAILOVER:
      cc_fail_fallback_sip(CC_SRC_UI, RSP_START, CC_REG_FAILOVER_RSP, TRUE);
      gCCApp.cause = CC_CAUSE_FAILOVER;
    break;

    case FALLBACK:
      cc_fail_fallback_sip(CC_SRC_UI, RSP_START, CC_REG_FALLBACK_RSP, TRUE);
      gCCApp.cause = CC_CAUSE_FALLBACK;
    break;

    case NO_CUCM_SRST_AVAILABLE:
      gCCApp.cause = CC_CAUSE_REG_ALL_FAILED;
      ccapp_set_state(CC_OOS_IDLE);
    break;
    default:
    break;
  }

  // Notify OOS state to Session Manager
  switch (mapProviderState(ccapp_get_state())) {
  case CC_STATE_OOS:
      ccpro_handleOOS();
      break;
  default:
      break;
  }
  ccapp_hlapi_update_device_reg_state();
}
/**
 *
 * ccappHandleRegUpdates handles reg state changes.
 *
 * @param   featUpd - feature update with reg state msg
 *
 * @return  void
 *
 * @pre     None
 */
void ccappHandleRegStateUpdates(feature_update_t *featUpd)
{
  static const char fname[] = "ccappHandleRegStateUpdates";

  CCAPP_DEBUG(DEB_F_PREFIX"called. feature=%d=%s, state=%d\n",
       DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), featUpd->featureID, CCAPP_TASK_CMD_PRINT(featUpd->featureID), gCCApp.state);
  gCCApp.cause = CC_CAUSE_NONE;

  // Update state varaibles to track state
  switch (featUpd->featureID)
  {
     case CCAPP_MODE_NOTIFY:
        gCCApp.mode = featUpd->update.ccFeatUpd.data.line_info.info;
        CCAPP_DEBUG(DEB_F_PREFIX"called. gCCApp.mode= %d gCCApp.state=%d. Returning\n",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), gCCApp.mode, gCCApp.state);
        return;

     case CCAPP_FAILOVER_IND:
        ccapp_set_state(CC_OOS_FAILOVER);
        gCCApp.cucm_mode = FAILOVER;
        gCCApp.cause = CC_CAUSE_FAILOVER;
        if ( featUpd->update.ccFeatUpd.data.line_info.info == CC_TYPE_CCM ){
           gCCApp.mode = CC_MODE_CCM;
        }

        else if (featUpd->update.ccFeatUpd.data.line_info.info == 3) {
           gCCApp.mode = CC_MODE_NONCCM;
        }

        if ( ccappPreserveCall() == FALSE) {
          ccapp_set_state(CC_OOS_REGISTERING);
          cc_fail_fallback_sip(CC_SRC_UI, RSP_START, CC_REG_FAILOVER_RSP, FALSE);
        }
        break;

     case CCAPP_FALLBACK_IND:
        gCCApp.cucm_mode = FALLBACK;
        if ( featUpd->update.ccFeatUpd.data.line_info.info == CC_TYPE_CCM ){
           gCCApp.mode = CC_MODE_CCM;
        }
        if ( isNoCallExist() ) {
          ccapp_set_state(CC_OOS_REGISTERING);
          gCCApp.cause = CC_CAUSE_FALLBACK;
          cc_fail_fallback_sip(CC_SRC_UI, RSP_START, CC_REG_FALLBACK_RSP, FALSE);
        }
        break;

      case CCAPP_SHUTDOWN_ACK:
        ccapp_set_state(CC_OOS_IDLE);
        gCCApp.cucm_mode = NONE_AVAIL;
        gCCApp.inPreservation = FALSE;
        gCCApp.cause = CC_CAUSE_SHUTDOWN;

        break;
      case CCAPP_REG_ALL_FAIL:
        ccapp_set_state(CC_OOS_IDLE);
        gCCApp.cucm_mode = NO_CUCM_SRST_AVAILABLE;
        gCCApp.inPreservation = FALSE;
        if (ccappPreserveCall() == FALSE) {
            gCCApp.cause = CC_CAUSE_REG_ALL_FAILED;
        } else {
            gCCApp.cause = CC_CAUSE_FAILOVER;
        }
        break;

      case CCAPP_LOGOUT_RESET:
        ccapp_set_state(CC_OOS_IDLE);
        gCCApp.cucm_mode = NONE_AVAIL;
        /*gCCApp.inFailover = FALSE;
        gCCApp.inFallback = FALSE;*/
        gCCApp.inPreservation = FALSE;
        gCCApp.cause = CC_CAUSE_LOGOUT_RESET;
        break;
    }
    // Notify INS/OOS state to Session Manager if required
    CCAPP_DEBUG(DEB_F_PREFIX"called. service_state=%d, mode=%d, cause=%d\n",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
            mapProviderState(gCCApp.state),
            gCCApp.mode,
            gCCApp.cause);
    switch (mapProviderState(ccapp_get_state())) {
    case CC_STATE_INS:
        ccpro_handleINS();
        break;
    case CC_STATE_OOS:
        ccpro_handleOOS();
        break;
    default:
        break;
    }
    ccapp_hlapi_update_device_reg_state();
}

/*
 * this function determine if the called number (i.e., dialed number) contains
 * string x-cisco-serviceuri-cfwdall.
 */
static boolean ccappCldNumIsCfwdallString(string_t cld_number) {
    static char cfwdAllString[STATUS_LINE_MAX_LEN] = "x-cisco-serviceuri-cfwdall";

    if (strncmp(cld_number, cfwdAllString, strlen(cfwdAllString)) == 0) {
        /* The Called Party number is the Call Forward all URI */
        return ((int) TRUE);
    }
    return ((int) FALSE);
}

/**
 * Api to update mute state of connected call
 */
cc_call_handle_t ccappGetConnectedCall(){
    session_data_t * data;
    hashItr_t itr;

    hashItrInit(&itr);
    while ( (data = (session_data_t*)hashItrNext(&itr)) != NULL ) {
        if( data->state == CONNECTED ) {
            return CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id);
        }
    }
    return 0;
}

/**
 *
 * CCApp Provider cache session data to hashTable routine
 *
 * @param   sessUpd - Session update from CC
 *
 * @return  void
 *
 * @pre     None
 */

static void ccappUpdateSessionData (session_update_t *sessUpd)
{
    static const char fname[] = "ccappUpdateSessionData";
    /* TODO -- I don't think the hash handling is synchronized; we could
       end up with data integrity issues here if we end up in a race.
       <adam@nostrum.com> */
    session_data_t *data = (session_data_t *)findhash(sessUpd->sessionID);
    session_data_t *sess_data_p;
    boolean createdSessionData = TRUE;
    cc_deviceinfo_ref_t  handle = 0;
    boolean previouslyInConference = FALSE;

    if ( data == NULL ) {
        cc_call_state_t call_state = sessUpd->update.ccSessionUpd.data.state_data.state;

        if ( sessUpd->eventID == CALL_INFORMATION ||
             sessUpd->eventID == CALL_STATE ||
             sessUpd->eventID == CALL_NEWCALL ||
             sessUpd->eventID == CREATE_OFFER ||
             sessUpd->eventID == CREATE_ANSWER ||
             sessUpd->eventID == SET_LOCAL_DESC  ||
             sessUpd->eventID == SET_REMOTE_DESC ||
             sessUpd->eventID == UPDATE_LOCAL_DESC  ||
             sessUpd->eventID == UPDATE_REMOTE_DESC ||
             sessUpd->eventID == ICE_CANDIDATE_ADD ||
             sessUpd->eventID == REMOTE_STREAM_ADD ) {

            CCAPP_DEBUG(DEB_F_PREFIX"CALL_SESSION_CREATED for session id 0x%x event is 0x%x \n",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sessUpd->sessionID,
                    sessUpd->eventID);

            if (sessUpd->eventID == CALL_INFORMATION  ) {
                call_state = RINGIN;

            } else {
                if ( sessUpd->update.ccSessionUpd.data.state_data.state == ONHOOK ) {
                    CCAPP_DEBUG(DEB_F_PREFIX"NOT CREATING Session Ignoring event %d sessid %x as state is ONHOOK\n",
                            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sessUpd->eventID, sessUpd->sessionID );
                    //Even session data is not created, we need to send ONHOOK to application to terminate call.
                    createdSessionData = FALSE;
                }
                call_state = sessUpd->update.ccSessionUpd.data.state_data.state;
            }
            //Create a new call.
            if (createdSessionData == TRUE) {
                cc_call_handle_t callHandle = CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID);
                //<Added for automation test purpose, <CallBegin>
                NOTIFY_CALL_DEBUG(DEB_NOTIFY_PREFIX"[line=%d][call.ref=%d]\n",
                                "CallBegin", GET_LINE_ID(callHandle), GET_CALL_ID(callHandle));
                //>
        data = cpr_malloc(sizeof(session_data_t));
        if ( data == NULL ) {
            APP_ERR_MSG("ccappUpdateSessionData Error: cpr_malloc failed for session data\n");
            return;
        }
        //Populate the session hash data the first time.
        memset(data, 0, sizeof(session_data_t));
        data->sess_id = sessUpd->sessionID;
				data->state = call_state;
        data->line = sessUpd->update.ccSessionUpd.data.state_data.line_id;
        if (sessUpd->eventID == CALL_NEWCALL ||
            sessUpd->eventID == CREATE_OFFER ||
            sessUpd->eventID == CREATE_ANSWER ||
            sessUpd->eventID == SET_LOCAL_DESC ||
            sessUpd->eventID == SET_REMOTE_DESC ||
            sessUpd->eventID == UPDATE_LOCAL_DESC ||
            sessUpd->eventID == UPDATE_REMOTE_DESC ||
            sessUpd->eventID == ICE_CANDIDATE_ADD ||
            sessUpd->eventID == REMOTE_STREAM_ADD ) {
            data->attr = sessUpd->update.ccSessionUpd.data.state_data.attr;
            data->inst = sessUpd->update.ccSessionUpd.data.state_data.inst;
        }
        data->cause = sessUpd->update.ccSessionUpd.data.state_data.cause;
        data->clg_name = strlib_empty();
        data->clg_number =  strlib_empty();
        data->alt_number = strlib_empty();
        data->cld_name = strlib_empty();
        data->cld_number = strlib_empty();
        data->orig_called_name = strlib_empty();
        data->orig_called_number = strlib_empty();
        data->last_redir_name = strlib_empty();
        data->last_redir_number = strlib_empty();
        data->plcd_name = strlib_empty();
        data->plcd_number = strlib_empty();
        data->status = strlib_empty();
        data->gci[0] = 0;
	data->vid_dir = SDP_DIRECTION_INACTIVE;
        data->callref = 0;
        data->sdp = strlib_empty();
        calllogger_init_call_log(&data->call_log);

        switch (sessUpd->eventID) {
            case CREATE_OFFER:
            case CREATE_ANSWER:
            case SET_LOCAL_DESC:
            case SET_REMOTE_DESC:
            case UPDATE_LOCAL_DESC:
            case UPDATE_REMOTE_DESC:
            case ICE_CANDIDATE_ADD:
                data->sdp = sessUpd->update.ccSessionUpd.data.state_data.sdp;
                /* Fall through to the next case... */
            case REMOTE_STREAM_ADD:
                data->cause =
                    sessUpd->update.ccSessionUpd.data.state_data.cause;
                data->media_stream_track_id = sessUpd->update.ccSessionUpd.data.
                    state_data.media_stream_track_id;
                data->media_stream_id = sessUpd->update.ccSessionUpd.data.
                    state_data.media_stream_id;
                break;
            default:
                break;
        }

        /*
         * If phone was idle, we not going to active state
         * send notification to resetmanager that we
         * are no longer resetReady.
         */
        if ((CCAPI_DeviceInfo_isPhoneIdle(handle) == TRUE) && (sendResetUpdates)) {
            resetNotReady();
        }
        (void) addhash(data->sess_id, data);
            }

            //The update accordingly
            switch (sessUpd->eventID) {
            case CALL_INFORMATION:
                data->clg_name = ccsnap_EscapeStrToLocaleStr(data->clg_name, sessUpd->update.ccSessionUpd.data.call_info.clgName, LEN_UNKNOWN);
                data->cld_name = ccsnap_EscapeStrToLocaleStr(data->cld_name, sessUpd->update.ccSessionUpd.data.call_info.cldName, LEN_UNKNOWN);
                data->orig_called_name = ccsnap_EscapeStrToLocaleStr(data->orig_called_name, sessUpd->update.ccSessionUpd.data.call_info.origCalledName, LEN_UNKNOWN);
                data->last_redir_name = ccsnap_EscapeStrToLocaleStr(data->last_redir_name, sessUpd->update.ccSessionUpd.data.call_info.lastRedirectingName, LEN_UNKNOWN);

                data->clg_number = strlib_update(data->clg_number, sessUpd->update.ccSessionUpd.data.call_info.clgNumber);
				data->cld_number = strlib_update(data->cld_number, sessUpd->update.ccSessionUpd.data.call_info.cldNumber);
				data->alt_number = strlib_update(data->alt_number, sessUpd->update.ccSessionUpd.data.call_info.altClgNumber);
				data->orig_called_number = strlib_update(data->orig_called_number, sessUpd->update.ccSessionUpd.data.call_info.origCalledNumber);
				data->last_redir_number = strlib_update(data->last_redir_number, sessUpd->update.ccSessionUpd.data.call_info.lastRedirectingNumber);
				data->type = sessUpd->update.ccSessionUpd.data.call_info.call_type;
				data->inst = sessUpd->update.ccSessionUpd.data.call_info.instance_id;
				data->security = sessUpd->update.ccSessionUpd.data.call_info.security;
				data->policy = sessUpd->update.ccSessionUpd.data.call_info.policy;
                break;
            case CALL_STATE:
                if (createdSessionData == FALSE) {
                    return;
                }
                data->state = sessUpd->update.ccSessionUpd.data.state_data.state;
				data->line = sessUpd->update.ccSessionUpd.data.state_data.line_id;
                break;
            default:
                break;
            }
        } else if (sessUpd->eventID == CALL_DELETE_LAST_DIGIT) {
            CCAPP_DEBUG(DEB_F_PREFIX"CALL_DELETE_LAST_DIGIT: event %d sessid %x.\n",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sessUpd->eventID, sessUpd->sessionID );
            return;
        } else {
            CCAPP_DEBUG(DEB_F_PREFIX"NOT CREATING Session Ignoring event %d sessid %x \n",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sessUpd->eventID, sessUpd->sessionID );
            return;
        }

        // send event to csf2g API
        if (data != NULL) {
            capset_get_allowed_features(gCCApp.mode, data->state, data->allowed_features);
            ccsnap_gen_callEvent(CCAPI_CALL_EV_CREATED, CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id));
        }
        return;

    }

    CCAPP_DEBUG(DEB_F_PREFIX"Found data for sessid %x event %d\n",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sessUpd->sessionID, sessUpd->eventID);
	switch(sessUpd->eventID) {
	case CALL_SESSION_CLOSED:
	    // find and deep free then delete
        sess_data_p = (session_data_t *)findhash(sessUpd->sessionID);
        if ( sess_data_p != NULL ){
            cleanSessionData(sess_data_p);
            if ( 0 >  delhash(sessUpd->sessionID) ) {
                APP_ERR_MSG (DEB_F_PREFIX"failed to delete hash sessid=0x%08x\n",
                        DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),sessUpd->sessionID);
            }
            cpr_free(sess_data_p);
        }
        if ( (gCCApp.inPreservation || (gCCApp.cucm_mode == FALLBACK)) && isNoCallExist()) {
            /* The phone is now Idle. Clear the inPreservation Flag */
            gCCApp.inPreservation = FALSE;
            proceedWithFOFB();
        }
        if ((CCAPI_DeviceInfo_isPhoneIdle(handle) == TRUE) && (sendResetUpdates)) {
            resetReady();
        }
        if (pending_action_type != NO_ACTION) {
            perform_deferred_action();
        }
		break;

	case CALL_STATE:
	    DEF_DEBUG(DEB_F_PREFIX"Call_STATE:. state=%d, attr=%d, cause=%d, instance=%d\n",
	                        DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
	                        sessUpd->update.ccSessionUpd.data.state_data.state,
	                        data->attr,
	                        sessUpd->update.ccSessionUpd.data.state_data.cause,
	                        data->inst);
            if ( sessUpd->update.ccSessionUpd.data.state_data.state == HOLD &&
                 (data->state == REMHOLD || data->state == REMINUSE)){
                data->state = REMHOLD;
            } else {
                data->state = sessUpd->update.ccSessionUpd.data.state_data.state;
            }
            data->line = sessUpd->update.ccSessionUpd.data.state_data.line_id;
            sessUpd->update.ccSessionUpd.data.state_data.attr = data->attr;
            sessUpd->update.ccSessionUpd.data.state_data.inst = data->inst;

            data->cause = sessUpd->update.ccSessionUpd.data.state_data.cause;

            //Update call state
             if ( data != NULL ){
                 capset_get_allowed_features(gCCApp.mode, data->state, data->allowed_features);
             }
             calllogger_update(data);
             ccsnap_gen_callEvent(CCAPI_CALL_EV_STATE, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));

            //<Added for automation test purpose, <CallEnd>
            if (data->state == ONHOOK) {
                NOTIFY_CALL_DEBUG(DEB_NOTIFY_PREFIX"[line=%d][call.ref=%d]\n",
                                "CallEND", data->line, data->id);
            }
            //>

            if (data->state == ONHOOK) {
                // find and deep free then delete
                sess_data_p = (session_data_t *)findhash(sessUpd->sessionID);
                if ( sess_data_p != NULL ){
                    cleanSessionData(sess_data_p);
                    if ( 0 >  delhash(sessUpd->sessionID) ) {
                          APP_ERR_MSG (DEB_F_PREFIX"failed to delete hash sessid=0x%08x\n",
                                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),sessUpd->sessionID);
                  	}
                        cpr_free(sess_data_p);
                     data = NULL;
                 }
                if ((gCCApp.inPreservation || (gCCApp.cucm_mode == FALLBACK)) && isNoCallExist()) {
                    /* The phone is now Idle. Clear the inPreservation Flag */
                    gCCApp.inPreservation = FALSE;
                    proceedWithFOFB();
                }
                if ((CCAPI_DeviceInfo_isPhoneIdle(handle) == TRUE) && (sendResetUpdates)) {
                    resetReady();
                }
                if (pending_action_type != NO_ACTION) {
                    perform_deferred_action();
                }
             }
            break;

	case CALL_CALLREF:
            data->callref = sessUpd->update.ccSessionUpd.data.callref;
            break;
	case CALL_GCID:
		if ( ! strncasecmp(data->gci, sessUpd->update.ccSessionUpd.data.gcid, CC_MAX_GCID)) {
		  // No change in gci we can ignore the update
		  return;
		}
		sstrncpy(data->gci, sessUpd->update.ccSessionUpd.data.gcid, CC_MAX_GCID);
		ccsnap_gen_callEvent(CCAPI_CALL_EV_GCID, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
		break;
	case CALL_NEWCALL:
	    data->state = sessUpd->update.ccSessionUpd.data.state_data.state;
        data->line = sessUpd->update.ccSessionUpd.data.state_data.line_id;
        data->attr = sessUpd->update.ccSessionUpd.data.state_data.attr;
        data->inst = sessUpd->update.ccSessionUpd.data.state_data.inst;
        return;
		break;

	case CALL_ATTR:
		data->attr = sessUpd->update.ccSessionUpd.data.state_data.attr;
                calllogger_update(data);
		ccsnap_gen_callEvent(CCAPI_CALL_EV_ATTR, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
		break;

	case CALL_INFORMATION:
        // check conference state, if it changes, we'll send a conference event notification
        previouslyInConference = CCAPI_CallInfo_getIsConference(data);

		data->clg_name = ccsnap_EscapeStrToLocaleStr(data->clg_name, sessUpd->update.ccSessionUpd.data.call_info.clgName, LEN_UNKNOWN);
		data->cld_name = ccsnap_EscapeStrToLocaleStr(data->cld_name, sessUpd->update.ccSessionUpd.data.call_info.cldName, LEN_UNKNOWN);
		data->orig_called_name = ccsnap_EscapeStrToLocaleStr(data->orig_called_name, sessUpd->update.ccSessionUpd.data.call_info.origCalledName, LEN_UNKNOWN);
		data->last_redir_name = ccsnap_EscapeStrToLocaleStr(data->last_redir_name, sessUpd->update.ccSessionUpd.data.call_info.lastRedirectingName, LEN_UNKNOWN);
		data->clg_number = strlib_update(data->clg_number, sessUpd->update.ccSessionUpd.data.call_info.clgNumber);
		data->cld_number = strlib_update(data->cld_number, sessUpd->update.ccSessionUpd.data.call_info.cldNumber);
		data->alt_number = strlib_update(data->alt_number, sessUpd->update.ccSessionUpd.data.call_info.altClgNumber);
		data->orig_called_number = strlib_update(data->orig_called_number, sessUpd->update.ccSessionUpd.data.call_info.origCalledNumber);
		data->last_redir_number = strlib_update(data->last_redir_number, sessUpd->update.ccSessionUpd.data.call_info.lastRedirectingNumber);
		data->type = sessUpd->update.ccSessionUpd.data.call_info.call_type;
		data->inst = sessUpd->update.ccSessionUpd.data.call_info.instance_id;
		data->security = sessUpd->update.ccSessionUpd.data.call_info.security;
		data->policy = sessUpd->update.ccSessionUpd.data.call_info.policy;
        if ((data->cld_number[0]) && ccappCldNumIsCfwdallString(data->cld_number)) {
            DEF_DEBUG(DEB_F_PREFIX"Not updating the UI. Called Number = %s\n",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), data->cld_number);
            return;
        }
        /*
         * For 3rd Gen and 4th Gen phones... Use sessUpd->update structure to pass call information
         * For 5th Gen phones, the information will be localized by a 5th Gen Application, and the
         *   dictionaries might not be equivalent.
         */
         calllogger_update(data);

         ccsnap_gen_callEvent(CCAPI_CALL_EV_CALLINFO, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));

         // if we're entering a conference, then indicate conference participant info (in case the info came earlier, app
         // will receive the notification now.  If info comes later, then app will receive an additional subsequent info notice
         // at that time.
         if ((!previouslyInConference) && (CCAPI_CallInfo_getIsConference(data))) {
            ccsnap_gen_callEvent(CCAPI_CALL_EV_CONF_PARTICIPANT_INFO, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
         }

		break;

	case CALL_PLACED_INFO:
		data->plcd_number = strlib_update(data->plcd_number, sessUpd->update.ccSessionUpd.data.plcd_info.cldNum);
		data->plcd_name = ccsnap_EscapeStrToLocaleStr(data->plcd_name, sessUpd->update.ccSessionUpd.data.plcd_info.cldName, LEN_UNKNOWN);
                calllogger_setPlacedCallInfo(data);

		break;

	case CALL_SELECTED:
		data->isSelected = sessUpd->update.ccSessionUpd.data.action;
		ccsnap_gen_callEvent(CCAPI_CALL_EV_SELECT, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
		break;

	case CALL_SELECT_FEATURE_SET:
                // Not quite perfect but should do for now
                if ( strcmp("CONNECTEDNOFEAT", sessUpd->update.ccSessionUpd.data.feat_set.featSet ) ) {
                    data->allowed_features[CCAPI_CALL_CAP_HOLD] = FALSE;
                    data->allowed_features[CCAPI_CALL_CAP_CONFERENCE] = FALSE;
                    data->allowed_features[CCAPI_CALL_CAP_TRANSFER] = FALSE;
                } else if ( sessUpd->update.ccSessionUpd.data.feat_set.featMask[0] == skConfrn ) {
                    data->allowed_features[CCAPI_CALL_CAP_CONFERENCE] = FALSE;
                }
		ccsnap_gen_callEvent(CCAPI_CALL_EV_CAPABILITY, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
		break;

	case CALL_STATUS:
	/*
	 * For 5th Gen Phones, data->status is localized.
	 * 3rd Gen and 4th Gen phones should use sessUpd->update struct to pass the status to the application.
	 */
	data->status = ccsnap_EscapeStrToLocaleStr(data->status, sessUpd->update.ccSessionUpd.data.status.status, LEN_UNKNOWN);
        if (data->status != NULL) {
            if(strncmp(data->status, UNKNOWN_PHRASE_STR, UNKNOWN_PHRASE_STR_SIZE) == 0){
                    data->status = strlib_empty();
            }
            if(strcmp(data->status, strlib_empty()) != 0){
                    ccsnap_gen_callEvent(CCAPI_CALL_EV_STATUS, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
            }
        }
        //<Added for automation test purpose, <CallStatusChanged>
        NOTIFY_CALL_DEBUG(DEB_NOTIFY_PREFIX"[line=%d][call.ref=%d][status=%s]\n",
                 "callStatusChange", data->line, data->id,
                  NOTIFY_CALL_STATUS);
        //>

	break;
       if(strcmp(data->status, strlib_empty()) != 0){
    	   ccsnap_gen_callEvent(CCAPI_CALL_EV_STATUS, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
       }

	case CALL_PRESERVATION_ACTIVE:
		data->state = PRESERVATION;
		ccsnap_gen_callEvent(CCAPI_CALL_EV_PRESERVATION, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
		break;

    case RINGER_STATE:
        data->ringer_start = sessUpd->update.ccSessionUpd.data.ringer.start;
        data->ringer_mode = sessUpd->update.ccSessionUpd.data.ringer.mode;
        data->ringer_once = sessUpd->update.ccSessionUpd.data.ringer.once;
	ccsnap_gen_callEvent(CCAPI_CALL_EV_RINGER_STATE, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;

    case VIDEO_AVAIL:
        if ( data->vid_dir == sessUpd->update.ccSessionUpd.data.action ) {
            // no change don't update
            return;
        }
        data->vid_dir = sessUpd->update.ccSessionUpd.data.action;
	ccsnap_gen_callEvent(CCAPI_CALL_EV_VIDEO_AVAIL, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case VIDEO_OFFERED:
        data->vid_offer = sessUpd->update.ccSessionUpd.data.action;
	ccsnap_gen_callEvent(CCAPI_CALL_EV_VIDEO_OFFERED, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case CALL_ENABLE_BKSP:
        data->allowed_features[CCAPI_CALL_CAP_BACKSPACE] = TRUE;
	ccsnap_gen_callEvent(CCAPI_CALL_EV_CAPABILITY, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case CALL_SECURITY:
        data->security = sessUpd->update.ccSessionUpd.data.security;
	ccsnap_gen_callEvent(CCAPI_CALL_EV_SECURITY, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case CALL_LOGDISP:
        data->log_disp = sessUpd->update.ccSessionUpd.data.action;
        calllogger_updateLogDisp(data);
        // No need to generate this event anymore
	// ccsnap_gen_callEvent(CCAPI_CALL_EV_LOG_DISP, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case CALL_DELETE_LAST_DIGIT:
	ccsnap_gen_callEvent(CCAPI_CALL_EV_LAST_DIGIT_DELETED, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case CALL_FEATURE_CANCEL:
	ccsnap_gen_callEvent(CCAPI_CALL_EV_XFR_OR_CNF_CANCELLED, CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case CALL_RECV_INFO_LIST:
        //Should not come here. It's dealed in CCAPP_RCVD_INFO.
        break;
    case MEDIA_INTERFACE_UPDATE_BEGIN:
        ccsnap_gen_callEvent(CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_BEGIN,
            CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case MEDIA_INTERFACE_UPDATE_SUCCESSFUL:
        ccsnap_gen_callEvent(CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_SUCCESSFUL,
            CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case MEDIA_INTERFACE_UPDATE_FAIL:
        ccsnap_gen_callEvent(CCAPI_CALL_EV_MEDIA_INTERFACE_UPDATE_FAIL,
            CREATE_CALL_HANDLE_FROM_SESSION_ID(sessUpd->sessionID));
        break;
    case CREATE_OFFER:
    case CREATE_ANSWER:
    case SET_LOCAL_DESC:
    case SET_REMOTE_DESC:
    case UPDATE_LOCAL_DESC:
    case UPDATE_REMOTE_DESC:
    case ICE_CANDIDATE_ADD:
        data->sdp = sessUpd->update.ccSessionUpd.data.state_data.sdp;
        /* Fall through to the next case... */
    case REMOTE_STREAM_ADD:
        data->cause = sessUpd->update.ccSessionUpd.data.state_data.cause;
        data->state = sessUpd->update.ccSessionUpd.data.state_data.state;
        data->media_stream_track_id = sessUpd->update.ccSessionUpd.data.state_data.media_stream_track_id;
        data->media_stream_id = sessUpd->update.ccSessionUpd.data.state_data.media_stream_id;
        capset_get_allowed_features(gCCApp.mode, data->state, data->allowed_features);
        ccsnap_gen_callEvent(CCAPI_CALL_EV_STATE, CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id));
        break;
    default:
        DEF_DEBUG(DEB_F_PREFIX"Unknown event, id = %d\n",
                            DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sessUpd->eventID);
        break;
	}
    return;
}

static void freeSessionData(session_update_t *sessUpd)
{
    switch(sessUpd->eventID) {
    case CALL_INFORMATION:
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.clgName);
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.clgNumber);
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.cldName);
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.cldNumber);
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.altClgNumber);
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.origCalledName);
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.origCalledNumber);
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.lastRedirectingName);
        strlib_free(sessUpd->update.ccSessionUpd.data.call_info.lastRedirectingNumber);
        break;
    case CALL_PLACED_INFO:
	strlib_free(sessUpd->update.ccSessionUpd.data.plcd_info.cldName);
        strlib_free(sessUpd->update.ccSessionUpd.data.plcd_info.cldNum);
        break;
    case CALL_SELECT_FEATURE_SET:
	strlib_free(sessUpd->update.ccSessionUpd.data.feat_set.featSet);
        break;
    case CALL_STATUS:
        strlib_free(sessUpd->update.ccSessionUpd.data.status.status);
        break;
    case CALL_RECV_INFO_LIST:
        strlib_free(sessUpd->update.ccSessionUpd.data.recv_info_list);
        break;
    }
}

static void freeSessionMgmtData(session_mgmt_t *sessMgmt)
{
    switch(sessMgmt->func_id) {
    case SESSION_MGMT_APPLY_CONFIG:
        strlib_free(sessMgmt->data.config.log_server);
        strlib_free(sessMgmt->data.config.load_server);
        strlib_free(sessMgmt->data.config.load_id);
        strlib_free(sessMgmt->data.config.inactive_load_id);
        strlib_free(sessMgmt->data.config.cucm_result);
        strlib_free(sessMgmt->data.config.fcp_version_stamp);
        strlib_free(sessMgmt->data.config.dialplan_version_stamp);
        strlib_free(sessMgmt->data.config.config_version_stamp);
        break;
    case SESSION_MGMT_EXECUTE_URI:
        strlib_free(sessMgmt->data.uri.uri);
        break;
    default:
        break;
    }
}

static void freeRcvdInfo(session_rcvd_info_t *rcvdInfo)
{
    switch(rcvdInfo->packageID) {
    case INFO_PKG_ID_GENERIC_RAW:
        strlib_free(rcvdInfo->info.generic_raw.info_package);
        strlib_free(rcvdInfo->info.generic_raw.content_type);
        strlib_free(rcvdInfo->info.generic_raw.message_body);
        break;
    }
}

void dump_msg(char * name, unsigned int *msg, int len, unsigned int cmd) {
int i,j;
  CCAPP_DEBUG(DEB_F_PREFIX"\n%s %x %d cmd=%d\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "dump_msg"), name, msg, len, cmd);
  for ( j=0;j<10;j++) {
    for(i=0;i<16;i++) {
      CCAPP_DEBUG(DEB_F_PREFIX"%08X ", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "dump_msg"), msg[i+j]);
      if ( (i+j+1)*4 >= len ) return;
    }
    CCAPP_DEBUG(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, "dump_msg"));
  }
}


/**
 * A ccapp task listener
 */
void ccp_handler(void* msg, int type) {
    static const char fname[] = "ccp_handler";
    sessionProvider_cmd_t *cmdMsg;
    session_feature_t *featMsg;
    session_update_t *sessUpd;
    feature_update_t *featUpd;
    session_mgmt_t *sessMgmt;
    session_send_info_t *sendInfo;
    session_rcvd_info_t *rcvdInfo;
    session_id_t sess_id;
    session_data_t *data;
    int length;
    cc_reg_state_t state;

    CCAPP_DEBUG(DEB_F_PREFIX"Received Cmd %s\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
        CCAPP_TASK_CMD_PRINT(type) );


    switch (type) {
    case CCAPP_SERVICE_CMD:
        cmdMsg = (sessionProvider_cmd_t *) msg;
        CCApp_processCmds (cmdMsg->cmd, cmdMsg->cmdData.ccData.reason,
                         cmdMsg->cmdData.ccData.reason_info);
        break;

    case CCAPP_INVOKEPROVIDER_FEATURE:
        featMsg = (session_feature_t *) msg;
        processProviderEvent(GET_LINEID(featMsg->session_id), featMsg->featureID,
                            featMsg->featData.ccData.state);
        if (featMsg->featData.ccData.info != NULL) {
        strlib_free(featMsg->featData.ccData.info);
        }
        if (featMsg->featData.ccData.info1 != NULL) {
            strlib_free(featMsg->featData.ccData.info1);
        }
        break;
    case CCAPP_INVOKE_FEATURE:
        featMsg = (session_feature_t *) msg;
        CCAPP_DEBUG(DEB_F_PREFIX"CCAPP_INVOKE_FEATURE:sid=%d, fid=%d, line=%d, cid=%d, info=%s info1=%s\n",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                featMsg->session_id,
                featMsg->featureID,
                GET_LINEID(featMsg->session_id),
                GET_CALLID(featMsg->session_id),
                ((featMsg->featureID == CC_FEATURE_KEYPRESS) ? "..." : featMsg->featData.ccData.info),
                ((featMsg->featureID == CC_FEATURE_KEYPRESS) ? "..." : featMsg->featData.ccData.info1));
        processSessionEvent(GET_LINEID(featMsg->session_id), GET_CALLID(featMsg->session_id),
            featMsg->featureID, featMsg->featData.ccData.state,
            featMsg->featData.ccData);
        if (featMsg->featData.ccData.info != NULL) {
            strlib_free(featMsg->featData.ccData.info);
        }
        if (featMsg->featData.ccData.info1 != NULL) {
            strlib_free(featMsg->featData.ccData.info1);
        }
        break;

    case CCAPP_CLOSE_SESSION:
        sess_id = *(session_id_t*)msg;
        cc_feature(CC_SRC_UI, GET_CALLID(sess_id),
                GET_LINEID(sess_id), CC_FEATURE_END_CALL, NULL);
        break;

    case CCAPP_SESSION_UPDATE:
        sessUpd = (session_update_t *) msg;
        // Udpate the local cache
        CCAPP_DEBUG(DEB_F_PREFIX"CCAPP_SESSION_UPDATE:type:%d.\n",
              DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), GET_SESS_TYPE(sessUpd->sessionID));
        // XXX Why do this when sessType is in session_update_t already?
        if (GET_SESS_TYPE(sessUpd->sessionID) == SESSIONTYPE_CALLCONTROL) {
            ccappUpdateSessionData(sessUpd);
        }
        else {
            CCAPP_DEBUG(DEB_F_PREFIX"Unknown type:%d\n",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), sessUpd->sessType);
        }
        freeSessionData(sessUpd);
        break;

    case CCAPP_FEATURE_UPDATE:
        state = ccapp_get_state();
        featUpd = (feature_update_t *) msg;
        // Update Registration state
        if (featUpd->featureID == DEVICE_REG_STATE)
        	CCAPP_DEBUG(DEB_F_PREFIX"DEVICE_REG_STATE\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

        if(featUpd->update.ccFeatUpd.data.line_info.info == CC_REGISTERED)
        	CCAPP_DEBUG(DEB_F_PREFIX"CC_REGISTERED\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

        if(state == (int) CC_INSERVICE)
        	CCAPP_DEBUG(DEB_F_PREFIX"CC_INSERVICE\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

        if ( featUpd->featureID == DEVICE_REG_STATE &&
            featUpd->update.ccFeatUpd.data.line_info.info == CC_REGISTERED &&
            state != (int) CC_INSERVICE )
        {
            cc_uint32_t major_ver=0, minor_ver=0,addtnl_ver=0;
            char name[CC_MAX_LEN_REQ_SUPP_PARAM_CISCO_SISTAG]={0};
            platGetSISProtocolVer( &major_ver, &minor_ver, &addtnl_ver, name);
            CCAPP_DEBUG(DEB_F_PREFIX"The SIS verion is: %s, sis ver: %d.%d.%d \n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), name, major_ver, minor_ver, addtnl_ver);
            if(!strncmp(name, REQ_SUPP_PARAM_CISCO_CME_SISTAG, strlen(REQ_SUPP_PARAM_CISCO_CME_SISTAG))){
                CCAPP_DEBUG(DEB_F_PREFIX"This is CUCME mode.\n", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
            } else if(!strncmp(name, REQ_SUPP_PARAM_CISCO_SISTAG, strlen(REQ_SUPP_PARAM_CISCO_SISTAG))){
                CCAPP_DEBUG(DEB_F_PREFIX"This is CUCM mode.\n",DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
             } else {
                CCAPP_DEBUG(DEB_F_PREFIX"This is unknown mode.\n",DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
             }
            ccapp_set_state(CC_INSERVICE);
            gCCApp.cause = CC_CAUSE_NONE;

            // Notify INS/OOS state to Session Manager if required
            ccapp_hlapi_update_device_reg_state();
            ccpro_handleINS();

            if (gCCApp.cucm_mode == FAILOVER) {
                cc_fail_fallback_sip(CC_SRC_UI, RSP_COMPLETE, CC_REG_FAILOVER_RSP, FALSE);
            }
            if (gCCApp.cucm_mode == FALLBACK) {
                cc_fail_fallback_sip(CC_SRC_UI, RSP_COMPLETE, CC_REG_FALLBACK_RSP, FALSE);
            }
            gCCApp.cucm_mode = NONE_AVAIL;
        }

        ccappFeatureUpdated(featUpd);
        break;

    case CCAPP_SESSION_MGMT:
        sessMgmt = (session_mgmt_t *) msg;
        ccappSyncSessionMgmt(sessMgmt);
        break;

    case CCAPP_SEND_INFO:
        sendInfo = (session_send_info_t *) msg;
        data = (session_data_t *)findhash(sendInfo->sessionID);

        if ( data != NULL && data->state == CONNECTED ) {
            cc_int_info(CC_SRC_UI, CC_SRC_SIP,
                    GET_CALLID(sendInfo->sessionID),
                    GET_LINEID(sendInfo->sessionID),
                    sendInfo->generic_raw.info_package,
                    sendInfo->generic_raw.content_type,
                    sendInfo->generic_raw.message_body);
        }
        strlib_free(sendInfo->generic_raw.message_body);
        strlib_free(sendInfo->generic_raw.content_type);
        strlib_free(sendInfo->generic_raw.info_package);
    break;

    case CCAPP_RCVD_INFO:
        sendInfo = (session_send_info_t *) msg;
        data = (session_data_t *)findhash(sendInfo->sessionID);
        rcvdInfo = (session_rcvd_info_t *) msg;

        length = strlen((const char*)rcvdInfo->info.generic_raw.message_body);
        if (data != NULL) {
            CCAPP_DEBUG(DEB_F_PREFIX"rcvdInfo: addr=%x length=%d, xml=%s\n",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, "CCAPI-CONFPARSE"),
                    &data->call_conference, length, rcvdInfo->info.generic_raw.message_body);

            data->info_package = rcvdInfo->info.generic_raw.info_package;
            data->info_type = rcvdInfo->info.generic_raw.content_type;
            data->info_body = rcvdInfo->info.generic_raw.message_body;

            ccsnap_gen_callEvent(CCAPI_CALL_EV_RECEIVED_INFO, CREATE_CALL_HANDLE_FROM_SESSION_ID(rcvdInfo->sessionID));

            // most of the time isConference will be true at this point, we can notify the app of
            // the updated participant info.  However, if we're transitioning from non conference into
            // conference, then we'll delay this notification until we're fully in conference state
            if (CCAPI_CallInfo_getIsConference(data))
            {   // in conference - send the info
                ccsnap_gen_callEvent(CCAPI_CALL_EV_CONF_PARTICIPANT_INFO, CREATE_CALL_HANDLE_FROM_SESSION_ID(rcvdInfo->sessionID));
            }

            // one shot notify cleanup after event generation
            data->info_package = strlib_empty();
            data->info_type = strlib_empty();
            data->info_body = strlib_empty();
        }
        freeRcvdInfo(rcvdInfo);
        break;

    case CCAPP_UPDATELINES:
        notify_register_update(*(int *)msg);
        break;

    case CCAPP_MODE_NOTIFY:
    case CCAPP_FAILOVER_IND:
    case CCAPP_SHUTDOWN_ACK:
    case CCAPP_FALLBACK_IND:
    case CCAPP_REG_ALL_FAIL:
        featUpd = (feature_update_t *) msg;
        ccappHandleRegStateUpdates(featUpd);
        break;

    case CCAPP_THREAD_UNLOAD:
        destroy_ccapp_thread();
        break;
    default:
        APP_ERR_MSG("CCApp_Task: Error: Unknown message %d msg =0x%x\n", type, msg);
        break;
    }
}

/*
 *  Function: destroy_ccapp_thread
 *  Description:  shutdown and kill ccapp thread
 *  Parameters:   none
 *  Returns: none
 */
void destroy_ccapp_thread()
{
    static const char fname[] = "destroy_ccapp_thread";
    TNP_DEBUG(DEB_F_PREFIX"Unloading ccapp and destroying ccapp thread\n",
        DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
    platform_initialized = FALSE;
    CCAppShutdown();
    (void)cprDestroyThread(ccapp_thread);
}

/**
 * CCAPP wrapper to update device features.
 * @param featUpd - feature_update_t
 * @return void
 */
static
void ccappFeatureUpdated (feature_update_t *featUpd) {
    cc_line_info_t *line_info;

    switch(featUpd->featureID) {
    case DEVICE_FEATURE_CFWD:
        line_info = ccsnap_getLineInfoFromBtn(featUpd->update.ccFeatUpd.data.cfwd.line);
        if ( line_info != NULL ) {
            line_info->isCFWD = featUpd->update.ccFeatUpd.data.cfwd.isFwd;
            line_info->isLocalCFWD = featUpd->update.ccFeatUpd.data.cfwd.isLocal;
            line_info->cfwd_dest = strlib_update(line_info->cfwd_dest, featUpd->update.ccFeatUpd.data.cfwd.cfa_num);
	    ccsnap_gen_lineEvent(CCAPI_LINE_EV_CFWDALL, line_info->button);
        }
        CC_Config_setStringValue(CFGID_LINE_CFWDALL+featUpd->update.ccFeatUpd.data.cfwd.line-1, featUpd->update.ccFeatUpd.data.cfwd.cfa_num);

        break;
    case DEVICE_FEATURE_MWI:
        line_info = ccsnap_getLineInfoFromBtn(featUpd->update.ccFeatUpd.data.mwi_status.line);
        if ( line_info != NULL ) {
            line_info->mwi.status = featUpd->update.ccFeatUpd.data.mwi_status.status;
            line_info->mwi.type = featUpd->update.ccFeatUpd.data.mwi_status.type;
            line_info->mwi.new_count = featUpd->update.ccFeatUpd.data.mwi_status.newCount;
            line_info->mwi.old_count = featUpd->update.ccFeatUpd.data.mwi_status.oldCount;
            line_info->mwi.pri_new_count = featUpd->update.ccFeatUpd.data.mwi_status.hpNewCount;
            line_info->mwi.pri_old_count = featUpd->update.ccFeatUpd.data.mwi_status.hpOldCount;
	    ccsnap_gen_lineEvent(CCAPI_LINE_EV_MWI, line_info->button);
        }
        //Added for automation test
        NOTIFY_LINE_DEBUG(DEB_NOTIFY_PREFIX"[line=%d][state=%s]",
                        "MWIChanged", featUpd->update.ccFeatUpd.data.mwi_status.line,
                        (featUpd->update.ccFeatUpd.data.mwi_status.status)?"ON":"OFF");

        break;
    case DEVICE_FEATURE_MWILAMP:
        g_deviceInfo.mwi_lamp = featUpd->update.ccFeatUpd.data.state_data.state;
	ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_MWI_LAMP, CC_DEVICE_ID);
        break;
    case DEVICE_FEATURE_BLF:
        sub_hndlr_NotifyBLFStatus(featUpd->update.ccFeatUpd.data.blf_data.request_id,
                                  featUpd->update.ccFeatUpd.data.blf_data.state,
                                  featUpd->update.ccFeatUpd.data.blf_data.app_id);
        break;
    case DEVICE_FEATURE_MNC_REACHED:
        line_info = ccsnap_getLineInfoFromBtn(featUpd->update.ccFeatUpd.data.line_info.line);
        if ( line_info != NULL ) {
            ccsnap_handle_mnc_reached(line_info,
                featUpd->update.ccFeatUpd.data.line_info.info, gCCApp.mode);
	    ccsnap_gen_lineEvent(CCAPI_LINE_EV_CAPSET_CHANGED, line_info->button);
        }
        break;
    case DEVICE_SERVICE_CONTROL_REQ:
        reset_type = (cc_srv_ctrl_req_t) featUpd->update.ccFeatUpd.data.reset_type;
        ccpro_handleserviceControlNotify();
        break;
    case DEVICE_NOTIFICATION:
        g_deviceInfo.not_prompt = ccsnap_EscapeStrToLocaleStr(g_deviceInfo.not_prompt, featUpd->update.ccFeatUpd.data.notification.prompt, LEN_UNKNOWN);
        g_deviceInfo.not_prompt_prio = featUpd->update.ccFeatUpd.data.notification.priority;
        g_deviceInfo.not_prompt_prog = featUpd->update.ccFeatUpd.data.notification.notifyProgress;
	ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_NOTIFYPROMPT, CC_DEVICE_ID);
        break;
    case DEVICE_LABEL_N_SPEED:
	// todo
        break;
    case DEVICE_REG_STATE:
        line_info = ccsnap_getLineInfoFromBtn(featUpd->update.ccFeatUpd.data.line_info.line);
        if ( line_info != NULL ) {
            line_info->reg_state = featUpd->update.ccFeatUpd.data.line_info.info;
	    ccsnap_gen_lineEvent(CCAPI_LINE_EV_REG_STATE, line_info->button);
        }
        break;
    case DEVICE_CCM_CONN_STATUS:
        ccsnap_update_ccm_status(featUpd->update.ccFeatUpd.data.ccm_conn.addr,
			featUpd->update.ccFeatUpd.data.ccm_conn.status);
	ccsnap_gen_deviceEvent(CCAPI_DEVICE_EV_SERVER_STATUS, CC_DEVICE_ID);
        break;
    default:
        DEF_DEBUG(DEB_F_PREFIX"Unknown event: id= %d\n",
                            DEB_F_PREFIX_ARGS(SIP_CC_PROV, "ccappFeatureUpdated"), featUpd->featureID);
        break;

    }
    if ( featUpd->featureID == DEVICE_NOTIFICATION) {
        strlib_free(featUpd->update.ccFeatUpd.data.notification.prompt);
    }

    if ( featUpd->featureID == DEVICE_CCM_CONN_STATUS) {
        strlib_free(featUpd->update.ccFeatUpd.data.ccm_conn.addr);
    }

    if ( featUpd->featureID == DEVICE_FEATURE_CFWD) {
        strlib_free(featUpd->update.ccFeatUpd.data.cfwd.cfa_num);
    }

    if (featUpd->featureID == DEVICE_SYNC_CONFIG_VERSION) {
        strlib_free(featUpd->update.ccFeatUpd.data.cfg_ver_data.cfg_ver);
        strlib_free(featUpd->update.ccFeatUpd.data.cfg_ver_data.dp_ver);
        strlib_free(featUpd->update.ccFeatUpd.data.cfg_ver_data.softkey_ver);
    }

    if (featUpd->featureID == DEVICE_LABEL_N_SPEED) {
        strlib_free(featUpd->update.ccFeatUpd.data.cfg_lbl_n_spd.speed);
        strlib_free(featUpd->update.ccFeatUpd.data.cfg_lbl_n_spd.label);
    }

}

/**
 *
 * CCApp Provider wrapper for synchronous calls.
 *
 * @param   sessMgmt - session management message
 *
 * @return  void
 *
 * @pre     None
 */
void ccappSyncSessionMgmt(session_mgmt_t *sessMgmt)
{
    cc_line_info_t *line_info;
    CCAPP_DEBUG(DEB_F_PREFIX"ccappSyncSessionMgmt: func_id=%d \n",
            DEB_F_PREFIX_ARGS(SIP_CC_PROV, "ccappSyncSessionMgmt"),
            sessMgmt->func_id);

    //sessionMgmt(sessMgmt);
    switch (sessMgmt->func_id) {
    case SESSION_MGMT_SET_TIME:
        g_deviceInfo.reg_time = sessMgmt->data.time.gmt_time;
        CCAPP_DEBUG(DEB_F_PREFIX"Setting reg_time to == %lld\n",
                           DEB_F_PREFIX_ARGS(SIP_CC_PROV,
                                "ccappSyncSessionMgmt"), g_deviceInfo.reg_time);
        platSetCucmRegTime();
        break;
    case SESSION_MGMT_GET_PHRASE_TEXT:
        sessMgmt->data.phrase_text.ret_val =
        platGetPhraseText(sessMgmt->data.phrase_text.ndx,
                sessMgmt->data.phrase_text.outstr,
                sessMgmt->data.phrase_text.len);
        break;
    case SESSION_MGMT_GET_UNREG_REASON:
        sessMgmt->data.unreg_reason.unreg_reason = platGetUnregReason();
        break;
    case SESSION_MGMT_UPDATE_KPMLCONFIG:
        platSetKPMLConfig(sessMgmt->data.kpmlconfig.kpml_val);
        break;
    case SESSION_MGMT_GET_AUDIO_DEVICE_STATUS:
        //Noop
        break;
    case SESSION_MGMT_CHECK_SPEAKER_HEADSET_MODE:
        //Noop
        break;
    case SESSION_MGMT_LINE_HAS_MWI_ACTIVE:
        line_info = ccsnap_getLineInfoFromBtn(sessMgmt->data.line_mwi_active.line);
        if (line_info != NULL) {
            sessMgmt->data.line_mwi_active.ret_val = line_info->mwi.status;
        }
        break;
    case SESSION_MGMT_APPLY_CONFIG:
        // save the proposed versions of fcp and dialplan to apply.  Will check against
        // current versions and redownload if necessary

    if (pending_action_type == NO_ACTION) {
            configApplyConfigNotify(sessMgmt->data.config.config_version_stamp,
                sessMgmt->data.config.dialplan_version_stamp,
                sessMgmt->data.config.fcp_version_stamp,
                sessMgmt->data.config.cucm_result,
                sessMgmt->data.config.load_id,
                sessMgmt->data.config.inactive_load_id,
                sessMgmt->data.config.load_server,
                sessMgmt->data.config.log_server,
                sessMgmt->data.config.ppid);
	}
        break;
    default:
        break;
    }
    freeSessionMgmtData(sessMgmt);

}

/**
 *  ccCreateSession
 *
 *      Called to create a CC session
 *
 *  @param param - ccSession_create_param_t
 *               Contains the type of session and specific data
 *
 *  @return  ccSession_id_t - id of the session created
 */
session_id_t createSessionId(line_t line, callid_t call)
{
    return ( SESSIONTYPE_CALLCONTROL << SID_TYPE_SHIFT ) +
             (line << SID_LINE_SHIFT ) + call;
}

/**
 *  getLineIdAndCallId
 *
 *      get Line and call_id
 *
 *  @param *line_id
 *  @param *call_id
 *
 */
void getLineIdAndCallId (line_t *line_id, callid_t *call_id)
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
