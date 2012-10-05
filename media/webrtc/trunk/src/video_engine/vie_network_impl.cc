/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_network_impl.h"

#include <stdio.h>
#if (defined(WIN32_) || defined(WIN64_))
#include <qos.h>
#endif

#include "engine_configurations.h"  // NOLINT
#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_shared_data.h"

namespace webrtc {

ViENetwork* ViENetwork::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_NETWORK_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViENetworkImpl* vie_networkImpl = vie_impl;
  // Increase ref count.
  (*vie_networkImpl)++;
  return vie_networkImpl;
#else
  return NULL;
#endif
}

int ViENetworkImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_->instance_id(),
               "ViENetwork::Release()");
  // Decrease ref count.
  (*this)--;

  WebRtc_Word32 ref_count = GetCount();
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

ViENetworkImpl::ViENetworkImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViENetworkImpl::ViENetworkImpl() Ctor");
}

ViENetworkImpl::~ViENetworkImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViENetworkImpl::~ViENetworkImpl() Dtor");
}

int ViENetworkImpl::SetLocalReceiver(const int video_channel,
                                     const uint16_t rtp_port,
                                     const uint16_t rtcp_port,
                                     const char* ip_address) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, rtp_port: %u, rtcp_port: %u, ip_address: %s)",
               __FUNCTION__, video_channel, rtp_port, rtcp_port, ip_address);
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    // The channel doesn't exists.
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }

  if (vie_channel->Receiving()) {
    shared_data_->SetLastError(kViENetworkAlreadyReceiving);
    return -1;
  }
  if (vie_channel->SetLocalReceiver(rtp_port, rtcp_port, ip_address) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::GetLocalReceiver(const int video_channel,
                                     uint16_t& rtp_port,
                                     uint16_t& rtcp_port,
                                     char* ip_address) {
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
  if (vie_channel->GetLocalReceiver(&rtp_port, &rtcp_port, ip_address) != 0) {
    shared_data_->SetLastError(kViENetworkLocalReceiverNotSet);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::SetSendDestination(const int video_channel,
                                       const char* ip_address,
                                       const uint16_t rtp_port,
                                       const uint16_t rtcp_port,
                                       const uint16_t source_rtp_port,
                                       const uint16_t source_rtcp_port) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, ip_address: %s, rtp_port: %u, rtcp_port: %u, "
               "sourceRtpPort: %u, source_rtcp_port: %u)",
               __FUNCTION__, video_channel, ip_address, rtp_port, rtcp_port,
               source_rtp_port, source_rtcp_port);
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }

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
  if (vie_channel->SetSendDestination(ip_address, rtp_port, rtcp_port,
                                          source_rtp_port,
                                          source_rtcp_port) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::GetSendDestination(const int video_channel,
                                       char* ip_address,
                                       uint16_t& rtp_port,
                                       uint16_t& rtcp_port,
                                       uint16_t& source_rtp_port,
                                       uint16_t& source_rtcp_port) {
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
  if (vie_channel->GetSendDestination(ip_address, &rtp_port, &rtcp_port,
                                      &source_rtp_port,
                                      &source_rtcp_port) != 0) {
    shared_data_->SetLastError(kViENetworkDestinationNotSet);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::RegisterSendTransport(const int video_channel,
                                          Transport& transport) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }
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
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }
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
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }
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

int ViENetworkImpl::GetSourceInfo(const int video_channel,
                                  uint16_t& rtp_port,
                                  uint16_t& rtcp_port, char* ip_address,
                                  unsigned int ip_address_length) {
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
  if (vie_channel->GetSourceInfo(&rtp_port, &rtcp_port, ip_address,
                                 ip_address_length) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::GetLocalIP(char ip_address[64], bool ipv6) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s( ip_address, ipV6: %d)", __FUNCTION__, ipv6);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }

  if (!ip_address) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: No argument", __FUNCTION__);
    shared_data_->SetLastError(kViENetworkInvalidArgument);
    return -1;
  }

  WebRtc_UWord8 num_socket_threads = 1;
  UdpTransport* socket_transport = UdpTransport::Create(
      ViEModuleId(shared_data_->instance_id(), -1), num_socket_threads);

  if (!socket_transport) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Could not create socket module", __FUNCTION__);
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }

  char local_ip_address[64];
  if (ipv6) {
    char local_ip[16];
    if (socket_transport->LocalHostAddressIPV6(local_ip) != 0) {
      UdpTransport::Destroy(socket_transport);
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                   "%s: Could not get local IP", __FUNCTION__);
      shared_data_->SetLastError(kViENetworkUnknownError);
      return -1;
    }
    // Convert 128-bit address to character string (a:b:c:d:e:f:g:h).
    // TODO(mflodman) Change sprintf.
    sprintf(local_ip_address,  // NOLINT
            "%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:"
            "%.2x%.2x",
            local_ip[0], local_ip[1], local_ip[2], local_ip[3], local_ip[4],
            local_ip[5], local_ip[6], local_ip[7], local_ip[8], local_ip[9],
            local_ip[10], local_ip[11], local_ip[12], local_ip[13],
            local_ip[14], local_ip[15]);
  } else {
    WebRtc_UWord32 local_ip = 0;
    if (socket_transport->LocalHostAddress(local_ip) != 0) {
      UdpTransport::Destroy(socket_transport);
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                   "%s: Could not get local IP", __FUNCTION__);
      shared_data_->SetLastError(kViENetworkUnknownError);
      return -1;
    }
    // Convert 32-bit address to character string (x.y.z.w).
    // TODO(mflodman) Change sprintf.
    sprintf(local_ip_address, "%d.%d.%d.%d",  // NOLINT
            static_cast<int>((local_ip >> 24) & 0x0ff),
            static_cast<int>((local_ip >> 16) & 0x0ff),
            static_cast<int>((local_ip >> 8) & 0x0ff),
            static_cast<int>(local_ip & 0x0ff));
  }
  strncpy(ip_address, local_ip_address, sizeof(local_ip_address));
  UdpTransport::Destroy(socket_transport);
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s: local ip = %s", __FUNCTION__, local_ip_address);
  return 0;
