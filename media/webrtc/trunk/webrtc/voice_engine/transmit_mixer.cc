/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "transmit_mixer.h"

#include "audio_frame_operations.h"
#include "channel.h"
#include "channel_manager.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "statistics.h"
#include "trace.h"
#include "utility.h"
#include "voe_base_impl.h"
#include "voe_external_media.h"
#include "webrtc/system_wrappers/interface/logging.h"

#define WEBRTC_ABS(a) (((a) < 0) ? -(a) : (a))

namespace webrtc {

namespace voe {

// Used for downmixing before resampling.
// TODO(ajm): audio_device should advertise the maximum sample rate it can
//            provide.
static const int kMaxMonoDeviceDataSizeSamples = 960;  // 10 ms, 96 kHz, mono.

// TODO(ajm): The thread safety of this is dubious...
void
TransmitMixer::OnPeriodicProcess()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::OnPeriodicProcess()");

#if defined(WEBRTC_VOICE_ENGINE_TYPING_DETECTION)
    if (_typingNoiseWarning)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        if (_voiceEngineObserverPtr)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                         "TransmitMixer::OnPeriodicProcess() => "
                         "CallbackOnError(VE_TYPING_NOISE_WARNING)");
            _voiceEngineObserverPtr->CallbackOnError(-1,
                                                     VE_TYPING_NOISE_WARNING);
        }
        _typingNoiseWarning = false;
    }
#endif

    bool saturationWarning = false;
    {
      // Modify |_saturationWarning| under lock to avoid conflict with write op
      // in ProcessAudio and also ensure that we don't hold the lock during the
      // callback.
      CriticalSectionScoped cs(&_critSect);
      saturationWarning = _saturationWarning;
      if (_saturationWarning)
        _saturationWarning = false;
    }

    if (saturationWarning)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        if (_voiceEngineObserverPtr)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                         "TransmitMixer::OnPeriodicProcess() =>"
                         " CallbackOnError(VE_SATURATION_WARNING)");
            _voiceEngineObserverPtr->CallbackOnError(-1, VE_SATURATION_WARNING);
        }
    }
}


void TransmitMixer::PlayNotification(const int32_t id,
                                     const uint32_t durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void TransmitMixer::RecordNotification(const int32_t id,
                                       const uint32_t durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::RecordNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void TransmitMixer::PlayFileEnded(const int32_t id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayFileEnded(id=%d)", id);

    assert(id == _filePlayerId);

    CriticalSectionScoped cs(&_critSect);

    _filePlaying = false;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayFileEnded() =>"
                 "file player module is shutdown");
}

void
TransmitMixer::RecordFileEnded(const int32_t id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::RecordFileEnded(id=%d)", id);

    if (id == _fileRecorderId)
    {
        CriticalSectionScoped cs(&_critSect);
        _fileRecording = false;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordFileEnded() => fileRecorder module"
                     "is shutdown");
    } else if (id == _fileCallRecorderId)
    {
        CriticalSectionScoped cs(&_critSect);
        _fileCallRecording = false;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordFileEnded() => fileCallRecorder"
                     "module is shutdown");
    }
}

int32_t
TransmitMixer::Create(TransmitMixer*& mixer, const uint32_t instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, -1),
                 "TransmitMixer::Create(instanceId=%d)", instanceId);
    mixer = new TransmitMixer(instanceId);
    if (mixer == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, -1),
                     "TransmitMixer::Create() unable to allocate memory"
                     "for mixer");
        return -1;
    }
    return 0;
}

void
TransmitMixer::Destroy(TransmitMixer*& mixer)
{
    if (mixer)
    {
        delete mixer;
        mixer = NULL;
    }
}

