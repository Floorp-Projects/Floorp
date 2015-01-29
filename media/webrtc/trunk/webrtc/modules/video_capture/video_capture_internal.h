/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_INTERNAL_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_INTERNAL_H_

#ifdef ANDROID
#include <jni.h>

namespace webrtc {

// In order to be able to use the internal webrtc video capture
// for android, the jvm objects must be set via this method.
int32_t SetCaptureAndroidVM(JavaVM* javaVM, jobject context);

}  // namespace webrtc

#endif  // ANDROID

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_INTERNAL_H_
