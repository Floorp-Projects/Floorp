/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_RECEIVER_TESTS_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_RECEIVER_TESTS_H_

#include "webrtc/common_types.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/modules/video_coding/main/test/video_source.h"
#include "webrtc/typedefs.h"

#include <stdio.h>
#include <string>

class RtpDataCallback : public webrtc::NullRtpData {
 public:
  RtpDataCallback(webrtc::VideoCodingModule* vcm) : vcm_(vcm) {}
  virtual ~RtpDataCallback() {}

  int32_t OnReceivedPayloadData(
      const uint8_t* payload_data,
      const size_t payload_size,
      const webrtc::WebRtcRTPHeader* rtp_header) override {
    return vcm_->IncomingPacket(payload_data, payload_size, *rtp_header);
  }

 private:
  webrtc::VideoCodingModule* vcm_;
};

int RtpPlay(const CmdArgs& args);

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_RECEIVER_TESTS_H_
