/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TWO_WAY_COMMUNICATION_H
#define TWO_WAY_COMMUNICATION_H

#include "ACMTest.h"
#include "Channel.h"
#include "PCMFile.h"
#include "audio_coding_module.h"
#include "utility.h"

namespace webrtc {

class TwoWayCommunication : public ACMTest
{
public:
    TwoWayCommunication(int testMode = 1);
    ~TwoWayCommunication();

    void Perform();
private:
    WebRtc_UWord8 ChooseCodec(WebRtc_UWord8* codecID_A, WebRtc_UWord8* codecID_B);
    WebRtc_Word16 ChooseFile(char* fileName, WebRtc_Word16 maxLen, WebRtc_UWord16* frequencyHz);
    WebRtc_Word16 SetUp();
    WebRtc_Word16 SetUpAutotest();

    AudioCodingModule* _acmA;
    AudioCodingModule* _acmB;

    AudioCodingModule* _acmRefA;
    AudioCodingModule* _acmRefB;

    Channel* _channel_A2B;
    Channel* _channel_B2A;

    Channel* _channelRef_A2B;
    Channel* _channelRef_B2A;

    PCMFile _inFileA;
    PCMFile _inFileB;

    PCMFile _outFileA;
    PCMFile _outFileB;

    PCMFile _outFileRefA;
    PCMFile _outFileRefB;

    DTMFDetector* _dtmfDetectorA;
    DTMFDetector* _dtmfDetectorB;

    int _testMode;
};

} // namespace webrtc

#endif
