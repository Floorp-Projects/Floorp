/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INTERFACE_HELPERS_ANDROID_H_
#define WEBRTC_MODULES_UTILITY_INTERFACE_HELPERS_ANDROID_H_

#include <jni.h>

namespace webrtc {

// Attach thread to JVM if necessary and detach at scope end if originally
// attached.
class AttachThreadScoped {
 public:
  explicit AttachThreadScoped(JavaVM* jvm);
  ~AttachThreadScoped();
  JNIEnv* env();

 private:
  bool attached_;
  JavaVM* jvm_;
  JNIEnv* env_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_UTILITY_INTERFACE_HELPERS_ANDROID_H_
