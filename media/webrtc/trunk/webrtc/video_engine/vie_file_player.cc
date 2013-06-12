/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_file_player.h"

#include "modules/utility/interface/file_player.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "system_wrappers/interface/thread_wrapper.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_file.h"
#include "video_engine/vie_defines.h"
#include "voice_engine/include/voe_base.h"
#include "voice_engine/include/voe_file.h"
#include "voice_engine/include/voe_video_sync.h"

namespace webrtc {

const int kThreadWaitTimeMs = 100;

ViEFilePlayer* ViEFilePlayer::CreateViEFilePlayer(
    int file_id,
    int engine_id,
    const char* file_nameUTF8,
    const bool loop,
    const FileFormats file_format,
    VoiceEngine* voe_ptr) {
  ViEFilePlayer* self = new ViEFilePlayer(file_id, engine_id);
  if (!self || self->Init(file_nameUTF8, loop, file_format, voe_ptr) != 0) {
    delete self;
    self = NULL;
  }
  return self;
}

ViEFilePlayer::ViEFilePlayer(int Id,
                             int engine_id)
    : ViEFrameProviderBase(Id, engine_id),
      play_back_started_(false),
      feedback_cs_(NULL),
      audio_cs_(NULL),
      file_player_(NULL),
      audio_stream_(false),
      video_clients_(0),
      audio_clients_(0),
      local_audio_channel_(-1),
      observer_(NULL),
      voe_file_interface_(NULL),
      voe_video_sync_(NULL),
      decode_thread_(NULL),
      decode_event_(NULL),
      decoded_audio_length_(0) {
  memset(file_name_, 0, FileWrapper::kMaxFileNameSize);
  memset(decoded_audio_, 0, kMaxDecodedAudioLength);
}

ViEFilePlayer::~ViEFilePlayer() {
  // StopPlay deletes decode_thread_.
  StopPlay();
  delete decode_event_;
  delete audio_cs_;
  delete feedback_cs_;
}

int ViEFilePlayer::Init(const char* file_nameUTF8,
                        const bool loop,
                        const FileFormats file_format,
                        VoiceEngine* voice_engine) {
  feedback_cs_ = CriticalSectionWrapper::CreateCriticalSection();
  if (!feedback_cs_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StartPlay() failed to allocate critsect");
    return -1;
  }

  audio_cs_ = CriticalSectionWrapper::CreateCriticalSection();
  if (!audio_cs_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StartPlay() failed to allocate critsect");
    return -1;
  }

  decode_event_ = EventWrapper::Create();
  if (!decode_event_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StartPlay() failed to allocate event");
    return -1;
  }
  if (strlen(file_nameUTF8) > FileWrapper::kMaxFileNameSize) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StartPlay() Too long filename");
    return -1;
  }
  strncpy(file_name_, file_nameUTF8, strlen(file_nameUTF8) + 1);

  file_player_ = FilePlayer::CreateFilePlayer(ViEId(engine_id_, id_),
                                              file_format);
  if (!file_player_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StartPlay() failed to create file player");
    return -1;
  }
  if (file_player_->RegisterModuleFileCallback(this) == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StartPlay() failed to "
                 "RegisterModuleFileCallback");
    file_player_ = NULL;
    return -1;
  }
  decode_thread_ = ThreadWrapper::CreateThread(FilePlayDecodeThreadFunction,
                                               this, kHighestPriority,
                                               "ViEFilePlayThread");
  if (!decode_thread_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StartPlay() failed to start decode thread.");
    file_player_ = NULL;
    return -1;
  }

  // Always try to open with Audio since we don't know on what channels the
  // audio should be played on.
  WebRtc_Word32 error = file_player_->StartPlayingVideoFile(file_name_, loop,
                                                            false);
  if (error) {
    // Failed to open the file with audio, try without.
    error = file_player_->StartPlayingVideoFile(file_name_, loop, true);
    audio_stream_ = false;
    if (error) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() failed to Start play video "
                   "file");
      return -1;
    }

  } else {
    audio_stream_ = true;
  }

  if (audio_stream_) {
    if (voice_engine) {
      // A VoiceEngine has been provided and we want to play audio on local
      // a channel.
      voe_file_interface_ = VoEFile::GetInterface(voice_engine);
      if (!voe_file_interface_) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                     "ViEFilePlayer::StartPlay() failed to get VEFile "
                     "interface");
        return -1;
      }
      voe_video_sync_ = VoEVideoSync::GetInterface(voice_engine);
      if (!voe_video_sync_) {
        WEBRTC_TRACE(kTraceError, kTraceVideo,
                     ViEId(engine_id_, id_),
                     "ViEFilePlayer::StartPlay() failed to get "
                     "VoEVideoSync interface");
        return -1;
      }
    }
  }

  // Read audio /(or just video) every 10ms.
  decode_event_->StartTimer(true, 10);
  return 0;
}

