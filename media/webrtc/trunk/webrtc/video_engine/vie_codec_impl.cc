/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_codec_impl.h"

#include <list>

#include "engine_configurations.h"  // NOLINT
#include "modules/video_coding/main/interface/video_coding.h"
#include "system_wrappers/interface/logging.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/vie_capturer.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_input_manager.h"
#include "video_engine/vie_shared_data.h"

namespace webrtc {

ViECodec* ViECodec::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViECodecImpl* vie_codec_impl = vie_impl;
  // Increase ref count.
  (*vie_codec_impl)++;
  return vie_codec_impl;
#else
  return NULL;
#endif
}

int ViECodecImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_->instance_id(),
               "ViECodecImpl::Release()");
  // Decrease ref count.
  (*this)--;

  WebRtc_Word32 ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, shared_data_->instance_id(),
                 "ViECodec released too many times");
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, shared_data_->instance_id(),
               "ViECodec reference count: %d", ref_count);
  return ref_count;
}

ViECodecImpl::ViECodecImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViECodecImpl::ViECodecImpl() Ctor");
}

ViECodecImpl::~ViECodecImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViECodecImpl::~ViECodecImpl() Dtor");
}

int ViECodecImpl::NumberOfCodecs() const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s", __FUNCTION__);
  // +2 because of FEC(RED and ULPFEC)
  return static_cast<int>((VideoCodingModule::NumberOfCodecs() + 2));
}

int ViECodecImpl::GetCodec(const unsigned char list_number,
                           VideoCodec& video_codec) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(list_number: %d, codec_type: %d)", __FUNCTION__,
               list_number, video_codec.codecType);
  if (list_number == VideoCodingModule::NumberOfCodecs()) {
    memset(&video_codec, 0, sizeof(VideoCodec));
    strncpy(video_codec.plName, "red", 3);
    video_codec.codecType = kVideoCodecRED;
    video_codec.plType = VCM_RED_PAYLOAD_TYPE;
  } else if (list_number == VideoCodingModule::NumberOfCodecs() + 1) {
    memset(&video_codec, 0, sizeof(VideoCodec));
    strncpy(video_codec.plName, "ulpfec", 6);
    video_codec.codecType = kVideoCodecULPFEC;
    video_codec.plType = VCM_ULPFEC_PAYLOAD_TYPE;
  } else if (VideoCodingModule::Codec(list_number, &video_codec) != VCM_OK) {
    WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Could not get codec for list_number: %u", __FUNCTION__,
                 list_number);
    shared_data_->SetLastError(kViECodecInvalidArgument);
    return -1;
  }
  return 0;
}

