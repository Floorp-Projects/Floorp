/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/rawtransportchannel.h"

#include <string>
#include <vector>
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/portinterface.h"
#include "webrtc/p2p/base/rawtransport.h"
#include "webrtc/p2p/base/relayport.h"
#include "webrtc/p2p/base/stunport.h"
#include "webrtc/base/common.h"

#if defined(FEATURE_ENABLE_PSTN)

namespace {

const uint32 MSG_DESTROY_RTC_UNUSED_PORTS = 1;

}  // namespace

namespace cricket {

RawTransportChannel::RawTransportChannel(const std::string& content_name,
                                         int component,
                                         RawTransport* transport,
                                         rtc::Thread *worker_thread,
                                         PortAllocator *allocator)
  : TransportChannelImpl(content_name, component),
    raw_transport_(transport),
    allocator_(allocator),
    allocator_session_(NULL),
    stun_port_(NULL),
    relay_port_(NULL),
    port_(NULL),
    use_relay_(false) {
  if (worker_thread == NULL)
    worker_thread_ = raw_transport_->worker_thread();
  else
    worker_thread_ = worker_thread;
}

RawTransportChannel::~RawTransportChannel() {
  delete allocator_session_;
}

int RawTransportChannel::SendPacket(const char *data, size_t size,
                                    const rtc::PacketOptions& options,
                                    int flags) {
  if (port_ == NULL)
    return -1;
  if (remote_address_.IsNil())
    return -1;
  if (flags != 0)
    return -1;
  return port_->SendTo(data, size, remote_address_, options, true);
}

int RawTransportChannel::SetOption(rtc::Socket::Option opt, int value) {
  // TODO: allow these to be set before we have a port
  if (port_ == NULL)
    return -1;
  return port_->SetOption(opt, value);
}

bool RawTransportChannel::GetOption(rtc::Socket::Option opt, int* value) {
  return false;
}

int RawTransportChannel::GetError() {
  return (port_ != NULL) ? port_->GetError() : 0;
}

void RawTransportChannel::Connect() {
  // Create an allocator that only returns stun and relay ports.
  // Use empty string for ufrag and pwd here. There won't be any STUN or relay
  // interactions when using RawTC.
  // TODO: Change raw to only use local udp ports.
  allocator_session_ = allocator_->CreateSession(
      SessionId(), content_name(), component(), "", "");

  uint32 flags = PORTALLOCATOR_DISABLE_UDP | PORTALLOCATOR_DISABLE_TCP;

#if !defined(FEATURE_ENABLE_STUN_CLASSIFICATION)
  flags |= PORTALLOCATOR_DISABLE_RELAY;
#endif
  allocator_session_->set_flags(flags);
  allocator_session_->SignalPortReady.connect(
      this, &RawTransportChannel::OnPortReady);
  allocator_session_->SignalCandidatesReady.connect(
      this, &RawTransportChannel::OnCandidatesReady);

  // The initial ports will include stun.
  allocator_session_->StartGettingPorts();
}

void RawTransportChannel::Reset() {
  set_readable(false);
  set_writable(false);

  delete allocator_session_;

  allocator_session_ = NULL;
  stun_port_ = NULL;
  relay_port_ = NULL;
  port_ = NULL;
  remote_address_ = rtc::SocketAddress();
}

void RawTransportChannel::OnCandidate(const Candidate& candidate) {
  remote_address_ = candidate.address();
  ASSERT(!remote_address_.IsNil());
  set_readable(true);

  // We can write once we have a port and a remote address.
  if (port_ != NULL)
    SetWritable();
}

void RawTransportChannel::OnRemoteAddress(
    const rtc::SocketAddress& remote_address) {
  remote_address_ = remote_address;
  set_readable(true);

  if (port_ != NULL)
    SetWritable();
}

// Note about stun classification
// Code to classify our NAT type and use the relay port if we are behind an
// asymmetric NAT is under a FEATURE_ENABLE_STUN_CLASSIFICATION #define.
// To turn this one we will have to enable a second stun address and make sure
// that the relay server works for raw UDP.
//
// Another option is to classify the NAT type early and not offer the raw
// transport type at all if we can't support it.

void RawTransportChannel::OnPortReady(
    PortAllocatorSession* session, PortInterface* port) {
  ASSERT(session == allocator_session_);

  if (port->Type() == STUN_PORT_TYPE) {
    stun_port_ = static_cast<StunPort*>(port);
  } else if (port->Type() == RELAY_PORT_TYPE) {
    relay_port_ = static_cast<RelayPort*>(port);
  } else {
    ASSERT(false);
  }
}

void RawTransportChannel::OnCandidatesReady(
    PortAllocatorSession *session, const std::vector<Candidate>& candidates) {
  ASSERT(session == allocator_session_);
  ASSERT(candidates.size() >= 1);

  // The most recent candidate is the one we haven't seen yet.
  Candidate c = candidates[candidates.size() - 1];

  if (c.type() == STUN_PORT_TYPE) {
    ASSERT(stun_port_ != NULL);

#if defined(FEATURE_ENABLE_STUN_CLASSIFICATION)
    // We need to wait until we have two addresses.
    if (stun_port_->candidates().size() < 2)
      return;

    // This is the second address.  If these addresses are the same, then we
    // are not behind a symmetric NAT.  Hence, a stun port should be sufficient.
    if (stun_port_->candidates()[0].address() ==
        stun_port_->candidates()[1].address()) {
      SetPort(stun_port_);
      return;
    }

    // We will need to use relay.
    use_relay_ = true;

    // If we already have a relay address, we're good.  Otherwise, we will need
    // to wait until one arrives.
    if (relay_port_->candidates().size() > 0)
      SetPort(relay_port_);
#else  // defined(FEATURE_ENABLE_STUN_CLASSIFICATION)
    // Always use the stun port.  We don't classify right now so just assume it
    // will work fine.
    SetPort(stun_port_);
#endif
  } else if (c.type() == RELAY_PORT_TYPE) {
    if (use_relay_)
      SetPort(relay_port_);
  } else {
    ASSERT(false);
  }
}

void RawTransportChannel::SetPort(PortInterface* port) {
  ASSERT(port_ == NULL);
  port_ = port;

  // We don't need any ports other than the one we picked.
  allocator_session_->StopGettingPorts();
  worker_thread_->Post(
      this, MSG_DESTROY_RTC_UNUSED_PORTS, NULL);

  // Send a message to the other client containing our address.

  ASSERT(port_->Candidates().size() >= 1);
  ASSERT(port_->Candidates()[0].protocol() == "udp");
  SignalCandidateReady(this, port_->Candidates()[0]);

  // Read all packets from this port.
  port_->EnablePortPackets();
  port_->SignalReadPacket.connect(this, &RawTransportChannel::OnReadPacket);

  // We can write once we have a port and a remote address.
  if (!remote_address_.IsAny())
    SetWritable();
}

void RawTransportChannel::SetWritable() {
  ASSERT(port_ != NULL);
  ASSERT(!remote_address_.IsAny());

  set_writable(true);

  Candidate remote_candidate;
  remote_candidate.set_address(remote_address_);
  SignalRouteChange(this, remote_candidate);
}

void RawTransportChannel::OnReadPacket(
    PortInterface* port, const char* data, size_t size,
    const rtc::SocketAddress& addr) {
  ASSERT(port_ == port);
  SignalReadPacket(this, data, size, rtc::CreatePacketTime(0), 0);
}

void RawTransportChannel::OnMessage(rtc::Message* msg) {
  ASSERT(msg->message_id == MSG_DESTROY_RTC_UNUSED_PORTS);
  ASSERT(port_ != NULL);
  if (port_ != stun_port_) {
    stun_port_->Destroy();
    stun_port_ = NULL;
  }
  if (port_ != relay_port_ && relay_port_ != NULL) {
    relay_port_->Destroy();
    relay_port_ = NULL;
  }
}

}  // namespace cricket
#endif  // defined(FEATURE_ENABLE_PSTN)
