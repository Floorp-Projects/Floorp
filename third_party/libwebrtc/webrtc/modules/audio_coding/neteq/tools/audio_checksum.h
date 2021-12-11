/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_AUDIO_CHECKSUM_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_AUDIO_CHECKSUM_H_

#include <string>

#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/md5digest.h"
#include "rtc_base/stringencode.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {
namespace test {

class AudioChecksum : public AudioSink {
 public:
  AudioChecksum() : finished_(false) {}

  bool WriteArray(const int16_t* audio, size_t num_samples) override {
    if (finished_)
      return false;

#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Big-endian gives a different checksum"
#endif
    checksum_.Update(audio, num_samples * sizeof(*audio));
    return true;
  }

  // Finalizes the computations, and returns the checksum.
  std::string Finish() {
    if (!finished_) {
      finished_ = true;
      checksum_.Finish(checksum_result_, rtc::Md5Digest::kSize);
    }
    return rtc::hex_encode(checksum_result_, rtc::Md5Digest::kSize);
  }

 private:
  rtc::Md5Digest checksum_;
  char checksum_result_[rtc::Md5Digest::kSize];
  bool finished_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AudioChecksum);
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_AUDIO_CHECKSUM_H_