TransmitMixer::TransmitMixer(const uint32_t instanceId) :
    _engineStatisticsPtr(NULL),
    _channelManagerPtr(NULL),
    audioproc_(NULL),
    _voiceEngineObserverPtr(NULL),
    _processThreadPtr(NULL),
    _filePlayerPtr(NULL),
    _fileRecorderPtr(NULL),
    _fileCallRecorderPtr(NULL),
    // Avoid conflict with other channels by adding 1024 - 1026,
    // won't use as much as 1024 channels.
    _filePlayerId(instanceId + 1024),
    _fileRecorderId(instanceId + 1025),
    _fileCallRecorderId(instanceId + 1026),
    _filePlaying(false),
    _fileRecording(false),
    _fileCallRecording(false),
    _audioLevel(),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    _timeActive(0),
    _timeSinceLastTyping(0),
    _penaltyCounter(0),
    _typingNoiseWarning(false),
    _timeWindow(10), // 10ms slots accepted to count as a hit
    _costPerTyping(100), // Penalty added for a typing + activity coincide
    _reportingThreshold(300), // Threshold for _penaltyCounter
    _penaltyDecay(1), // how much we reduce _penaltyCounter every 10 ms.
    _typeEventDelay(2), // how "old" event we check for
#endif
    _saturationWarning(false),
    _instanceId(instanceId),
    _mixFileWithMicrophone(false),
    _captureLevel(0),
    external_postproc_ptr_(NULL),
    external_preproc_ptr_(NULL),
    _mute(false),
    _remainingMuteMicTimeMs(0),
    stereo_codec_(false),
    swap_stereo_channels_(false)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::TransmitMixer() - ctor");
}

TransmitMixer::~TransmitMixer()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::~TransmitMixer() - dtor");
    _monitorModule.DeRegisterObserver();
    if (_processThreadPtr)
    {
        _processThreadPtr->DeRegisterModule(&_monitorModule);
    }
    DeRegisterExternalMediaProcessing(kRecordingAllChannelsMixed);
    DeRegisterExternalMediaProcessing(kRecordingPreprocessing);
    {
        CriticalSectionScoped cs(&_critSect);
        if (_fileRecorderPtr)
        {
            _fileRecorderPtr->RegisterModuleFileCallback(NULL);
            _fileRecorderPtr->StopRecording();
            FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
            _fileRecorderPtr = NULL;
        }
        if (_fileCallRecorderPtr)
        {
            _fileCallRecorderPtr->RegisterModuleFileCallback(NULL);
            _fileCallRecorderPtr->StopRecording();
            FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
            _fileCallRecorderPtr = NULL;
        }
        if (_filePlayerPtr)
        {
            _filePlayerPtr->RegisterModuleFileCallback(NULL);
            _filePlayerPtr->StopPlayingFile();
            FilePlayer::DestroyFilePlayer(_filePlayerPtr);
            _filePlayerPtr = NULL;
        }
    }
    delete &_critSect;
    delete &_callbackCritSect;
}

int32_t
TransmitMixer::SetEngineInformation(ProcessThread& processThread,
                                    Statistics& engineStatistics,
                                    ChannelManager& channelManager)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetEngineInformation()");

    _processThreadPtr = &processThread;
    _engineStatisticsPtr = &engineStatistics;
    _channelManagerPtr = &channelManager;

    if (_processThreadPtr->RegisterModule(&_monitorModule) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::SetEngineInformation() failed to"
                     "register the monitor module");
    } else
    {
        _monitorModule.RegisterObserver(*this);
    }

    return 0;
}

int32_t
TransmitMixer::RegisterVoiceEngineObserver(VoiceEngineObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::RegisterVoiceEngineObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_voiceEngineObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterVoiceEngineObserver() observer already enabled");
        return -1;
    }
    _voiceEngineObserverPtr = &observer;
    return 0;
}

int32_t
TransmitMixer::SetAudioProcessingModule(AudioProcessing* audioProcessingModule)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetAudioProcessingModule("
                 "audioProcessingModule=0x%x)",
                 audioProcessingModule);
    audioproc_ = audioProcessingModule;
    return 0;
}

void TransmitMixer::GetSendCodecInfo(int* max_sample_rate, int* max_channels) {
  ScopedChannel sc(*_channelManagerPtr);
  void* iterator = NULL;
  Channel* channel = sc.GetFirstChannel(iterator);

  *max_sample_rate = 8000;
  *max_channels = 1;
  while (channel != NULL) {
    if (channel->Sending()) {
      CodecInst codec;
      channel->GetSendCodec(codec);
      // TODO(tlegrand): Remove the 32 kHz restriction once we have full 48 kHz
      // support in Audio Coding Module.
      *max_sample_rate = std::min(32000,
                                  std::max(*max_sample_rate, codec.plfreq));
      *max_channels = std::max(*max_channels, codec.channels);
    }
    channel = sc.GetNextChannel(iterator);
  }
}

