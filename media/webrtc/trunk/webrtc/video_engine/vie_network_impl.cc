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

#include "webrtc/base/checks.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/system_wrappers/interface/logging.h"
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
  // Decrease ref count.
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    LOG(LS_ERROR) << "ViENetwork release too many times";
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  return ref_count;
}

ViENetworkImpl::ViENetworkImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
}

ViENetworkImpl::~ViENetworkImpl() {
}

void ViENetworkImpl::SetBitrateConfig(int video_channel,
                                      int min_bitrate_bps,
                                      int start_bitrate_bps,
                                      int max_bitrate_bps) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " new bitrate config: min=" << min_bitrate_bps
                 << ", start=" << start_bitrate_bps
                 << ", max=" << max_bitrate_bps;
  bool success = shared_data_->channel_manager()->SetBitrateConfig(
      video_channel, min_bitrate_bps, start_bitrate_bps, max_bitrate_bps);
  DCHECK(success);
}

void ViENetworkImpl::SetNetworkTransmissionState(const int video_channel,
                                                 const bool is_transmitting) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " transmitting: " << (is_transmitting ? "yes" : "no");
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return;
  }
  vie_encoder->SetNetworkTransmissionState(is_transmitting);
}

int ViENetworkImpl::RegisterSendTransport(const int video_channel,
                                          Transport& transport) {
  LOG_F(LS_INFO) << "channel: " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->Sending()) {
    LOG_F(LS_ERROR) << "Already sending on channel: " << video_channel;
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
  LOG_F(LS_INFO) << "channel: " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->Sending()) {
    LOG_F(LS_ERROR) << "Actively sending on channel: " << video_channel;
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
                                      const size_t length,
                                      const PacketTime& packet_time) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  return vie_channel->ReceivedRTPPacket(data, length, packet_time);
}

int ViENetworkImpl::ReceivedRTCPPacket(const int video_channel,
                                       const void* data, const size_t length) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  return vie_channel->ReceivedRTCPPacket(data, length);
}

int ViENetworkImpl::SetMTU(int video_channel, unsigned int mtu) {
  LOG_F(LS_INFO) << "channel: " << video_channel << " mtu: " << mtu;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetMTU(mtu) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::ReceivedBWEPacket(const int video_channel,
                                      int64_t arrival_time_ms,
                                      size_t payload_size,
                                      const RTPHeader& header) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }

  vie_channel->ReceivedBWEPacket(arrival_time_ms, payload_size, header);
  return 0;
}
}  // namespace webrtc
