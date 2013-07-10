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

#include "video_coding.h"
#include "module_common_types.h"
#include "common_types.h"
#include "rtp_rtcp.h"
#include "typedefs.h"
#include "test_util.h"

#include <string>
#include <stdio.h>

class RtpDataCallback : public webrtc::RtpData {
 public:
  RtpDataCallback(webrtc::VideoCodingModule* vcm) : vcm_(vcm) {}
  virtual ~RtpDataCallback() {}

  virtual WebRtc_Word32 OnReceivedPayloadData(
      const WebRtc_UWord8* payload_data,
      const WebRtc_UWord16 payload_size,
      const webrtc::WebRtcRTPHeader* rtp_header) {
    return vcm_->IncomingPacket(payload_data, payload_size, *rtp_header);
  }

 private:
  webrtc::VideoCodingModule* vcm_;
};

int RtpPlay(const CmdArgs& args);
int RtpPlayMT(const CmdArgs& args);
int ReceiverTimingTests(CmdArgs& args);
int JitterBufferTest(CmdArgs& args);
int DecodeFromStorageTest(const CmdArgs& args);

// Thread functions:
bool ProcessingThread(void* obj);
bool RtpReaderThread(void* obj);
bool DecodeThread(void* obj);
bool NackThread(void* obj);

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_RECEIVER_TESTS_H_