int32_t
TransmitMixer::PrepareDemux(const void* audioSamples,
                            const uint32_t nSamples,
                            const uint8_t nChannels,
                            const uint32_t samplesPerSec,
                            const uint16_t totalDelayMS,
                            const int32_t clockDrift,
                            const uint16_t currentMicLevel)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PrepareDemux(nSamples=%u, nChannels=%u,"
                 "samplesPerSec=%u, totalDelayMS=%u, clockDrift=%d,"
                 "currentMicLevel=%u)", nSamples, nChannels, samplesPerSec,
                 totalDelayMS, clockDrift, currentMicLevel);

    // --- Resample input audio and create/store the initial audio frame
    if (GenerateAudioFrame(static_cast<const int16_t*>(audioSamples),
                           nSamples,
                           nChannels,
                           samplesPerSec) == -1)
    {
        return -1;
    }

    {
      CriticalSectionScoped cs(&_callbackCritSect);
      if (external_preproc_ptr_) {
        external_preproc_ptr_->Process(-1, kRecordingPreprocessing,
                                       _audioFrame.data_,
                                       _audioFrame.samples_per_channel_,
                                       _audioFrame.sample_rate_hz_,
                                       _audioFrame.num_channels_ == 2);
      }
    }

    // --- Near-end audio processing.
    ProcessAudio(totalDelayMS, clockDrift, currentMicLevel);

    if (swap_stereo_channels_ && stereo_codec_)
      // Only bother swapping if we're using a stereo codec.
      AudioFrameOperations::SwapStereoChannels(&_audioFrame);

    // --- Annoying typing detection (utilizes the APM/VAD decision)
#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    TypingDetection();
#endif

    // --- Mute during DTMF tone if direct feedback is enabled
    if (_remainingMuteMicTimeMs > 0)
    {
        AudioFrameOperations::Mute(_audioFrame);
        _remainingMuteMicTimeMs -= 10;
        if (_remainingMuteMicTimeMs < 0)
        {
            _remainingMuteMicTimeMs = 0;
        }
    }

    // --- Mute signal
    if (_mute)
    {
        AudioFrameOperations::Mute(_audioFrame);
    }

    // --- Mix with file (does not affect the mixing frequency)
    if (_filePlaying)
    {
        MixOrReplaceAudioWithFile(_audioFrame.sample_rate_hz_);
    }

    // --- Record to file
    if (_fileRecording)
    {
        RecordAudioToFile(_audioFrame.sample_rate_hz_);
    }

    {
      CriticalSectionScoped cs(&_callbackCritSect);
      if (external_postproc_ptr_) {
        external_postproc_ptr_->Process(-1, kRecordingAllChannelsMixed,
                                        _audioFrame.data_,
                                        _audioFrame.samples_per_channel_,
                                        _audioFrame.sample_rate_hz_,
                                        _audioFrame.num_channels_ == 2);
      }
    }

    // --- Measure audio level of speech after all processing.
    _audioLevel.ComputeLevel(_audioFrame);
    return 0;
}

int32_t
TransmitMixer::DemuxAndMix()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::DemuxAndMix()");

    ScopedChannel sc(*_channelManagerPtr);
    void* iterator(NULL);
    Channel* channelPtr = sc.GetFirstChannel(iterator);
    while (channelPtr != NULL)
    {
        if (channelPtr->InputIsOnHold())
        {
            channelPtr->UpdateLocalTimeStamp();
        } else if (channelPtr->Sending())
        {
            // Demultiplex makes a copy of its input.
            channelPtr->Demultiplex(_audioFrame);
            channelPtr->PrepareEncodeAndSend(_audioFrame.sample_rate_hz_);
        }
        channelPtr = sc.GetNextChannel(iterator);
    }
    return 0;
}

