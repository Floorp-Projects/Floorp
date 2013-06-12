/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_

#include <list>

#include "typedefs.h"

namespace webrtc {

class BitRateStats {
 public:
  BitRateStats();
  ~BitRateStats();

  void Init();
  void Update(uint32_t packet_size_bytes, int64_t now_ms);
  uint32_t BitRate(int64_t now_ms);

 private:
  struct DataTimeSizeTuple {
    DataTimeSizeTuple(uint32_t size_bytes_in, int64_t time_complete_ms_in)
        : size_bytes(size_bytes_in),
          time_complete_ms(time_complete_ms_in) {
    }

    uint32_t size_bytes;
    int64_t time_complete_ms;
  };

  void EraseOld(int64_t now_ms);

  std::list<DataTimeSizeTuple*> data_samples_;
  uint32_t accumulated_bytes_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_
