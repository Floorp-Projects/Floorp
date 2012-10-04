/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_FILE_PLAYER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_FILE_PLAYER_H_

#include <list>
#include <set>

#include "common_types.h"  // NOLINT
#include "modules/media_file/interface/media_file_defines.h"
#include "system_wrappers/interface/file_wrapper.h"
#include "typedefs.h"  // NOLINT
#include "video_engine/vie_frame_provider_base.h"

namespace webrtc {

class EventWrapper;
class FilePlayer;
class ThreadWrapper;
class ViEFileObserver;
class VoEFile;
class VoEVideoSync;
class VoiceEngine;

class ViEFilePlayer
    : public ViEFrameProviderBase,
      protected FileCallback,
      protected InStream {
 public:
  static ViEFilePlayer* CreateViEFilePlayer(int file_id,
                                            int engine_id,
                                            const char* file_nameUTF8,
                                            const bool loop,
                                            const FileFormats file_format,
                                            VoiceEngine* voe_ptr);

  static int GetFileInformation(const int engine_id,
                                const char* file_name,
                                VideoCodec& video_codec,
                                CodecInst& audio_codec,
                                const FileFormats file_format);
  ~ViEFilePlayer();

  bool IsObserverRegistered();
  int RegisterObserver(ViEFileObserver* observer);
  int DeRegisterObserver();
  int SendAudioOnChannel(const int audio_channel,
                         bool mix_microphone,
                         float volume_scaling);
  int StopSendAudioOnChannel(const int audio_channel);
  int PlayAudioLocally(const int audio_channel, float volume_scaling);
  int StopPlayAudioLocally(const int audio_channel);

  // Implements ViEFrameProviderBase.
  virtual int FrameCallbackChanged();

 protected:
  ViEFilePlayer(int Id, int engine_id);
  int Init(const char* file_nameUTF8,
           const bool loop,
           const FileFormats file_format,
           VoiceEngine* voe_ptr);
  int StopPlay();
  int StopPlayAudio();

  // File play decode function.
  static bool FilePlayDecodeThreadFunction(void* obj);
  bool FilePlayDecodeProcess();
  bool NeedsAudioFromFile(void* buf);

  // Implements webrtc::InStream.
  virtual int Read(void* buf, int len);
  virtual int Rewind() {
    return 0;
  }

  // Implements FileCallback.
  virtual void PlayNotification(const WebRtc_Word32 /*id*/,
                                const WebRtc_UWord32 /*notification_ms*/) {}
  virtual void RecordNotification(const WebRtc_Word32 /*id*/,
                                  const WebRtc_UWord32 /*notification_ms*/) {}
  virtual void PlayFileEnded(const WebRtc_Word32 id);
  virtual void RecordFileEnded(const WebRtc_Word32 /*id*/) {}

 private:
  static const int kMaxDecodedAudioLength = 320;
  bool play_back_started_;

  CriticalSectionWrapper* feedback_cs_;
  CriticalSectionWrapper* audio_cs_;

  FilePlayer* file_player_;
  bool audio_stream_;

  // Number of active video clients.
  int video_clients_;

  // Number of audio channels sending this audio.
  int audio_clients_;

  // Local audio channel playing this video. Sync video against this.
  int local_audio_channel_;

  ViEFileObserver* observer_;
  char file_name_[FileWrapper::kMaxFileNameSize];

  // VoE Interface.
  VoEFile* voe_file_interface_;
  VoEVideoSync* voe_video_sync_;

  // Thread for decoding video (and audio if no audio clients connected).
  ThreadWrapper* decode_thread_;
  EventWrapper* decode_event_;
  WebRtc_Word16 decoded_audio_[kMaxDecodedAudioLength];
  int decoded_audio_length_;

  // Trick - list containing VoE buffer reading this file. Used if multiple
  // audio channels are sending.
  std::list<void*> audio_channel_buffers_;

  // AudioChannels sending audio from this file.
  std::set<int> audio_channels_sending_;

  // Frame receiving decoded video from file.
  VideoFrame decoded_video_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_FILE_PLAYER_H_