int32_t
TransmitMixer::EncodeAndSend()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::EncodeAndSend()");

    ScopedChannel sc(*_channelManagerPtr);
    void* iterator(NULL);
    Channel* channelPtr = sc.GetFirstChannel(iterator);
    while (channelPtr != NULL)
    {
        if (channelPtr->Sending() && !channelPtr->InputIsOnHold())
        {
            channelPtr->EncodeAndSend();
        }
        channelPtr = sc.GetNextChannel(iterator);
    }
    return 0;
}

uint32_t TransmitMixer::CaptureLevel() const
{
    CriticalSectionScoped cs(&_critSect);
    return _captureLevel;
}

void
TransmitMixer::UpdateMuteMicrophoneTime(const uint32_t lengthMs)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::UpdateMuteMicrophoneTime(lengthMs=%d)",
               lengthMs);
    _remainingMuteMicTimeMs = lengthMs;
}

int32_t
TransmitMixer::StopSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::StopSend()");
    _audioLevel.Clear();
    return 0;
}

int TransmitMixer::StartPlayingFileAsMicrophone(const char* fileName,
                                                const bool loop,
                                                const FileFormats format,
                                                const int startPosition,
                                                const float volumeScaling,
                                                const int stopPosition,
                                                const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartPlayingFileAsMicrophone("
                 "fileNameUTF8[]=%s,loop=%d, format=%d, volumeScaling=%5.3f,"
                 " startPosition=%d, stopPosition=%d)", fileName, loop,
                 format, volumeScaling, startPosition, stopPosition);

    if (_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() is already playing");
        return 0;
    }

    CriticalSectionScoped cs(&_critSect);

    // Destroy the old instance
    if (_filePlayerPtr)
    {
        _filePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_filePlayerPtr);
        _filePlayerPtr = NULL;
    }

    // Dynamically create the instance
    _filePlayerPtr
        = FilePlayer::CreateFilePlayer(_filePlayerId,
                                       (const FileFormats) format);

    if (_filePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
        return -1;
    }

    const uint32_t notificationTime(0);

    if (_filePlayerPtr->StartPlayingFile(
        fileName,
        loop,
        startPosition,
        volumeScaling,
        notificationTime,
        stopPosition,
        (const CodecInst*) codecInst) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFile() failed to start file playout");
        _filePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_filePlayerPtr);
        _filePlayerPtr = NULL;
        return -1;
    }

    _filePlayerPtr->RegisterModuleFileCallback(this);
    _filePlaying = true;

    return 0;
}

int TransmitMixer::StartPlayingFileAsMicrophone(InStream* stream,
                                                const FileFormats format,
                                                const int startPosition,
                                                const float volumeScaling,
                                                const int stopPosition,
                                                const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::StartPlayingFileAsMicrophone(format=%d,"
                 " volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
                 format, volumeScaling, startPosition, stopPosition);

    if (stream == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFileAsMicrophone() NULL as input stream");
        return -1;
    }

    if (_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() is already playing");
        return 0;
    }

    CriticalSectionScoped cs(&_critSect);

    // Destroy the old instance
    if (_filePlayerPtr)
    {
        _filePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_filePlayerPtr);
        _filePlayerPtr = NULL;
    }

    // Dynamically create the instance
    _filePlayerPtr
        = FilePlayer::CreateFilePlayer(_filePlayerId,
                                       (const FileFormats) format);

    if (_filePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceWarning,
            "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
        return -1;
    }

    const uint32_t notificationTime(0);

    if (_filePlayerPtr->StartPlayingFile(
        (InStream&) *stream,
        startPosition,
        volumeScaling,
        notificationTime,
        stopPosition,
        (const CodecInst*) codecInst) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFile() failed to start file playout");
        _filePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_filePlayerPtr);
        _filePlayerPtr = NULL;
        return -1;
    }
    _filePlayerPtr->RegisterModuleFileCallback(this);
    _filePlaying = true;

    return 0;
}

