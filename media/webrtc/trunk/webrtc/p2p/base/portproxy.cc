/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/portproxy.h"

namespace cricket {

void PortProxy::set_impl(PortInterface* port) {
  impl_ = port;
  impl_->SignalUnknownAddress.connect(
      this, &PortProxy::OnUnknownAddress);
  impl_->SignalDestroyed.connect(this, &PortProxy::OnPortDestroyed);
  impl_->SignalRoleConflict.connect(this, &PortProxy::OnRoleConflict);
}

const std::string& PortProxy::Type() const {
  ASSERT(impl_ != NULL);
  return impl_->Type();
}

rtc::Network* PortProxy::Network() const {
  ASSERT(impl_ != NULL);
  return impl_->Network();
}

void PortProxy::SetIceProtocolType(IceProtocolType protocol) {
  ASSERT(impl_ != NULL);
  impl_->SetIceProtocolType(protocol);
}

IceProtocolType PortProxy::IceProtocol() const {
  ASSERT(impl_ != NULL);
  return impl_->IceProtocol();
}

// Methods to set/get ICE role and tiebreaker values.
void PortProxy::SetIceRole(IceRole role) {
  ASSERT(impl_ != NULL);
  impl_->SetIceRole(role);
}

IceRole PortProxy::GetIceRole() const {
  ASSERT(impl_ != NULL);
  return impl_->GetIceRole();
}

void PortProxy::SetIceTiebreaker(uint64 tiebreaker) {
  ASSERT(impl_ != NULL);
  impl_->SetIceTiebreaker(tiebreaker);
}

uint64 PortProxy::IceTiebreaker() const {
  ASSERT(impl_ != NULL);
  return impl_->IceTiebreaker();
}

bool PortProxy::SharedSocket() const {
  ASSERT(impl_ != NULL);
  return impl_->SharedSocket();
}

void PortProxy::PrepareAddress() {
  ASSERT(impl_ != NULL);
  impl_->PrepareAddress();
}

Connection* PortProxy::CreateConnection(const Candidate& remote_candidate,
                                        CandidateOrigin origin) {
  ASSERT(impl_ != NULL);
  return impl_->CreateConnection(remote_candidate, origin);
}

int PortProxy::SendTo(const void* data,
                      size_t size,
                      const rtc::SocketAddress& addr,
                      const rtc::PacketOptions& options,
                      bool payload) {
  ASSERT(impl_ != NULL);
  return impl_->SendTo(data, size, addr, options, payload);
}

int PortProxy::SetOption(rtc::Socket::Option opt,
                         int value) {
  ASSERT(impl_ != NULL);
  return impl_->SetOption(opt, value);
}

int PortProxy::GetOption(rtc::Socket::Option opt,
                         int* value) {
  ASSERT(impl_ != NULL);
  return impl_->GetOption(opt, value);
}

int PortProxy::GetError() {
  ASSERT(impl_ != NULL);
  return impl_->GetError();
}

const std::vector<Candidate>& PortProxy::Candidates() const {
  ASSERT(impl_ != NULL);
  return impl_->Candidates();
}

void PortProxy::SendBindingResponse(
    StunMessage* request, const rtc::SocketAddress& addr) {
  ASSERT(impl_ != NULL);
  impl_->SendBindingResponse(request, addr);
}

Connection* PortProxy::GetConnection(
    const rtc::SocketAddress& remote_addr) {
  ASSERT(impl_ != NULL);
  return impl_->GetConnection(remote_addr);
}

void PortProxy::SendBindingErrorResponse(
    StunMessage* request, const rtc::SocketAddress& addr,
    int error_code, const std::string& reason) {
  ASSERT(impl_ != NULL);
  impl_->SendBindingErrorResponse(request, addr, error_code, reason);
}

void PortProxy::EnablePortPackets() {
  ASSERT(impl_ != NULL);
  impl_->EnablePortPackets();
}

std::string PortProxy::ToString() const {
  ASSERT(impl_ != NULL);
  return impl_->ToString();
}

void PortProxy::OnUnknownAddress(
    PortInterface *port,
    const rtc::SocketAddress &addr,
    ProtocolType proto,
    IceMessage *stun_msg,
    const std::string &remote_username,
    bool port_muxed) {
  ASSERT(port == impl_);
  ASSERT(!port_muxed);
  SignalUnknownAddress(this, addr, proto, stun_msg, remote_username, true);
}

void PortProxy::OnRoleConflict(PortInterface* port) {
  ASSERT(port == impl_);
  SignalRoleConflict(this);
}

void PortProxy::OnPortDestroyed(PortInterface* port) {
  ASSERT(port == impl_);
  // |port| will be destroyed in PortAllocatorSessionMuxer.
  SignalDestroyed(this);
}

}  // namespace cricket
