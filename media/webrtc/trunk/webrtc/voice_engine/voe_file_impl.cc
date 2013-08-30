/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_file_impl.h"

#include "webrtc/modules/media_file/interface/media_file.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoEFile* VoEFile::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_FILE_API
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

#ifdef WEBRTC_VOICE_ENGINE_FILE_API

VoEFileImpl::VoEFileImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEFileImpl::VoEFileImpl() - ctor");
}

VoEFileImpl::~VoEFileImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEFileImpl::~VoEFileImpl() - dtor");
}

int VoEFileImpl::StartPlayingFileLocally(
    int channel,
    const char fileNameUTF8[1024],
    bool loop, FileFormats format,
    float volumeScaling,
    int startPointMs,
    int stopPointMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartPlayingFileLocally(channel=%d, fileNameUTF8[]=%s, "
                 "loop=%d, format=%d, volumeScaling=%5.3f, startPointMs=%d,"
                 " stopPointMs=%d)",
                 channel, fileNameUTF8, loop, format, volumeScaling,
                 startPointMs, stopPointMs);
    assert(1024 == FileWrapper::kMaxFileNameSize);
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
            "StartPlayingFileLocally() failed to locate channel");
        return -1;
    }

    return channelPtr->StartPlayingFileLocally(fileNameUTF8,
                                               loop,
                                               format,
                                               startPointMs,
                                               volumeScaling,
                                               stopPointMs,
                                               NULL);
}

int VoEFileImpl::StartPlayingFileLocally(int channel,
                                         InStream* stream,
                                         FileFormats format,
                                         float volumeScaling,
                                         int startPointMs,
                                         int stopPointMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartPlayingFileLocally(channel=%d, stream, format=%d, "
                 "volumeScaling=%5.3f, startPointMs=%d, stopPointMs=%d)",
                 channel, format, volumeScaling, startPointMs, stopPointMs);

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
            "StartPlayingFileLocally() failed to locate channel");
        return -1;
    }

    return channelPtr->StartPlayingFileLocally(stream,
                                               format,
                                               startPointMs,
                                               volumeScaling,
                                               stopPointMs,
                                               NULL);
}

int VoEFileImpl::StopPlayingFileLocally(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StopPlayingFileLocally()");
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
            "StopPlayingFileLocally() failed to locate channel");
        return -1;
    }
    return channelPtr->StopPlayingFileLocally();
}

int VoEFileImpl::IsPlayingFileLocally(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "IsPlayingFileLocally(channel=%d)", channel);
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
            "StopPlayingFileLocally() failed to locate channel");
        return -1;
    }
    return channelPtr->IsPlayingFileLocally();
}

int VoEFileImpl::ScaleLocalFilePlayout(int channel, float scale)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ScaleLocalFilePlayout(channel=%d, scale=%5.3f)",
                 channel, scale);
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
            "StopPlayingFileLocally() failed to locate channel");
        return -1;
    }
    return channelPtr->ScaleLocalFilePlayout(scale);
}

int VoEFileImpl::StartPlayingFileAsMicrophone(int channel,
                                              const char fileNameUTF8[1024],
                                              bool loop,
                                              bool mixWithMicrophone,
                                              FileFormats format,
                                              float volumeScaling)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartPlayingFileAsMicrophone(channel=%d, fileNameUTF8=%s, "
                 "loop=%d, mixWithMicrophone=%d, format=%d, "
                 "volumeScaling=%5.3f)",
                 channel, fileNameUTF8, loop, mixWithMicrophone, format,
                 volumeScaling);
    assert(1024 == FileWrapper::kMaxFileNameSize);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    const uint32_t startPointMs(0);
    const uint32_t stopPointMs(0);

    if (channel == -1)
    {
        int res = _shared->transmit_mixer()->StartPlayingFileAsMicrophone(
            fileNameUTF8,
            loop,
            format,
            startPointMs,
            volumeScaling,
            stopPointMs,
            NULL);
        if (res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartPlayingFileAsMicrophone() failed to start playing file");
            return(-1);
        }
        else
        {
            _shared->transmit_mixer()->SetMixWithMicStatus(mixWithMicrophone);
            return(0);
        }
    }
    else
    {
        // Add file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "StartPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }

        int res = channelPtr->StartPlayingFileAsMicrophone(fileNameUTF8,
                                                           loop,
                                                           format,
                                                           startPointMs,
                                                           volumeScaling,
                                                           stopPointMs,
                                                           NULL);
        if (res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartPlayingFileAsMicrophone() failed to start playing file");
            return -1;
        }
        else
        {
            channelPtr->SetMixWithMicStatus(mixWithMicrophone);
            return 0;
        }
    }
}