int TransmitMixer::StopPlayingFileAsMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::StopPlayingFileAsMicrophone()");

    if (!_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "StopPlayingFileAsMicrophone() isnot playing");
        return 0;
    }

    CriticalSectionScoped cs(&_critSect);

    if (_filePlayerPtr->StopPlayingFile() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_STOP_PLAYOUT, kTraceError,
            "StopPlayingFile() couldnot stop playing file");
        return -1;
    }

    _filePlayerPtr->RegisterModuleFileCallback(NULL);
    FilePlayer::DestroyFilePlayer(_filePlayerPtr);
    _filePlayerPtr = NULL;
    _filePlaying = false;

    return 0;
}

int TransmitMixer::IsPlayingFileAsMicrophone() const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::IsPlayingFileAsMicrophone()");
    return _filePlaying;
}

int TransmitMixer::ScaleFileAsMicrophonePlayout(const float scale)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::ScaleFileAsMicrophonePlayout(scale=%5.3f)",
                 scale);

    CriticalSectionScoped cs(&_critSect);

    if (!_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "ScaleFileAsMicrophonePlayout() isnot playing file");
        return -1;
    }

    if ((_filePlayerPtr == NULL) ||
        (_filePlayerPtr->SetAudioScaling(scale) != 0))
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "SetAudioScaling() failed to scale playout");
        return -1;
    }

    return 0;
}

int TransmitMixer::StartRecordingMicrophone(const char* fileName,
                                            const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingMicrophone(fileName=%s)",
                 fileName);

    if (_fileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingMicrophone() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL &&
      (codecInst->channels < 0 || codecInst->channels > 2))
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_critSect);

    // Destroy the old instance
    if (_fileRecorderPtr)
    {
        _fileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
        _fileRecorderPtr = NULL;
    }

    _fileRecorderPtr =
        FileRecorder::CreateFileRecorder(_fileRecorderId,
                                         (const FileFormats) format);
    if (_fileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() fileRecorder format isnot correct");
        return -1;
    }

    if (_fileRecorderPtr->StartRecordingAudioFile(
        fileName,
        (const CodecInst&) *codecInst,
        notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRecordingAudioFile() failed to start file recording");
        _fileRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
        _fileRecorderPtr = NULL;
        return -1;
    }
    _fileRecorderPtr->RegisterModuleFileCallback(this);
    _fileRecording = true;

    return 0;
}

int TransmitMixer::StartRecordingMicrophone(OutStream* stream,
                                            const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::StartRecordingMicrophone()");

    if (_fileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "StartRecordingMicrophone() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_critSect);

    // Destroy the old instance
    if (_fileRecorderPtr)
    {
        _fileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
        _fileRecorderPtr = NULL;
    }

    _fileRecorderPtr =
        FileRecorder::CreateFileRecorder(_fileRecorderId,
                                         (const FileFormats) format);
    if (_fileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() fileRecorder format isnot correct");
        return -1;
    }

    if (_fileRecorderPtr->StartRecordingAudioFile(*stream,
                                                  *codecInst,
                                                  notificationTime) != 0)
    {
    _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
      "StartRecordingAudioFile() failed to start file recording");
    _fileRecorderPtr->StopRecording();
    FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
    _fileRecorderPtr = NULL;
    return -1;
    }

    _fileRecorderPtr->RegisterModuleFileCallback(this);
    _fileRecording = true;

    return 0;
}


int TransmitMixer::StopRecordingMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StopRecordingMicrophone()");

    if (!_fileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "StopRecordingMicrophone() isnot recording");
        return 0;
    }

    CriticalSectionScoped cs(&_critSect);

    if (_fileRecorderPtr->StopRecording() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopRecording(), could not stop recording");
        return -1;
    }
    _fileRecorderPtr->RegisterModuleFileCallback(NULL);
    FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
    _fileRecorderPtr = NULL;
    _fileRecording = false;

    return 0;
}

