/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(mflodman) Remove this class and use ref count class in system_wrappers.

#ifndef WEBRTC_VIDEO_ENGINE_VIE_REF_COUNT_H_
#define WEBRTC_VIDEO_ENGINE_VIE_REF_COUNT_H_

#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CriticalSectionWrapper;

class ViERefCount {
 public:
  ViERefCount();
  ~ViERefCount();

  ViERefCount& operator++(int);  // NOLINT
  ViERefCount& operator--(int);  // NOLINT

  void Reset();
  int GetCount() const;

 private:
  volatile int count_;
  scoped_ptr<CriticalSectionWrapper> crit_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_REF_COUNT_H_
