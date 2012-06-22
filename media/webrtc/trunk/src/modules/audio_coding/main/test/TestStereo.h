/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_STEREO_H
#define TEST_STEREO_H

#include <math.h>

#include "ACMTest.h"
#include "Channel.h"
#include "PCMFile.h"

namespace webrtc {

enum StereoMonoMode {
    kNotSet,
    kMono,
    kStereo
};

class TestPackStereo : public AudioPacketizationCallback
{
public:
    TestPackStereo();
    ~TestPackStereo();
    
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
    void set_codec_mode(StereoMonoMode mode);
    void set_lost_packet(bool lost);

private:
    AudioCodingModule* _receiverACM;
    WebRtc_Word16            _seqNo;
    WebRtc_UWord8            _payloadData[60 * 32 * 2 * 2]; 
    WebRtc_UWord32           _timeStampDiff;
    WebRtc_UWord32           _lastInTimestamp;
    WebRtc_UWord64           _totalBytes;
    WebRtc_UWord16           _payloadSize;
    StereoMonoMode           _codec_mode;
    // Simulate packet losses
    bool _lost_packet;
};

class TestStereo : public ACMTest
{
public:
    TestStereo(int testMode);
    ~TestStereo();

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
        int channels,
        int payload_type);

    void Run(TestPackStereo* channel, int in_channels, int out_channels,
             int percent_loss = 0);
    void OpenOutFile(WebRtc_Word16 testNumber);
    void DisplaySendReceiveCodec();

    WebRtc_Word32 SendData(
        const FrameType       frameType,
        const WebRtc_UWord8   payloadType,
        const WebRtc_UWord32  timeStamp,
        const WebRtc_UWord8*  payloadData, 
        const WebRtc_UWord16  payloadSize,
        const RTPFragmentationHeader* fragmentation);

    int                    _testMode;

    AudioCodingModule*     _acmA;
    AudioCodingModule*     _acmB;

    TestPackStereo*        _channelA2B;

    PCMFile                _in_file_stereo;
    PCMFile                _in_file_mono;
    PCMFile                _outFileB;
    WebRtc_Word16          _testCntr;
    WebRtc_UWord16         _packSizeSamp;
    WebRtc_UWord16         _packSizeBytes;
    int                    _counter;

    // Payload types for stereo codecs and CNG
    int g722_pltype_;
    int l16_8khz_pltype_;
    int l16_16khz_pltype_;
    int l16_32khz_pltype_;
    int pcma_pltype_;
    int pcmu_pltype_;
    int celt_pltype_;
    int cn_8khz_pltype_;
    int cn_16khz_pltype_;
    int cn_32khz_pltype_;
};

} // namespace webrtc

#endif