int TransmitMixer::StartRecordingCall(const char* fileName,
                                      const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingCall(fileName=%s)", fileName);

    if (_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingCall() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingCall() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_critSect);

    // Destroy the old instance
    if (_fileCallRecorderPtr)
    {
        _fileCallRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
        _fileCallRecorderPtr = NULL;
    }

    _fileCallRecorderPtr
        = FileRecorder::CreateFileRecorder(_fileCallRecorderId,
                                           (const FileFormats) format);
    if (_fileCallRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingCall() fileRecorder format isnot correct");
        return -1;
    }

    if (_fileCallRecorderPtr->StartRecordingAudioFile(
        fileName,
        (const CodecInst&) *codecInst,
        notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRecordingAudioFile() failed to start file recording");
        _fileCallRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
        _fileCallRecorderPtr = NULL;
        return -1;
    }
    _fileCallRecorderPtr->RegisterModuleFileCallback(this);
    _fileCallRecording = true;

    return 0;
}

int TransmitMixer::StartRecordingCall(OutStream* stream,
                                      const  CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingCall()");

    if (_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingCall() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingCall() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_critSect);

    // Destroy the old instance
    if (_fileCallRecorderPtr)
    {
        _fileCallRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
        _fileCallRecorderPtr = NULL;
    }

    _fileCallRecorderPtr =
        FileRecorder::CreateFileRecorder(_fileCallRecorderId,
                                         (const FileFormats) format);
    if (_fileCallRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingCall() fileRecorder format isnot correct");
        return -1;
    }

    if (_fileCallRecorderPtr->StartRecordingAudioFile(*stream,
                                                      *codecInst,
                                                      notificationTime) != 0)
    {
    _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
     "StartRecordingAudioFile() failed to start file recording");
    _fileCallRecorderPtr->StopRecording();
    FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
    _fileCallRecorderPtr = NULL;
    return -1;
    }

    _fileCallRecorderPtr->RegisterModuleFileCallback(this);
    _fileCallRecording = true;

    return 0;
}

int TransmitMixer::StopRecordingCall()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StopRecordingCall()");

    if (!_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                     "StopRecordingCall() file isnot recording");
        return -1;
    }

    CriticalSectionScoped cs(&_critSect);

    if (_fileCallRecorderPtr->StopRecording() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopRecording(), could not stop recording");
        return -1;
    }

    _fileCallRecorderPtr->RegisterModuleFileCallback(NULL);
    FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
    _fileCallRecorderPtr = NULL;
    _fileCallRecording = false;

    return 0;
}

void
TransmitMixer::SetMixWithMicStatus(bool mix)
{
    _mixFileWithMicrophone = mix;
}

int TransmitMixer::RegisterExternalMediaProcessing(
    VoEMediaProcess* object,
    ProcessingTypes type) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::RegisterExternalMediaProcessing()");

  CriticalSectionScoped cs(&_callbackCritSect);
  if (!object) {
    return -1;
  }

  // Store the callback object according to the processing type.
  if (type == kRecordingAllChannelsMixed) {
    external_postproc_ptr_ = object;
  } else if (type == kRecordingPreprocessing) {
    external_preproc_ptr_ = object;
  } else {
    return -1;
  }
  return 0;
}

int TransmitMixer::DeRegisterExternalMediaProcessing(ProcessingTypes type) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::DeRegisterExternalMediaProcessing()");

  CriticalSectionScoped cs(&_callbackCritSect);
  if (type == kRecordingAllChannelsMixed) {
    external_postproc_ptr_ = NULL;
  } else if (type == kRecordingPreprocessing) {
    external_preproc_ptr_ = NULL;
  } else {
    return -1;
  }
  return 0;
}

int
TransmitMixer::SetMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetMute(enable=%d)", enable);
    _mute = enable;
    return 0;
}

bool
TransmitMixer::Mute() const
{
    return _mute;
}

int8_t TransmitMixer::AudioLevel() const
{
    // Speech + file level [0,9]
    return _audioLevel.Level();
}

int16_t TransmitMixer::AudioLevelFullRange() const
{
    // Speech + file level [0,32767]
    return _audioLevel.LevelFullRange();
}

bool TransmitMixer::IsRecordingCall()
{
    return _fileCallRecording;
}

bool TransmitMixer::IsRecordingMic()
{

    return _fileRecording;
}

