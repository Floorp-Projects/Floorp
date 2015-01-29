/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_base_impl.h"

#include <string>
#include <utility>

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/include/vie_errors.h"
#include "webrtc/video_engine/vie_capturer.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_channel_manager.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_impl.h"
#include "webrtc/video_engine/vie_input_manager.h"
#include "webrtc/video_engine/vie_shared_data.h"

namespace webrtc {

ViEBase* ViEBase::GetInterface(VideoEngine* video_engine) {
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = static_cast<VideoEngineImpl*>(video_engine);
  ViEBaseImpl* vie_base_impl = vie_impl;
  (*vie_base_impl)++;  // Increase ref count.

  return vie_base_impl;
}

int ViEBaseImpl::Release() {
  (*this)--;  // Decrease ref count.

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    LOG(LS_WARNING) << "ViEBase released too many times.";
    return -1;
  }
  return ref_count;
}

ViEBaseImpl::ViEBaseImpl(const Config& config)
    : shared_data_(config) {}

ViEBaseImpl::~ViEBaseImpl() {}

int ViEBaseImpl::Init() {
  return 0;
}

int ViEBaseImpl::SetVoiceEngine(VoiceEngine* voice_engine) {
  LOG_F(LS_INFO) << "SetVoiceEngine";
  if (shared_data_.channel_manager()->SetVoiceEngine(voice_engine) != 0) {
    shared_data_.SetLastError(kViEBaseVoEFailure);
    return -1;
  }
  return 0;
}

void ViEBaseImpl::SetLoadManager(CPULoadStateCallbackInvoker* aLoadManager) {
  shared_data_.set_load_manager(aLoadManager);
}

int ViEBaseImpl::RegisterCpuOveruseObserver(int video_channel,
                                            CpuOveruseObserver* observer) {
  LOG_F(LS_INFO) << "RegisterCpuOveruseObserver on channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder);

  ViEInputManagerScoped is(*(shared_data_.input_manager()));
  ViEFrameProviderBase* provider = is.FrameProvider(vie_encoder);
  if (provider) {
    ViECapturer* capturer = is.Capture(provider->Id());
    assert(capturer);
    capturer->RegisterCpuOveruseObserver(observer);
  }

  shared_data_.overuse_observers()->insert(
      std::pair<int, CpuOveruseObserver*>(video_channel, observer));
  return 0;
}

int ViEBaseImpl::SetCpuOveruseOptions(int video_channel,
                                      const CpuOveruseOptions& options) {
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder);

  ViEInputManagerScoped is(*(shared_data_.input_manager()));
  ViEFrameProviderBase* provider = is.FrameProvider(vie_encoder);
  if (provider) {
    ViECapturer* capturer = is.Capture(provider->Id());
    if (capturer) {
      capturer->SetCpuOveruseOptions(options);
      return 0;
    }
  }
  return -1;
}

int ViEBaseImpl::GetCpuOveruseMetrics(int video_channel,
                                      CpuOveruseMetrics* metrics) {
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder);

  ViEInputManagerScoped is(*(shared_data_.input_manager()));
  ViEFrameProviderBase* provider = is.FrameProvider(vie_encoder);
  if (provider) {
    ViECapturer* capturer = is.Capture(provider->Id());
    if (capturer) {
      capturer->GetCpuOveruseMetrics(metrics);
      return 0;
    }
  }
  return -1;
}

void ViEBaseImpl::RegisterSendSideDelayObserver(
    int channel, SendSideDelayObserver* observer) {
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(channel);
  assert(vie_channel);
  vie_channel->RegisterSendSideDelayObserver(observer);
}

int ViEBaseImpl::CreateChannel(int& video_channel) {  // NOLINT
  return CreateChannel(video_channel, static_cast<const Config*>(NULL));
}

int ViEBaseImpl::CreateChannel(int& video_channel,  // NOLINT
                               const Config* config) {
  if (shared_data_.channel_manager()->CreateChannel(&video_channel,
                                                    config) == -1) {
    video_channel = -1;
    shared_data_.SetLastError(kViEBaseChannelCreationFailed);
    return -1;
  }
  LOG(LS_INFO) << "Video channel created: " << video_channel;
  return 0;
}

int ViEBaseImpl::CreateChannel(int& video_channel,  // NOLINT
                               int original_channel) {
  return CreateChannel(video_channel, original_channel, true);
}