#else
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s: not available for external transport", __FUNCTION__);

  return -1;
#endif
}

int ViENetworkImpl::EnableIPv6(int video_channel) {
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
  if (vie_channel->EnableIPv6() != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

bool ViENetworkImpl::IsIPv6Enabled(int video_channel) {
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
    return false;
  }
  return vie_channel->IsIPv6Enabled();
}

int ViENetworkImpl::SetSourceFilter(const int video_channel,
                                    const uint16_t rtp_port,
                                    const uint16_t rtcp_port,
                                    const char* ip_address) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, rtp_port: %u, rtcp_port: %u, ip_address: %s)",
               __FUNCTION__, video_channel, rtp_port, rtcp_port, ip_address);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetSourceFilter(rtp_port, rtcp_port, ip_address) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::GetSourceFilter(const int video_channel,
                                    uint16_t& rtp_port,
                                    uint16_t& rtcp_port,
                                    char* ip_address) {
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
  if (vie_channel->GetSourceFilter(&rtp_port, &rtcp_port, ip_address) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::SetSendToS(const int video_channel, const int DSCP,
                               const bool use_set_sockOpt = false) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, DSCP: %d, use_set_sockOpt: %d)", __FUNCTION__,
               video_channel, DSCP, use_set_sockOpt);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }

#if defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
  WEBRTC_TRACE(kTraceInfo, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "   force use_set_sockopt=true since there is no alternative"
               " implementation");
  if (vie_channel->SetToS(DSCP, true) != 0) {
#else
  if (vie_channel->SetToS(DSCP, use_set_sockOpt) != 0) {
#endif
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::GetSendToS(const int video_channel,
                               int& DSCP,  // NOLINT
                               bool& use_set_sockOpt) {  // NOLINT
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
  if (vie_channel->GetToS(&DSCP, &use_set_sockOpt) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

int ViENetworkImpl::SetSendGQoS(const int video_channel, const bool enable,
                                const int service_type,
                                const int overrideDSCP) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, enable: %d, service_type: %d, "
               "overrideDSCP: %d)", __FUNCTION__, video_channel, enable,
               service_type, overrideDSCP);
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }

#if (defined(WIN32_) || defined(WIN64_))
  // Sanity check. We might crash if testing and relying on an OS socket error.
  if (enable &&
      (service_type != SERVICETYPE_BESTEFFORT) &&
      (service_type != SERVICETYPE_CONTROLLEDLOAD) &&
      (service_type != SERVICETYPE_GUARANTEED) &&
      (service_type != SERVICETYPE_QUALITATIVE)) {
    WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: service type %d not supported", __FUNCTION__,
                 video_channel, service_type);
    shared_data_->SetLastError(kViENetworkServiceTypeNotSupported);
    return -1;
  }
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  VideoCodec video_codec;
  if (vie_encoder->GetEncoder(video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not get max bitrate for the channel",
                 __FUNCTION__);
    shared_data_->SetLastError(kViENetworkSendCodecNotSet);
    return -1;
  }
  if (vie_channel->SetSendGQoS(enable, service_type, video_codec.maxBitrate,
                               overrideDSCP) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s: Not supported", __FUNCTION__);
  shared_data_->SetLastError(kViENetworkNotSupported);
  return -1;
#endif
}

int ViENetworkImpl::GetSendGQoS(const int video_channel,
                                bool& enabled,  // NOLINT
                                int& service_type,  // NOLINT
                                int& overrideDSCP) {  // NOLINT
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
  if (vie_channel->GetSendGQoS(&enabled, &service_type, &overrideDSCP) != 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
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

int ViENetworkImpl::SendUDPPacket(const int video_channel, const void* data,
                                  const unsigned int length,
                                  int& transmitted_bytes,
                                  bool use_rtcp_socket = false) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, data: -, length: %d, transmitter_bytes: -, "
               "useRtcpSocket: %d)", __FUNCTION__, video_channel, length,
               use_rtcp_socket);
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Channel doesn't exist");
    shared_data_->SetLastError(kViENetworkInvalidChannelId);
    return -1;
  }
  if (vie_channel->SendUDPPacket((const WebRtc_Word8*) data, length,
                                     (WebRtc_Word32&) transmitted_bytes,
                                     use_rtcp_socket) < 0) {
    shared_data_->SetLastError(kViENetworkUnknownError);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
