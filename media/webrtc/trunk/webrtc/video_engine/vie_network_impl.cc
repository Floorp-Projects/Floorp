/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_network_impl.h"

#include <stdio.h>
#if (defined(WIN32_) || defined(WIN64_))
#include <qos.h>
#endif

#include "webrtc/engine_configurations.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/include/vie_errors.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_channel_manager.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_impl.h"
#include "webrtc/video_engine/vie_shared_data.h"

namespace webrtc {

ViENetwork* ViENetwork::GetInterface(VideoEngine* video_engine) {
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = static_cast<VideoEngineImpl*>(video_engine);
  ViENetworkImpl* vie_networkImpl = vie_impl;
  // Increase ref count.
  (*vie_networkImpl)++;
  return vie_networkImpl;
}

int ViENetworkImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_->instance_id(),
               "ViENetwork::Release()");
  // Decrease ref count.
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, shared_data_->instance_id(),
                 "ViENetwork release too many times");
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, shared_data_->instance_id(),
               "ViENetwork reference count: %d", ref_count);
  return ref_count;
}

void ViENetworkImpl::SetNetworkTransmissionState(const int video_channel,
                                                 const bool is_transmitting) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(event: Network %s)", __FUNCTION__,
               is_transmitting ? "transmitting" : "not transmitting");
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "An encoder doesn't exist for this channel");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return;
  }
  vie_encoder->SetNetworkTransmissionState(is_transmitting);
}

ViENetworkImpl::ViENetworkImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViENetworkImpl::ViENetworkImpl() Ctor");
}

ViENetworkImpl::~ViENetworkImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViENetworkImpl::~ViENetworkImpl() Dtor");
}

int ViENetworkImpl::RegisterSendTransport(const int video_channel,
                                          Transport& transport) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s Channel doesn't exist", __FUNCTION__);
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s Channel already sending.", __FUNCTION__);
    shared_data_->SetLastError(kViENetworkAlreadySending);
    return -1;
  }
  if (vie_channel->RegisterSendTransport(&transport) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::DeregisterSendTransport(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s Channel doesn't exist", __FUNCTION__);
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s Channel already sending", __FUNCTION__);
    shared_data_->SetLastError(kViENetworkAlreadySending);
    return -1;
  }
  if (vie_channel->DeregisterSendTransport() != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::ReceivedRTPPacket(const int video_channel, const void* data,
                                      const int length) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, data: -, length: %d)", __FUNCTION__,
               video_channel, length);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    // The channel doesn't exists
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  return vie_channel->ReceivedRTPPacket(data, length);
}

int ViENetworkImpl::ReceivedRTCPPacket(const int video_channel,
                                       const void* data, const int length) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, data: -, length: %d)", __FUNCTION__,
               video_channel, length);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  return vie_channel->ReceivedRTCPPacket(data, length);
}

int ViENetworkImpl::SetMTU(int video_channel, unsigned int mtu) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, mtu: %u)", __FUNCTION__, video_channel, mtu);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetMTU(mtu) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::SetPacketTimeoutNotification(const int video_channel,
                                                 bool enable,
                                                 int timeout_seconds) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, enable: %d, timeout_seconds: %u)",
               __FUNCTION__, video_channel, enable, timeout_seconds);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetPacketTimeoutNotification(enable,
                                                timeout_seconds) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::RegisterObserver(const int video_channel,
                                     ViENetworkObserver& observer) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterNetworkObserver(&observer) != 0) {
    shared_data_->SetLastError(kViENetworkObserverAlreadyRegistered);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::DeregisterObserver(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (!vie_channel->NetworkObserverRegistered()) {
    shared_data_->SetLastError(kViENetworkObserverNotRegistered);
    return -1;
  }
  return vie_channel->RegisterNetworkObserver(NULL);
}

int ViENetworkImpl::SetPeriodicDeadOrAliveStatus(
    const int video_channel,
    bool enable,
    unsigned int sample_time_seconds) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, enable: %d, sample_time_seconds: %ul)",
               __FUNCTION__, video_channel, enable, sample_time_seconds);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (!vie_channel->NetworkObserverRegistered()) {
    shared_data_->SetLastError(kViENetworkObserverNotRegistered);
    return -1;
  }
  if (vie_channel->SetPeriodicDeadOrAliveStatus(enable, sample_time_seconds)
      != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
