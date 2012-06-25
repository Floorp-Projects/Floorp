/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_encryption_impl.h"


#include "channel.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc {

VoEEncryption* VoEEncryption::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s =
        reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoEEncryptionImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API

VoEEncryptionImpl::VoEEncryptionImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEEncryptionImpl::VoEEncryptionImpl() - ctor");
}

VoEEncryptionImpl::~VoEEncryptionImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEEncryptionImpl::~VoEEncryptionImpl() - dtor");
}

int VoEEncryptionImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEEncryption::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
      // reset reference counter to zero => OK to delete VE
        Reset();
        _shared->SetLastError(VE_INTERFACE_NOT_FOUND, kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "VoEEncryption reference counter = %d", refCount);
    return (refCount);
}

int VoEEncryptionImpl::EnableSRTPSend(
    int channel,
    CipherTypes cipherType,
    int cipherKeyLength,
    AuthenticationTypes authType,
    int authKeyLength,
    int authTagLength,
    SecurityLevels level,
    const unsigned char key[kVoiceEngineMaxSrtpKeyLength],
    bool useForRTCP)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "EnableSRTPSend(channel=%i, cipherType=%i, cipherKeyLength=%i,"
                 " authType=%i, authKeyLength=%i, authTagLength=%i, level=%i, "
                 "key=?, useForRTCP=%d)",
                 channel, cipherType, cipherKeyLength, authType,
                 authKeyLength, authTagLength, level, useForRTCP);
#ifdef WEBRTC_SRTP
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
            "EnableSRTPSend() failed to locate channel");
        return -1;
    }
    return channelPtr->EnableSRTPSend(cipherType,
                                      cipherKeyLength,
                                      authType,
                                      authKeyLength,
                                      authTagLength,
                                      level,
                                      key,
                                      useForRTCP);
#else
   _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
       "EnableSRTPSend() SRTP is not supported");
    return -1;
#endif
}

int VoEEncryptionImpl::DisableSRTPSend(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "DisableSRTPSend(channel=%i)",channel);
#ifdef WEBRTC_SRTP
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
            "DisableSRTPSend() failed to locate channel");
        return -1;
    }
    return channelPtr->DisableSRTPSend();
#else
   _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
       "DisableSRTPSend() SRTP is not supported");
    return -1;
#endif
}

int VoEEncryptionImpl::EnableSRTPReceive(
    int channel,
    CipherTypes cipherType,
    int cipherKeyLength,
    AuthenticationTypes authType,
    int authKeyLength,
    int authTagLength,
    SecurityLevels level,
    const unsigned char key[kVoiceEngineMaxSrtpKeyLength],
		bool useForRTCP)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "EnableSRTPReceive(channel=%i, cipherType=%i, "
                 "cipherKeyLength=%i, authType=%i, authKeyLength=%i, "
                 "authTagLength=%i, level=%i, key=?, useForRTCP=%d)",
                 channel, cipherType, cipherKeyLength, authType,
                 authKeyLength, authTagLength, level, useForRTCP);
#ifdef WEBRTC_SRTP
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
            "EnableSRTPReceive() failed to locate channel");
        return -1;
    }
    return channelPtr->EnableSRTPReceive(cipherType,
                                         cipherKeyLength,
	                                 authType,
	                                 authKeyLength,
	                                 authTagLength,
	                                 level,
	                                 key,
	                                 useForRTCP);
#else
   _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
       "EnableSRTPReceive() SRTP is not supported");
    return -1;
#endif
}

int VoEEncryptionImpl::DisableSRTPReceive(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DisableSRTPReceive(channel=%i)", channel);
#ifdef WEBRTC_SRTP
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
            "DisableSRTPReceive() failed to locate channel");
        return -1;
    }
    return channelPtr->DisableSRTPReceive();
#else
    _shared->SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
        "DisableSRTPReceive() SRTP is not supported");
    return -1;
#endif
}

int VoEEncryptionImpl::RegisterExternalEncryption(int channel,
                                                  Encryption& encryption)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "RegisterExternalEncryption(channel=%d, encryption=0x%x)",
                 channel, &encryption);
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
            "RegisterExternalEncryption() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterExternalEncryption(encryption);
}

int VoEEncryptionImpl::DeRegisterExternalEncryption(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DeRegisterExternalEncryption(channel=%d)", channel);
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
            "DeRegisterExternalEncryption() failed to locate channel");
        return -1;
    }
    return channelPtr->DeRegisterExternalEncryption();
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API

// EOF
}  // namespace webrtc
