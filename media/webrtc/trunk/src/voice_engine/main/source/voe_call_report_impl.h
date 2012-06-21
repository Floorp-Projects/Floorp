/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_CALL_REPORT_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_CALL_REPORT_IMPL_H

#include "voe_call_report.h"

#include "ref_count.h"
#include "shared_data.h"


namespace webrtc
{
class FileWrapper;

class VoECallReportImpl: public VoECallReport,
                         public voe::RefCount
{
public:
    virtual int Release();

    virtual int ResetCallReportStatistics(int channel);

    virtual int GetEchoMetricSummary(EchoStatistics& stats);

    virtual int GetRoundTripTimeSummary(int channel,
                                        StatVal& delaysMs);

    virtual int GetDeadOrAliveSummary(int channel, int& numOfDeadDetections,
                                      int& numOfAliveDetections);

    virtual int WriteReportToFile(const char* fileNameUTF8);

protected:
    VoECallReportImpl(voe::SharedData* shared);
    virtual ~VoECallReportImpl();

private:
    int GetDeadOrAliveSummaryInternal(int channel,
                                      int& numOfDeadDetections,
                                      int& numOfAliveDetections);

    int GetEchoMetricSummaryInternal(EchoStatistics& stats);

    int GetSpeechAndNoiseSummaryInternal(LevelStatistics& stats);

    FileWrapper& _file;
    voe::SharedData* _shared;
};

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_CALL_REPORT_IMPL_H
