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
