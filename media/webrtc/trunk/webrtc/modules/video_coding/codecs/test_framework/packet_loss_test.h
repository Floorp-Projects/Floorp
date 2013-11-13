/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_PACKET_LOSS_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_PACKET_LOSS_TEST_H_

#include <list>

#include "webrtc/modules/video_coding/codecs/test_framework/normal_async_test.h"

class PacketLossTest : public NormalAsyncTest
{
public:
    PacketLossTest();
    virtual ~PacketLossTest() {if(_lastFrame) {delete [] _lastFrame; _lastFrame = NULL;}}
    virtual void Encoded(const webrtc::EncodedImage& encodedImage);
    virtual void Decoded(const webrtc::I420VideoFrame& decodedImage);
protected:
    PacketLossTest(std::string name, std::string description);
    PacketLossTest(std::string name,
                   std::string description,
                   double lossRate,
                   bool useNack,
                   unsigned int rttFrames = 0);

    virtual void Setup();
    virtual void Teardown();
    virtual void CodecSpecific_InitBitrate();
    virtual int DoPacketLoss();
    virtual int NextPacket(int size, unsigned char **pkg);
    virtual int ByteLoss(int size, unsigned char *pkg, int bytesToLose);
    virtual void InsertPacket(webrtc::VideoFrame *buf, unsigned char *pkg,
                              int size);
    int _inBufIdx;
    int _outBufIdx;

    // When NACK is being simulated _lossProbabilty is zero,
    // otherwise it is set equal to _lossRate.
    // Desired channel loss rate.
    double _lossRate;
    // Probability used to simulate packet drops.
    double _lossProbability;

    int _totalKept;
    int _totalThrown;
    int _sumChannelBytes;
    std::list<uint32_t> _frameQueue;
    uint8_t* _lastFrame;
    uint32_t _lastFrameLength;
};


#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_PACKET_LOSS_TEST_H_