int VoEFileImpl::StartPlayingFileAsMicrophone(int channel,
                                              InStream* stream,
                                              bool mixWithMicrophone,
                                              FileFormats format,
                                              float volumeScaling)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartPlayingFileAsMicrophone(channel=%d, stream,"
                 " mixWithMicrophone=%d, format=%d, volumeScaling=%5.3f)",
                 channel, mixWithMicrophone, format, volumeScaling);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    const uint32_t startPointMs(0);
    const uint32_t stopPointMs(0);

    if (channel == -1)
    {
        int res = _shared->transmit_mixer()->StartPlayingFileAsMicrophone(
            stream,
            format,
            startPointMs,
            volumeScaling,
            stopPointMs,
            NULL);
        if (res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartPlayingFileAsMicrophone() failed to start "
                "playing stream");
            return(-1);
        }
        else
        {
            _shared->transmit_mixer()->SetMixWithMicStatus(mixWithMicrophone);
            return(0);
        }
    }
    else
    {
        // Add file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "StartPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }

        int res = channelPtr->StartPlayingFileAsMicrophone(
            stream, format, startPointMs, volumeScaling, stopPointMs, NULL);
        if (res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartPlayingFileAsMicrophone() failed to start "
                "playing stream");
            return -1;
        }
        else
        {
            channelPtr->SetMixWithMicStatus(mixWithMicrophone);
            return 0;
        }
    }
}

int VoEFileImpl::StopPlayingFileAsMicrophone(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StopPlayingFileAsMicrophone(channel=%d)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        // Stop adding file before demultiplexing <=> affects all channels
        return _shared->transmit_mixer()->StopPlayingFileAsMicrophone();
    }
    else
    {
        // Stop adding file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "StopPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }
        return channelPtr->StopPlayingFileAsMicrophone();
    }
}

int VoEFileImpl::IsPlayingFileAsMicrophone(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "IsPlayingFileAsMicrophone(channel=%d)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _shared->transmit_mixer()->IsPlayingFileAsMicrophone();
    }
    else
    {
        // Stop adding file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "IsPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }
        return channelPtr->IsPlayingFileAsMicrophone();
    }
}

int VoEFileImpl::ScaleFileAsMicrophonePlayout(int channel, float scale)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ScaleFileAsMicrophonePlayout(channel=%d, scale=%5.3f)",
                 channel, scale);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _shared->transmit_mixer()->ScaleFileAsMicrophonePlayout(scale);
    }
    else
    {
        // Stop adding file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "IsPlayingFileAsMicrophone() failed to locate channel");
            return -1;
        }
        return channelPtr->ScaleFileAsMicrophonePlayout(scale);
    }
}

int VoEFileImpl::StartRecordingPlayout(
    int channel, const char* fileNameUTF8, CodecInst* compression,
    int maxSizeBytes)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartRecordingPlayout(channel=%d, fileNameUTF8=%s, "
                 "compression, maxSizeBytes=%d)",
                 channel, fileNameUTF8, maxSizeBytes);
    assert(1024 == FileWrapper::kMaxFileNameSize);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _shared->output_mixer()->StartRecordingPlayout
          (fileNameUTF8, compression);
    }
    else
    {
        // Add file after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "StartRecordingPlayout() failed to locate channel");
            return -1;
        }
        return channelPtr->StartRecordingPlayout(fileNameUTF8, compression);
    }
}

int VoEFileImpl::StartRecordingPlayout(
    int channel, OutStream* stream, CodecInst* compression)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartRecordingPlayout(channel=%d, stream, compression)",
                 channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _shared->output_mixer()->
            StartRecordingPlayout(stream, compression);
    }
    else
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "StartRecordingPlayout() failed to locate channel");
            return -1;
        }
        return channelPtr->StartRecordingPlayout(stream, compression);
    }
}

int VoEFileImpl::StopRecordingPlayout(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StopRecordingPlayout(channel=%d)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _shared->output_mixer()->StopRecordingPlayout();
    }
    else
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "StopRecordingPlayout() failed to locate channel");
            return -1;
        }
        return channelPtr->StopRecordingPlayout();
    }
}

