/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "string_lib.h"
#include "cc_constants.h"
#include "ccapi_snapshot.h"
#include "ccapi_device_info.h"
#include "sessionHash.h"
#include "CCProvider.h"
#include "phone_debug.h"
#include "ccapi_snapshot.h"
#include "ccapi_device_info.h"
#include "util_string.h"
#include "CSFLog.h"

static const char* logTag = "ccapi_device_info";

/**
 * gets the device name
 * @returns - a pointer to the device name
 */
cc_deviceinfo_ref_t CCAPI_DeviceInfo_getDeviceHandle ()
{
   static const char *fname="CCAPI_DeviceInfo_getDeviceHandle";
   CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
   CCAPP_DEBUG(DEB_F_PREFIX"returned 0 (default)", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
   return 0;
}

/**
 * gets the device name
 * @returns - a pointer to the device name
 */
cc_string_t CCAPI_DeviceInfo_getDeviceName (cc_deviceinfo_ref_t handle)
{
  CSFLogDebug(logTag, "Call to deprecated function %s, returning empty string", __FUNCTION__);
  return strlib_empty();
}

/**
 * gets the device idle status
 * @param [in] handle - reference to device info
 * @returns boolean - idle status
 */
cc_boolean CCAPI_DeviceInfo_isPhoneIdle(cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_isPhoneIdle";
    boolean ret = TRUE;
    hashItr_t itr;
    session_data_t * session_data;
    cc_call_state_t call_state;

    hashItrInit(&itr);

    while ((session_data = hashItrNext(&itr)) != NULL) {
        call_state = session_data->state;
        if (call_state != ONHOOK &&
            call_state != REMINUSE) {
            ret = FALSE;
            break;
        }
    }
    CCAPP_DEBUG(DEB_F_PREFIX"idle state=%d session_id=0x%x call-state=%d handle=%p",
        DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), ret,
        (session_data != NULL)? session_data->sess_id: 0,
        (session_data != NULL)? session_data->state: 0,
        (handle)? handle:0);
    return ret;

}

/**
 * gets the service state
 * @param [in] handle - reference to device info
 * @returns cc_service_state_t - INS/OOS
 */
cc_service_state_t CCAPI_DeviceInfo_getServiceState (cc_deviceinfo_ref_t handle)
{
  static const char *fname="CCAPI_DeviceInfo_getServiceState";
  cc_device_info_t *device = handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( device != NULL ) {
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device->ins_state);
     return device->ins_state;
  }

  return CC_STATE_IDLE;
}

/**
 * gets the service cause
 * @param [in] handle - reference to device info
 * @returns cc_service_cause_t - reason for service state
 */
cc_service_cause_t CCAPI_DeviceInfo_getServiceCause (cc_deviceinfo_ref_t handle)
{
  static const char *fname="CCAPI_DeviceInfo_getServiceCause";
  cc_device_info_t *device = handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( device != NULL ) {
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device->ins_cause);
     return device->ins_cause;
  }

  return CC_CAUSE_NONE;

}

/**
 * gets the cucm mode
 * @returns cc_cucm_mode_t - CUCM mode
 */
cc_cucm_mode_t CCAPI_DeviceInfo_getCUCMMode (cc_deviceinfo_ref_t handle)
{
  static const char *fname="CCAPI_DeviceInfo_getCUCMMode";
  cc_device_info_t *device = handle;
  CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

  if ( device != NULL ) {
     CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device->cucm_mode);
     return device->cucm_mode;
  }

  return CC_MODE_INVALID;
}

/**
 * gets list of handles to calls on the device
 * @param handle - device handle
 * @param handles - array of call handle to be returned
 * @param count[in/out] number allocated in array/elements returned
 * @returns
 */
void CCAPI_DeviceInfo_getCalls (cc_deviceinfo_ref_t handle, cc_call_handle_t handles[], cc_uint16_t *count)
{
    static const char *fname="CCAPI_DeviceInfo_getCalls";
    hashItr_t itr;
    session_data_t *data;
    int i=0;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    hashItrInit(&itr);
    while ( (data = (session_data_t*)hashItrNext(&itr)) != NULL &&
              i<*count ) {
        handles[i++] = CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id);
    }
    *count=i;
    CCAPP_DEBUG(DEB_F_PREFIX"Finished (no return)", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
}

/**
 * gets list of handles to calls on the device by State
 * @param handle - device handle
 * @param state - state for whcih calls are requested
 * @param handles - array of call handle to be returned
 * @param count[in/out] number allocated in array/elements returned
 * @returns
 */
