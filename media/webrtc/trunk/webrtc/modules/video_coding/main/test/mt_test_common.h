/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Common multi-thread functionality across video coding module tests
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_MT_TEST_COMMON_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_MT_TEST_COMMON_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/test_callbacks.h"
#include "webrtc/modules/video_coding/main/test/video_source.h"

namespace webrtc {

class SendSharedState
{
public:
    SendSharedState(webrtc::VideoCodingModule& vcm, webrtc::RtpRtcp& rtp,
            CmdArgs args) :
            _vcm(vcm),
            _rtp(rtp),
            _args(args),
            _sourceFile(NULL),
            _frameCnt(0),
            _timestamp(0) {}

    webrtc::VideoCodingModule&  _vcm;
    webrtc::RtpRtcp&            _rtp;
    CmdArgs                     _args;
    FILE*                       _sourceFile;
    int32_t               _frameCnt;
    int32_t               _timestamp;
};

// MT implementation of the RTPSendCompleteCallback (Transport)
class TransportCallback:public RTPSendCompleteCallback
{
 public:
    // constructor input: (receive side) rtp module to send encoded data to
    TransportCallback(Clock* clock, const char* filename = NULL);
    virtual ~TransportCallback();
    // Add packets to list
    // Incorporate network conditions - delay and packet loss
    // Actual transmission will occur on a separate thread
    int SendPacket(int channel, const void *data, int len);
    // Send to the receiver packets which are ready to be submitted
    int TransportPackets();
};

class SharedRTPState
{
public:
    SharedRTPState(webrtc::VideoCodingModule& vcm, webrtc::RtpRtcp& rtp) :
        _vcm(vcm),
        _rtp(rtp) {}
    webrtc::VideoCodingModule&  _vcm;
    webrtc::RtpRtcp&            _rtp;
};


class SharedTransportState
{
public:
    SharedTransportState(webrtc::RtpRtcp& rtp, TransportCallback& transport):
        _rtp(rtp),
        _transport(transport) {}
    webrtc::RtpRtcp&            _rtp;
    TransportCallback&          _transport;
};

bool VCMProcessingThread(void* obj);
bool VCMDecodeThread(void* obj);
bool TransportThread(void *obj);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_TEST_MT_TEST_COMMON_H_
