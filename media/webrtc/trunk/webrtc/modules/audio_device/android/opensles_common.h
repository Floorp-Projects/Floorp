/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_COMMON_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_COMMON_H_

#include <SLES/OpenSLES.h>

namespace webrtc_opensl {

SLDataFormat_PCM CreatePcmConfiguration(int sample_rate);

}  // namespace webrtc_opensl

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_COMMON_H_
