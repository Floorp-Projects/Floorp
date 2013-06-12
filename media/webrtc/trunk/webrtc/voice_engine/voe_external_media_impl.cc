/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_external_media_impl.h"

#include "channel.h"
#include "critical_section_wrapper.h"
#include "output_mixer.h"
#include "trace.h"
#include "transmit_mixer.h"
#include "voice_engine_impl.h"
#include "voe_errors.h"

namespace webrtc {

VoEExternalMedia* VoEExternalMedia::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
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

#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API

VoEExternalMediaImpl::VoEExternalMediaImpl(voe::SharedData* shared)
    :
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    playout_delay_ms_(0),
#endif
    shared_(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(shared_->instance_id(), -1),
                 "VoEExternalMediaImpl() - ctor");
}

VoEExternalMediaImpl::~VoEExternalMediaImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(shared_->instance_id(), -1),
                 "~VoEExternalMediaImpl() - dtor");
}

int VoEExternalMediaImpl::RegisterExternalMediaProcessing(
    int channel,
    ProcessingTypes type,
    VoEMediaProcess& processObject)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(shared_->instance_id(), -1),
                 "RegisterExternalMediaProcessing(channel=%d, type=%d, "
                 "processObject=0x%x)", channel, type, &processObject);
    if (!shared_->statistics().Initialized())
    {
        shared_->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    switch (type)
    {
        case kPlaybackPerChannel:
        case kRecordingPerChannel:
        {
            voe::ScopedChannel sc(shared_->channel_manager(), channel);
            voe::Channel* channelPtr = sc.ChannelPtr();
            if (channelPtr == NULL)
            {
                shared_->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                    "RegisterExternalMediaProcessing() failed to locate "
                    "channel");
                return -1;
            }
            return channelPtr->RegisterExternalMediaProcessing(type,
                                                               processObject);
        }
        case kPlaybackAllChannelsMixed:
        {
            return shared_->output_mixer()->RegisterExternalMediaProcessing(
                processObject);
        }
        case kRecordingAllChannelsMixed:
        case kRecordingPreprocessing:
        {
            return shared_->transmit_mixer()->RegisterExternalMediaProcessing(
                &processObject, type);
        }
    }
    return -1;
}

int VoEExternalMediaImpl::DeRegisterExternalMediaProcessing(
    int channel,
    ProcessingTypes type)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(shared_->instance_id(), -1),
                 "DeRegisterExternalMediaProcessing(channel=%d)", channel);
    if (!shared_->statistics().Initialized())
    {
        shared_->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    switch (type)
    {
        case kPlaybackPerChannel:
        case kRecordingPerChannel:
        {
            voe::ScopedChannel sc(shared_->channel_manager(), channel);
            voe::Channel* channelPtr = sc.ChannelPtr();
            if (channelPtr == NULL)
            {
                shared_->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                    "RegisterExternalMediaProcessing() "
                    "failed to locate channel");
                return -1;
            }
            return channelPtr->DeRegisterExternalMediaProcessing(type);
        }
        case kPlaybackAllChannelsMixed:
        {
            return shared_->output_mixer()->
                DeRegisterExternalMediaProcessing();
        }
        case kRecordingAllChannelsMixed:
        case kRecordingPreprocessing:
        {
            return shared_->transmit_mixer()->
                DeRegisterExternalMediaProcessing(type);
        }
    }
    return -1;
}

int VoEExternalMediaImpl::SetExternalRecordingStatus(bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(shared_->instance_id(), -1),
                 "SetExternalRecordingStatus(enable=%d)", enable);
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    if (shared_->audio_device()->Recording())
    {
        shared_->SetLastError(VE_ALREADY_SENDING, kTraceError,
            "SetExternalRecordingStatus() cannot set state while sending");
        return -1;
    }
    shared_->set_ext_recording(enable);
    return 0;
#else
    shared_->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetExternalRecordingStatus() external recording is not supported");
    return -1;
#endif
}

int VoEExternalMediaImpl::ExternalRecordingInsertData(
        const int16_t speechData10ms[],
        int lengthSamples,
        int samplingFreqHz,
        int current_delay_ms)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(shared_->instance_id(), -1),
                 "ExternalRecordingInsertData(speechData10ms=0x%x,"
                 " lengthSamples=%u, samplingFreqHz=%d, current_delay_ms=%d)",
                 &speechData10ms[0], lengthSamples, samplingFreqHz,
              current_delay_ms);
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    if (!shared_->statistics().Initialized())
    {
        shared_->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (!shared_->ext_recording())
    {
       shared_->SetLastError(VE_INVALID_OPERATION, kTraceError,
           "ExternalRecordingInsertData() external recording is not enabled");
        return -1;
    }
    if (shared_->NumOfSendingChannels() == 0)
    {
        shared_->SetLastError(VE_ALREADY_SENDING, kTraceError,
            "SetExternalRecordingStatus() no channel is sending");
        return -1;
    }
    if ((16000 != samplingFreqHz) && (32000 != samplingFreqHz) &&
        (48000 != samplingFreqHz) && (44000 != samplingFreqHz))
    {
         shared_->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
             "SetExternalRecordingStatus() invalid sample rate");
        return -1;
    }
    if ((0 == lengthSamples) ||
        ((lengthSamples % (samplingFreqHz / 100)) != 0))
    {
         shared_->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
             "SetExternalRecordingStatus() invalid buffer size");
        return -1;
    }
    if (current_delay_ms < 0)
    {
        shared_->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetExternalRecordingStatus() invalid delay)");
        return -1;
    }

    uint16_t blockSize = samplingFreqHz / 100;
    uint32_t nBlocks = lengthSamples / blockSize;
    int16_t totalDelayMS = 0;
    uint16_t playoutDelayMS = 0;

    for (uint32_t i = 0; i < nBlocks; i++)
    {
        if (!shared_->ext_playout())
        {
            // Use real playout delay if external playout is not enabled.
            if (shared_->audio_device()->PlayoutDelay(&playoutDelayMS) != 0) {
              shared_->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceWarning,
                  "PlayoutDelay() unable to get the playout delay");
            }
            totalDelayMS = current_delay_ms + playoutDelayMS;
        }
        else
        {
            // Use stored delay value given the last call
            // to ExternalPlayoutGetData.
            totalDelayMS = current_delay_ms + playout_delay_ms_;
            // Compensate for block sizes larger than 10ms
            totalDelayMS -= (int16_t)(i*10);
            if (totalDelayMS < 0)
                totalDelayMS = 0;
        }
        shared_->transmit_mixer()->PrepareDemux(
            (const int8_t*)(&speechData10ms[i*blockSize]),
            blockSize,
            1,
            samplingFreqHz,
            totalDelayMS,
            0,
            0);

        shared_->transmit_mixer()->DemuxAndMix();
        shared_->transmit_mixer()->EncodeAndSend();
    }
    return 0;