void CCAPI_DeviceInfo_getCallsByState (cc_deviceinfo_ref_t handle, cc_call_state_t state,
                  cc_call_handle_t handles[], cc_uint16_t *count)
{
    static const char *fname="CCAPI_DeviceInfo_getCallsByState";
    hashItr_t itr;
    session_data_t *data;
    int i=0;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    hashItrInit(&itr);
    while ( (data = (session_data_t*)hashItrNext(&itr)) != NULL &&
              i<*count ) {
        if ( data->state == state ) {
            handles[i++] = CREATE_CALL_HANDLE_FROM_SESSION_ID(data->sess_id);
        }
    }
    *count=i;
    CCAPP_DEBUG(DEB_F_PREFIX"Finished (no return)", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
}

/**
 * gets list of handles to lines on the device
 * @param handles[in,out] - array of line handle to be returned
 * @param count[in/out] number allocated in array/elements returned
 * @returns
 */
void CCAPI_DeviceInfo_getLines (cc_deviceinfo_ref_t handle, cc_lineid_t handles[], cc_uint16_t *count)
{
    static const char *fname="CCAPI_DeviceInfo_getLines";
    cc_line_info_t *line;
    int i=1, j=0;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    CCAPP_DEBUG(" LINES Start ");

    while ( (line = ccsnap_getLineInfo(i++)) != NULL &&
              j<*count ) {
    	CCAPP_DEBUG(" LINE  handle[%d]=%d", j, line->button );
        /* We will use button as line handles */
        handles[j++] = line->button;
    }
    *count=j;
    CCAPP_DEBUG(DEB_F_PREFIX"Finished (no return)", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
}

/**
 * gets list of handles to features on the device
 * @param handles[in,out] - array of feature handle to be returned
 * @param count[in/out] number allocated in array/elements returned
 * @returns
 */
void CCAPI_DeviceInfo_getFeatures (cc_deviceinfo_ref_t handle, cc_featureinfo_ref_t handles[], cc_uint16_t *count)
{
    static const char *fname="CCAPI_DeviceInfo_getFeatures";
    cc_featureinfo_ref_t feature;
    int i=0, j=0;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    for (i=1;i<=MAX_CONFIG_LINES && j<*count;i++) {
    	feature = (cc_featureinfo_ref_t) ccsnap_getFeatureInfo(i);
    	if(feature != NULL){
			handles[j++] = feature;
    	}
    }
    *count=j;
    CCAPP_DEBUG(DEB_F_PREFIX"Finished (no return)", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
}

/**
 * gets handles of call agent servers
 * @param handles - array of handles to call agent servers
 * @param count[in/out] number allocated in array/elements returned
 * @returns
 */
void CCAPI_DeviceInfo_getCallServers (cc_deviceinfo_ref_t handle, cc_callserver_ref_t handles[], cc_uint16_t *count)
{
    static const char *fname="CCAPI_DeviceInfo_getCallServers";
    int i, j=0;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    for (i=0;i<CCAPI_MAX_SERVERS && i< *count; i++) {
        if (g_deviceInfo.ucm[i].name != 0 && strlen(g_deviceInfo.ucm[i].name)) {
	    handles[j++] = &g_deviceInfo.ucm[i];
	}
    }
    *count = j;
    CCAPP_DEBUG(DEB_F_PREFIX"Finished (no return)", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
}

/**
 * gets call server name
 * @param handle - handle of call server
 * @returns name of the call server
 * NOTE: The memory for the string will be freed once the device info reference is freed. No need to free this memory explicitly.
 */
cc_string_t CCAPI_DeviceInfo_getCallServerName (cc_callserver_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_getCallServerName";
    cc_call_server_t *ref = (cc_call_server_t *) handle;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if (ref != NULL && ref->name != 0) {
        CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), ref->name);
	    return ref->name;
    }
    return strlib_empty();
}

/**
 * gets call server mode
 * @param handle - handle of call server
 * @returns - mode of the call server
 */
cc_cucm_mode_t CCAPI_DeviceInfo_getCallServerMode (cc_callserver_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_getCallServerMode";
    cc_call_server_t *ref = (cc_call_server_t *) handle;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if (ref != NULL) {
        CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), ref->type);
        return ref->type;
    }

    return CC_MODE_INVALID;
}

/**
 * gets calls erver name
 * @param handle - handle of call server
 * @returns status of the call server
 */
cc_ccm_status_t CCAPI_DeviceInfo_getCallServerStatus (cc_callserver_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_getCallServerStatus";
    cc_call_server_t *ref = (cc_call_server_t *) handle;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if (ref != NULL) {
        CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), (ref->status));
        return ref->status;
    }

    return CC_CCM_STATUS_NONE;
}

/**
 * get the NOTIFICATION PROMPT
 * @param [in] handle - reference to device info
 * @returns
 */
cc_string_t CCAPI_DeviceInfo_getNotifyPrompt (cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_getNotifyPrompt";
    cc_device_info_t *ref = (cc_device_info_t *) handle;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if (ref != NULL) {
        CCAPP_DEBUG(DEB_F_PREFIX"returned %p", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), (ref->not_prompt));
        return ref->not_prompt;
    }

    return strlib_empty();
}

