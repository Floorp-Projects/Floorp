/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_call_report_impl.h"

#include "audio_processing.h"
#include "channel.h"
#include "critical_section_wrapper.h"
#include "file_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc
{

VoECallReport* VoECallReport::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
    s->AddRef();
    return s;
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API

VoECallReportImpl::VoECallReportImpl(voe::SharedData* shared) :
    _file(*FileWrapper::Create()), _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoECallReportImpl() - ctor");
}

VoECallReportImpl::~VoECallReportImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "~VoECallReportImpl() - dtor");
    delete &_file;
}

int VoECallReportImpl::ResetCallReportStatistics(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ResetCallReportStatistics(channel=%d)", channel);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    assert(_shared->audio_processing() != NULL);

    bool echoMode =
        _shared->audio_processing()->echo_cancellation()->are_metrics_enabled();

    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "  current AudioProcessingModule echo metric state %d)",
                 echoMode);
    // Reset the APM statistics
    if (_shared->audio_processing()->echo_cancellation()->enable_metrics(true)
        != 0)
    {
        _shared->SetLastError(VE_APM_ERROR, kTraceError,
            "ResetCallReportStatistics() unable to "
            "set the AudioProcessingModule echo metrics state");
        return -1;
    }
    // Restore metric states
    _shared->audio_processing()->echo_cancellation()->enable_metrics(echoMode);

    // Reset channel dependent statistics
    if (channel != -1)
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "ResetCallReportStatistics() failed to locate channel");
            return -1;
        }
        channelPtr->ResetDeadOrAliveCounters();
        channelPtr->ResetRTCPStatistics();
    }
    else
    {
        int32_t numOfChannels =
            _shared->channel_manager().NumOfChannels();
        if (numOfChannels <= 0)
        {
            return 0;
        }
        int32_t* channelsArray = new int32_t[numOfChannels];
        _shared->channel_manager().GetChannelIds(channelsArray, numOfChannels);
        for (int i = 0; i < numOfChannels; i++)
        {
            voe::ScopedChannel sc(_shared->channel_manager(), channelsArray[i]);
            voe::Channel* channelPtr = sc.ChannelPtr();
            if (channelPtr)
            {
                channelPtr->ResetDeadOrAliveCounters();
                channelPtr->ResetRTCPStatistics();
            }
        }
        delete[] channelsArray;
    }

    return 0;
}

int VoECallReportImpl::GetEchoMetricSummary(EchoStatistics& stats)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetEchoMetricSummary()");
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    assert(_shared->audio_processing() != NULL);

    return (GetEchoMetricSummaryInternal(stats));
}

int VoECallReportImpl::GetEchoMetricSummaryInternal(EchoStatistics& stats)
{
    // Retrieve echo metrics from the AudioProcessingModule
    int ret(0);
    bool mode(false);
    EchoCancellation::Metrics metrics;

    // Ensure that echo metrics is enabled

    mode =
        _shared->audio_processing()->echo_cancellation()->are_metrics_enabled();
    if (mode != false)
    {
        ret = _shared->audio_processing()->echo_cancellation()->
              GetMetrics(&metrics);
        if (ret != 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "  AudioProcessingModule GetMetrics() => error");
        }
    }
    else
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
            VoEId(_shared->instance_id(), -1),
            "  AudioProcessingModule echo metrics is not enabled");
    }

    if ((ret != 0) || (mode == false))
    {
        // Mark complete struct as invalid (-100 dB)
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
            VoEId(_shared->instance_id(), -1),
            "  unable to retrieve echo metrics from the AudioProcessingModule");
        stats.erl.min = -100;
        stats.erl.max = -100;
        stats.erl.average = -100;
        stats.erle.min = -100;
        stats.erle.max = -100;
        stats.erle.average = -100;
        stats.rerl.min = -100;
        stats.rerl.max = -100;
        stats.rerl.average = -100;
        stats.a_nlp.min = -100;
        stats.a_nlp.max = -100;
        stats.a_nlp.average = -100;
    }
    else
    {

        // Deliver output results to user
        stats.erl.min = metrics.echo_return_loss.minimum;
        stats.erl.max = metrics.echo_return_loss.maximum;
        stats.erl.average = metrics.echo_return_loss.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
            VoEId(_shared->instance_id(), -1), "  erl: min=%d, max=%d, avg=%d",
            stats.erl.min, stats.erl.max, stats.erl.average);

        stats.erle.min = metrics.echo_return_loss_enhancement.minimum;
        stats.erle.max = metrics.echo_return_loss_enhancement.maximum;
        stats.erle.average = metrics.echo_return_loss_enhancement.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
            VoEId(_shared->instance_id(), -1), "  erle: min=%d, max=%d, avg=%d",
            stats.erle.min, stats.erle.max, stats.erle.average);

        stats.rerl.min = metrics.residual_echo_return_loss.minimum;
        stats.rerl.max = metrics.residual_echo_return_loss.maximum;
        stats.rerl.average = metrics.residual_echo_return_loss.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
            VoEId(_shared->instance_id(), -1), "  rerl: min=%d, max=%d, avg=%d",
            stats.rerl.min, stats.rerl.max, stats.rerl.average);

        stats.a_nlp.min = metrics.a_nlp.minimum;
        stats.a_nlp.max = metrics.a_nlp.maximum;
        stats.a_nlp.average = metrics.a_nlp.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
            VoEId(_shared->instance_id(), -1),
            "  a_nlp: min=%d, max=%d, avg=%d",
            stats.a_nlp.min, stats.a_nlp.max, stats.a_nlp.average);
    }
    return 0;
}