int VoEFileImpl::StartRecordingMicrophone(
    const char* fileNameUTF8, CodecInst* compression, int maxSizeBytes)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartRecordingMicrophone(fileNameUTF8=%s, compression, "
                 "maxSizeBytes=%d)", fileNameUTF8, maxSizeBytes);
    assert(1024 == FileWrapper::kMaxFileNameSize);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (_shared->transmit_mixer()->StartRecordingMicrophone(fileNameUTF8,
                                                          compression))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
            VoEId(_shared->instance_id(), -1),
            "StartRecordingMicrophone() failed to start recording");
        return -1;
    }
    if (_shared->audio_device()->Recording())
    {
        return 0;
    }
    if (!_shared->ext_recording())
    {
        if (_shared->audio_device()->InitRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartRecordingMicrophone() failed to initialize recording");
            return -1;
        }
        if (_shared->audio_device()->StartRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartRecordingMicrophone() failed to start recording");
            return -1;
        }
    }
    return 0;
}

int VoEFileImpl::StartRecordingMicrophone(
    OutStream* stream, CodecInst* compression)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartRecordingMicrophone(stream, compression)");

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (_shared->transmit_mixer()->StartRecordingMicrophone(stream,
                                                          compression) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
            VoEId(_shared->instance_id(), -1),
            "StartRecordingMicrophone() failed to start recording");
        return -1;
    }
    if (_shared->audio_device()->Recording())
    {
        return 0;
    }
    if (!_shared->ext_recording())
    {
        if (_shared->audio_device()->InitRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartRecordingMicrophone() failed to initialize recording");
            return -1;
        }
        if (_shared->audio_device()->StartRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartRecordingMicrophone() failed to start recording");
            return -1;
        }
    }
    return 0;
}

int VoEFileImpl::StopRecordingMicrophone()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StopRecordingMicrophone()");
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    int err = 0;

    // TODO(xians): consider removing Start/StopRecording() in
    // Start/StopRecordingMicrophone() if no channel is recording.
    if (_shared->NumOfSendingChannels() == 0 &&
        _shared->audio_device()->Recording())
    {
        // Stop audio-device recording if no channel is recording
        if (_shared->audio_device()->StopRecording() != 0)
        {
            _shared->SetLastError(VE_CANNOT_STOP_RECORDING, kTraceError,
                "StopRecordingMicrophone() failed to stop recording");
            err = -1;
        }
    }

    if (_shared->transmit_mixer()->StopRecordingMicrophone() != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StopRecordingMicrophone() failed to stop recording to mixer");
        err = -1;
    }

    return err;
}

