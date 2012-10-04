/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_PACKET_LOSS_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_PACKET_LOSS_TEST_H_

#include "modules/video_coding/codecs/test_framework/packet_loss_test.h"

class VP8PacketLossTest : public PacketLossTest
{
public:
    VP8PacketLossTest();
    VP8PacketLossTest(double lossRate, bool useNack, int rttFrames);

protected:
    VP8PacketLossTest(std::string name, std::string description);
    virtual int ByteLoss(int size, unsigned char *pkg, int bytesToLose);
    WebRtc_Word32 ReceivedDecodedReferenceFrame(const WebRtc_UWord64 pictureId);
    // |lossRate| is the probability of packet loss between 0 and 1.
    // |numLosses| is the number of packets already lost in the current frame.
    virtual bool PacketLoss(double lossRate, int numLosses);

    webrtc::CodecSpecificInfo* CreateEncoderSpecificInfo() const;

};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_PACKET_LOSS_TEST_H_
