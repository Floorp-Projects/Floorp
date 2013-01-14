/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_GENERIC_CODEC_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_GENERIC_CODEC_TEST_H_

#include "video_coding.h"

#include <string.h>
#include <fstream>

#include "test_callbacks.h"
#include "test_util.h"
/*
Test consists of:
1. Sanity checks
2. Bit rate validation
3. Encoder control test / General API functionality
4. Decoder control test / General API functionality

*/

namespace webrtc {

int VCMGenericCodecTest(CmdArgs& args);

class FakeTickTime;

class GenericCodecTest
{
public:
    GenericCodecTest(webrtc::VideoCodingModule* vcm,
                     webrtc::FakeTickTime* clock);
    ~GenericCodecTest();
    static int RunTest(CmdArgs& args);
    WebRtc_Word32 Perform(CmdArgs& args);
    float WaitForEncodedFrame() const;

private:
    void Setup(CmdArgs& args);
    void Print();
    WebRtc_Word32 TearDown();
    void IncrementDebugClock(float frameRate);

    webrtc::FakeTickTime*                _clock;
    webrtc::VideoCodingModule*           _vcm;
    webrtc::VideoCodec                   _sendCodec;
    webrtc::VideoCodec                   _receiveCodec;
    std::string                          _inname;
    std::string                          _outname;
    std::string                          _encodedName;
    WebRtc_Word32                        _sumEncBytes;
    FILE*                                _sourceFile;
    FILE*                                _decodedFile;
    FILE*                                _encodedFile;
    WebRtc_UWord16                       _width;
    WebRtc_UWord16                       _height;
    float                                _frameRate;
    WebRtc_UWord32                       _lengthSourceFrame;
    WebRtc_UWord32                       _timeStamp;
    VCMDecodeCompleteCallback*           _decodeCallback;
    VCMEncodeCompleteCallback*           _encodeCompleteCallback;

}; // end of GenericCodecTest class definition

class RTPSendCallback_SizeTest : public webrtc::Transport
{
public:
    // constructor input: (receive side) rtp module to send encoded data to
    RTPSendCallback_SizeTest() : _maxPayloadSize(0), _payloadSizeSum(0), _nPackets(0) {}
    virtual int SendPacket(int channel, const void *data, int len);
    virtual int SendRTCPPacket(int channel, const void *data, int len) {return 0;}
    void SetMaxPayloadSize(WebRtc_UWord32 maxPayloadSize);
    void Reset();
    float AveragePayloadSize() const;
private:
    WebRtc_UWord32         _maxPayloadSize;
    WebRtc_UWord32         _payloadSizeSum;
    WebRtc_UWord32         _nPackets;
};

class VCMEncComplete_KeyReqTest : public webrtc::VCMPacketizationCallback
{
public:
    VCMEncComplete_KeyReqTest(webrtc::VideoCodingModule &vcm) : _vcm(vcm), _seqNo(0), _timeStamp(0) {}
    WebRtc_Word32 SendData(
            const webrtc::FrameType frameType,
            const WebRtc_UWord8 payloadType,
            WebRtc_UWord32 timeStamp,
            int64_t capture_time_ms,
            const WebRtc_UWord8* payloadData,
            const WebRtc_UWord32 payloadSize,
            const webrtc::RTPFragmentationHeader& fragmentationHeader,
            const webrtc::RTPVideoHeader* videoHdr);
private:
    webrtc::VideoCodingModule& _vcm;
    WebRtc_UWord16 _seqNo;
    WebRtc_UWord32 _timeStamp;
}; // end of VCMEncodeCompleteCallback

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_GENERIC_CODEC_TEST_H_