/**
 * get the NOTIFICATION PROMPT PRIORITY
 * @param [in] handle - reference to device info
 * @returns
 */
cc_uint32_t CCAPI_DeviceInfo_getNotifyPromptPriority (cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_getNotifyPromptPriority";
    cc_device_info_t *ref = (cc_device_info_t *) handle;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if (ref != NULL) {
        CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), (ref->not_prompt_prio));
        return ref->not_prompt_prio;
    }

    return 0;
}

/**
 * get the NOTIFICATION PROMPT PROGRESS
 * @param [in] handle - reference to device info
 * @returns
 */
cc_uint32_t CCAPI_DeviceInfo_getNotifyPromptProgress (cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_getNotifyPromptProgress";
    cc_device_info_t *ref = (cc_device_info_t *) handle;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if (ref != NULL) {
        CCAPP_DEBUG(DEB_F_PREFIX"returned %02X", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), (ref->not_prompt_prog));
        return ref->not_prompt_prog;
    }

    return 0;
}

/**
 * gets provisioing for missed call logging
 * @param [in] handle - reference to device info
 * @returns boolean - false => disabled true => enabled
 */
cc_boolean CCAPI_DeviceInfo_isMissedCallLoggingEnabled (cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_isMissedCallLoggingEnabled";

    CCAPP_DEBUG(DEB_F_PREFIX" return val %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), ccsnap_isMissedCallLoggingEnabled());
    return ccsnap_isMissedCallLoggingEnabled();
}

/**
 * gets provisioing for placed call logging
 * @param [in] handle - reference to device info
 * @returns boolean - false => disabled true => enabled
 */
cc_boolean CCAPI_DeviceInfo_isPlacedCallLoggingEnabled (cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_isPlacedCallLoggingEnabled";

    CCAPP_DEBUG(DEB_F_PREFIX" return val %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), ccsnap_isPlacedCallLoggingEnabled());
    return ccsnap_isPlacedCallLoggingEnabled();
}


/**
 * gets provisioing for received call logging
 * @param [in] handle - reference to device info
 * @returns boolean - false => disabled true => enabled
 */
cc_boolean CCAPI_DeviceInfo_isReceivedCallLoggingEnabled (cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_isReceivedCallLoggingEnabled";

    CCAPP_DEBUG(DEB_F_PREFIX" return val %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), ccsnap_isReceivedCallLoggingEnabled());
    return ccsnap_isReceivedCallLoggingEnabled();
}

/**
 * gets time registration completed successfully
 * @param [in] handle - reference to device info
 * @returns long - the time registration completed successfully
 */
long long CCAPI_DeviceInfo_getRegTime (cc_deviceinfo_ref_t handle)
{
    return (g_deviceInfo.reg_time);
}

/**
 * Returns dot notation IP address phone used for registration purpose. If phone is not
 * registered, then "0.0.0.0" is returned.
 * @return  char IP address used to register phone.
 */
cc_string_t CCAPI_DeviceInfo_getSignalingIPAddress(cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_getSignalingIPAddress";
    cpr_ip_addr_t ip_addr = {0,{0}};

    sip_config_get_net_device_ipaddr(&ip_addr);
    ipaddr2dotted(g_deviceInfo.registration_ip_addr, &ip_addr);
    CCAPP_DEBUG(DEB_F_PREFIX"returned %s", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), g_deviceInfo.registration_ip_addr);
    return g_deviceInfo.registration_ip_addr;
}

/**
 * Returns camera admin enable/disable status
 * @param [in] handle - reference to device info
 * @return  cc_boolean  - TRUE => enabled
 */
cc_boolean CCAPI_DeviceInfo_isCameraEnabled(cc_deviceinfo_ref_t handle) {
   // returns the current status not the snapshot status
   return cc_media_isTxCapEnabled();
}

/**
 * Returns Video Capability admin enable/disable status
 * @param [in] handle - reference to device info
 * @return  cc_boolean  - TRUE => enabled
 */
cc_boolean CCAPI_DeviceInfo_isVideoCapEnabled(cc_deviceinfo_ref_t handle) {
	   // returns the current status not the snapshot status
	   return cc_media_isVideoCapEnabled();
}

/**
 * gets the device mwi_lamp state
 * @param [in] handle - reference to device info
 * @returns boolean - mwi_lamp state
 */
cc_boolean CCAPI_DeviceInfo_getMWILampState(cc_deviceinfo_ref_t handle)
{
    static const char *fname="CCAPI_DeviceInfo_getMWILampState";
    cc_device_info_t *device = handle;
    CCAPP_DEBUG(DEB_F_PREFIX"Entering", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    if ( device != NULL ) {
       CCAPP_DEBUG(DEB_F_PREFIX"returned %d", DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname), device->mwi_lamp);
       return device->mwi_lamp;
    }

    return FALSE;
}