int ViECodecImpl::SetSendCodec(const int video_channel,
                               const VideoCodec& video_codec) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d, codec_type: %d)", __FUNCTION__,
               video_channel, video_codec.codecType);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s: codec: %d, pl_type: %d, width: %d, height: %d, bitrate: %d"
               "maxBr: %d, min_br: %d, frame_rate: %d, qpMax: %u,"
               "numberOfSimulcastStreams: %u )", __FUNCTION__,
               video_codec.codecType, video_codec.plType, video_codec.width,
               video_codec.height, video_codec.startBitrate,
               video_codec.maxBitrate, video_codec.minBitrate,
               video_codec.maxFramerate, video_codec.qpMax,
               video_codec.numberOfSimulcastStreams);
  if (video_codec.codecType == kVideoCodecVP8) {
    WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "pictureLossIndicationOn: %d, feedbackModeOn: %d, "
                 "complexity: %d, resilience: %d, numberOfTemporalLayers: %u",
                 video_codec.codecSpecific.VP8.pictureLossIndicationOn,
                 video_codec.codecSpecific.VP8.feedbackModeOn,
                 video_codec.codecSpecific.VP8.complexity,
                 video_codec.codecSpecific.VP8.resilience,
                 video_codec.codecSpecific.VP8.numberOfTemporalLayers);
  }
  if (!CodecValid(video_codec)) {
    // Error logged.
    shared_data_->SetLastError(kViECodecInvalidCodec);
    return -1;
  }

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }

  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder);
  if (vie_encoder->Owner() != video_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Receive only channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecReceiveOnlyChannel);
    return -1;
  }

  // Set a max_bitrate if the user hasn't set one.
  VideoCodec video_codec_internal;
  memcpy(&video_codec_internal, &video_codec, sizeof(VideoCodec));
  if (video_codec_internal.maxBitrate == 0) {
    // Max is one bit per pixel.
    video_codec_internal.maxBitrate = (video_codec_internal.width *
                                       video_codec_internal.height *
                                       video_codec_internal.maxFramerate)
                                       / 1000;
    if (video_codec_internal.startBitrate > video_codec_internal.maxBitrate) {
      // Don't limit the set start bitrate.
      video_codec_internal.maxBitrate = video_codec_internal.startBitrate;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: New max bitrate set to %d kbps", __FUNCTION__,
                 video_codec_internal.maxBitrate);
  }

  VideoCodec encoder;
  vie_encoder->GetEncoder(&encoder);

  // Make sure to generate a new SSRC if the codec type and/or resolution has
  // changed. This won't have any effect if the user has set an SSRC.
  bool new_rtp_stream = false;
  if (encoder.codecType != video_codec_internal.codecType) {
    new_rtp_stream = true;
  }

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViEFrameProviderBase* frame_provider = NULL;

  // Stop the media flow while reconfiguring.
  vie_encoder->Pause();

  // Check if we have a frame provider that is a camera and can provide this
  // codec for us.
  bool use_capture_device_as_encoder = false;
  frame_provider = is.FrameProvider(vie_encoder);
  if (frame_provider) {
    if (frame_provider->Id() >= kViECaptureIdBase &&
        frame_provider->Id() <= kViECaptureIdMax) {
      ViECapturer* vie_capture = static_cast<ViECapturer*>(frame_provider);
      // Try to get preencoded. Nothing to do if it is not supported.
      if (vie_capture && vie_capture->PreEncodeToViEEncoder(
          video_codec_internal,
          *vie_encoder,
          video_channel) == 0) {
        use_capture_device_as_encoder = true;
      }
    }
  }

  // Update the encoder settings if we are not using a capture device capable
  // of this codec.
  if (!use_capture_device_as_encoder &&
      vie_encoder->SetEncoder(video_codec_internal) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not change encoder for channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }

  // Give the channel(s) the new information.
  ChannelList channels;
  cs.ChannelsUsingViEEncoder(video_channel, &channels);
  for (ChannelList::iterator it = channels.begin(); it != channels.end();
       ++it) {
    bool ret = true;
    if ((*it)->SetSendCodec(video_codec_internal, new_rtp_stream) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo,
                   ViEId(shared_data_->instance_id(), video_channel),
                   "%s: Could not set send codec for channel %d", __FUNCTION__,
                   video_channel);
      ret = false;
    }
    if (!ret) {
      shared_data_->SetLastError(kViECodecUnknownError);
      return -1;
    }
  }

  // TODO(mflodman) Break out this part in GetLocalSsrcList().
  // Update all SSRCs to ViEEncoder.
  std::list<unsigned int> ssrcs;
  if (video_codec_internal.numberOfSimulcastStreams == 0) {
    unsigned int ssrc = 0;
    if (vie_channel->GetLocalSSRC(0, &ssrc) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo,
                   ViEId(shared_data_->instance_id(), video_channel),
                   "%s: Could not get ssrc", __FUNCTION__);
    }
    ssrcs.push_back(ssrc);
  } else {
    for (int idx = 0; idx < video_codec_internal.numberOfSimulcastStreams;
         ++idx) {
      unsigned int ssrc = 0;
      if (vie_channel->GetLocalSSRC(idx, &ssrc) != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo,
                     ViEId(shared_data_->instance_id(), video_channel),
                     "%s: Could not get ssrc for idx %d", __FUNCTION__, idx);
      }
      ssrcs.push_back(ssrc);
    }
  }
  vie_encoder->SetSsrcs(ssrcs);
  shared_data_->channel_manager()->UpdateSsrcs(video_channel, ssrcs);

  // Update the protection mode, we might be switching NACK/FEC.
  vie_encoder->UpdateProtectionMethod();

  // Get new best format for frame provider.
  if (frame_provider) {
    frame_provider->FrameCallbackChanged();
  }
  // Restart the media flow
  if (new_rtp_stream) {
    // Stream settings changed, make sure we get a key frame.
    vie_encoder->SendKeyFrame();
  }
  vie_encoder->Restart();
  return 0;
}

