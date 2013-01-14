/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_base_impl.h"

#include <sstream>
#include <string>

#include "engine_configurations.h"  // NOLINT
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_render/include/video_render.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_input_manager.h"
#include "video_engine/vie_shared_data.h"

namespace webrtc {

ViEBase* ViEBase::GetInterface(VideoEngine* video_engine) {
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViEBaseImpl* vie_base_impl = vie_impl;
  (*vie_base_impl)++;  // Increase ref count.

  return vie_base_impl;
}

int ViEBaseImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_.instance_id(),
               "ViEBase::Release()");
  (*this)--;  // Decrease ref count.

  WebRtc_Word32 ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, shared_data_.instance_id(),
                 "ViEBase release too many times");
    shared_data_.SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, shared_data_.instance_id(),
               "ViEBase reference count: %d", ref_count);
  return ref_count;
}

ViEBaseImpl::ViEBaseImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_.instance_id(),
               "ViEBaseImpl::ViEBaseImpl() Ctor");
}

ViEBaseImpl::~ViEBaseImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_.instance_id(),
               "ViEBaseImpl::ViEBaseImpl() Dtor");
}

int ViEBaseImpl::Init() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_.instance_id(),
               "Init");
  if (shared_data_.Initialized()) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, shared_data_.instance_id(),
                 "Init called twice");
    return 0;
  }

  shared_data_.SetInitialized();
  return 0;
}

int ViEBaseImpl::SetVoiceEngine(VoiceEngine* voice_engine) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s", __FUNCTION__);
  if (!(shared_data_.Initialized())) {
    shared_data_.SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_.instance_id());
    return -1;
  }

  if (shared_data_.channel_manager()->SetVoiceEngine(voice_engine) != 0) {
    shared_data_.SetLastError(kViEBaseVoEFailure);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::CreateChannel(int& video_channel) {  // NOLINT
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s", __FUNCTION__);

  if (!(shared_data_.Initialized())) {
    shared_data_.SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_.instance_id());
    return -1;
  }

  if (shared_data_.channel_manager()->CreateChannel(&video_channel) == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s: Could not create channel", __FUNCTION__);
    video_channel = -1;
    shared_data_.SetLastError(kViEBaseChannelCreationFailed);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s: channel created: %d", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s(%d)", __FUNCTION__, video_channel);

  if (!(shared_data_.Initialized())) {
    shared_data_.SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_.instance_id());
    return -1;
  }

  {
    ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
    ViEChannel* vie_channel = cs.Channel(video_channel);
    if (!vie_channel) {
      WEBRTC_TRACE(kTraceError, kTraceVideo,
                   ViEId(shared_data_.instance_id()),
                   "%s: channel %d doesn't exist", __FUNCTION__, video_channel);
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
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s: Could not delete channel %d", __FUNCTION__,
                 video_channel);
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s: channel deleted: %d", __FUNCTION__, video_channel);
  return 0;
}

int ViEBaseImpl::ConnectAudioChannel(const int video_channel,
                                     const int audio_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s(%d)", __FUNCTION__, video_channel);

  if (!(shared_data_.Initialized())) {
    shared_data_.SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_.instance_id());
    return -1;
  }

  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  if (!cs.Channel(video_channel)) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s: channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s(%d)", __FUNCTION__, video_channel);
  if (!(shared_data_.Initialized())) {
    shared_data_.SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_.instance_id());
    return -1;
  }
  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  if (!cs.Channel(video_channel)) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s: channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_.instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_.instance_id(), video_channel),
                 "%s: Channel %d does not exist", __FUNCTION__, video_channel);
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder != NULL);
  if (vie_encoder->Owner() != video_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_.instance_id(), video_channel),
                 "Can't start ssend on a receive only channel.");
    shared_data_.SetLastError(kViEBaseReceiveOnlyChannel);
    return -1;
  }

  // Pause and trigger a key frame.
  vie_encoder->Pause();
  WebRtc_Word32 error = vie_channel->StartSend();
  if (error != 0) {
    vie_encoder->Restart();
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_.instance_id(), video_channel),
                 "%s: Could not start sending on channel %d", __FUNCTION__,
                 video_channel);
    if (error == kViEBaseAlreadySending) {
      shared_data_.SetLastError(kViEBaseAlreadySending);
    }
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  vie_encoder->SendKeyFrame();
  vie_encoder->Restart();
  return 0;
}

int ViEBaseImpl::StopSend(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_.instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_.instance_id(), video_channel),
                 "%s: Channel %d does not exist", __FUNCTION__, video_channel);
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  WebRtc_Word32 error = vie_channel->StopSend();
  if (error != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_.instance_id(), video_channel),
                 "%s: Could not stop sending on channel %d", __FUNCTION__,
                 video_channel);
    if (error == kViEBaseNotSending) {
      shared_data_.SetLastError(kViEBaseNotSending);
    } else {
      shared_data_.SetLastError(kViEBaseUnknownError);
    }
    return -1;
  }
  return 0;
}

int ViEBaseImpl::StartReceive(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_.instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_.instance_id(), video_channel),
                 "%s: Channel %d does not exist", __FUNCTION__, video_channel);
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  if (vie_channel->Receiving()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_.instance_id(), video_channel),
                 "%s: Channel %d already receive.", __FUNCTION__,
                 video_channel);
    shared_data_.SetLastError(kViEBaseAlreadyReceiving);
    return -1;
  }
  if (vie_channel->StartReceive() != 0) {
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::StopReceive(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_.instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_.instance_id(), video_channel),
                 "%s: Channel %d does not exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "GetVersion(version=?)");
  assert(kViEVersionMaxMessageSize == 1024);
  if (!version) {
    shared_data_.SetLastError(kViEBaseInvalidArgument);
    return -1;
  }

  // Add WebRTC Version.
  std::stringstream version_stream;
  version_stream << "VideoEngine 3.20.0" << std::endl;

  // Add build info.
  version_stream << "Build: svn:" << WEBRTC_SVNREVISION << " " << BUILDINFO
                 << std::endl;

#ifdef WEBRTC_EXTERNAL_TRANSPORT
  version_stream << "External transport build" << std::endl;
#endif
  int version_length = version_stream.tellp();
  assert(version_length < 1024);
  memcpy(version, version_stream.str().c_str(), version_length);
  version[version_length] = '\0';

  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo,
               ViEId(shared_data_.instance_id()), "GetVersion() => %s",
               version);
  return 0;
}

int ViEBaseImpl::LastError() {
  return shared_data_.LastErrorInternal();
}

int ViEBaseImpl::CreateChannel(int& video_channel,  // NOLINT
                               int original_channel, bool sender) {
  if (!(shared_data_.Initialized())) {
    shared_data_.SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_.instance_id());
    return -1;
  }

  ViEChannelManagerScoped cs(*(shared_data_.channel_manager()));
  if (!cs.Channel(original_channel)) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s - original_channel does not exist.", __FUNCTION__,
                 shared_data_.instance_id());
    shared_data_.SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  if (shared_data_.channel_manager()->CreateChannel(&video_channel,
                                                    original_channel,
                                                    sender) == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s: Could not create channel", __FUNCTION__);
    video_channel = -1;
    shared_data_.SetLastError(kViEBaseChannelCreationFailed);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s: channel created: %d", __FUNCTION__, video_channel);
  return 0;
}

}  // namespace webrtc