#else
       shared_->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "ExternalRecordingInsertData() external recording is not supported");
    return -1;
#endif
}

int VoEExternalMediaImpl::SetExternalPlayoutStatus(bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(shared_->instance_id(), -1),
                 "SetExternalPlayoutStatus(enable=%d)", enable);
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    if (shared_->audio_device()->Playing())
    {
        shared_->SetLastError(VE_ALREADY_SENDING, kTraceError,
            "SetExternalPlayoutStatus() cannot set state while playing");
        return -1;
    }
    shared_->set_ext_playout(enable);
    return 0;
#else
    shared_->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetExternalPlayoutStatus() external playout is not supported");
    return -1;
#endif
}

int VoEExternalMediaImpl::ExternalPlayoutGetData(
    int16_t speechData10ms[],
    int samplingFreqHz,
    int current_delay_ms,
    int& lengthSamples)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(shared_->instance_id(), -1),
                 "ExternalPlayoutGetData(speechData10ms=0x%x, samplingFreqHz=%d"
                 ",  current_delay_ms=%d)", &speechData10ms[0], samplingFreqHz,
                 current_delay_ms);
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    if (!shared_->statistics().Initialized())
    {
        shared_->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (!shared_->ext_playout())
    {
       shared_->SetLastError(VE_INVALID_OPERATION, kTraceError,
           "ExternalPlayoutGetData() external playout is not enabled");
        return -1;
    }
    if ((16000 != samplingFreqHz) && (32000 != samplingFreqHz) &&
        (48000 != samplingFreqHz) && (44000 != samplingFreqHz))
    {
        shared_->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "ExternalPlayoutGetData() invalid sample rate");
        return -1;
    }
    if (current_delay_ms < 0)
    {
        shared_->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "ExternalPlayoutGetData() invalid delay)");
        return -1;
    }

    AudioFrame audioFrame;

    // Retrieve mixed output at the specified rate
    shared_->output_mixer()->MixActiveChannels();
    shared_->output_mixer()->DoOperationsOnCombinedSignal();
    shared_->output_mixer()->GetMixedAudio(samplingFreqHz, 1, &audioFrame);

    // Deliver audio (PCM) samples to the external sink
    memcpy(speechData10ms,
           audioFrame.data_,
           sizeof(int16_t)*(audioFrame.samples_per_channel_));
    lengthSamples = audioFrame.samples_per_channel_;

    // Store current playout delay (to be used by ExternalRecordingInsertData).
    playout_delay_ms_ = current_delay_ms;

    return 0;
#else
    shared_->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
       "ExternalPlayoutGetData() external playout is not supported");
    return -1;
#endif
}

int VoEExternalMediaImpl::GetAudioFrame(int channel, int desired_sample_rate_hz,
                                        AudioFrame* frame) {
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
                 VoEId(shared_->instance_id(), channel),
                 "GetAudioFrame(channel=%d, desired_sample_rate_hz=%d)",
                 channel, desired_sample_rate_hz);
    if (!shared_->statistics().Initialized())
    {
        shared_->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(shared_->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        shared_->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetAudioFrame() failed to locate channel");
        return -1;
    }
    if (!channelPtr->ExternalMixing()) {
        shared_->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "GetAudioFrame() was called on channel that is not"
            " externally mixed.");
        return -1;
    }
    if (!channelPtr->Playing()) {
        shared_->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "GetAudioFrame() was called on channel that is not playing.");
        return -1;
    }
    if (desired_sample_rate_hz == -1) {
          shared_->SetLastError(VE_BAD_ARGUMENT, kTraceError,
              "GetAudioFrame() was called with bad sample rate.");
          return -1;
    }
    frame->sample_rate_hz_ = desired_sample_rate_hz == 0 ? -1 :
                             desired_sample_rate_hz;
    return channelPtr->GetAudioFrame(channel, *frame);
}

int VoEExternalMediaImpl::SetExternalMixing(int channel, bool enable) {
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
                 VoEId(shared_->instance_id(), channel),
                 "SetExternalMixing(channel=%d, enable=%d)", channel, enable);
    if (!shared_->statistics().Initialized())
    {
        shared_->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(shared_->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        shared_->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetExternalMixing() failed to locate channel");
        return -1;
    }
    return channelPtr->SetExternalMixing(enable);
}

#endif  // WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API

}  // namespace webrtc