int ViECodecImpl::GetSendCodec(const int video_channel,
                               VideoCodec& video_codec) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No encoder for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  return vie_encoder->GetEncoder(&video_codec);
}

int ViECodecImpl::SetReceiveCodec(const int video_channel,
                                  const VideoCodec& video_codec) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d, codec_type: %d)", __FUNCTION__,
               video_channel, video_codec.codecType);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s: codec: %d, pl_type: %d, width: %d, height: %d, bitrate: %d,"
               "maxBr: %d, min_br: %d, frame_rate: %d", __FUNCTION__,
               video_codec.codecType, video_codec.plType, video_codec.width,
               video_codec.height, video_codec.startBitrate,
               video_codec.maxBitrate, video_codec.minBitrate,
               video_codec.maxFramerate);

  if (CodecValid(video_codec) == false) {
    // Error logged.
    shared_data_->SetLastError(kViECodecInvalidCodec);
    return -1;
  }

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }

  if (vie_channel->SetReceiveCodec(video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not set receive codec for channel %d",
                 __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::GetReceiveCodec(const int video_channel,
                                  VideoCodec& video_codec) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d, codec_type: %d)", __FUNCTION__,
               video_channel, video_codec.codecType);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }

  if (vie_channel->GetReceiveCodec(&video_codec) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::GetCodecConfigParameters(
  const int video_channel,
  unsigned char config_parameters[kConfigParameterSize],
  unsigned char& config_parameters_size) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No encoder for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }

  if (vie_encoder->GetCodecConfigParameters(config_parameters,
                                            config_parameters_size) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::SetImageScaleStatus(const int video_channel,
                                      const bool enable) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d, enable: %d)", __FUNCTION__, video_channel,
               enable);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }

  if (vie_encoder->ScaleInputImage(enable) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::GetSendCodecStastistics(const int video_channel,
                                          unsigned int& key_frames,
                                          unsigned int& delta_frames) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No send codec for channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }

  if (vie_encoder->SendCodecStatistics(&key_frames, &delta_frames) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::GetReceiveCodecStastistics(const int video_channel,
                                             unsigned int& key_frames,
                                             unsigned int& delta_frames) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d)", __FUNCTION__,
               video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_channel->ReceiveCodecStatistics(&key_frames, &delta_frames) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::GetReceiveSideDelay(const int video_channel,
                                      int* delay_ms) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);
  if (delay_ms == NULL) {
    LOG_F(LS_ERROR) << "NULL pointer argument.";
    return -1;
  }

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  *delay_ms = vie_channel->ReceiveDelay();
  if (*delay_ms < 0) {
    return -1;
  }
  return 0;
}


int ViECodecImpl::GetCodecTargetBitrate(const int video_channel,
                                        unsigned int* bitrate) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d, codec_type: %d)", __FUNCTION__,
               video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No send codec for channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  return vie_encoder->CodecTargetBitrate(static_cast<WebRtc_UWord32*>(bitrate));
}

unsigned int ViECodecImpl::GetDiscardedPackets(const int video_channel) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d, codec_type: %d)", __FUNCTION__,
               video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  return vie_channel->DiscardedPackets();
}

