/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CHANNEL_MANAGER_H_
#define PC_CHANNEL_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "api/audio_options.h"
#include "api/crypto/crypto_options.h"
#include "api/rtp_parameters.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "call/call.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/media_config.h"
#include "media/base/media_engine.h"
#include "pc/channel_factory_interface.h"
#include "pc/channel_interface.h"
#include "pc/session_description.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/unique_id_generator.h"

namespace cricket {

// ChannelManager allows the MediaEngine to run on a separate thread, and takes
// care of marshalling calls between threads. It also creates and keeps track of
// voice and video channels; by doing so, it can temporarily pause all the
// channels when a new audio or video device is chosen. The voice and video
// channels are stored in separate vectors, to easily allow operations on just
// voice or just video channels.
// ChannelManager also allows the application to discover what devices it has
// using device manager.
class ChannelManager : public ChannelFactoryInterface {
 public:
  // Returns an initialized instance of ChannelManager.
  // If media_engine is non-nullptr, then the returned ChannelManager instance
  // will own that reference and media engine initialization
  static std::unique_ptr<ChannelManager> Create(
      std::unique_ptr<MediaEngineInterface> media_engine,
      bool enable_rtx,
      rtc::Thread* worker_thread,
      rtc::Thread* network_thread);

  ChannelManager() = delete;
  ~ChannelManager() override;

  rtc::Thread* worker_thread() const { return worker_thread_; }
  rtc::Thread* network_thread() const { return network_thread_; }
  MediaEngineInterface* media_engine() { return media_engine_.get(); }
  rtc::UniqueRandomIdGenerator& ssrc_generator() { return ssrc_generator_; }

  // Retrieves the list of supported audio & video codec types.
  // Can be called before starting the media engine.
  void GetSupportedAudioSendCodecs(std::vector<AudioCodec>* codecs) const;
  void GetSupportedAudioReceiveCodecs(std::vector<AudioCodec>* codecs) const;
  void GetSupportedVideoSendCodecs(std::vector<VideoCodec>* codecs) const;
  void GetSupportedVideoReceiveCodecs(std::vector<VideoCodec>* codecs) const;

  // The operations below all occur on the worker thread.
  // The caller is responsible for ensuring that destruction happens
  // on the worker thread.

  // Creates a voice channel, to be associated with the specified session.
  std::unique_ptr<VoiceChannel> CreateVoiceChannel(
      webrtc::Call* call,
      const MediaConfig& media_config,
      absl::string_view mid,
      bool srtp_required,
      const webrtc::CryptoOptions& crypto_options,
      const AudioOptions& options) override;

  // Creates a video channel, synced with the specified voice channel, and
  // associated with the specified session.
  // Version of the above that takes PacketTransportInternal.
  std::unique_ptr<VideoChannel> CreateVideoChannel(
      webrtc::Call* call,
      const MediaConfig& media_config,
      absl::string_view mid,
      bool srtp_required,
      const webrtc::CryptoOptions& crypto_options,
      const VideoOptions& options,
      webrtc::VideoBitrateAllocatorFactory* video_bitrate_allocator_factory)
      override;

 protected:
  ChannelManager(std::unique_ptr<MediaEngineInterface> media_engine,
                 bool enable_rtx,
                 rtc::Thread* worker_thread,
                 rtc::Thread* network_thread);

  // Destroys a voice channel created by CreateVoiceChannel.
  void DestroyVoiceChannel(VoiceChannel* voice_channel);

  // Destroys a video channel created by CreateVideoChannel.
  void DestroyVideoChannel(VideoChannel* video_channel);

 private:
  const std::unique_ptr<MediaEngineInterface> media_engine_;  // Nullable.
  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;

  // This object should be used to generate any SSRC that is not explicitly
  // specified by the user (or by the remote party).
  // TODO(bugs.webrtc.org/12666): This variable is used from both the signaling
  // and worker threads. See if we can't restrict usage to a single thread.
  rtc::UniqueRandomIdGenerator ssrc_generator_;

  const bool enable_rtx_;
};

}  // namespace cricket

#endif  // PC_CHANNEL_MANAGER_H_