int ViEFilePlayer::FrameCallbackChanged() {
  // Starts the decode thread when someone cares.
  if (ViEFrameProviderBase::NumberOfRegisteredFrameCallbacks() >
      video_clients_) {
    if (!play_back_started_) {
      play_back_started_ = true;
      unsigned int thread_id;
      if (decode_thread_->Start(thread_id)) {
        WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, id_),
                     "ViEFilePlayer::FrameCallbackChanged() Started file decode"
                     " thread %u", thread_id);
      } else {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                     "ViEFilePlayer::FrameCallbackChanged() Failed to start "
                     "file decode thread.");
      }
    } else if (!file_player_->IsPlayingFile()) {
      if (file_player_->StartPlayingVideoFile(file_name_, false,
                                              !audio_stream_) != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                     "ViEFilePlayer::FrameCallbackChanged(), Failed to restart "
                     "the file player.");
      }
    }
  }
  video_clients_ = ViEFrameProviderBase::NumberOfRegisteredFrameCallbacks();
  return 0;
}

bool ViEFilePlayer::FilePlayDecodeThreadFunction(void* obj) {
  return static_cast<ViEFilePlayer*>(obj)->FilePlayDecodeProcess();
}

bool ViEFilePlayer::FilePlayDecodeProcess() {
  if (decode_event_->Wait(kThreadWaitTimeMs) == kEventSignaled) {
    if (audio_stream_ && audio_clients_ == 0) {
      // There is audio but no one cares, read the audio here.
      Read(NULL, 0);
    }
    if (file_player_->TimeUntilNextVideoFrame() < 10) {
      // Less than 10ms to next videoframe.
      if (file_player_->GetVideoFromFile(decoded_video_) != 0) {
      }
    }
    if (!decoded_video_.IsZeroSize()) {
      if (local_audio_channel_ != -1 && voe_video_sync_) {
        // We are playing audio locally.
        int audio_delay = 0;
        if (voe_video_sync_->GetPlayoutBufferSize(audio_delay) == 0) {
          decoded_video_.set_render_time_ms(decoded_video_.render_time_ms() +
                                            audio_delay);
        }
      }
      DeliverFrame(&decoded_video_);
      decoded_video_.ResetSize();
    }
  }
  return true;
}

int ViEFilePlayer::StopPlay() {
  // Only called from destructor.
  if (decode_thread_) {
    decode_thread_->SetNotAlive();
    if (decode_thread_->Stop()) {
      delete decode_thread_;
    } else {
      assert(false && "ViEFilePlayer::StopPlay() Failed to stop decode thread");
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                   "ViEFilePlayer::StartPlay() Failed to stop file decode "
                   "thread.");
    }
  }
  decode_thread_ = NULL;
  if (decode_event_) {
    decode_event_->StopTimer();
  }
  StopPlayAudio();

  if (voe_file_interface_) {
    voe_file_interface_->Release();
    voe_file_interface_ = NULL;
  }
  if (voe_video_sync_) {
    voe_video_sync_->Release();
    voe_video_sync_ = NULL;
  }

  if (file_player_) {
    file_player_->StopPlayingFile();
    FilePlayer::DestroyFilePlayer(file_player_);
    file_player_ = NULL;
  }
  return 0;
}

