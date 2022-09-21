/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/channel_manager.h"

#include <utility>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "api/media_types.h"
#include "api/sequence_checker.h"
#include "media/base/media_constants.h"
#include "pc/channel.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/trace_event.h"

namespace cricket {

// static
std::unique_ptr<ChannelManager> ChannelManager::Create(
    std::unique_ptr<MediaEngineInterface> media_engine,
    bool enable_rtx,
    rtc::Thread* worker_thread,
    rtc::Thread* network_thread) {
  RTC_DCHECK(network_thread);
  RTC_DCHECK(worker_thread);

  return absl::WrapUnique(new ChannelManager(
      std::move(media_engine), enable_rtx, worker_thread, network_thread));
}

ChannelManager::ChannelManager(
    std::unique_ptr<MediaEngineInterface> media_engine,
    bool enable_rtx,
    rtc::Thread* worker_thread,
    rtc::Thread* network_thread)
    : media_engine_(std::move(media_engine)),
      signaling_thread_(rtc::Thread::Current()),
      worker_thread_(worker_thread),
      network_thread_(network_thread),
      enable_rtx_(enable_rtx) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(network_thread_);

  if (media_engine_) {
    // TODO(tommi): Change VoiceEngine to do ctor time initialization so that
    // this isn't necessary.
    worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] { media_engine_->Init(); });
  }
}

ChannelManager::~ChannelManager() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    // While `media_engine_` is const throughout the ChannelManager's lifetime,
    // it requires destruction to happen on the worker thread. Instead of
    // marking the pointer as non-const, we live with this const_cast<> in the
    // destructor.
    const_cast<std::unique_ptr<MediaEngineInterface>&>(media_engine_).reset();
  });
}

void ChannelManager::GetSupportedAudioSendCodecs(
    std::vector<AudioCodec>* codecs) const {
  if (!media_engine_) {
    return;
  }
  *codecs = media_engine_->voice().send_codecs();
}

void ChannelManager::GetSupportedAudioReceiveCodecs(
    std::vector<AudioCodec>* codecs) const {
  if (!media_engine_) {
    return;
  }
  *codecs = media_engine_->voice().recv_codecs();
}

void ChannelManager::GetSupportedVideoSendCodecs(
    std::vector<VideoCodec>* codecs) const {
  if (!media_engine_) {
    return;
  }
  codecs->clear();

  std::vector<VideoCodec> video_codecs = media_engine_->video().send_codecs();
  for (const auto& video_codec : video_codecs) {
    if (!enable_rtx_ &&
        absl::EqualsIgnoreCase(kRtxCodecName, video_codec.name)) {
      continue;
    }
    codecs->push_back(video_codec);
  }
}

void ChannelManager::GetSupportedVideoReceiveCodecs(
    std::vector<VideoCodec>* codecs) const {
  if (!media_engine_) {
    return;
  }
  codecs->clear();

  std::vector<VideoCodec> video_codecs = media_engine_->video().recv_codecs();
  for (const auto& video_codec : video_codecs) {
    if (!enable_rtx_ &&
        absl::EqualsIgnoreCase(kRtxCodecName, video_codec.name)) {
      continue;
    }
    codecs->push_back(video_codec);
  }
}

std::unique_ptr<VoiceChannel> ChannelManager::CreateVoiceChannel(
    webrtc::Call* call,
    const MediaConfig& media_config,
    absl::string_view mid,
    bool srtp_required,
    const webrtc::CryptoOptions& crypto_options,
    const AudioOptions& options) {
  RTC_DCHECK(call);
  RTC_DCHECK(media_engine_);
  // TODO(bugs.webrtc.org/11992): Remove this workaround after updates in
  // PeerConnection and add the expectation that we're already on the right
  // thread.
  if (!worker_thread_->IsCurrent()) {
    return worker_thread_->Invoke<std::unique_ptr<VoiceChannel>>(
        RTC_FROM_HERE, [&] {
          return CreateVoiceChannel(call, media_config, mid, srtp_required,
                                    crypto_options, options);
        });
  }

  RTC_DCHECK_RUN_ON(worker_thread_);

  VoiceMediaChannel* media_channel = media_engine_->voice().CreateMediaChannel(
      call, media_config, options, crypto_options);
  if (!media_channel) {
    return nullptr;
  }

  auto voice_channel = std::make_unique<VoiceChannel>(
      worker_thread_, network_thread_, signaling_thread_,
      absl::WrapUnique(media_channel), mid, srtp_required, crypto_options,
      &ssrc_generator_);

  return voice_channel;
}

std::unique_ptr<VideoChannel> ChannelManager::CreateVideoChannel(
    webrtc::Call* call,
    const MediaConfig& media_config,
    absl::string_view mid,
    bool srtp_required,
    const webrtc::CryptoOptions& crypto_options,
    const VideoOptions& options,
    webrtc::VideoBitrateAllocatorFactory* video_bitrate_allocator_factory) {
  RTC_DCHECK(call);
  RTC_DCHECK(media_engine_);
  // TODO(bugs.webrtc.org/11992): Remove this workaround after updates in
  // PeerConnection and add the expectation that we're already on the right
  // thread.
  if (!worker_thread_->IsCurrent()) {
    return worker_thread_->Invoke<std::unique_ptr<VideoChannel>>(
        RTC_FROM_HERE, [&] {
          return CreateVideoChannel(call, media_config, mid, srtp_required,
                                    crypto_options, options,
                                    video_bitrate_allocator_factory);
        });
  }

  RTC_DCHECK_RUN_ON(worker_thread_);

  VideoMediaChannel* media_channel = media_engine_->video().CreateMediaChannel(
      call, media_config, options, crypto_options,
      video_bitrate_allocator_factory);
  if (!media_channel) {
    return nullptr;
  }

  auto video_channel = std::make_unique<VideoChannel>(
      worker_thread_, network_thread_, signaling_thread_,
      absl::WrapUnique(media_channel), mid, srtp_required, crypto_options,
      &ssrc_generator_);

  return video_channel;
}

}  // namespace cricket
