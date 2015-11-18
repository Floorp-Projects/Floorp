/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/tcpport.h"

#include "webrtc/p2p/base/common.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"

namespace cricket {

TCPPort::TCPPort(rtc::Thread* thread,
                 rtc::PacketSocketFactory* factory,
                 rtc::Network* network,
                 const rtc::IPAddress& ip,
                 uint16 min_port,
                 uint16 max_port,
                 const std::string& username,
                 const std::string& password,
                 bool allow_listen)
    : Port(thread, LOCAL_PORT_TYPE, factory, network, ip, min_port, max_port,
           username, password),
      incoming_only_(false),
      allow_listen_(allow_listen),
      socket_(NULL),
      error_(0) {
  // TODO(mallinath) - Set preference value as per RFC 6544.
  // http://b/issue?id=7141794
}

bool TCPPort::Init() {
  if (allow_listen_) {
    // Treat failure to create or bind a TCP socket as fatal.  This
    // should never happen.
    socket_ = socket_factory()->CreateServerTcpSocket(
        rtc::SocketAddress(ip(), 0), min_port(), max_port(),
        false /* ssl */);
    if (!socket_) {
      LOG_J(LS_ERROR, this) << "TCP socket creation failed.";
      return false;
    }
    socket_->SignalNewConnection.connect(this, &TCPPort::OnNewConnection);
    socket_->SignalAddressReady.connect(this, &TCPPort::OnAddressReady);
  }
  return true;
}

TCPPort::~TCPPort() {
  delete socket_;
  std::list<Incoming>::iterator it;
  for (it = incoming_.begin(); it != incoming_.end(); ++it)
    delete it->socket;
  incoming_.clear();
}

Connection* TCPPort::CreateConnection(const Candidate& address,
                                      CandidateOrigin origin) {
  // We only support TCP protocols
  if ((address.protocol() != TCP_PROTOCOL_NAME) &&
      (address.protocol() != SSLTCP_PROTOCOL_NAME)) {
    return NULL;
  }

  if (address.tcptype() == TCPTYPE_ACTIVE_STR ||
      (address.tcptype().empty() && address.address().port() == 0)) {
    // It's active only candidate, we should not try to create connections
    // for these candidates.
    return NULL;
  }

  // We can't accept TCP connections incoming on other ports
  if (origin == ORIGIN_OTHER_PORT)
    return NULL;

  // Check if we are allowed to make outgoing TCP connections
  if (incoming_only_ && (origin == ORIGIN_MESSAGE))
    return NULL;

  // We don't know how to act as an ssl server yet
  if ((address.protocol() == SSLTCP_PROTOCOL_NAME) &&
      (origin == ORIGIN_THIS_PORT)) {
    return NULL;
  }

  if (!IsCompatibleAddress(address.address())) {
    return NULL;
  }

  TCPConnection* conn = NULL;
  if (rtc::AsyncPacketSocket* socket =
      GetIncoming(address.address(), true)) {
    socket->SignalReadPacket.disconnect(this);
    conn = new TCPConnection(this, address, socket);
  } else {
    conn = new TCPConnection(this, address);
  }
  AddConnection(conn);
  return conn;
}

void TCPPort::PrepareAddress() {
  if (socket_) {
    // If socket isn't bound yet the address will be added in
    // OnAddressReady(). Socket may be in the CLOSED state if Listen()
    // failed, we still want to add the socket address.
    LOG(LS_VERBOSE) << "Preparing TCP address, current state: "
                    << socket_->GetState();
    if (socket_->GetState() == rtc::AsyncPacketSocket::STATE_BOUND ||
        socket_->GetState() == rtc::AsyncPacketSocket::STATE_CLOSED)
      AddAddress(socket_->GetLocalAddress(), socket_->GetLocalAddress(),
                 rtc::SocketAddress(),
                 TCP_PROTOCOL_NAME, TCPTYPE_PASSIVE_STR, LOCAL_PORT_TYPE,
                 ICE_TYPE_PREFERENCE_HOST_TCP, 0, true);
  } else {
    LOG_J(LS_INFO, this) << "Not listening due to firewall restrictions.";
    // Note: We still add the address, since otherwise the remote side won't
    // recognize our incoming TCP connections.
    AddAddress(rtc::SocketAddress(ip(), 0),
               rtc::SocketAddress(ip(), 0), rtc::SocketAddress(),
               TCP_PROTOCOL_NAME, TCPTYPE_ACTIVE_STR, LOCAL_PORT_TYPE,
               ICE_TYPE_PREFERENCE_HOST_TCP, 0, true);
  }
}

int TCPPort::SendTo(const void* data, size_t size,
                    const rtc::SocketAddress& addr,
                    const rtc::PacketOptions& options,
                    bool payload) {
  rtc::AsyncPacketSocket * socket = NULL;
  if (TCPConnection * conn = static_cast<TCPConnection*>(GetConnection(addr))) {
    socket = conn->socket();
  } else {
    socket = GetIncoming(addr);
  }
  if (!socket) {
    LOG_J(LS_ERROR, this) << "Attempted to send to an unknown destination, "
                          << addr.ToSensitiveString();
    return -1;  // TODO: Set error_
  }

  int sent = socket->Send(data, size, options);
  if (sent < 0) {
    error_ = socket->GetError();
    LOG_J(LS_ERROR, this) << "TCP send of " << size
                          << " bytes failed with error " << error_;
  }
  return sent;
}

int TCPPort::GetOption(rtc::Socket::Option opt, int* value) {
  if (socket_) {
    return socket_->GetOption(opt, value);
  } else {
    return SOCKET_ERROR;
  }
}

int TCPPort::SetOption(rtc::Socket::Option opt, int value) {
  if (socket_) {
    return socket_->SetOption(opt, value);
  } else {
    return SOCKET_ERROR;
  }
}

int TCPPort::GetError() {
  return error_;
}

void TCPPort::OnNewConnection(rtc::AsyncPacketSocket* socket,
                              rtc::AsyncPacketSocket* new_socket) {
  ASSERT(socket == socket_);

  Incoming incoming;
  incoming.addr = new_socket->GetRemoteAddress();
  incoming.socket = new_socket;
  incoming.socket->SignalReadPacket.connect(this, &TCPPort::OnReadPacket);
  incoming.socket->SignalReadyToSend.connect(this, &TCPPort::OnReadyToSend);

  LOG_J(LS_VERBOSE, this) << "Accepted connection from "
                          << incoming.addr.ToSensitiveString();
  incoming_.push_back(incoming);
}

rtc::AsyncPacketSocket* TCPPort::GetIncoming(
    const rtc::SocketAddress& addr, bool remove) {
  rtc::AsyncPacketSocket* socket = NULL;
  for (std::list<Incoming>::iterator it = incoming_.begin();
       it != incoming_.end(); ++it) {
    if (it->addr == addr) {
      socket = it->socket;
      if (remove)
        incoming_.erase(it);
      break;
    }
  }
  return socket;
}

void TCPPort::OnReadPacket(rtc::AsyncPacketSocket* socket,
                           const char* data, size_t size,
                           const rtc::SocketAddress& remote_addr,
                           const rtc::PacketTime& packet_time) {
  Port::OnReadPacket(data, size, remote_addr, PROTO_TCP);
}

void TCPPort::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  Port::OnReadyToSend();
}

