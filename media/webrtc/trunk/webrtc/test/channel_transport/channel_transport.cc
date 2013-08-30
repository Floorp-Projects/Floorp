/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/channel_transport/include/channel_transport.h"

#include <stdio.h>

#ifndef WEBRTC_ANDROID
#include "testing/gtest/include/gtest/gtest.h"
#endif
#include "webrtc/test/channel_transport/udp_transport.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/voice_engine/include/voe_network.h"

#ifdef WEBRTC_ANDROID
#undef NDEBUG
#include <assert.h>
#endif

namespace webrtc {
namespace test {

VoiceChannelTransport::VoiceChannelTransport(VoENetwork* voe_network,
                                             int channel)
    : channel_(channel),
      voe_network_(voe_network) {
  uint8_t socket_threads = 1;
  socket_transport_ = UdpTransport::Create(channel, socket_threads);
  int registered = voe_network_->RegisterExternalTransport(channel,
                                                           *socket_transport_);
#ifndef WEBRTC_ANDROID
  EXPECT_EQ(0, registered);
#else
  assert(registered == 0);
#endif
}

VoiceChannelTransport::~VoiceChannelTransport() {
  voe_network_->DeRegisterExternalTransport(channel_);
  UdpTransport::Destroy(socket_transport_);
}

void VoiceChannelTransport::IncomingRTPPacket(
    const int8_t* incoming_rtp_packet,
    const int32_t packet_length,
    const char* /*from_ip*/,
    const uint16_t /*from_port*/) {
  voe_network_->ReceivedRTPPacket(channel_, incoming_rtp_packet, packet_length);
}

void VoiceChannelTransport::IncomingRTCPPacket(
    const int8_t* incoming_rtcp_packet,
    const int32_t packet_length,
    const char* /*from_ip*/,
    const uint16_t /*from_port*/) {
  voe_network_->ReceivedRTCPPacket(channel_, incoming_rtcp_packet,
                                   packet_length);
}

int VoiceChannelTransport::SetLocalReceiver(uint16_t rtp_port) {
  int return_value = socket_transport_->InitializeReceiveSockets(this,
                                                                 rtp_port);
  if (return_value == 0) {
    return socket_transport_->StartReceiving(kViENumReceiveSocketBuffers);
  }
  return return_value;
}

int VoiceChannelTransport::SetSendDestination(const char* ip_address,
                                              uint16_t rtp_port) {
  return socket_transport_->InitializeSendSockets(ip_address, rtp_port);
}


VideoChannelTransport::VideoChannelTransport(ViENetwork* vie_network,
                                             int channel)
    : channel_(channel),
      vie_network_(vie_network) {
  uint8_t socket_threads = 1;
  socket_transport_ = UdpTransport::Create(channel, socket_threads);
  int registered = vie_network_->RegisterSendTransport(channel,
                                                       *socket_transport_);
#ifndef WEBRTC_ANDROID
  EXPECT_EQ(0, registered);
#else
  assert(registered == 0);
#endif
}

VideoChannelTransport::~VideoChannelTransport() {
  vie_network_->DeregisterSendTransport(channel_);
  UdpTransport::Destroy(socket_transport_);
}

void VideoChannelTransport::IncomingRTPPacket(
    const int8_t* incoming_rtp_packet,
    const int32_t packet_length,
    const char* /*from_ip*/,
    const uint16_t /*from_port*/) {
  vie_network_->ReceivedRTPPacket(channel_, incoming_rtp_packet, packet_length);
}

void VideoChannelTransport::IncomingRTCPPacket(
    const int8_t* incoming_rtcp_packet,
    const int32_t packet_length,
    const char* /*from_ip*/,
    const uint16_t /*from_port*/) {
  vie_network_->ReceivedRTCPPacket(channel_, incoming_rtcp_packet,
                                   packet_length);
}

int VideoChannelTransport::SetLocalReceiver(uint16_t rtp_port) {
  int return_value = socket_transport_->InitializeReceiveSockets(this,
                                                                 rtp_port);
  if (return_value == 0) {
    return socket_transport_->StartReceiving(kViENumReceiveSocketBuffers);
  }
  return return_value;
}

int VideoChannelTransport::SetSendDestination(const char* ip_address,
                                              uint16_t rtp_port) {
  return socket_transport_->InitializeSendSockets(ip_address, rtp_port);
}

}  // namespace test
}  // namespace webrtc
