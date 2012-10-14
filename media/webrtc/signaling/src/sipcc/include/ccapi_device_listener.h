/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CCAPI_DEVICE_LISTENER_H_
#define _CCAPI_DEVICE_LISTENER_H_

#include "cc_constants.h"

/**
 * Generates Device event
 * @param [in] type - event type for device
 * @param [in] device_id - device id
 * @param [in] dev_info - reference to device info
 * @returns
 * NOTE: The memory associated with deviceInfo will be freed immediately upon
 * return from this method. If the application wishes to retain a copy it should
 * invoke CCAPI_Device_retainDeviceInfo() API. once the info is retained it can be
 * released by invoking CCAPI_Device_releaseDeviceInfo() API
 */
void CCAPI_DeviceListener_onDeviceEvent(ccapi_device_event_e type, cc_device_handle_t device_id, cc_deviceinfo_ref_t dev_info);

/**
 * Generates Feature event
 * @param [in] type - event type for device
 * @param [in] device_id - device id
 * @param [in] feature_info - reference to feature info
 * @returns
 * NOTE: The memory associated with featureInfo will be freed immediately upon
 * return from this method. If the application wishes to retain a copy it should
 * invoke CCAPI_Device_retainFeatureInfo() API. once the info is retained it can be
 * released by invoking CCAPI_Device_releaseFeatureInfo() API
 */
void CCAPI_DeviceListener_onFeatureEvent(ccapi_device_event_e type, cc_deviceinfo_ref_t device_info, cc_featureinfo_ref_t feature_info);

#endif /* _CCAPIAPI_DEVICE_LISTENER_H_ */