// TODO(andrew): use RemixAndResample for this.
int TransmitMixer::GenerateAudioFrame(const int16_t audio[],
                                      int samples_per_channel,
                                      int num_channels,
                                      int sample_rate_hz)
{
    int destination_rate;
    int num_codec_channels;
    GetSendCodecInfo(&destination_rate, &num_codec_channels);

    // Never upsample the capture signal here. This should be done at the
    // end of the send chain.
    destination_rate = std::min(destination_rate, sample_rate_hz);
    stereo_codec_ = num_codec_channels == 2;

    const int16_t* audio_ptr = audio;
    int16_t mono_audio[kMaxMonoDeviceDataSizeSamples];
    assert(samples_per_channel <= kMaxMonoDeviceDataSizeSamples);
    // If no stereo codecs are in use, we downmix a stereo stream from the
    // device early in the chain, before resampling.
    if (num_channels == 2 && !stereo_codec_) {
      AudioFrameOperations::StereoToMono(audio, samples_per_channel,
                                         mono_audio);
      audio_ptr = mono_audio;
      num_channels = 1;
    }

    ResamplerType resampler_type = (num_channels == 1) ?
            kResamplerSynchronous : kResamplerSynchronousStereo;

    if (_audioResampler.ResetIfNeeded(sample_rate_hz,
                                      destination_rate,
                                      resampler_type) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::GenerateAudioFrame() unable to resample");
        return -1;
    }
    if (_audioResampler.Push(audio_ptr,
                             samples_per_channel * num_channels,
                             _audioFrame.data_,
                             AudioFrame::kMaxDataSizeSamples,
                             _audioFrame.samples_per_channel_) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::GenerateAudioFrame() resampling failed");
        return -1;
    }

    _audioFrame.samples_per_channel_ /= num_channels;
    _audioFrame.id_ = _instanceId;
    _audioFrame.timestamp_ = -1;
    _audioFrame.sample_rate_hz_ = destination_rate;
    _audioFrame.speech_type_ = AudioFrame::kNormalSpeech;
    _audioFrame.vad_activity_ = AudioFrame::kVadUnknown;
    _audioFrame.num_channels_ = num_channels;

    return 0;
}

int32_t TransmitMixer::RecordAudioToFile(
    const uint32_t mixingFrequency)
{
    CriticalSectionScoped cs(&_critSect);
    if (_fileRecorderPtr == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordAudioToFile() filerecorder doesnot"
                     "exist");
        return -1;
    }

    if (_fileRecorderPtr->RecordAudioToFile(_audioFrame) != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordAudioToFile() file recording"
                     "failed");
        return -1;
    }

    return 0;
}

int32_t TransmitMixer::MixOrReplaceAudioWithFile(
    const int mixingFrequency)
{
    scoped_array<int16_t> fileBuffer(new int16_t[640]);

    int fileSamples(0);
    {
        CriticalSectionScoped cs(&_critSect);
        if (_filePlayerPtr == NULL)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, -1),
                         "TransmitMixer::MixOrReplaceAudioWithFile()"
                         "fileplayer doesnot exist");
            return -1;
        }

        if (_filePlayerPtr->Get10msAudioFromFile(fileBuffer.get(),
                                                 fileSamples,
                                                 mixingFrequency) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                         "TransmitMixer::MixOrReplaceAudioWithFile() file"
                         " mixing failed");
            return -1;
        }
    }

    assert(_audioFrame.samples_per_channel_ == fileSamples);

    if (_mixFileWithMicrophone)
    {
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        Utility::MixWithSat(_audioFrame.data_,
                            _audioFrame.num_channels_,
                            fileBuffer.get(),
                            1,
                            fileSamples);
    } else
    {
        // Replace ACM audio with file.
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        _audioFrame.UpdateFrame(-1,
                                -1,
                                fileBuffer.get(),
                                fileSamples,
                                mixingFrequency,
                                AudioFrame::kNormalSpeech,
                                AudioFrame::kVadUnknown,
                                1);
    }
    return 0;
}