int ViECodecImpl::SetKeyFrameRequestCallbackStatus(const int video_channel,
                                                   const bool enable) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_channel->EnableKeyFrameRequestCallback(enable) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::SetSignalKeyPacketLossStatus(const int video_channel,
                                               const bool enable,
                                               const bool only_key_frames) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d, enable: %d, only_key_frames: %d)",
               __FUNCTION__, video_channel, enable);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetSignalPacketLossStatus(enable, only_key_frames) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::RegisterEncoderObserver(const int video_channel,
                                          ViEEncoderObserver& observer) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s", __FUNCTION__);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No encoder for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_encoder->RegisterCodecObserver(&observer) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not register codec observer at channel",
                 __FUNCTION__);
    shared_data_->SetLastError(kViECodecObserverAlreadyRegistered);
    return -1;
  }
  return 0;
}

int ViECodecImpl::DeregisterEncoderObserver(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s", __FUNCTION__);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No encoder for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_encoder->RegisterCodecObserver(NULL) != 0) {
    shared_data_->SetLastError(kViECodecObserverNotRegistered);
    return -1;
  }
  return 0;
}

int ViECodecImpl::RegisterDecoderObserver(const int video_channel,
                                          ViEDecoderObserver& observer) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s", __FUNCTION__);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterCodecObserver(&observer) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not register codec observer at channel",
                 __FUNCTION__);
    shared_data_->SetLastError(kViECodecObserverAlreadyRegistered);
    return -1;
  }
  return 0;
}

int ViECodecImpl::DeregisterDecoderObserver(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id()), "%s",
               __FUNCTION__);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterCodecObserver(NULL) != 0) {
    shared_data_->SetLastError(kViECodecObserverNotRegistered);
    return -1;
  }
  return 0;
}

int ViECodecImpl::SendKeyFrame(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_encoder->SendKeyFrame() != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViECodecImpl::WaitForFirstKeyFrame(const int video_channel,
                                       const bool wait) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id()),
               "%s(video_channel: %d, wait: %d)", __FUNCTION__, video_channel,
               wait);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidChannelId);
    return -1;
  }
  if (vie_channel->WaitForKeyFrame(wait) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

bool ViECodecImpl::CodecValid(const VideoCodec& video_codec) {
  // Check pl_name matches codec_type.
  if (video_codec.codecType == kVideoCodecRED) {
#if defined(WIN32)
    if (_strnicmp(video_codec.plName, "red", 3) == 0) {
#else
    if (strncasecmp(video_codec.plName, "red", 3) == 0) {
#endif
      // We only care about the type and name for red.
      return true;
    }
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                 "Codec type doesn't match pl_name", video_codec.plType);
    return false;
  } else if (video_codec.codecType == kVideoCodecULPFEC) {
#if defined(WIN32)
    if (_strnicmp(video_codec.plName, "ULPFEC", 6) == 0) {
#else
    if (strncasecmp(video_codec.plName, "ULPFEC", 6) == 0) {
#endif
      // We only care about the type and name for ULPFEC.
      return true;
    }
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                 "Codec type doesn't match pl_name", video_codec.plType);
    return false;
  } else if ((video_codec.codecType == kVideoCodecVP8 &&
                  strncmp(video_codec.plName, "VP8", 4) == 0) ||
              (video_codec.codecType == kVideoCodecI420 &&
                  strncmp(video_codec.plName, "I420", 4) == 0)) {
    // OK.
  } else {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                 "Codec type doesn't match pl_name", video_codec.plType);
    return false;
  }

  if (video_codec.plType == 0 && video_codec.plType > 127) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                 "Invalid codec payload type: %d", video_codec.plType);
    return false;
  }

  if (video_codec.width > kViEMaxCodecWidth ||
      video_codec.height > kViEMaxCodecHeight) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Invalid codec size: %u x %u",
                 video_codec.width, video_codec.height);
    return false;
  }

  if (video_codec.startBitrate < kViEMinCodecBitrate) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Invalid start_bitrate: %u",
                 video_codec.startBitrate);
    return false;
  }
  if (video_codec.minBitrate < kViEMinCodecBitrate) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Invalid min_bitrate: %u",
                 video_codec.minBitrate);
    return false;
  }
  return true;
}

}  // namespace webrtc
