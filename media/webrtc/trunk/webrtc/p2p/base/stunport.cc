/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/stunport.h"

#include "webrtc/p2p/base/common.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/stun.h"
#include "webrtc/base/common.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/nethelpers.h"

namespace cricket {

// TODO: Move these to a common place (used in relayport too)
const int KEEPALIVE_DELAY = 10 * 1000;  // 10 seconds - sort timeouts
const int RETRY_DELAY = 50;             // 50ms, from ICE spec
const int RETRY_TIMEOUT = 50 * 1000;    // ICE says 50 secs

// Handles a binding request sent to the STUN server.
class StunBindingRequest : public StunRequest {
 public:
  StunBindingRequest(UDPPort* port, bool keep_alive,
                     const rtc::SocketAddress& addr)
    : port_(port), keep_alive_(keep_alive), server_addr_(addr) {
    start_time_ = rtc::Time();
  }

  virtual ~StunBindingRequest() {
  }

  const rtc::SocketAddress& server_addr() const { return server_addr_; }

  virtual void Prepare(StunMessage* request) {
    request->SetType(STUN_BINDING_REQUEST);
  }

  virtual void OnResponse(StunMessage* response) {
    const StunAddressAttribute* addr_attr =
        response->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
    if (!addr_attr) {
      LOG(LS_ERROR) << "Binding response missing mapped address.";
    } else if (addr_attr->family() != STUN_ADDRESS_IPV4 &&
               addr_attr->family() != STUN_ADDRESS_IPV6) {
      LOG(LS_ERROR) << "Binding address has bad family";
    } else {
      rtc::SocketAddress addr(addr_attr->ipaddr(), addr_attr->port());
      port_->OnStunBindingRequestSucceeded(server_addr_, addr);
    }

    // We will do a keep-alive regardless of whether this request succeeds.
    // This should have almost no impact on network usage.
    if (keep_alive_) {
      port_->requests_.SendDelayed(
          new StunBindingRequest(port_, true, server_addr_),
          port_->stun_keepalive_delay());
    }
  }

  virtual void OnErrorResponse(StunMessage* response) {
    const StunErrorCodeAttribute* attr = response->GetErrorCode();
    if (!attr) {
      LOG(LS_ERROR) << "Bad allocate response error code";
    } else {
      LOG(LS_ERROR) << "Binding error response:"
                 << " class=" << attr->eclass()
                 << " number=" << attr->number()
                 << " reason='" << attr->reason() << "'";
    }

    port_->OnStunBindingOrResolveRequestFailed(server_addr_);

    if (keep_alive_
        && (rtc::TimeSince(start_time_) <= RETRY_TIMEOUT)) {
      port_->requests_.SendDelayed(
          new StunBindingRequest(port_, true, server_addr_),
          port_->stun_keepalive_delay());
    }
  }

  virtual void OnTimeout() {
    LOG(LS_ERROR) << "Binding request timed out from "
      << port_->GetLocalAddress().ToSensitiveString()
      << " (" << port_->Network()->name() << ")";

    port_->OnStunBindingOrResolveRequestFailed(server_addr_);

    if (keep_alive_
        && (rtc::TimeSince(start_time_) <= RETRY_TIMEOUT)) {
      port_->requests_.SendDelayed(
          new StunBindingRequest(port_, true, server_addr_),
          RETRY_DELAY);
    }
  }