// TODO(andrew): a cursory inspection suggests there's a large amount of
// overlap in these convert functions which could be refactored to a helper.
int VoEFileImpl::ConvertPCMToWAV(const char* fileNameInUTF8,
                                 const char* fileNameOutUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ConvertPCMToWAV(fileNameInUTF8=%s, fileNameOutUTF8=%s)",
                 fileNameInUTF8, fileNameOutUTF8);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1,
        kFileFormatPcm16kHzFile));

    int res=playerObj.StartPlayingFile(fileNameInUTF8,false,0,1.0,0,0, NULL);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToWAV failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatWavFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(fileNameOutUTF8,codecInst,0);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToWAV failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    int16_t decodedData[160];
    int decLength=0;
    const uint32_t frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }

        audioFrame.UpdateFrame(-1, 0, decodedData,
                               (uint16_t)decLength,
                               frequency, AudioFrame::kNormalSpeech,
                               AudioFrame::kVadActive);

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "ConvertPCMToWAV failed during conversion (write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertPCMToWAV(InStream* streamIn, OutStream* streamOut)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ConvertPCMToWAV(streamIn, streamOut)");

    if ((streamIn == NULL) || (streamOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
            VoEId(_shared->instance_id(), -1), "invalid stream handles");
        return (-1);
    }

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(-1,
        kFileFormatPcm16kHzFile));
    int res = playerObj.StartPlayingFile(*streamIn,0,1.0,0,0,NULL);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToWAV failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(-1,
        kFileFormatWavFile));
    CodecInst codecInst;
    strncpy(codecInst.plname, "L16", 32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;
    res = recObj.StartRecordingAudioFile(*streamOut,codecInst,0);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToWAV failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    int16_t decodedData[160];
    int decLength=0;
    const uint32_t frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }

        audioFrame.UpdateFrame(-1, 0, decodedData,
                               (uint16_t)decLength, frequency,
                               AudioFrame::kNormalSpeech,
                               AudioFrame::kVadActive);

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "ConvertPCMToWAV failed during conversion (write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertWAVToPCM(const char* fileNameInUTF8,
                                 const char* fileNameOutUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ConvertWAVToPCM(fileNameInUTF8=%s, fileNameOutUTF8=%s)",
                 fileNameInUTF8, fileNameOutUTF8);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(-1,
                                                        kFileFormatWavFile));
    int res = playerObj.StartPlayingFile(fileNameInUTF8,false,0,1.0,0,0,NULL);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertWAVToPCM failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatPcm16kHzFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(fileNameOutUTF8,codecInst,0);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertWAVToPCM failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    int16_t decodedData[160];
    int decLength=0;
    const uint32_t frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }

        audioFrame.UpdateFrame(-1, 0, decodedData,
                               (uint16_t)decLength,
                               frequency, AudioFrame::kNormalSpeech,
                               AudioFrame::kVadActive);

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "ConvertWAVToPCM failed during conversion (write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertWAVToPCM(InStream* streamIn, OutStream* streamOut)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ConvertWAVToPCM(streamIn, streamOut)");

    if ((streamIn == NULL) || (streamOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
          VoEId(_shared->instance_id(), -1), "invalid stream handles");
        return (-1);
    }

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(-1,
                                                        kFileFormatWavFile));
    int res = playerObj.StartPlayingFile(*streamIn,0,1.0,0,0,NULL);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertWAVToPCM failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatPcm16kHzFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(*streamOut,codecInst,0);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertWAVToPCM failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    int16_t decodedData[160];
    int decLength=0;
    const uint32_t frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }

        audioFrame.UpdateFrame(-1, 0, decodedData,
                               (uint16_t)decLength, frequency,
                               AudioFrame::kNormalSpeech,
                               AudioFrame::kVadActive);

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "ConvertWAVToPCM failed during conversion (write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertPCMToCompressed(const char* fileNameInUTF8,
                                        const char* fileNameOutUTF8,
                                        CodecInst* compression)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ConvertPCMToCompressed(fileNameInUTF8=%s, fileNameOutUTF8=%s"
                 ",  compression)", fileNameInUTF8, fileNameOutUTF8);
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "  compression: plname=%s, plfreq=%d, pacsize=%d",
                 compression->plname, compression->plfreq,
                 compression->pacsize);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1,
        kFileFormatPcm16kHzFile));
    int res = playerObj.StartPlayingFile(fileNameInUTF8,false,0,1.0,0,0, NULL);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToCompressed failed to create player object");
        // Clean up and shutdown the file player
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1,
        kFileFormatCompressedFile));
    res = recObj.StartRecordingAudioFile(fileNameOutUTF8, *compression,0);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToCompressed failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    int16_t decodedData[160];
    int decLength=0;
    const uint32_t frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }
        audioFrame.UpdateFrame(-1, 0, decodedData,
                               (uint16_t)decLength,
                               frequency, AudioFrame::kNormalSpeech,
                               AudioFrame::kVadActive);

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "ConvertPCMToCompressed failed during conversion "
                "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertPCMToCompressed(InStream* streamIn,
                                        OutStream* streamOut,
                                        CodecInst* compression)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ConvertPCMToCompressed(streamIn, streamOut, compression)");

    if ((streamIn == NULL) || (streamOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
            VoEId(_shared->instance_id(), -1), "invalid stream handles");
        return (-1);
    }

    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "  compression: plname=%s, plfreq=%d, pacsize=%d",
                 compression->plname, compression->plfreq,
                 compression->pacsize);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1, kFileFormatPcm16kHzFile));

    int res = playerObj.StartPlayingFile(*streamIn,0,1.0,0,0,NULL);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToCompressed failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatCompressedFile));
    res = recObj.StartRecordingAudioFile(*streamOut,*compression,0);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertPCMToCompressed failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    int16_t decodedData[160];
    int decLength=0;
    const uint32_t frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }
        audioFrame.UpdateFrame(-1, 0, decodedData,
                               (uint16_t)decLength,
                               frequency, AudioFrame::kNormalSpeech,
                               AudioFrame::kVadActive);

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "ConvertPCMToCompressed failed during conversion "
                "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertCompressedToPCM(const char* fileNameInUTF8,
                                        const char* fileNameOutUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ConvertCompressedToPCM(fileNameInUTF8=%s,"
                 " fileNameOutUTF8=%s)",
                 fileNameInUTF8, fileNameOutUTF8);

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1, kFileFormatCompressedFile));

    int res = playerObj.StartPlayingFile(fileNameInUTF8,false,0,1.0,0,0,NULL);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertCompressedToPCM failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatPcm16kHzFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(fileNameOutUTF8,codecInst,0);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertCompressedToPCM failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    int16_t decodedData[160];
    int decLength=0;
    const uint32_t frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }
        audioFrame.UpdateFrame(-1, 0, decodedData,
                               (uint16_t)decLength,
                               frequency,
                               AudioFrame::kNormalSpeech,
                               AudioFrame::kVadActive);

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "ConvertCompressedToPCM failed during conversion "
                "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}

