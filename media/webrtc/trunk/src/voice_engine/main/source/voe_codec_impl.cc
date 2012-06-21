/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_codec_impl.h"

#include "audio_coding_module.h"
#include "channel.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc
{

VoECodec* VoECodec::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_CODEC_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s =
            reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoECodecImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_CODEC_API

VoECodecImpl::VoECodecImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoECodecImpl() - ctor");
}

VoECodecImpl::~VoECodecImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "~VoECodecImpl() - dtor");
}

int VoECodecImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoECodecImpl::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset();
        _shared->SetLastError(VE_INTERFACE_NOT_FOUND, kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "VoECodecImpl reference counter = %d", refCount);
    return (refCount);
}

int VoECodecImpl::NumOfCodecs()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "NumOfCodecs()");

    // Number of supported codecs in the ACM
    WebRtc_UWord8 nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "NumOfCodecs() => %u", nSupportedCodecs);
    return (nSupportedCodecs);
}

int VoECodecImpl::GetCodec(int index, CodecInst& codec)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetCodec(index=%d, codec=?)", index);
    CodecInst acmCodec;
    if (AudioCodingModule::Codec(index, (CodecInst&) acmCodec)
            == -1)
    {
        _shared->SetLastError(VE_INVALID_LISTNR, kTraceError,
            "GetCodec() invalid index");
        return -1;
    }
    ACMToExternalCodecRepresentation(codec, acmCodec);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetCodec() => plname=%s, pacsize=%d, plfreq=%d, pltype=%d, "
        "channels=%d, rate=%d", codec.plname, codec.pacsize,
        codec.plfreq, codec.pltype, codec.channels, codec.rate);
    return 0;
}

int VoECodecImpl::SetSendCodec(int channel, const CodecInst& codec)
{
    CodecInst copyCodec;
    ExternalToACMCodecRepresentation(copyCodec, codec);

    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetSendCodec(channel=%d, codec)", channel);
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "codec: plname=%s, pacsize=%d, plfreq=%d, pltype=%d, "
                 "channels=%d, rate=%d", codec.plname, codec.pacsize,
                 codec.plfreq, codec.pltype, codec.channels, codec.rate);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    // External sanity checks performed outside the ACM
    if ((STR_CASE_CMP(copyCodec.plname, "L16") == 0) &&
            (copyCodec.pacsize >= 960))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSendCodec() invalid L16 packet size");
        return -1;
    }
    if (!STR_CASE_CMP(copyCodec.plname, "CN")
            || !STR_CASE_CMP(copyCodec.plname, "TELEPHONE-EVENT")
            || !STR_CASE_CMP(copyCodec.plname, "RED"))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSendCodec() invalid codec name");
        return -1;
    }
    if ((copyCodec.channels != 1) && (copyCodec.channels != 2))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSendCodec() invalid number of channels");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetSendCodec() failed to locate channel");
        return -1;
    }
    if (!AudioCodingModule::IsCodecValid(
            (CodecInst&) copyCodec))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSendCodec() invalid codec");
        return -1;
    }
    if (channelPtr->SetSendCodec(copyCodec) != 0)
    {
        _shared->SetLastError(VE_CANNOT_SET_SEND_CODEC, kTraceError,
            "SetSendCodec() failed to set send codec");
        return -1;
    }

    // Need to check if we should change APM settings for mono/stereo.
    // We'll check all channels (sending or not), so we don't have to
    // check this again when starting/stopping sending.

    voe::ScopedChannel sc2(_shared->channel_manager());
    void* iterator(NULL);
    channelPtr = sc2.GetFirstChannel(iterator);
    int maxNumChannels = 1;
    while (channelPtr != NULL)
    {
        CodecInst tmpCdc;
        channelPtr->GetSendCodec(tmpCdc);
        if (tmpCdc.channels > maxNumChannels)
            maxNumChannels = tmpCdc.channels;

        channelPtr = sc2.GetNextChannel(iterator);
    }

    // Reuse the currently set number of capture channels. We need to wait
    // until receiving a frame to determine the true number.
    //
    // TODO(andrew): AudioProcessing will return an error if there are more
    // output than input channels (it doesn't want to produce fake channels).
    // This will happen with a stereo codec and a device which doesn't support
    // stereo. AudioCoding should probably do the faking; look into how to
    // handle this case properly.
    //
    // Check if the number of channels has changed to avoid an unnecessary
    // reset.
    // TODO(andrew): look at handling this logic in AudioProcessing.
    if (_shared->audio_processing()->num_output_channels() != maxNumChannels)
    {
        if (_shared->audio_processing()->set_num_channels(
                _shared->audio_processing()->num_input_channels(),
                maxNumChannels) != 0)
        {
            _shared->SetLastError(VE_APM_ERROR, kTraceError,
                "Init() failed to set APM channels for the send audio stream");
            return -1;
        }
    }

    return 0;
}

