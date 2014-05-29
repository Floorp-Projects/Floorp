/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/utility/interface/helpers_android.h"

#include <assert.h>
#include <stddef.h>

namespace webrtc {

AttachThreadScoped::AttachThreadScoped(JavaVM* jvm)
    : attached_(false), jvm_(jvm), env_(NULL) {
  jint ret_val = jvm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_4);
  if (ret_val == JNI_EDETACHED) {
    // Attach the thread to the Java VM.
    ret_val = jvm_->AttachCurrentThread(&env_, NULL);
    attached_ = ret_val == JNI_OK;
    assert(attached_);
  }
}

AttachThreadScoped::~AttachThreadScoped() {
  if (attached_ && (jvm_->DetachCurrentThread() < 0)) {
    assert(false);
  }
}

JNIEnv* AttachThreadScoped::env() { return env_; }

}  // namespace webrtc
