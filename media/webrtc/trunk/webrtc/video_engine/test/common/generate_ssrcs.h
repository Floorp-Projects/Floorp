/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_GENERATE_SSRCS_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_GENERATE_SSRCS_H_

#include <map>
#include <vector>

#include "webrtc/typedefs.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

namespace webrtc {
namespace test {

void GenerateRandomSsrcs(newapi::VideoSendStream::Config* config,
                         std::map<uint32_t, bool>* reserved_ssrcs) {
  size_t num_ssrcs = config->codec.numberOfSimulcastStreams;
  std::vector<uint32_t>* ssrcs = &config->rtp.ssrcs;
  assert(ssrcs->size() == 0);

  if (num_ssrcs == 0) {
    num_ssrcs = 1;
  }

  while (ssrcs->size() < num_ssrcs) {
    uint32_t rand_ssrc = static_cast<uint32_t>(rand() + 1);  // NOLINT

    // Make sure the ssrc is unique per-call
    while (true) {
      if (!(*reserved_ssrcs)[rand_ssrc]) {
        (*reserved_ssrcs)[rand_ssrc] = true;
        break;
      }
      ++rand_ssrc;
    }

    ssrcs->push_back(rand_ssrc);
  }
}
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_GENERATE_SSRCS_H_
