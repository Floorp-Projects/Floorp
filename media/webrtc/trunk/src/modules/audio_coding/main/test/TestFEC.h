/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FEC_H
#define TEST_FEC_H

#include "ACMTest.h"
#include "Channel.h"
#include "PCMFile.h"

namespace webrtc {

class TestFEC : public ACMTest
{
public:
    TestFEC(int testMode);
    ~TestFEC();

    void Perform();
private:
    // The default value of '-1' indicates that the registration is based only on codec name
    // and a sampling frequncy matching is not required. This is useful for codecs which support
    // several sampling frequency.
    WebRtc_Word16 RegisterSendCodec(char side, char* codecName, WebRtc_Word32 sampFreqHz = -1);
    void Run();
    void OpenOutFile(WebRtc_Word16 testNumber);
    void DisplaySendReceiveCodec();
    WebRtc_Word32 SetVAD(bool enableDTX, bool enableVAD, ACMVADMode vadMode);
    AudioCodingModule* _acmA;
    AudioCodingModule* _acmB;

    Channel*               _channelA2B;

    PCMFile                _inFileA;
    PCMFile                _outFileB;
    WebRtc_Word16            _testCntr;
    int                    _testMode;
};

} // namespace webrtc

#endif
