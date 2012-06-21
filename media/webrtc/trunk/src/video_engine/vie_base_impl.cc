/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "critical_section_wrapper.h"
#include "engine_configurations.h"
#include "rtp_rtcp.h"
#include "trace.h"
#include "video_coding.h"
#include "video_processing.h"
#include "video_render.h"
#include "vie_base_impl.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_defines.h"
#include "vie_encoder.h"
#include "vie_errors.h"
#include "vie_impl.h"
#include "vie_input_manager.h"
#include "vie_performance_monitor.h"
#include "vie_shared_data.h"

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

int ViEBaseImpl::CreateChannel(int& video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s", __FUNCTION__);

  if (!(shared_data_.Initialized())) {
    shared_data_.SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_.instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_.instance_id());
    return -1;
  }

  if (shared_data_.channel_manager()->CreateChannel(video_channel) == -1) {
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

int ViEBaseImpl::CreateChannel(int& video_channel, int original_channel) {
  return CreateChannel(video_channel, original_channel, true);
}

int ViEBaseImpl::CreateReceiveChannel(int& video_channel,
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

int ViEBaseImpl::RegisterObserver(ViEBaseObserver& observer) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s", __FUNCTION__);
  if (shared_data_.vie_performance_monitor()->ViEBaseObserverRegistered()) {
    shared_data_.SetLastError(kViEBaseObserverAlreadyRegistered);
    return -1;
  }
  return shared_data_.vie_performance_monitor()->Init(&observer);
}

int ViEBaseImpl::DeregisterObserver() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_.instance_id()),
               "%s", __FUNCTION__);

  if (!shared_data_.vie_performance_monitor()->ViEBaseObserverRegistered()) {
    shared_data_.SetLastError(kViEBaseObserverNotRegistered);
    WEBRTC_TRACE(kTraceError, kTraceVideo, shared_data_.instance_id(),
                 "%s No observer registered.", __FUNCTION__);
    return -1;
  }
  shared_data_.vie_performance_monitor()->Terminate();
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

  char version_buf[kViEVersionMaxMessageSize];
  char* version_ptr = version_buf;

  WebRtc_Word32 len = 0;  // Does not include NULL termination.
  WebRtc_Word32 acc_len = 0;

  len = AddViEVersion(version_ptr);
  if (len == -1) {
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

  len = AddBuildInfo(version_ptr);
  if (len == -1) {
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

  len = AddExternalTransportBuild(version_ptr);
  if (len == -1) {
    shared_data_.SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

  memcpy(version, version_buf, acc_len);
  version[acc_len] = '\0';

  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo,
               ViEId(shared_data_.instance_id()), "GetVersion() => %s",
               version);
  return 0;
}

int ViEBaseImpl::LastError() {
  return shared_data_.LastErrorInternal();
}

WebRtc_Word32 ViEBaseImpl::AddBuildInfo(char* str) const {
  return sprintf(str, "Build: %s\n", BUILDINFO);
}

WebRtc_Word32 ViEBaseImpl::AddViEVersion(char* str) const {
  return sprintf(str, "VideoEngine 3.2.0\n");
}

WebRtc_Word32 ViEBaseImpl::AddExternalTransportBuild(char* str) const {
#ifdef WEBRTC_EXTERNAL_TRANSPORT
  return sprintf(str, "External transport build\n");
#else
  return 0;
#endif
}

int ViEBaseImpl::CreateChannel(int& video_channel, int original_channel,
                               bool sender) {
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

  if (shared_data_.channel_manager()->CreateChannel(video_channel,
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
