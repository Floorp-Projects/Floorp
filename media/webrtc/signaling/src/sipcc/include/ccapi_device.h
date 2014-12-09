/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_DEVICE_H_
#define _CCAPI_DEVICE_H_

#include "ccapi_types.h"


/**
 * Get device ID
 * @return cc_deviceinfo_ref_t - reference handle of the device
 */
cc_device_handle_t CCAPI_Device_getDeviceID();

/**
 * Get device reference handle
 * @param [in] id - device ID
 * @return cc_deviceinfo_ref_t - reference handle of the device
 * NOTE: The info returned by this method must be released using CCAPI_Device_releaseDeviceInfo()
 */
cc_deviceinfo_ref_t CCAPI_Device_getDeviceInfo(cc_device_handle_t id);

/**
 * Set full path of device configuration file. API will download and parse the config file using
 * provided location.
 * @param handle - device handle
 * @param [in] file_path - device config file full path
 * @return void
 */
void CCAPI_Device_configUpdate(cc_device_handle_t handle, file_path_t file_path);

/**
 * Retain the deviceInfo snapshot - this info shall not be freed until a
 * CCAPI_Device_releaseDeviceInfo() API releases this resource.
 * @param [in] ref - refrence to the block to be freed
 * @return void
 * NOTE: Application may use this API to retain the device info using this API inside
 * CCAPI_DeviceListener_onDeviceEvent() App must release the Info using
 * CCAPI_Device_releaseDeviceInfo() once it is done with it.
 */
void CCAPI_Device_retainDeviceInfo(cc_deviceinfo_ref_t ref);

/**
 * Release the deviceInfo snapshot
 * @param [in] ref - reference to the block to be freed
 * @return void
 */
void CCAPI_Device_releaseDeviceInfo(cc_deviceinfo_ref_t ref);

/**
 * Create a call on the device
 * Line selection is on the first available line. Lines that have there MNC reached will be skipped.
 * @param handle - device handle
 * @return cc_call_handle_t - handle of the call created
 */
cc_call_handle_t CCAPI_Device_CreateCall(cc_device_handle_t handle);

/**
 * Enable or disable video capability of the device.
 * @param handle - device handle
 * @param [in] enable - a flag to indicate that application wants to enable of
 * disable video capability of the device.
 * @return void
 */
void CCAPI_Device_enableVideo(cc_device_handle_t handle, cc_boolean enable);

/**
 * Enable or disable camera capability of the device.
 * @param handle - device handle
 * @param [in] enable - a flag to indicate that application wants to enable of
 * disable camera capability of the device.
 * @return void
 */
void CCAPI_Device_enableCamera(cc_device_handle_t handle, cc_boolean enable);


/**
 * CCAPI_Device_setDigestNamePasswd
 *
 * @param handle - device handle
 * @param name - The Digest auth name
 * @param passwd - The password for that name for the line
 * @return void
 */
void CCAPI_Device_setDigestNamePasswd (cc_device_handle_t handle,
                                       char *name, char *pw);


/**
 * CCAPI_Device_IP_Update
 *
 * There is a update in the IP address and the values of new set
 * of signaling and media IP addresses are provided.
 * These value are compared with the current IP address values
 * and depending on what changed, restart and/or re-invite
 * action is taken.
 *
 * The case being addressed.
 * 1) If the signaling IP change  happens during a call,
 *    the change is deferred till phone is idle.
 * 2)If media IP change happens during a call, it is applied immediately.
 * 3) If both change, and call is active, that is treated same
 *    combination of case 1) and 2).
 * 4) If no call is present, and signaling IP change,
 *    sipcc will re-register with new IP.
 *
 * @param handle - device handle
 * @param signaling_ip - IP address that Must be used for signalling
 * @param signaling_interface - Interface name associated with signaling IP
 * @param signaling_int_type - Interface Type associated with signaling IP
 * @param media_ip - IP address that Must be used for media
 * @param media_interface - Interface name associated with Media IP
 * @param media_int_type - Interface Type associated with Media IP
 * @return void
 */
void CCAPI_Device_IP_Update (cc_device_handle_t handle,
                              const char *signaling_ip,
                              const char *sig_interface,
                              int sig_int_type,
                              const char *media_ip,
                              const char *media_interface,
                              int media_int_type);


/**
 * CCAPI_Device_setVideoAutoTxPreference
 *
 * @param handle - device handle
 * @param txPref - TRUE=> auto Tx Video prefered
 * @return void
 */
void CCAPI_Device_setVideoAutoTxPreference (cc_device_handle_t handle, cc_boolean txPref);

#endif /* _CCAPIAPI_DEVICE_H_ */