int ViEBaseImpl::CreateReceiveChannel(int& video_channel,  // NOLINT
                                      int original_channel) {
  return CreateChannel(video_channel, original_channel, false);
}

int ViEBaseImpl::DeleteChannel(const int video_channel) {
  {
    ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
    ViEChannel* vie_channel = cs.Channel(video_channel);
    if (!vie_channel) {
      shared_data_.SetLastError(kViEBaseInvalidChannelId);
      return -1;
    }

    // Deregister the ViEEncoder if no other channel is using it.
    ViEEncoder* vie_encoder = cs.Encoder(video_channel);
    if (cs.ChannelUsingViEEncoder(video_channel) == false) {
      ViEInputManagerScoped is(*(shared_data_.input_manager()));
      ViEFrameProviderBase* provider = is.FrameProvider(vie_encoder);
      if (provider) {
        provider->DeregisterFrameCallback(vie_encoder);
      }
    }
  }

  if (shared_data_.channel_manager()->DeleteChannel(video_channel) == -1) {
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  LOG(LS_INFO) << "Channel deleted " << video_channel;
  return 0;
}

int ViEBaseImpl::ConnectAudioChannel(const int video_channel,
                                     const int audio_channel) {
  LOG_F(LS_INFO) << "ConnectAudioChannel, video channel " << video_channel
                 << ", audio channel " << audio_channel;
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  if (!cs.Channel(video_channel)) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  if (shared_data_.channel_manager()->ConnectVoiceChannel(video_channel,
                                                          audio_channel) != 0) {
    shared_data_.SetLastError(kViEBaseVoEFailure);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::DisconnectAudioChannel(const int video_channel) {
  LOG_F(LS_INFO) << "DisconnectAudioChannel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  if (!cs.Channel(video_channel)) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  if (shared_data_.channel_manager()->DisconnectVoiceChannel(
      video_channel) != 0) {
    shared_data_.SetLastError(kViEBaseVoEFailure);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::StartSend(const int video_channel) {
  LOG_F(LS_INFO) << "StartSend: " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder != NULL);
  if (vie_encoder->Owner() != video_channel) {
    LOG_F(LS_ERROR) <<  "Can't start send on a receive only channel.";
    shared_data_.SetLastError(kViEBaseReceiveOnlyChannel);
    return -1;
  }

  // Pause and trigger a key frame.
  vie_encoder->Pause();
  int32_t error = vie_channel->StartSend();
  if (error != 0) {
    vie_encoder->Restart();
    if (error == kViEBaseAlreadySending) {
      shared_data_.SetLastError(kViEBaseAlreadySending);
    }
    LOG_F(LS_ERROR) << "Could not start sending " << video_channel;
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  vie_encoder->SendKeyFrame();
  vie_encoder->Restart();
  return 0;
}

int ViEBaseImpl::StopSend(const int video_channel) {
  LOG_F(LS_INFO) << "StopSend " << video_channel;

  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  int32_t error = vie_channel->StopSend();
  if (error != 0) {
    if (error == kViEBaseNotSending) {
      shared_data_.SetLastError(kViEBaseNotSending);
    } else {
      LOG_F(LS_ERROR) << "Could not stop sending " << video_channel;
      shared_data_.SetLastError(kViEBaseUnknownError);
    }
    return -1;
  }
  return 0;
}

int ViEBaseImpl::StartReceive(const int video_channel) {
  LOG_F(LS_INFO) << "StartReceive " << video_channel;

  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  if (vie_channel->StartReceive() != 0) {
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::StopReceive(const int video_channel) {
  LOG_F(LS_INFO) << "StopReceive " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  if (vie_channel->StopReceive() != 0) {
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::GetVersion(char version[1024]) {
  assert(version != NULL);
  strcpy(version, "VideoEngine 39");
  return 0;
}

int ViEBaseImpl::LastError() {
  return shared_data_.LastErrorInternal();
}

int ViEBaseImpl::CreateChannel(int& video_channel,  // NOLINT
                               int original_channel, bool sender) {
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  if (!cs.Channel(original_channel)) {
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  if (shared_data_.channel_manager()->CreateChannel(&video_channel,
                                                    original_channel,
                                                    sender) == -1) {
    video_channel = -1;
    shared_data_.SetLastError(kViEBaseChannelCreationFailed);
    return -1;
  }
  LOG_F(LS_INFO) << "VideoChannel created: " << video_channel
                 << ", base channel " << original_channel
                 << ", is send channel : " << sender;
  return 0;
}

}  // namespace webrtc
