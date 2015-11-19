/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_TCPPORT_H_
#define WEBRTC_P2P_BASE_TCPPORT_H_

#include <list>
#include <string>
#include "webrtc/p2p/base/port.h"
#include "webrtc/base/asyncpacketsocket.h"

namespace cricket {

class TCPConnection;

// Communicates using a local TCP port.
//
// This class is designed to allow subclasses to take advantage of the
// connection management provided by this class.  A subclass should take of all
// packet sending and preparation, but when a packet is received, it should
// call this TCPPort::OnReadPacket (3 arg) to dispatch to a connection.
class TCPPort : public Port {
 public:
  static TCPPort* Create(rtc::Thread* thread,
                         rtc::PacketSocketFactory* factory,
                         rtc::Network* network,
                         const rtc::IPAddress& ip,
                         uint16 min_port,
                         uint16 max_port,
                         const std::string& username,
                         const std::string& password,
                         bool allow_listen) {
    TCPPort* port = new TCPPort(thread, factory, network, ip, min_port,
                                max_port, username, password, allow_listen);
    if (!port->Init()) {
      delete port;
      port = NULL;
    }
    return port;
  }
  virtual ~TCPPort();

  virtual Connection* CreateConnection(const Candidate& address,
                                       CandidateOrigin origin);

  virtual void PrepareAddress();

  virtual int GetOption(rtc::Socket::Option opt, int* value);
  virtual int SetOption(rtc::Socket::Option opt, int value);
  virtual int GetError();

 protected:
  TCPPort(rtc::Thread* thread,
          rtc::PacketSocketFactory* factory,
          rtc::Network* network,
          const rtc::IPAddress& ip,
          uint16 min_port,
          uint16 max_port,
          const std::string& username,
          const std::string& password,
          bool allow_listen);
  bool Init();

  // Handles sending using the local TCP socket.
  virtual int SendTo(const void* data, size_t size,
                     const rtc::SocketAddress& addr,
                     const rtc::PacketOptions& options,
                     bool payload);

  // Accepts incoming TCP connection.
  void OnNewConnection(rtc::AsyncPacketSocket* socket,
                       rtc::AsyncPacketSocket* new_socket);

 private:
  struct Incoming {
    rtc::SocketAddress addr;
    rtc::AsyncPacketSocket* socket;
  };

  rtc::AsyncPacketSocket* GetIncoming(
      const rtc::SocketAddress& addr, bool remove = false);

  // Receives packet signal from the local TCP Socket.
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data, size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const rtc::PacketTime& packet_time);

  void OnReadyToSend(rtc::AsyncPacketSocket* socket);

  void OnAddressReady(rtc::AsyncPacketSocket* socket,
                      const rtc::SocketAddress& address);

  // TODO: Is this still needed?
  bool incoming_only_;
  bool allow_listen_;
  rtc::AsyncPacketSocket* socket_;
  int error_;
  std::list<Incoming> incoming_;

  friend class TCPConnection;
};

class TCPConnection : public Connection {
 public:
  // Connection is outgoing unless socket is specified
  TCPConnection(TCPPort* port, const Candidate& candidate,
                rtc::AsyncPacketSocket* socket = 0);
  virtual ~TCPConnection();

  virtual int Send(const void* data, size_t size,
                   const rtc::PacketOptions& options);
  virtual int GetError();

  rtc::AsyncPacketSocket* socket() { return socket_; }

 private:
  void OnConnect(rtc::AsyncPacketSocket* socket);
  void OnClose(rtc::AsyncPacketSocket* socket, int error);
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data, size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const rtc::PacketTime& packet_time);
  void OnReadyToSend(rtc::AsyncPacketSocket* socket);

  rtc::AsyncPacketSocket* socket_;
  int error_;

  friend class TCPPort;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TCPPORT_H_
