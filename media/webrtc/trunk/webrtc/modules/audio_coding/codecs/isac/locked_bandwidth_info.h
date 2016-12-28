/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_LOCKED_BANDWIDTH_INFO_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_LOCKED_BANDWIDTH_INFO_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_coding/codecs/isac/bandwidth_info.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

// An IsacBandwidthInfo that's safe to access from multiple threads because
// it's protected by a mutex.
class LockedIsacBandwidthInfo final {
 public:
  LockedIsacBandwidthInfo();
  ~LockedIsacBandwidthInfo();

  IsacBandwidthInfo Get() const {
    CriticalSectionScoped cs(lock_.get());
    return bwinfo_;
  }

  void Set(const IsacBandwidthInfo& bwinfo) {
    CriticalSectionScoped cs(lock_.get());
    bwinfo_ = bwinfo;
  }

 private:
  const rtc::scoped_ptr<CriticalSectionWrapper> lock_;
  IsacBandwidthInfo bwinfo_ GUARDED_BY(lock_);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_LOCKED_BANDWIDTH_INFO_H_
