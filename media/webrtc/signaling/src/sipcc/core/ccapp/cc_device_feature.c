/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cc_device_feature.h"
#include "sessionConstants.h"
#include "sessionTypes.h"
#include "CCProvider.h"
#include "phone_debug.h"
#include "ccapp_task.h"

/**
 * Internal method
 */
void cc_invokeDeviceFeature(session_feature_t *feature) {

    if (ccappTaskPostMsg(CCAPP_INVOKEPROVIDER_FEATURE, feature,
                       sizeof(session_feature_t), CCAPP_CCPROVIER) == CPR_FAILURE) {
        CCAPP_DEBUG(DEB_F_PREFIX"cc_invokeDeviceFeature failed",
                DEB_F_PREFIX_ARGS("cc_device_feature", "cc_invokeDeviceFeature"));
    }

}

void CC_DeviceFeature_supportsVideo(boolean enable) {
    session_feature_t feat;

    feat.session_id = (SESSIONTYPE_CALLCONTROL << CC_SID_TYPE_SHIFT);
    feat.featureID = DEVICE_SUPPORTS_NATIVE_VIDEO;
    feat.featData.ccData.info = NULL;
    feat.featData.ccData.info1 = NULL;
    feat.featData.ccData.state = enable;
    cc_invokeDeviceFeature(&feat);
}

/**
 * Enable video/camera.
 * @param enable true or false
 * @return void
 */
void CC_DeviceFeature_enableVideo(boolean enable) {
    session_feature_t feat;

    feat.session_id = (SESSIONTYPE_CALLCONTROL << CC_SID_TYPE_SHIFT);
    feat.featureID = DEVICE_ENABLE_VIDEO;
    feat.featData.ccData.info = NULL;
    feat.featData.ccData.info1 = NULL;
    feat.featData.ccData.state = enable;
    cc_invokeDeviceFeature(&feat);
}

void CC_DeviceFeature_enableCamera(boolean enable) {
    session_feature_t feat;

    feat.session_id = (SESSIONTYPE_CALLCONTROL << CC_SID_TYPE_SHIFT);
    feat.featureID = DEVICE_ENABLE_CAMERA;
    feat.featData.ccData.info = NULL;
    feat.featData.ccData.info1 = NULL;
    feat.featData.ccData.state = enable;
    cc_invokeDeviceFeature(&feat);
}

