/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef stunserver_h__
#define stunserver_h__

#include <map>
#include <string>
#include "prio.h"
#include "nsError.h"
#include "mozilla/UniquePtr.h"

typedef struct nr_stun_server_ctx_ nr_stun_server_ctx;
typedef struct nr_socket_ nr_socket;
typedef struct nr_local_addr_ nr_local_addr;


namespace mozilla {

class TestStunServer {
 public:
  // Generally, you should only call API in this class from the same thread that
  // the initial |GetInstance| call was made from.
  static TestStunServer *GetInstance(int address_family = AF_INET);
  static void ShutdownInstance();
  // |ConfigurePort| will only have an effect if called before the first call
  // to |GetInstance| (possibly following a |ShutdownInstance| call)
  static void ConfigurePort(uint16_t port);
  // AF_INET, AF_INET6
  static UniquePtr<TestStunServer> Create(int address_family);

  virtual ~TestStunServer();

  void SetActive(bool active);
  void SetDelay(uint32_t delay_ms);
  void SetDropInitialPackets(uint32_t count);
  const std::string& addr() const { return listen_addr_; }
  uint16_t port() const { return listen_port_; }

  // These should only be called from the same thread as the initial
  // |GetInstance| call.
  nsresult SetResponseAddr(nr_transport_addr *addr);
  nsresult SetResponseAddr(const std::string& addr, uint16_t port);

  void Reset();

  static const size_t max_stun_message_size = 4096;

  virtual nr_socket* GetReceivingSocket(NR_SOCKET s);
  virtual nr_socket* GetSendingSocket(nr_socket *sock);

 protected:
  TestStunServer()
      : listen_port_(0),
        listen_sock_(nullptr),
        send_sock_(nullptr),
        stun_server_(nullptr),
        active_(true),
        delay_ms_(0),
        initial_ct_(0),
        response_addr_(nullptr),
        timer_handle_(nullptr) {}

  int SetInternalPort(nr_local_addr *addr, uint16_t port);
  int Initialize(int address_family);

  static void readable_cb(NR_SOCKET sock, int how, void *cb_arg);

 private:
  void Process(const uint8_t *msg, size_t len, nr_transport_addr *addr_in, nr_socket *sock);
  virtual int TryOpenListenSocket(nr_local_addr *addr, uint16_t port);
  static void process_cb(NR_SOCKET sock, int how, void *cb_arg);

 protected:
  std::string listen_addr_;
  uint16_t listen_port_;
  nr_socket *listen_sock_;
  nr_socket *send_sock_;
  nr_stun_server_ctx *stun_server_;
 private:
  bool active_;
  uint32_t delay_ms_;
  uint32_t initial_ct_;
  nr_transport_addr *response_addr_;
  void *timer_handle_;
  std::map<std::string, uint32_t> received_ct_;

  static TestStunServer *instance;
  static TestStunServer *instance6;
  static uint16_t instance_port;
};

class TestStunTcpServer: public TestStunServer {
 public:
  static TestStunTcpServer *GetInstance(int address_family);
  static void ShutdownInstance();
  static void ConfigurePort(uint16_t port);
  virtual ~TestStunTcpServer();

  virtual nr_socket* GetReceivingSocket(NR_SOCKET s);
  virtual nr_socket* GetSendingSocket(nr_socket *sock);

 protected:
  TestStunTcpServer() {}
  static void accept_cb(NR_SOCKET sock, int how, void *cb_arg);

 private:
  virtual int TryOpenListenSocket(nr_local_addr *addr, uint16_t port);
  static UniquePtr<TestStunTcpServer> Create(int address_family);

  static TestStunTcpServer *instance;
  static TestStunTcpServer *instance6;
  static uint16_t instance_port;

  std::map<NR_SOCKET, nr_socket*> connections_;
};
} // End of namespace mozilla
#endif
