/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/ensure_initialized.h"

#include <pthread.h>

#include "base/android/jni_android.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/android/audio_device_template.h"
#include "webrtc/modules/audio_device/android/audio_record_jni.h"
#include "webrtc/modules/audio_device/android/audio_track_jni.h"
#include "webrtc/modules/audio_device/android/opensles_input.h"
#include "webrtc/modules/audio_device/android/opensles_output.h"

namespace webrtc {
namespace audiodevicemodule {

static pthread_once_t g_initialize_once = PTHREAD_ONCE_INIT;

void EnsureInitializedOnce() {
  CHECK(::base::android::IsVMInitialized());
  JNIEnv* jni = ::base::android::AttachCurrentThread();
  JavaVM* jvm = NULL;
  CHECK_EQ(0, jni->GetJavaVM(&jvm));
  jobject context = ::base::android::GetApplicationContext();

  // Provide JVM and context to Java and OpenSL ES implementations.
  using AudioDeviceJava = AudioDeviceTemplate<AudioRecordJni, AudioTrackJni>;
  AudioDeviceJava::SetAndroidAudioDeviceObjects(jvm, context);

  // TODO(henrika): enable OpenSL ES when it has been refactored to avoid
  // crashes.
  // using AudioDeviceOpenSLES =
  //    AudioDeviceTemplate<OpenSlesInput, OpenSlesOutput>;
  // AudioDeviceOpenSLES::SetAndroidAudioDeviceObjects(jvm, context);
}

void EnsureInitialized() {
  CHECK_EQ(0, pthread_once(&g_initialize_once, &EnsureInitializedOnce));
}

}  // namespace audiodevicemodule
}  // namespace webrtc
