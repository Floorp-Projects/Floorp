/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "packet_loss_test.h"
#include <cassert>

VP8PacketLossTest::VP8PacketLossTest()
:
PacketLossTest("VP8PacketLossTest", "Encode, remove lost packets, decode")
{
}

VP8PacketLossTest::VP8PacketLossTest(std::string name, std::string description)
:
PacketLossTest(name, description)
{
}

VP8PacketLossTest::VP8PacketLossTest(double lossRate,
                                     bool useNack,
                                     int rttFrames)
:
PacketLossTest("VP8PacketLossTest", "Encode, remove lost packets, decode",
               lossRate, useNack, rttFrames)
{
}

int VP8PacketLossTest::ByteLoss(int size, unsigned char* /* pkg */, int bytesToLose)
{
    int retLength = size - bytesToLose;
    if (retLength < 4)
    {
        retLength = 4;
    }
    return retLength;
}

WebRtc_Word32
VP8PacketLossTest::ReceivedDecodedReferenceFrame(const WebRtc_UWord64 pictureId)
{
    _pictureIdRPSI = pictureId;
    _hasReceivedRPSI = true;
    return 0;
}

webrtc::CodecSpecificInfo*
VP8PacketLossTest::CreateEncoderSpecificInfo() const
{
    webrtc::CodecSpecificInfo* vp8CodecSpecificInfo =
      new webrtc::CodecSpecificInfo();
    vp8CodecSpecificInfo->codecType = webrtc::kVideoCodecVP8;
    vp8CodecSpecificInfo->codecSpecific.VP8.hasReceivedRPSI = _hasReceivedRPSI;
    vp8CodecSpecificInfo->codecSpecific.VP8.pictureIdRPSI = _pictureIdRPSI;
    vp8CodecSpecificInfo->codecSpecific.VP8.hasReceivedSLI = _hasReceivedSLI;
    vp8CodecSpecificInfo->codecSpecific.VP8.pictureIdSLI = _pictureIdSLI;

    _hasReceivedSLI = false;
    _hasReceivedRPSI = false;

    return vp8CodecSpecificInfo;
}

bool VP8PacketLossTest::PacketLoss(double lossRate, int numLosses) {
  if (numLosses)
    return true;
  return RandUniform() < lossRate;
}
