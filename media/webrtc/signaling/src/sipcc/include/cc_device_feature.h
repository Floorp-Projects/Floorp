/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_DEVICE_FEATURE_H_
#define _CC_DEVICE_FEATURE_H_

#include "cc_constants.h"


/**
 * Enable or disable video capability of the device.
 * @param enable - a flag to indicate that application wants to enable of
 * disable video capability of the device.
 * @return void
 */
void CC_DeviceFeature_enableVideo(cc_boolean enable);

/**
 * Indicate native video support for the device
 * @param enable - a flag to indicate that device supports native video
 * @return void
 */
void CC_DeviceFeature_supportsVideo(cc_boolean enable);

/**
 * Enable or disable camera capability of the device.
 * @param enable - a flag to indicate that application wants to enable of
 * disable camera capability of the device.
 * @return void
 */
void CC_DeviceFeature_enableCamera(cc_boolean enable);


#endif /* _CC_DEVICE_FEATURE_H_ */
