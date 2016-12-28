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

#include "webrtc/base/checks.h"

namespace webrtc {

SLDataFormat_PCM CreatePcmConfiguration(int sample_rate);

// Helper class for using SLObjectItf interfaces.
template <typename SLType, typename SLDerefType>
class ScopedSLObject {
 public:
  ScopedSLObject() : obj_(nullptr) {}

  ~ScopedSLObject() { Reset(); }

  SLType* Receive() {
    RTC_DCHECK(!obj_);
    return &obj_;
  }

  SLDerefType operator->() { return *obj_; }

  SLType Get() const { return obj_; }

  void Reset() {
    if (obj_) {
      (*obj_)->Destroy(obj_);
      obj_ = nullptr;
    }
  }

 private:
  SLType obj_;
};

typedef ScopedSLObject<SLObjectItf, const SLObjectItf_*> ScopedSLObjectItf;

}  // namespace webrtc_opensl

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_COMMON_H_