 private:
  UDPPort* port_;
  bool keep_alive_;
  const rtc::SocketAddress server_addr_;
  uint32 start_time_;
};

UDPPort::AddressResolver::AddressResolver(
    rtc::PacketSocketFactory* factory)
    : socket_factory_(factory) {}

UDPPort::AddressResolver::~AddressResolver() {
  for (ResolverMap::iterator it = resolvers_.begin();
       it != resolvers_.end(); ++it) {
    it->second->Destroy(true);
  }
}

void UDPPort::AddressResolver::Resolve(
    const rtc::SocketAddress& address) {
  if (resolvers_.find(address) != resolvers_.end())
    return;

  rtc::AsyncResolverInterface* resolver =
      socket_factory_->CreateAsyncResolver();
  resolvers_.insert(
      std::pair<rtc::SocketAddress, rtc::AsyncResolverInterface*>(
          address, resolver));

  resolver->SignalDone.connect(this,
                               &UDPPort::AddressResolver::OnResolveResult);

  resolver->Start(address);
}

bool UDPPort::AddressResolver::GetResolvedAddress(
    const rtc::SocketAddress& input,
    int family,
    rtc::SocketAddress* output) const {
  ResolverMap::const_iterator it = resolvers_.find(input);
  if (it == resolvers_.end())
    return false;

  return it->second->GetResolvedAddress(family, output);
}

void UDPPort::AddressResolver::OnResolveResult(
    rtc::AsyncResolverInterface* resolver) {
  for (ResolverMap::iterator it = resolvers_.begin();
       it != resolvers_.end(); ++it) {
    if (it->second == resolver) {
      SignalDone(it->first, resolver->GetError());
      return;
    }
  }
}

UDPPort::UDPPort(rtc::Thread* thread,
                 rtc::PacketSocketFactory* factory,
                 rtc::Network* network,
                 rtc::AsyncPacketSocket* socket,
                 const std::string& username,
                 const std::string& password,
                 const std::string& origin)
    : Port(thread, factory, network, socket->GetLocalAddress().ipaddr(),
           username, password),
      requests_(thread),
      socket_(socket),
      error_(0),
      ready_(false),
      stun_keepalive_delay_(KEEPALIVE_DELAY) {
  requests_.set_origin(origin);
}

UDPPort::UDPPort(rtc::Thread* thread,
                 rtc::PacketSocketFactory* factory,
                 rtc::Network* network,
                 const rtc::IPAddress& ip,
                 uint16 min_port,
                 uint16 max_port,
                 const std::string& username,
                 const std::string& password,
                 const std::string& origin)
    : Port(thread, LOCAL_PORT_TYPE, factory, network, ip, min_port, max_port,
           username, password),
      requests_(thread),
      socket_(NULL),
      error_(0),
      ready_(false),
      stun_keepalive_delay_(KEEPALIVE_DELAY) {
  requests_.set_origin(origin);
}

bool UDPPort::Init() {
  if (!SharedSocket()) {
    ASSERT(socket_ == NULL);
    socket_ = socket_factory()->CreateUdpSocket(
        rtc::SocketAddress(ip(), 0), min_port(), max_port());
    if (!socket_) {
      LOG_J(LS_WARNING, this) << "UDP socket creation failed";
      return false;
    }
    socket_->SignalReadPacket.connect(this, &UDPPort::OnReadPacket);
  }
  socket_->SignalReadyToSend.connect(this, &UDPPort::OnReadyToSend);
  socket_->SignalAddressReady.connect(this, &UDPPort::OnLocalAddressReady);
  requests_.SignalSendPacket.connect(this, &UDPPort::OnSendPacket);
  return true;
}

UDPPort::~UDPPort() {
  if (!SharedSocket())
    delete socket_;
}

void UDPPort::PrepareAddress() {
  ASSERT(requests_.empty());
  if (socket_->GetState() == rtc::AsyncPacketSocket::STATE_BOUND) {
    OnLocalAddressReady(socket_, socket_->GetLocalAddress());
  }
}

void UDPPort::MaybePrepareStunCandidate() {
  // Sending binding request to the STUN server if address is available to
  // prepare STUN candidate.
  if (!server_addresses_.empty()) {
    SendStunBindingRequests();
  } else {
    // Port is done allocating candidates.
    MaybeSetPortCompleteOrError();
  }
}

Connection* UDPPort::CreateConnection(const Candidate& address,
                                       CandidateOrigin origin) {
  if (address.protocol() != "udp")
    return NULL;

  if (!IsCompatibleAddress(address.address())) {
    return NULL;
  }

  if (SharedSocket() && Candidates()[0].type() != LOCAL_PORT_TYPE) {
    ASSERT(false);
    return NULL;
  }

  Connection* conn = new ProxyConnection(this, 0, address);
  AddConnection(conn);
  return conn;
}

int UDPPort::SendTo(const void* data, size_t size,
                    const rtc::SocketAddress& addr,
                    const rtc::PacketOptions& options,
                    bool payload) {
  int sent = socket_->SendTo(data, size, addr, options);
  if (sent < 0) {
    error_ = socket_->GetError();
    LOG_J(LS_ERROR, this) << "UDP send of " << size
                          << " bytes failed with error " << error_;
  }
  return sent;
}

int UDPPort::SetOption(rtc::Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

int UDPPort::GetOption(rtc::Socket::Option opt, int* value) {
  return socket_->GetOption(opt, value);
}

int UDPPort::GetError() {
  return error_;
}

void UDPPort::OnLocalAddressReady(rtc::AsyncPacketSocket* socket,
                                  const rtc::SocketAddress& address) {
  AddAddress(address, address, rtc::SocketAddress(),
             UDP_PROTOCOL_NAME, "", LOCAL_PORT_TYPE,
             ICE_TYPE_PREFERENCE_HOST, 0, false);
  MaybePrepareStunCandidate();
}

void UDPPort::OnReadPacket(
  rtc::AsyncPacketSocket* socket, const char* data, size_t size,
  const rtc::SocketAddress& remote_addr,
  const rtc::PacketTime& packet_time) {
  ASSERT(socket == socket_);
  ASSERT(!remote_addr.IsUnresolved());

  // Look for a response from the STUN server.
  // Even if the response doesn't match one of our outstanding requests, we
  // will eat it because it might be a response to a retransmitted packet, and
  // we already cleared the request when we got the first response.
  if (server_addresses_.find(remote_addr) != server_addresses_.end()) {
    requests_.CheckResponse(data, size);
    return;
  }

  if (Connection* conn = GetConnection(remote_addr)) {
    conn->OnReadPacket(data, size, packet_time);
  } else {
    Port::OnReadPacket(data, size, remote_addr, PROTO_UDP);
  }
}

void UDPPort::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  Port::OnReadyToSend();
}

void UDPPort::SendStunBindingRequests() {
  // We will keep pinging the stun server to make sure our NAT pin-hole stays
  // open during the call.
  ASSERT(requests_.empty());

  for (ServerAddresses::const_iterator it = server_addresses_.begin();
       it != server_addresses_.end(); ++it) {
    SendStunBindingRequest(*it);
  }
}

void UDPPort::ResolveStunAddress(const rtc::SocketAddress& stun_addr) {
  if (!resolver_) {
    resolver_.reset(new AddressResolver(socket_factory()));
    resolver_->SignalDone.connect(this, &UDPPort::OnResolveResult);
  }

  resolver_->Resolve(stun_addr);
}

void UDPPort::OnResolveResult(const rtc::SocketAddress& input,
                              int error) {
  ASSERT(resolver_.get() != NULL);

  rtc::SocketAddress resolved;
  if (error != 0 ||
      !resolver_->GetResolvedAddress(input, ip().family(), &resolved))  {
    LOG_J(LS_WARNING, this) << "StunPort: stun host lookup received error "
                            << error;
    OnStunBindingOrResolveRequestFailed(input);
    return;
  }

  server_addresses_.erase(input);

  if (server_addresses_.find(resolved) == server_addresses_.end()) {
    server_addresses_.insert(resolved);
    SendStunBindingRequest(resolved);
  }
}

void UDPPort::SendStunBindingRequest(
    const rtc::SocketAddress& stun_addr) {
  if (stun_addr.IsUnresolved()) {
    ResolveStunAddress(stun_addr);

  } else if (socket_->GetState() == rtc::AsyncPacketSocket::STATE_BOUND) {
    // Check if |server_addr_| is compatible with the port's ip.
    if (IsCompatibleAddress(stun_addr)) {
      requests_.Send(new StunBindingRequest(this, true, stun_addr));
    } else {
      // Since we can't send stun messages to the server, we should mark this
      // port ready.
      LOG(LS_WARNING) << "STUN server address is incompatible.";
      OnStunBindingOrResolveRequestFailed(stun_addr);
    }
  }
}

void UDPPort::OnStunBindingRequestSucceeded(
    const rtc::SocketAddress& stun_server_addr,
    const rtc::SocketAddress& stun_reflected_addr) {
  if (bind_request_succeeded_servers_.find(stun_server_addr) !=
          bind_request_succeeded_servers_.end()) {
    return;
  }
  bind_request_succeeded_servers_.insert(stun_server_addr);

  // If socket is shared and |stun_reflected_addr| is equal to local socket
  // address, or if the same address has been added by another STUN server,
  // then discarding the stun address.
  // For STUN, related address is the local socket address.
  if ((!SharedSocket() || stun_reflected_addr != socket_->GetLocalAddress()) &&
      !HasCandidateWithAddress(stun_reflected_addr)) {

    rtc::SocketAddress related_address = socket_->GetLocalAddress();
    if (!(candidate_filter() & CF_HOST)) {
      // If candidate filter doesn't have CF_HOST specified, empty raddr to
      // avoid local address leakage.
      related_address = rtc::EmptySocketAddressWithFamily(
          related_address.family());
    }

    AddAddress(stun_reflected_addr, socket_->GetLocalAddress(),
               related_address, UDP_PROTOCOL_NAME, "",
               STUN_PORT_TYPE, ICE_TYPE_PREFERENCE_SRFLX, 0, false);
  }
  MaybeSetPortCompleteOrError();
}

void UDPPort::OnStunBindingOrResolveRequestFailed(
    const rtc::SocketAddress& stun_server_addr) {
  if (bind_request_failed_servers_.find(stun_server_addr) !=
          bind_request_failed_servers_.end()) {
    return;
  }
  bind_request_failed_servers_.insert(stun_server_addr);
  MaybeSetPortCompleteOrError();
}

void UDPPort::MaybeSetPortCompleteOrError() {
  if (ready_)
    return;

  // Do not set port ready if we are still waiting for bind responses.
  const size_t servers_done_bind_request = bind_request_failed_servers_.size() +
      bind_request_succeeded_servers_.size();
  if (server_addresses_.size() != servers_done_bind_request) {
    return;
  }

  // Setting ready status.
  ready_ = true;

  // The port is "completed" if there is no stun server provided, or the bind
  // request succeeded for any stun server, or the socket is shared.
  if (server_addresses_.empty() ||
      bind_request_succeeded_servers_.size() > 0 ||
      SharedSocket()) {
    SignalPortComplete(this);
  } else {
    SignalPortError(this);
  }
}

// TODO: merge this with SendTo above.
void UDPPort::OnSendPacket(const void* data, size_t size, StunRequest* req) {
  StunBindingRequest* sreq = static_cast<StunBindingRequest*>(req);
  rtc::PacketOptions options(DefaultDscpValue());
  if (socket_->SendTo(data, size, sreq->server_addr(), options) < 0)
    PLOG(LERROR, socket_->GetError()) << "sendto";
}

bool UDPPort::HasCandidateWithAddress(const rtc::SocketAddress& addr) const {
  const std::vector<Candidate>& existing_candidates = Candidates();
  std::vector<Candidate>::const_iterator it = existing_candidates.begin();
  for (; it != existing_candidates.end(); ++it) {
    if (it->address() == addr)
      return true;
  }
  return false;
}

}  // namespace cricket
