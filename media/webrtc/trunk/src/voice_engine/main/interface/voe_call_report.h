/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Long-term speech and noise level metrics.
//  - Long-term echo metric statistics.
//  - Round Trip Time (RTT) statistics.
//  - Dead-or-Alive connection summary.
//  - Generation of call reports to text files.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoECallReport report = VoECallReport::GetInterface(voe);
//  base->Init();
//  LevelStatistics stats;
//  report->GetSpeechAndNoiseSummary(stats);
//  ...
//  base->Terminate();
//  base->Release();
//  report->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_CALL_REPORT_H
#define WEBRTC_VOICE_ENGINE_VOE_CALL_REPORT_H

#include "common_types.h"

namespace webrtc {

class VoiceEngine;

// VoECallReport
class WEBRTC_DLLEXPORT VoECallReport
{
public:
    // Factory for the VoECallReport sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoECallReport* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoECallReport sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Performs a combined reset of all components involved in generating
    // the call report for a specified |channel|. Pass in -1 to reset
    // all channels.
    virtual int ResetCallReportStatistics(int channel) = 0;

    // Gets minimum, maximum and average levels for long-term echo metrics.
    virtual int GetEchoMetricSummary(EchoStatistics& stats) = 0;

    // Gets minimum, maximum and average levels for Round Trip Time (RTT)
    // measurements.
    virtual int GetRoundTripTimeSummary(int channel,
                                        StatVal& delaysMs) = 0;

    // Gets the total amount of dead and alive connection detections
    // during a VoIP session.
    virtual int GetDeadOrAliveSummary(int channel, int& numOfDeadDetections,
                                      int& numOfAliveDetections) = 0;

    // Creates a text file in ASCII format, which contains a summary
    // of all the statistics that can be obtained by the call report sub-API.
    virtual int WriteReportToFile(const char* fileNameUTF8) = 0;

protected:
    VoECallReport() { }
    virtual ~VoECallReport() { }
};

}  // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_CALL_REPORT_H
