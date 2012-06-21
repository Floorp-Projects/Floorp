/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_ALL_CODECS_H
#define TEST_ALL_CODECS_H

#include "ACMTest.h"
#include "Channel.h"
#include "PCMFile.h"

namespace webrtc {

class TestPack : public AudioPacketizationCallback
{
public:
    TestPack();
    ~TestPack();
    
    void RegisterReceiverACM(AudioCodingModule* acm);
    
    virtual WebRtc_Word32 SendData(const FrameType frameType,
        const WebRtc_UWord8 payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData, 
        const WebRtc_UWord16 payloadSize,
        const RTPFragmentationHeader* fragmentation);

    WebRtc_UWord16 GetPayloadSize();
    WebRtc_UWord32 GetTimeStampDiff();
    void ResetPayloadSize();

private:
    AudioCodingModule* _receiverACM;
    WebRtc_Word16            _seqNo;
    WebRtc_UWord8            _payloadData[60 * 32 * 2 * 2]; 
    WebRtc_UWord32           _timeStampDiff;
    WebRtc_UWord32           _lastInTimestamp;
    WebRtc_UWord64           _totalBytes;
    WebRtc_UWord16           _payloadSize;
};

class TestAllCodecs : public ACMTest
{
public:
    TestAllCodecs(int testMode);
    ~TestAllCodecs();

    void Perform();
private:
    // The default value of '-1' indicates that the registration is based only on codec name
    // and a sampling frequncy matching is not required. This is useful for codecs which support
    // several sampling frequency.
    WebRtc_Word16 RegisterSendCodec(char side, 
        char* codecName, 
        WebRtc_Word32 sampFreqHz,
        int rate,
        int packSize,
        int extraByte);

    void Run(TestPack* channel);
    void OpenOutFile(WebRtc_Word16 testNumber);
    void DisplaySendReceiveCodec();

    WebRtc_Word32 SendData(
        const FrameType       frameType,
        const WebRtc_UWord8   payloadType,
        const WebRtc_UWord32  timeStamp,
        const WebRtc_UWord8*  payloadData, 
        const WebRtc_UWord16  payloadSize,
        const RTPFragmentationHeader* fragmentation);

    int                     _testMode;

    AudioCodingModule*      _acmA;
    AudioCodingModule*      _acmB;

    TestPack*               _channelA2B;

    PCMFile                _inFileA;
    PCMFile                _outFileB;
    WebRtc_Word16          _testCntr;
    WebRtc_UWord16         _packSizeSamp;
    WebRtc_UWord16         _packSizeBytes;
    int                    _counter;
};

#endif // TEST_ALL_CODECS_H

} // namespace webrtc