int ViEFilePlayer::StopPlayAudio() {
  // Stop sending audio.

  std::set<int>::iterator it = audio_channels_sending_.begin();
  while (it != audio_channels_sending_.end()) {
    StopSendAudioOnChannel(*it);
    // StopSendAudioOnChannel erases the item from the map.
    it = audio_channels_sending_.begin();
  }

  // Stop local audio playback.
  if (local_audio_channel_ != -1) {
    StopPlayAudioLocally(local_audio_channel_);
  }
  local_audio_channel_ = -1;
  audio_channel_buffers_.clear();
  audio_clients_ = 0;
  return 0;
}

int ViEFilePlayer::Read(void* buf, int len) {
  // Protect from simultaneous reading from multiple channels.
  CriticalSectionScoped lock(audio_cs_);
  if (NeedsAudioFromFile(buf)) {
    // We will run the VoE in 16KHz.
    if (file_player_->Get10msAudioFromFile(decoded_audio_,
                                           decoded_audio_length_, 16000) != 0) {
      // No data.
      decoded_audio_length_ = 0;
      return 0;
    }
    // 2 bytes per sample.
    decoded_audio_length_ *= 2;
    if (buf) {
      audio_channel_buffers_.push_back(buf);
    }
  } else {
    // No need for new audiobuffer from file, ie the buffer read from file has
    // not been played on this channel.
  }
  if (buf) {
    memcpy(buf, decoded_audio_, decoded_audio_length_);
  }
  return decoded_audio_length_;
}

bool ViEFilePlayer::NeedsAudioFromFile(void* buf) {
  bool needs_new_audio = false;
  if (audio_channel_buffers_.size() == 0) {
    return true;
  }

  // Check if we the buf already have read the current audio.
  for (std::list<void*>::iterator it = audio_channel_buffers_.begin();
       it != audio_channel_buffers_.end(); ++it) {
    if (*it == buf) {
      needs_new_audio = true;
      audio_channel_buffers_.erase(it);
      break;
    }
  }
  return needs_new_audio;
}

void ViEFilePlayer::PlayFileEnded(const WebRtc_Word32 id) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id),
               "%s: file_id %d", __FUNCTION__, id_);
  file_player_->StopPlayingFile();

  CriticalSectionScoped lock(feedback_cs_);
  if (observer_) {
    observer_->PlayFileEnded(id_);
  }
}

bool ViEFilePlayer::IsObserverRegistered() {
  CriticalSectionScoped lock(feedback_cs_);
  return observer_ != NULL;
}

int ViEFilePlayer::RegisterObserver(ViEFileObserver* observer) {
  CriticalSectionScoped lock(feedback_cs_);
  if (observer_) {
    return -1;
  }
  observer_ = observer;
  return 0;
}

int ViEFilePlayer::DeRegisterObserver() {
  CriticalSectionScoped lock(feedback_cs_);
  observer_ = NULL;
  return 0;
}

int ViEFilePlayer::SendAudioOnChannel(const int audio_channel,
                                      bool mix_microphone,
                                      float volume_scaling) {
  if (!voe_file_interface_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "%s No VEFile interface.", __FUNCTION__);
    return -1;
  }
  if (voe_file_interface_->StartPlayingFileAsMicrophone(audio_channel, this,
                                                       mix_microphone,
                                                       kFileFormatPcm16kHzFile,
                                                       volume_scaling) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::SendAudioOnChannel() "
                 "VE_StartPlayingFileAsMicrophone failed. audio_channel %d, "
                 " mix_microphone %d, volume_scaling %.2f",
                 audio_channel, mix_microphone, volume_scaling);
    return -1;
  }
  audio_channels_sending_.insert(audio_channel);

  CriticalSectionScoped lock(audio_cs_);
  audio_clients_++;
  return 0;
}

