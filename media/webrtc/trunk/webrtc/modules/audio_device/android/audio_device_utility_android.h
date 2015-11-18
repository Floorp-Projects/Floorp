/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Android audio device utility interface
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_ANDROID_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_ANDROID_H

#include <jni.h>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/audio_device_utility.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc {

// TODO(henrika): this utility class is not used but I would like to keep this
// file for the other helper methods which are unique for Android.
class AudioDeviceUtilityAndroid: public AudioDeviceUtility {
 public:
    AudioDeviceUtilityAndroid(const int32_t id);
    ~AudioDeviceUtilityAndroid();

    virtual int32_t Init();
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_ANDROID_H
