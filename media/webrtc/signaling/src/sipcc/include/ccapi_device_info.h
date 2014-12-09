/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_DEVICE_INFO_H_
#define _CCAPI_DEVICE_INFO_H_

#include "ccapi_types.h"

/**
 * gets the device name
 * @returns - a pointer to the device name
 */
cc_deviceinfo_ref_t CCAPI_DeviceInfo_getDeviceHandle() ;

/**
 * gets the device name
 * @param [in] handle - reference to device info
 * @returns - a pointer to the device name
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_DeviceInfo_getDeviceName(cc_deviceinfo_ref_t handle) ;

/**
 * gets the device idle status
 * @param [in] handle - reference to device info
 * @returns boolean - idle status
 */
cc_boolean CCAPI_DeviceInfo_isPhoneIdle(cc_deviceinfo_ref_t handle) ;

/**
 * gets the service state
 * @param [in] handle - reference to device info
 * @returns cc_service_state_t - INS/OOS
 */
cc_service_state_t CCAPI_DeviceInfo_getServiceState(cc_deviceinfo_ref_t handle) ;

/**
 * gets the service cause
 * @param [in] handle - reference to device info
 * @returns cc_service_cause_t - reason for service state
 */
cc_service_cause_t CCAPI_DeviceInfo_getServiceCause(cc_deviceinfo_ref_t handle) ;

/**
 * gets the cucm mode
 * @param [in] handle - reference to device info
 * @returns cc_cucm_mode_t - CUCM mode
 */
cc_cucm_mode_t CCAPI_DeviceInfo_getCUCMMode(cc_deviceinfo_ref_t handle) ;

/**
 * gets list of handles to calls on the device
 * @param [in] handle - reference to device info
 * @param [out] handles - array of call handle to be returned
 * @param [in,out] count number allocated in array/elements returned
 * @returns
 */
void CCAPI_DeviceInfo_getCalls(cc_deviceinfo_ref_t handle, cc_call_handle_t handles[], cc_uint16_t *count) ;

/**
 * gets list of handles to calls on the device by state
 * @param [in] handle - reference to device info
 * @param [in] state - call state for which the calls are requested
 * @param [out] handles - array of call handle to be returned
 * @param [in,out] count number allocated in array/elements returned
 * @returns
 */
void CCAPI_DeviceInfo_getCallsByState(cc_deviceinfo_ref_t handle, cc_call_state_t state,
                cc_call_handle_t handles[], cc_uint16_t *count) ;

/**
 * gets list of handles to lines on the device
 * @param [in] handle - reference to device info
 * @param [in] handles - array of line handle to be returned
 * @param [in,out] count - pointer to count in array in-> memory allocated out-> num populated
 * @returns
 */
void CCAPI_DeviceInfo_getLines(cc_deviceinfo_ref_t handle, cc_lineid_t handles[], cc_uint16_t *count) ;

/**
 * gets list of handles to features on the device
 * @param [in] handle - reference to device info
 * @param [in] handles - array of feature handle to be returned
 * @param [in,out] count - pointer to count in array in-> memory allocated out-> num populated
 * @returns
 */
void CCAPI_DeviceInfo_getFeatures(cc_deviceinfo_ref_t handle, cc_featureinfo_ref_t handles[], cc_uint16_t *count) ;

/**
 * gets handles of call agent servers
 * @param [in] handle - reference to device info
 * @param [in] handles - array of handles to call agent servers
 * @param [in,out] count - pointer to count in array in-> memory allocated out-> num populated
 * @returns
 */
void CCAPI_DeviceInfo_getCallServers(cc_deviceinfo_ref_t handle, cc_callserver_ref_t handles[], cc_uint16_t *count) ;

/**
 * gets call server name
 * @param [in] handle - handle of call server
 * @returns name of the call server
 * NOTE: The memory for return string doesn't need to be freed it will be freed when the info reference is freed
 */
cc_string_t CCAPI_DeviceInfo_getCallServerName(cc_callserver_ref_t handle);

/**
 * gets call server mode
 * @param [in] handle - handle of call server
 * @returns - mode of the call server
 */
cc_cucm_mode_t CCAPI_DeviceInfo_getCallServerMode(cc_callserver_ref_t handle);

/**
 * gets calls erver name
 * @param [in] handle - handle of call server
 * @returns status of the call server
 */
cc_ccm_status_t CCAPI_DeviceInfo_getCallServerStatus(cc_callserver_ref_t handle);

/**
 * get the NOTIFICATION PROMPT
 * @param [in] handle - reference to device info
 * @returns
 */
cc_string_t CCAPI_DeviceInfo_getNotifyPrompt(cc_deviceinfo_ref_t handle) ;

/**
 * get the NOTIFICATION PROMPT PRIORITY
 * @param [in] handle - reference to device info
 * @returns
 */
cc_uint32_t CCAPI_DeviceInfo_getNotifyPromptPriority(cc_deviceinfo_ref_t handle) ;

/**
 * get the NOTIFICATION PROMPT Progress
 * @param [in] handle - reference to device info
 * @returns
 */
cc_uint32_t CCAPI_DeviceInfo_getNotifyPromptProgress(cc_deviceinfo_ref_t handle) ;

/**
 * gets provisioing for missed call logging
 * This value should be reread on CONFIG_UPDATE event
 * @param [in] handle - reference to device info
 * @returns boolean - false => disabled true => enabled
 */
cc_boolean CCAPI_DeviceInfo_isMissedCallLoggingEnabled (cc_deviceinfo_ref_t handle);

/**
 * gets provisioing for placed call logging
 * This value should be reread on CONFIG_UPDATE event
 * @param [in] handle - reference to device info
 * @returns boolean - false => disabled true => enabled
 */
cc_boolean CCAPI_DeviceInfo_isPlacedCallLoggingEnabled (cc_deviceinfo_ref_t handle);

/**
 * gets provisioing for received call logging
 * This value should be reread on CONFIG_UPDATE event
 * @param [in] handle - reference to device info
 * @returns boolean - false => disabled true => enabled
 */
cc_boolean CCAPI_DeviceInfo_isReceivedCallLoggingEnabled (cc_deviceinfo_ref_t handle);

/**
 * gets register/in service time
 * This value should be read once the register completes successfully
 * @param [in] handle - reference to device info
 * @returns long - time registration completed successfully.
 */
long long CCAPI_DeviceInfo_getRegTime (cc_deviceinfo_ref_t handle);

/**
 * Returns dot notation IP address used for registering phone. If phone is not
 * registered, then "0.0.0.0" is returned.
 * @param [in] handle - reference to device info
 * @return  cc_string_t  IP address used to register phone.
 */
cc_string_t CCAPI_DeviceInfo_getSignalingIPAddress(cc_deviceinfo_ref_t handle);

/**
 * Returns camera admin enable/disable status
 * @param [in] handle - reference to device info
 * @return  cc_boolean  - TRUE => enabled
 */
cc_boolean CCAPI_DeviceInfo_isCameraEnabled(cc_deviceinfo_ref_t handle);

/**
 * Returns admin enable/disable video capablity for device
 * @param [in] handle - reference to device info
 * @return  cc_boolean  - TRUE => enabled
 */
cc_boolean CCAPI_DeviceInfo_isVideoCapEnabled(cc_deviceinfo_ref_t handle);

/**
 * gets the device mwi_lamp state
 * @param [in] handle - reference to device info
 * @returns boolean - mwi_lamp state
 */
cc_boolean CCAPI_DeviceInfo_getMWILampState(cc_deviceinfo_ref_t handle);

#endif /* _CCAPIAPI_DEVICE_INFO_H_ */
