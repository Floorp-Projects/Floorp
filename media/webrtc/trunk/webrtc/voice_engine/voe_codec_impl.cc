/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_codec_impl.h"

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

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
    VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
    s->AddRef();
    return s;
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

int VoECodecImpl::NumOfCodecs()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "NumOfCodecs()");

    // Number of supported codecs in the ACM
    uint8_t nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

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
    if (AudioCodingModule::Codec(index, &acmCodec)
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
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
#ifdef WEBRTC_CODEC_AMR
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
#ifdef WEBRTC_CODEC_AMR
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#ifdef WEBRTC_CODEC_AMRWB
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#ifdef WEBRTC_CODEC_AMRWB
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetSendCNPayloadType() failed to locate channel");
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
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#ifdef WEBRTC_CODEC_ISAC
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#ifdef WEBRTC_CODEC_ISAC
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    IPHONE_NOT_SUPPORTED(_shared->statistics());
#ifdef WEBRTC_CODEC_ISAC
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
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

int VoECodecImpl::SetSecondarySendCodec(int channel, const CodecInst& codec,
                                        int red_payload_type) {
  CodecInst copy_codec;
  ExternalToACMCodecRepresentation(copy_codec, codec);

  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetSecondarySendCodec(channel=%d, codec)", channel);
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "codec: plname=%s, pacsize=%d, plfreq=%d, pltype=%d, "
               "channels=%d, rate=%d", codec.plname, codec.pacsize,
               codec.plfreq, codec.pltype, codec.channels, codec.rate);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  // External sanity checks performed outside the ACM
  if ((STR_CASE_CMP(copy_codec.plname, "L16") == 0) &&
      (copy_codec.pacsize >= 960)) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSecondarySendCodec() invalid L16 packet size");
    return -1;
  }

  // None of the following codecs can be registered as the secondary encoder.
  if (!STR_CASE_CMP(copy_codec.plname, "CN") ||
      !STR_CASE_CMP(copy_codec.plname, "TELEPHONE-EVENT") ||
      !STR_CASE_CMP(copy_codec.plname, "RED"))  {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSecondarySendCodec() invalid codec name");
    return -1;
  }

  // Only mono and stereo are supported.
  if ((copy_codec.channels != 1) && (copy_codec.channels != 2)) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSecondarySendCodec() invalid number of channels");
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetSecondarySendCodec() failed to locate channel");
    return -1;
  }
  if (!AudioCodingModule::IsCodecValid(copy_codec)) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSecondarySendCodec() invalid codec");
    return -1;
  }
  if (channelPtr->SetSecondarySendCodec(copy_codec, red_payload_type) != 0) {
    _shared->SetLastError(VE_CANNOT_SET_SECONDARY_SEND_CODEC, kTraceError,
                          "SetSecondarySendCodec() failed to set secondary "
                          "send codec");
    return -1;
  }
  return 0;
}

int VoECodecImpl::GetSecondarySendCodec(int channel, CodecInst& codec) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSecondarySendCodec(channel=%d, codec=?)", channel);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetSecondarySendCodec() failed to locate channel");
    return -1;
  }
  CodecInst acm_codec;
  if (channelPtr->GetSecondarySendCodec(&acm_codec) != 0) {
    _shared->SetLastError(VE_CANNOT_GET_SECONDARY_SEND_CODEC, kTraceError,
                          "GetSecondarySendCodec() failed to get secondary "
                          "send codec");
    return -1;
  }
  ACMToExternalCodecRepresentation(codec, acm_codec);
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_shared->instance_id(), -1),
               "GetSecondarySendCodec() => plname=%s, pacsize=%d, plfreq=%d, "
               "channels=%d, rate=%d", codec.plname, codec.pacsize,
               codec.plfreq, codec.channels, codec.rate);
  return 0;
}

int VoECodecImpl::RemoveSecondarySendCodec(int channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "RemoveSecondarySendCodec(channel=%d)", channel);
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "RemoveSecondarySendCodec() failed to locate "
                          "channel");
    return -1;
  }
  channelPtr->RemoveSecondarySendCodec();
  return 0;
}

#endif  // WEBRTC_VOICE_ENGINE_CODEC_API

}  // namespace webrtc