void TransmitMixer::ProcessAudio(int delay_ms, int clock_drift,
                                 int current_mic_level) {
  if (audioproc_->set_num_channels(_audioFrame.num_channels_,
                                   _audioFrame.num_channels_) != 0) {
    LOG_FERR2(LS_ERROR, set_num_channels, _audioFrame.num_channels_,
              _audioFrame.num_channels_);
  }

  if (audioproc_->set_sample_rate_hz(_audioFrame.sample_rate_hz_) != 0) {
    LOG_FERR1(LS_ERROR, set_sample_rate_hz, _audioFrame.sample_rate_hz_);
  }

  if (audioproc_->set_stream_delay_ms(delay_ms) != 0) {
    // Report as a warning; we can occasionally run into very large delays.
    LOG_FERR1(LS_WARNING, set_stream_delay_ms, delay_ms);
  }

  GainControl* agc = audioproc_->gain_control();
  if (agc->set_stream_analog_level(current_mic_level) != 0) {
    LOG_FERR1(LS_ERROR, set_stream_analog_level, current_mic_level);
  }

  EchoCancellation* aec = audioproc_->echo_cancellation();
  if (aec->is_drift_compensation_enabled()) {
    aec->set_stream_drift_samples(clock_drift);
  }

  int err = audioproc_->ProcessStream(&_audioFrame);
  if (err != 0) {
    LOG(LS_ERROR) << "ProcessStream() error: " << err;
  }

  CriticalSectionScoped cs(&_critSect);

  // Store new capture level. Only updated when analog AGC is enabled.
  _captureLevel = agc->stream_analog_level();

  // Triggers a callback in OnPeriodicProcess().
  _saturationWarning |= agc->stream_is_saturated();
}

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
int TransmitMixer::TypingDetection()
{

    // We let the VAD determine if we're using this feature or not.
    if (_audioFrame.vad_activity_ == AudioFrame::kVadUnknown)
    {
        return (0);
    }

    int keyPressed = EventWrapper::KeyPressed();

    if (keyPressed < 0)
    {
        return (-1);
    }

    if (_audioFrame.vad_activity_ == AudioFrame::kVadActive)
        _timeActive++;
    else
        _timeActive = 0;

    // Keep track if time since last typing event
    if (keyPressed)
    {
      _timeSinceLastTyping = 0;
    }
    else
    {
      ++_timeSinceLastTyping;
    }

    if ((_timeSinceLastTyping < _typeEventDelay)
        && (_audioFrame.vad_activity_ == AudioFrame::kVadActive)
        && (_timeActive < _timeWindow))
    {
        _penaltyCounter += _costPerTyping;
        if (_penaltyCounter > _reportingThreshold)
        {
            // Triggers a callback in OnPeriodicProcess().
            _typingNoiseWarning = true;
        }
    }

    if (_penaltyCounter > 0)
        _penaltyCounter-=_penaltyDecay;

    return (0);
}
#endif

int TransmitMixer::GetMixingFrequency()
{
    assert(_audioFrame.sample_rate_hz_ != 0);
    return _audioFrame.sample_rate_hz_;
}

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
int TransmitMixer::TimeSinceLastTyping(int &seconds)
{
  // We check in VoEAudioProcessingImpl that this is only called when
  // typing detection is active.

  // Round to whole seconds
  seconds = (_timeSinceLastTyping + 50) / 100;
  return(0);
}
#endif

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
int TransmitMixer::SetTypingDetectionParameters(int timeWindow,
                                                int costPerTyping,
                                                int reportingThreshold,
                                                int penaltyDecay,
                                                int typeEventDelay)
{
  if(timeWindow != 0)
    _timeWindow = timeWindow;
  if(costPerTyping != 0)
    _costPerTyping = costPerTyping;
  if(reportingThreshold != 0)
    _reportingThreshold = reportingThreshold;
  if(penaltyDecay != 0)
    _penaltyDecay = penaltyDecay;
  if(typeEventDelay != 0)
    _typeEventDelay = typeEventDelay;


  return(0);
}
#endif

void TransmitMixer::EnableStereoChannelSwapping(bool enable) {
  swap_stereo_channels_ = enable;
}

bool TransmitMixer::IsStereoChannelSwappingEnabled() {
  return swap_stereo_channels_;
}

}  //  namespace voe

}  //  namespace webrtc