int VoECodecImpl::GetSendCodec(int channel, CodecInst& codec)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetSendCodec(channel=%d, codec=?)", channel);
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
            "GetSendCodec() failed to locate channel");
        return -1;
    }
    CodecInst acmCodec;
    if (channelPtr->GetSendCodec(acmCodec) != 0)
    {
        _shared->SetLastError(VE_CANNOT_GET_SEND_CODEC, kTraceError,
            "GetSendCodec() failed to get send codec");
        return -1;
    }
    ACMToExternalCodecRepresentation(codec, acmCodec);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetSendCodec() => plname=%s, pacsize=%d, plfreq=%d, "
        "channels=%d, rate=%d", codec.plname, codec.pacsize,
        codec.plfreq, codec.channels, codec.rate);
    return 0;
}

int VoECodecImpl::GetRecCodec(int channel, CodecInst& codec)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetRecCodec(channel=%d, codec=?)", channel);
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
            "GetRecCodec() failed to locate channel");
        return -1;
    }
    CodecInst acmCodec;
    if (channelPtr->GetRecCodec(acmCodec) != 0)
    {
        _shared->SetLastError(VE_CANNOT_GET_REC_CODEC, kTraceError,
            "GetRecCodec() failed to get received codec");
        return -1;
    }
    ACMToExternalCodecRepresentation(codec, acmCodec);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetRecCodec() => plname=%s, pacsize=%d, plfreq=%d, "
        "channels=%d, rate=%d", codec.plname, codec.pacsize,
        codec.plfreq, codec.channels, codec.rate);
    return 0;
}

int VoECodecImpl::SetAMREncFormat(int channel, AmrMode mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetAMREncFormat(channel=%d, mode=%d)", channel, mode);
#ifdef WEBRTC_CODEC_GSMAMR
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
            "SetAMREncFormat() failed to locate channel");
        return -1;
    }
    return channelPtr->SetAMREncFormat(mode);
#else
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetAMREncFormat() AMR codec is not supported");
    return -1;
#endif
}

int VoECodecImpl::SetAMRDecFormat(int channel, AmrMode mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetAMRDecFormat(channel=%i, mode=%i)", channel, mode);
#ifdef WEBRTC_CODEC_GSMAMR
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
            "SetAMRDecFormat() failed to locate channel");
        return -1;
    }
    return channelPtr->SetAMRDecFormat(mode);
#else
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetAMRDecFormat() AMR codec is not supported");
    return -1;
#endif
}

int VoECodecImpl::SetAMRWbEncFormat(int channel, AmrMode mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetAMRWbEncFormat(channel=%d, mode=%d)", channel, mode);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_CODEC_GSMAMRWB
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
            "SetAMRWbEncFormat() failed to locate channel");
        return -1;
    }
    return channelPtr->SetAMRWbEncFormat(mode);
#else
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetAMRWbEncFormat() AMR-wb codec is not supported");
    return -1;
#endif
}

int VoECodecImpl::SetAMRWbDecFormat(int channel, AmrMode mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetAMRWbDecFormat(channel=%i, mode=%i)", channel, mode);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_CODEC_GSMAMRWB
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
            "SetAMRWbDecFormat() failed to locate channel");
        return -1;
    }
    return channelPtr->SetAMRWbDecFormat(mode);
#else
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetAMRWbDecFormat() AMR-wb codec is not supported");
    return -1;
#endif
}

int VoECodecImpl::SetRecPayloadType(int channel, const CodecInst& codec)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetRecPayloadType(channel=%d, codec)", channel);
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "codec: plname=%s, plfreq=%d, pltype=%d, channels=%u, "
               "pacsize=%d, rate=%d", codec.plname, codec.plfreq, codec.pltype,
               codec.channels, codec.pacsize, codec.rate);
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
            "GetRecPayloadType() failed to locate channel");
        return -1;
    }
    return channelPtr->SetRecPayloadType(codec);
}

int VoECodecImpl::GetRecPayloadType(int channel, CodecInst& codec)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetRecPayloadType(channel=%d, codec)", channel);
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
            "GetRecPayloadType() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRecPayloadType(codec);
}

int VoECodecImpl::SetSendCNPayloadType(int channel, int type,
                                       PayloadFrequencies frequency)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetSendCNPayloadType(channel=%d, type=%d, frequency=%d)",
                 channel, type, frequency);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (type < 96 || type > 127)
    {
        // Only allow dynamic range: 96 to 127
        _shared->SetLastError(VE_INVALID_PLTYPE, kTraceError,
            "SetSendCNPayloadType() invalid payload type");
        return -1;
    }
    if ((frequency != kFreq16000Hz) && (frequency != kFreq32000Hz))
    {
        // It is not possible to modify the payload type for CN/8000.
        // We only allow modification of the CN payload type for CN/16000
        // and CN/32000.
        _shared->SetLastError(VE_INVALID_PLFREQ, kTraceError,
            "SetSendCNPayloadType() invalid payload frequency");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetSendCNPayloadType() failed to locate channel");
        return -1;
    }
    if (channelPtr->Sending())
    {
        _shared->SetLastError(VE_SENDING, kTraceError,
            "SetSendCNPayloadType unable so set payload type while sending");
        return -1;
    }
    return channelPtr->SetSendCNPayloadType(type, frequency);
}