int VoEFileImpl::ConvertCompressedToPCM(InStream* streamIn,
                                        OutStream* streamOut)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ConvertCompressedToPCM(file, file);");

    if ((streamIn == NULL) || (streamOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
            VoEId(_shared->instance_id(), -1), "invalid stream handles");
        return (-1);
    }

    // Create file player object
    FilePlayer& playerObj(*FilePlayer::CreateFilePlayer(
        -1, kFileFormatCompressedFile));
    int res;

    res = playerObj.StartPlayingFile(*streamIn,0,1.0,0,0,NULL);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertCompressedToPCM failed to create player object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        return -1;
    }

    // Create file recorder object
    FileRecorder& recObj(*FileRecorder::CreateFileRecorder(
        -1, kFileFormatPcm16kHzFile));

    CodecInst codecInst;
    strncpy(codecInst.plname,"L16",32);
            codecInst.channels = 1;
            codecInst.rate     = 256000;
            codecInst.plfreq   = 16000;
            codecInst.pltype   = 94;
            codecInst.pacsize  = 160;

    res = recObj.StartRecordingAudioFile(*streamOut,codecInst,0);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "ConvertCompressedToPCM failed to create recorder object");
        playerObj.StopPlayingFile();
        FilePlayer::DestroyFilePlayer(&playerObj);
        recObj.StopRecording();
        FileRecorder::DestroyFileRecorder(&recObj);
        return -1;
    }

    // Run throught the file
    AudioFrame audioFrame;
    int16_t decodedData[160];
    int decLength=0;
    const uint32_t frequency = 16000;

    while(!playerObj.Get10msAudioFromFile(decodedData,decLength,frequency))
    {
        if(decLength!=frequency/100)
        {
            // This is an OK way to end
            break;
        }
        audioFrame.UpdateFrame(-1, 0, decodedData,
                               (uint16_t)decLength,
                               frequency,
                               AudioFrame::kNormalSpeech,
                               AudioFrame::kVadActive);

        res=recObj.RecordAudioToFile(audioFrame);
        if(res)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "ConvertCompressedToPCM failed during conversion "
                "(write frame)");
        }
    }

    playerObj.StopPlayingFile();
    recObj.StopRecording();
    FilePlayer::DestroyFilePlayer(&playerObj);
    FileRecorder::DestroyFileRecorder(&recObj);

    return res;
}


int VoEFileImpl::GetFileDuration(const char* fileNameUTF8,
                                 int& durationMs,
                                 FileFormats format)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetFileDuration(fileNameUTF8=%s, format=%d)",
                 fileNameUTF8, format);

    // Create a dummy file module for this
    MediaFile * fileModule=MediaFile::CreateMediaFile(-1);

    // Temp container of the right format
    uint32_t duration;
    int res=fileModule->FileDurationMs(fileNameUTF8,duration,format);
    if (res)
    {
        _shared->SetLastError(VE_BAD_FILE, kTraceError,
            "GetFileDuration() failed measure file duration");
        return -1;
    }
    durationMs = duration;
    MediaFile::DestroyMediaFile(fileModule);
    fileModule = NULL;

    return(res);
}

int VoEFileImpl::GetPlaybackPosition(int channel, int& positionMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetPlaybackPosition(channel=%d)", channel);

    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetPlaybackPosition() failed to locate channel");
        return -1;
    }
    return channelPtr->GetLocalPlayoutPosition(positionMs);
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_FILE_API

}  // namespace webrtc