void TCPPort::OnAddressReady(rtc::AsyncPacketSocket* socket,
                             const rtc::SocketAddress& address) {
  AddAddress(address, address, rtc::SocketAddress(),
             TCP_PROTOCOL_NAME, TCPTYPE_PASSIVE_STR, LOCAL_PORT_TYPE,
             ICE_TYPE_PREFERENCE_HOST_TCP, 0, true);
}

TCPConnection::TCPConnection(TCPPort* port, const Candidate& candidate,
                             rtc::AsyncPacketSocket* socket)
    : Connection(port, 0, candidate), socket_(socket), error_(0) {
  bool outgoing = (socket_ == NULL);
  if (outgoing) {
    // TODO: Handle failures here (unlikely since TCP).
    int opts = (candidate.protocol() == SSLTCP_PROTOCOL_NAME) ?
        rtc::PacketSocketFactory::OPT_SSLTCP : 0;
    socket_ = port->socket_factory()->CreateClientTcpSocket(
        rtc::SocketAddress(port->ip(), 0),
        candidate.address(), port->proxy(), port->user_agent(), opts);
    if (socket_) {
      LOG_J(LS_VERBOSE, this) << "Connecting from "
                              << socket_->GetLocalAddress().ToSensitiveString()
                              << " to "
                              << candidate.address().ToSensitiveString();
      set_connected(false);
      socket_->SignalConnect.connect(this, &TCPConnection::OnConnect);
    } else {
      LOG_J(LS_WARNING, this) << "Failed to create connection to "
                              << candidate.address().ToSensitiveString();
    }
  } else {
    // Incoming connections should match the network address.
    ASSERT(socket_->GetLocalAddress().ipaddr() == port->ip());
  }

  if (socket_) {
    socket_->SignalReadPacket.connect(this, &TCPConnection::OnReadPacket);
    socket_->SignalReadyToSend.connect(this, &TCPConnection::OnReadyToSend);
    socket_->SignalClose.connect(this, &TCPConnection::OnClose);
  }
}

