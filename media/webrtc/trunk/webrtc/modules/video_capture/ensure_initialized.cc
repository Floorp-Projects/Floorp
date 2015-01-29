/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Platform-specific initialization bits, if any, go here.

#ifndef ANDROID

namespace webrtc {
namespace videocapturemodule {
void EnsureInitialized() {}
}  // namespace videocapturemodule
}  // namespace webrtc

#else

#include <pthread.h>

#include "base/android/jni_android.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/video_capture/video_capture_internal.h"

namespace webrtc {
namespace videocapturemodule {

static pthread_once_t g_initialize_once = PTHREAD_ONCE_INIT;

void EnsureInitializedOnce() {
  JNIEnv* jni = ::base::android::AttachCurrentThread();
  jobject context = ::base::android::GetApplicationContext();
  JavaVM* jvm = NULL;
  CHECK_EQ(0, jni->GetJavaVM(&jvm));
  CHECK_EQ(0, webrtc::SetCaptureAndroidVM(jvm, context));
}

void EnsureInitialized() {
  CHECK_EQ(0, pthread_once(&g_initialize_once, &EnsureInitializedOnce));
}

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // !ANDROID