int ViEFilePlayer::StopSendAudioOnChannel(const int audio_channel) {
  int result = 0;
  if (!voe_file_interface_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StopSendAudioOnChannel() - no VoE interface");
    return -1;
  }
  std::set<int>::iterator it = audio_channels_sending_.find(audio_channel);
  if (it == audio_channels_sending_.end()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StopSendAudioOnChannel AudioChannel %d not "
                 "sending", audio_channel);
    return -1;
  }
  result = voe_file_interface_->StopPlayingFileAsMicrophone(audio_channel);
  if (result != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "ViEFilePlayer::StopSendAudioOnChannel() "
                 "VE_StopPlayingFileAsMicrophone failed. audio_channel %d",
                 audio_channel);
  }
  audio_channels_sending_.erase(audio_channel);
  CriticalSectionScoped lock(audio_cs_);
  audio_clients_--;
  assert(audio_clients_ >= 0);
  return 0;
}

int ViEFilePlayer::PlayAudioLocally(const int audio_channel,
                                    float volume_scaling) {
  if (!voe_file_interface_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "%s No VEFile interface.", __FUNCTION__);
    return -1;
  }
  if (voe_file_interface_->StartPlayingFileLocally(audio_channel, this,
                                                   kFileFormatPcm16kHzFile,
                                                   volume_scaling) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "%s  VE_StartPlayingFileAsMicrophone failed. audio_channel %d,"
                 " mix_microphone %d, volume_scaling %.2f",
                 __FUNCTION__, audio_channel, volume_scaling);
    return -1;
  }

  CriticalSectionScoped lock(audio_cs_);
  local_audio_channel_ = audio_channel;
  audio_clients_++;
  return 0;
}

int ViEFilePlayer::StopPlayAudioLocally(const int audio_channel) {
  if (!voe_file_interface_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "%s No VEFile interface.", __FUNCTION__);
    return -1;
  }
  if (voe_file_interface_->StopPlayingFileLocally(audio_channel) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "%s VE_StopPlayingFileLocally failed. audio_channel %d.",
                 __FUNCTION__, audio_channel);
    return -1;
  }

  CriticalSectionScoped lock(audio_cs_);
  local_audio_channel_ = -1;
  audio_clients_--;
  return 0;
}

int ViEFilePlayer::GetFileInformation(int engine_id,
                                      const char* file_name,
                                      VideoCodec& video_codec,
                                      CodecInst& audio_codec,
                                      const FileFormats file_format) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, engine_id, "%s ", __FUNCTION__);

  FilePlayer* file_player = FilePlayer::CreateFilePlayer(engine_id,
                                                         file_format);
  if (!file_player) {
    return -1;
  }

  bool video_only = false;

  memset(&video_codec, 0, sizeof(video_codec));
  memset(&audio_codec, 0, sizeof(audio_codec));

  if (file_player->StartPlayingVideoFile(file_name, false, false) != 0) {
    video_only = true;
    if (file_player->StartPlayingVideoFile(file_name, false, true) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                   "%s Failed to open file.", __FUNCTION__);
      FilePlayer::DestroyFilePlayer(file_player);
      return -1;
    }
  }

  if (!video_only && file_player->AudioCodec(audio_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s Failed to get audio codec.", __FUNCTION__);
    FilePlayer::DestroyFilePlayer(file_player);
    return -1;
  }
  if (file_player->video_codec_info(video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, engine_id,
                 "%s Failed to get video codec.", __FUNCTION__);
    FilePlayer::DestroyFilePlayer(file_player);
    return -1;
  }
  FilePlayer::DestroyFilePlayer(file_player);
  return 0;
}

}  // namespace webrtc