TCPConnection::~TCPConnection() {
  delete socket_;
}

int TCPConnection::Send(const void* data, size_t size,
                        const rtc::PacketOptions& options) {
  if (!socket_) {
    error_ = ENOTCONN;
    return SOCKET_ERROR;
  }

  if (write_state() != STATE_WRITABLE) {
    // TODO: Should STATE_WRITE_TIMEOUT return a non-blocking error?
    error_ = EWOULDBLOCK;
    return SOCKET_ERROR;
  }
  sent_packets_total_++;
  int sent = socket_->Send(data, size, options);
  if (sent < 0) {
    sent_packets_discarded_++;
    error_ = socket_->GetError();
  } else {
    send_rate_tracker_.Update(sent);
  }
  return sent;
}

int TCPConnection::GetError() {
  return error_;
}

void TCPConnection::OnConnect(rtc::AsyncPacketSocket* socket) {
  ASSERT(socket == socket_);
  // Do not use this connection if the socket bound to a different address than
  // the one we asked for. This is seen in Chrome, where TCP sockets cannot be
  // given a binding address, and the platform is expected to pick the
  // correct local address.
  const rtc::IPAddress& socket_ip = socket->GetLocalAddress().ipaddr();
  if (socket_ip == port()->ip()) {
    LOG_J(LS_VERBOSE, this) << "Connection established to "
                            << socket->GetRemoteAddress().ToSensitiveString();
    set_connected(true);
  } else {
    LOG_J(LS_WARNING, this) << "Dropping connection as TCP socket bound to IP "
                            << socket_ip.ToSensitiveString()
                            << ", different from the local candidate IP "
                            << port()->ip().ToSensitiveString();
    socket_->Close();
  }
}

void TCPConnection::OnClose(rtc::AsyncPacketSocket* socket, int error) {
  ASSERT(socket == socket_);
  LOG_J(LS_INFO, this) << "Connection closed with error " << error;
  set_connected(false);
  set_write_state(STATE_WRITE_TIMEOUT);
}

void TCPConnection::OnReadPacket(
  rtc::AsyncPacketSocket* socket, const char* data, size_t size,
  const rtc::SocketAddress& remote_addr,
  const rtc::PacketTime& packet_time) {
  ASSERT(socket == socket_);
  Connection::OnReadPacket(data, size, packet_time);
}

void TCPConnection::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  ASSERT(socket == socket_);
  Connection::OnReadyToSend();
}

}  // namespace cricket