int VoECodecImpl::SetISACInitTargetRate(int channel, int rateBps,
                                        bool useFixedFrameSize)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetISACInitTargetRate(channel=%d, rateBps=%d, "
                 "useFixedFrameSize=%d)", channel, rateBps, useFixedFrameSize);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_CODEC_ISAC
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
            "SetISACInitTargetRate() failed to locate channel");
        return -1;
    }
    return channelPtr->SetISACInitTargetRate(rateBps, useFixedFrameSize);
#else
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetISACInitTargetRate() iSAC codec is not supported");
    return -1;
#endif
}

int VoECodecImpl::SetISACMaxRate(int channel, int rateBps)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetISACMaxRate(channel=%d, rateBps=%d)", channel, rateBps);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_CODEC_ISAC
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
            "SetISACMaxRate() failed to locate channel");
        return -1;
    }
    return channelPtr->SetISACMaxRate(rateBps);
#else
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetISACMaxRate() iSAC codec is not supported");
    return -1;
#endif
}

int VoECodecImpl::SetISACMaxPayloadSize(int channel, int sizeBytes)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetISACMaxPayloadSize(channel=%d, sizeBytes=%d)", channel,
                 sizeBytes);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_CODEC_ISAC
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
            "SetISACMaxPayloadSize() failed to locate channel");
        return -1;
    }
    return channelPtr->SetISACMaxPayloadSize(sizeBytes);
#else
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetISACMaxPayloadSize() iSAC codec is not supported");
    return -1;
#endif
    return 0;
}

int VoECodecImpl::SetVADStatus(int channel, bool enable, VadModes mode,
                               bool disableDTX)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetVADStatus(channel=%i, enable=%i, mode=%i, disableDTX=%i)",
                 channel, enable, mode, disableDTX);

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
            "SetVADStatus failed to locate channel");
        return -1;
    }

    ACMVADMode vadMode(VADNormal);
    switch (mode)
    {
        case kVadConventional:
            vadMode = VADNormal;
            break;
        case kVadAggressiveLow:
            vadMode = VADLowBitrate;
            break;
        case kVadAggressiveMid:
            vadMode = VADAggr;
            break;
        case kVadAggressiveHigh:
            vadMode = VADVeryAggr;
            break;
    }
    return channelPtr->SetVADStatus(enable, vadMode, disableDTX);
}

int VoECodecImpl::GetVADStatus(int channel, bool& enabled, VadModes& mode,
                               bool& disabledDTX)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetVADStatus(channel=%i)", channel);

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
            "GetVADStatus failed to locate channel");
        return -1;
    }

    ACMVADMode vadMode;
    int ret = channelPtr->GetVADStatus(enabled, vadMode, disabledDTX);

    if (ret != 0)
    {
        _shared->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "GetVADStatus failed to get VAD mode");
        return -1;
    }
    switch (vadMode)
    {
        case VADNormal:
            mode = kVadConventional;
            break;
        case VADLowBitrate:
            mode = kVadAggressiveLow;
            break;
        case VADAggr:
            mode = kVadAggressiveMid;
            break;
        case VADVeryAggr:
            mode = kVadAggressiveHigh;
            break;
    }

    return 0;
}

void VoECodecImpl::ACMToExternalCodecRepresentation(CodecInst& toInst,
                                                    const CodecInst& fromInst)
{
    toInst = fromInst;
    if (STR_CASE_CMP(fromInst.plname,"SILK") == 0)
    {
        if (fromInst.plfreq == 12000)
        {
            if (fromInst.pacsize == 320)
            {
                toInst.pacsize = 240;
            }
            else if (fromInst.pacsize == 640)
            {
                toInst.pacsize = 480;
            }
            else if (fromInst.pacsize == 960)
            {
                toInst.pacsize = 720;
            }
        }
        else if (fromInst.plfreq == 24000)
        {
            if (fromInst.pacsize == 640)
            {
                toInst.pacsize = 480;
            }
            else if (fromInst.pacsize == 1280)
            {
                toInst.pacsize = 960;
            }
            else if (fromInst.pacsize == 1920)
            {
                toInst.pacsize = 1440;
            }
        }
    }
}

void VoECodecImpl::ExternalToACMCodecRepresentation(CodecInst& toInst,
                                                    const CodecInst& fromInst)
{
    toInst = fromInst;
    if (STR_CASE_CMP(fromInst.plname,"SILK") == 0)
    {
        if (fromInst.plfreq == 12000)
        {
            if (fromInst.pacsize == 240)
            {
                toInst.pacsize = 320;
            }
            else if (fromInst.pacsize == 480)
            {
                toInst.pacsize = 640;
            }
            else if (fromInst.pacsize == 720)
            {
                toInst.pacsize = 960;
            }
        }
        else if (fromInst.plfreq == 24000)
        {
            if (fromInst.pacsize == 480)
            {
                toInst.pacsize = 640;
            }
            else if (fromInst.pacsize == 960)
            {
                toInst.pacsize = 1280;
            }
            else if (fromInst.pacsize == 1440)
            {
                toInst.pacsize = 1920;
            }
        }
    }
}

#endif  // WEBRTC_VOICE_ENGINE_CODEC_API

} // namespace webrtc
