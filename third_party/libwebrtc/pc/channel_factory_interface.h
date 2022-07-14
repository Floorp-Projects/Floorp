/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CHANNEL_FACTORY_INTERFACE_H_
#define PC_CHANNEL_FACTORY_INTERFACE_H_

#include <memory>
#include <string>

#include "api/audio_options.h"
#include "api/crypto/crypto_options.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "call/call.h"
#include "media/base/media_channel.h"
#include "media/base/media_config.h"

namespace cricket {

class VideoChannel;
class VoiceChannel;

class ChannelFactoryInterface {
 public:
  virtual std::unique_ptr<VideoChannel> CreateVideoChannel(
      webrtc::Call* call,
      const MediaConfig& media_config,
      const std::string& mid,
      bool srtp_required,
      const webrtc::CryptoOptions& crypto_options,
      const VideoOptions& options,
      webrtc::VideoBitrateAllocatorFactory*
          video_bitrate_allocator_factory) = 0;

  virtual std::unique_ptr<VoiceChannel> CreateVoiceChannel(
      webrtc::Call* call,
      const MediaConfig& media_config,
      const std::string& mid,
      bool srtp_required,
      const webrtc::CryptoOptions& crypto_options,
      const AudioOptions& options) = 0;

 protected:
  virtual ~ChannelFactoryInterface() = default;
};

}  // namespace cricket

#endif  // PC_CHANNEL_FACTORY_INTERFACE_H_
