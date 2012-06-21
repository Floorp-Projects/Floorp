/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_DTMF_INBAND_H
#define WEBRTC_VOICE_ENGINE_DTMF_INBAND_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "typedefs.h"
#include "voice_engine_defines.h"

namespace webrtc {
class CriticalSectionWrapper;

class DtmfInband
{
public:
    DtmfInband(const WebRtc_Word32 id);

    virtual ~DtmfInband();

    void Init();

    int SetSampleRate(const WebRtc_UWord16 frequency);

    int GetSampleRate(WebRtc_UWord16& frequency);

    int AddTone(const WebRtc_UWord8 eventCode,
                WebRtc_Word32 lengthMs,
                WebRtc_Word32 attenuationDb);

    int ResetTone();
    int StartTone(const WebRtc_UWord8 eventCode,
                  WebRtc_Word32 attenuationDb);

    int StopTone();

    bool IsAddingTone();

    int Get10msTone(WebRtc_Word16 output[320],
                    WebRtc_UWord16& outputSizeInSamples);

    WebRtc_UWord32 DelaySinceLastTone() const;

    void UpdateDelaySinceLastTone();

private:
    void ReInit();
    WebRtc_Word16 DtmfFix_generate(WebRtc_Word16* decoded,
                                   const WebRtc_Word16 value,
                                   const WebRtc_Word16 volume,
                                   const WebRtc_Word16 frameLen,
                                   const WebRtc_Word16 fs);

private:
    enum {kDtmfFrameSizeMs = 10};
    enum {kDtmfAmpHigh = 32768};
    enum {kDtmfAmpLow  = 23171};	// 3 dB lower than the high frequency

    WebRtc_Word16 DtmfFix_generateSignal(const WebRtc_Word16 a1_times2,
                                         const WebRtc_Word16 a2_times2,
                                         const WebRtc_Word16 volume,
                                         WebRtc_Word16* signal,
                                         const WebRtc_Word16 length);

private:
    CriticalSectionWrapper& _critSect;
    WebRtc_Word32 _id;
    WebRtc_UWord16 _outputFrequencyHz;  // {8000, 16000, 32000}
    WebRtc_Word16 _oldOutputLow[2];     // Data needed for oscillator model
    WebRtc_Word16 _oldOutputHigh[2];    // Data needed for oscillator model
    WebRtc_Word16 _frameLengthSamples;  // {80, 160, 320}
    WebRtc_Word32 _remainingSamples;
    WebRtc_Word16 _eventCode;           // [0, 15]
    WebRtc_Word16 _attenuationDb;       // [0, 36]
    WebRtc_Word32 _lengthMs;
    bool _reinit;  // 'true' if the oscillator should be reinit for next event
    bool _playing;
    WebRtc_UWord32 _delaySinceLastToneMS; // time since last generated tone [ms]
};

}   // namespace webrtc

#endif // #ifndef WEBRTC_VOICE_ENGINE_DTMF_INBAND_H