int VoECallReportImpl::GetRoundTripTimeSummary(int channel, StatVal& delaysMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetRoundTripTimeSummary()");
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRoundTripTimeSummary() failed to locate channel");
        return -1;
    }

    return channelPtr->GetRoundTripTimeSummary(delaysMs);
}

int VoECallReportImpl::GetDeadOrAliveSummary(int channel,
                                             int& numOfDeadDetections,
                                             int& numOfAliveDetections)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetDeadOrAliveSummary(channel=%d)", channel);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    return (GetDeadOrAliveSummaryInternal(channel, numOfDeadDetections,
                                          numOfAliveDetections));
}

int VoECallReportImpl::GetDeadOrAliveSummaryInternal(int channel,
                                                     int& numOfDeadDetections,
                                                     int& numOfAliveDetections)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetDeadOrAliveSummary(channel=%d)", channel);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRoundTripTimeSummary() failed to locate channel");
        return -1;
    }

    return channelPtr->GetDeadOrAliveCounters(numOfDeadDetections,
                                              numOfAliveDetections);
}

int VoECallReportImpl::WriteReportToFile(const char* fileNameUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "WriteReportToFile(fileNameUTF8=%s)", fileNameUTF8);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    if (NULL == fileNameUTF8)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "WriteReportToFile() invalid filename");
        return -1;
    }

    if (_file.Open())
    {
        _file.CloseFile();
    }

    // Open text file in write mode
    if (_file.OpenFile(fileNameUTF8, false, false, true) != 0)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "WriteReportToFile() unable to open the file");
        return -1;
    }

    // Summarize information and add it to the open file
    //
    _file.WriteText("WebRtc VoiceEngine Call Report\n");
    _file.WriteText("==============================\n");
    _file.WriteText("\nNetwork Packet Round Trip Time (RTT)\n");
    _file.WriteText("------------------------------------\n\n");

    int32_t numOfChannels = _shared->channel_manager().NumOfChannels();
    if (numOfChannels <= 0)
    {
        return 0;
    }
    int32_t* channelsArray = new int32_t[numOfChannels];
    _shared->channel_manager().GetChannelIds(channelsArray, numOfChannels);
    for (int ch = 0; ch < numOfChannels; ch++)
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channelsArray[ch]);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr)
        {
            StatVal delaysMs;
            _file.WriteText("channel %d:\n", ch);
            channelPtr->GetRoundTripTimeSummary(delaysMs);
            _file.WriteText("  min:%5d [ms]\n", delaysMs.min);
            _file.WriteText("  max:%5d [ms]\n", delaysMs.max);
            _file.WriteText("  avg:%5d [ms]\n", delaysMs.average);
        }
    }

    _file.WriteText("\nDead-or-Alive Connection Detections\n");
    _file.WriteText("------------------------------------\n\n");

    for (int ch = 0; ch < numOfChannels; ch++)
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channelsArray[ch]);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr)
        {
            int nDead(0);
            int nAlive(0);
            _file.WriteText("channel %d:\n", ch);
            GetDeadOrAliveSummary(ch, nDead, nAlive);
            _file.WriteText("  #dead :%6d\n", nDead);
            _file.WriteText("  #alive:%6d\n", nAlive);
        }
    }

    delete[] channelsArray;

    EchoStatistics echo;
    GetEchoMetricSummary(echo);

    _file.WriteText("\nEcho Metrics\n");
    _file.WriteText("------------\n\n");

    _file.WriteText("erl:\n");
    _file.WriteText("  min:%5d [dB]\n", echo.erl.min);
    _file.WriteText("  max:%5d [dB]\n", echo.erl.max);
    _file.WriteText("  avg:%5d [dB]\n", echo.erl.average);
    _file.WriteText("\nerle:\n");
    _file.WriteText("  min:%5d [dB]\n", echo.erle.min);
    _file.WriteText("  max:%5d [dB]\n", echo.erle.max);
    _file.WriteText("  avg:%5d [dB]\n", echo.erle.average);
    _file.WriteText("rerl:\n");
    _file.WriteText("  min:%5d [dB]\n", echo.rerl.min);
    _file.WriteText("  max:%5d [dB]\n", echo.rerl.max);
    _file.WriteText("  avg:%5d [dB]\n", echo.rerl.average);
    _file.WriteText("a_nlp:\n");
    _file.WriteText("  min:%5d [dB]\n", echo.a_nlp.min);
    _file.WriteText("  max:%5d [dB]\n", echo.a_nlp.max);
    _file.WriteText("  avg:%5d [dB]\n", echo.a_nlp.average);

    _file.WriteText("\n<END>");

    _file.Flush();
    _file.CloseFile();

    return 0;
}

#endif  // WEBRTC_VOICE_ENGINE_CALL_REPORT_API

} // namespace webrtc
