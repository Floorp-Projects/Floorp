/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

/* Some source code here from nICEr. Copyright is:

Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


// Implementation of nICEr/nr_socket that is tied to the Gecko
// SocketTransportService.

#ifndef nr_socket_prsock__
#define nr_socket_prsock__

#include <queue>

#include "nspr.h"
#include "prio.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsASocketHandler.h"
#include "nsISocketTransportService.h"
#include "nsXPCOM.h"
#include "nsIEventTarget.h"
#include "nsIUDPSocketChild.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#include "nsITCPSocketCallback.h"
#include "mediapacket.h"
#include "m_cpp_utils.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ClearOnShutdown.h"

// Stub declaration for nICEr type
typedef struct nr_socket_vtbl_ nr_socket_vtbl;
typedef struct nr_socket_ nr_socket;

#if defined(MOZILLA_INTERNAL_API)
namespace mozilla {
namespace dom {
class TCPSocketChild;
}
}
#endif

namespace mozilla {

namespace net {
  union NetAddr;
}

class NrSocketBase {
public:
  NrSocketBase() : connect_invoked_(false), poll_flags_(0) {
    memset(cbs_, 0, sizeof(cbs_));
    memset(cb_args_, 0, sizeof(cb_args_));
    memset(&my_addr_, 0, sizeof(my_addr_));
  }
  virtual ~NrSocketBase() {}

  // Factory method; will create either an NrSocket, NrUdpSocketIpc, or
  // NrTcpSocketIpc as appropriate.
  static int CreateSocket(nr_transport_addr *addr, RefPtr<NrSocketBase> *sock);

  // the nr_socket APIs
  virtual int create(nr_transport_addr *addr) = 0;
  virtual int sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to) = 0;
  virtual int recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from) = 0;
  virtual int getaddr(nr_transport_addr *addrp) = 0;
  virtual void close() = 0;
  virtual int connect(nr_transport_addr *addr) = 0;
  virtual int write(const void *msg, size_t len, size_t *written) = 0;
  virtual int read(void* buf, size_t maxlen, size_t *len) = 0;
  virtual int listen(int backlog) = 0;
  virtual int accept(nr_transport_addr *addrp, nr_socket **sockp) = 0;

   // Implementations of the async_event APIs
  virtual int async_wait(int how, NR_async_cb cb, void *cb_arg,
                         char *function, int line);
  virtual int cancel(int how);

  // nsISupport reference counted interface
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  uint32_t poll_flags() {
    return poll_flags_;
  }

  virtual nr_socket_vtbl *vtbl();  // To access in test classes.

  static TimeStamp short_term_violation_time();
  static TimeStamp long_term_violation_time();
  const nr_transport_addr& my_addr() const {
    return my_addr_;
  }

  void fire_callback(int how);

protected:

  bool connect_invoked_;
  nr_transport_addr my_addr_;

private:
  NR_async_cb cbs_[NR_ASYNC_WAIT_WRITE + 1];
  void *cb_args_[NR_ASYNC_WAIT_WRITE + 1];
  uint32_t poll_flags_;
};

class NrSocket : public NrSocketBase,
                 public nsASocketHandler {
public:
  NrSocket() : fd_(nullptr) {}

  // Implement nsASocket
  virtual void OnSocketReady(PRFileDesc *fd, int16_t outflags) override;
  virtual void OnSocketDetached(PRFileDesc *fd) override;
  virtual void IsLocal(bool *aIsLocal) override;
  virtual uint64_t ByteCountSent() override { return 0; }
  virtual uint64_t ByteCountReceived() override { return 0; }

  // nsISupports methods
  NS_DECL_THREADSAFE_ISUPPORTS

  // Implementations of the async_event APIs
  virtual int async_wait(int how, NR_async_cb cb, void *cb_arg,
                         char *function, int line) override;
  virtual int cancel(int how) override;


  // Implementations of the nr_socket APIs
  virtual int create(nr_transport_addr *addr) override; // (really init, but it's called create)
  virtual int sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to) override;
  virtual int recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from) override;
  virtual int getaddr(nr_transport_addr *addrp) override;
  virtual void close() override;
  virtual int connect(nr_transport_addr *addr) override;
  virtual int write(const void *msg, size_t len, size_t *written) override;
  virtual int read(void* buf, size_t maxlen, size_t *len) override;
  virtual int listen(int backlog) override;
  virtual int accept(nr_transport_addr *addrp, nr_socket **sockp) override;

protected:
  virtual ~NrSocket() {
    if (fd_)
      PR_Close(fd_);
  }

  DISALLOW_COPY_ASSIGN(NrSocket);

  PRFileDesc *fd_;
  nsCOMPtr<nsIEventTarget> ststhread_;
};

struct nr_udp_message {
  nr_udp_message(const PRNetAddr &from, nsAutoPtr<MediaPacket> &data)
      : from(from), data(data) {
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nr_udp_message);

  PRNetAddr from;
  nsAutoPtr<MediaPacket> data;

private:
  ~nr_udp_message() {}
  DISALLOW_COPY_ASSIGN(nr_udp_message);
};

class NrSocketIpc : public NrSocketBase {
public:

  enum NrSocketIpcState {
    NR_INIT,
    NR_CONNECTING,
    NR_CONNECTED,
    NR_CLOSING,
    NR_CLOSED,
  };

  NrSocketIpc(nsIEventTarget* aThread);

protected:
  nsCOMPtr<nsIEventTarget> sts_thread_;
  // Note: for UDP PBackground, this is a thread held by SingletonThreadHolder.
  // For TCP PNecko, this is MainThread (and TCPSocket requires MainThread currently)
  const nsCOMPtr<nsIEventTarget> io_thread_;
  virtual ~NrSocketIpc() {};

private:
  DISALLOW_COPY_ASSIGN(NrSocketIpc);
};

class NrUdpSocketIpc : public NrSocketIpc {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NrUdpSocketIpc, override)

  NS_IMETHODIMP CallListenerError(const nsACString &message,
                                  const nsACString &filename,
                                  uint32_t line_number);
  NS_IMETHODIMP CallListenerReceivedData(const nsACString &host,
                                         uint16_t port,
                                         const uint8_t *data,
                                         uint32_t data_length);
  NS_IMETHODIMP CallListenerOpened();
  NS_IMETHODIMP CallListenerConnected();
  NS_IMETHODIMP CallListenerClosed();

  NrUdpSocketIpc();

  // Implementations of the NrSocketBase APIs
  virtual int create(nr_transport_addr *addr) override;
  virtual int sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to) override;
  virtual int recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from) override;
  virtual int getaddr(nr_transport_addr *addrp) override;
  virtual void close() override;
  virtual int connect(nr_transport_addr *addr) override;
  virtual int write(const void *msg, size_t len, size_t *written) override;
  virtual int read(void* buf, size_t maxlen, size_t *len) override;
  virtual int listen(int backlog) override;
  virtual int accept(nr_transport_addr *addrp, nr_socket **sockp) override;

private:
  virtual ~NrUdpSocketIpc();

  DISALLOW_COPY_ASSIGN(NrUdpSocketIpc);

  nsresult SetAddress();  // Set the local address from parent info.

  // Main or private thread executors of the NrSocketBase APIs
  void create_i(const nsACString &host, const uint16_t port);
  void connect_i(const nsACString &host, const uint16_t port);
  void sendto_i(const net::NetAddr &addr, nsAutoPtr<MediaPacket> buf);
  void close_i();
#if defined(MOZILLA_INTERNAL_API) && !defined(MOZILLA_XPCOMRT_API)
  static void destroy_i(nsIUDPSocketChild* aChild,
                        nsCOMPtr<nsIEventTarget>& aStsThread);
#endif
  // STS thread executor
  void recv_callback_s(RefPtr<nr_udp_message> msg);

  ReentrantMonitor monitor_; // protects err_and state_
  bool err_;
  NrSocketIpcState state_;

  std::queue<RefPtr<nr_udp_message>> received_msgs_;

  RefPtr<nsIUDPSocketChild> socket_child_; // only accessed from the io_thread
};

// The socket child holds onto one of these, which just passes callbacks
// through and makes sure the ref to the NrSocketIpc is released on STS.
class NrUdpSocketIpcProxy : public nsIUDPSocketInternal {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETINTERNAL

  nsresult Init(const RefPtr<NrUdpSocketIpc>& socket);

private:
  virtual ~NrUdpSocketIpcProxy();

  RefPtr<NrUdpSocketIpc> socket_;
  nsCOMPtr<nsIEventTarget> sts_thread_;
};

struct nr_tcp_message {
  explicit nr_tcp_message(nsAutoPtr<MediaPacket> &data)
    : read_bytes(0)
    , data(data) {
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nr_tcp_message);

  const uint8_t *reading_pointer() const {
    return data->data() + read_bytes;
  }

  size_t unread_bytes() const {
    return data->len() - read_bytes;
  }

  size_t read_bytes;

private:
  ~nr_tcp_message() {}
  DISALLOW_COPY_ASSIGN(nr_tcp_message);

  nsAutoPtr<MediaPacket> data;
};

#if defined(MOZILLA_INTERNAL_API) && !defined(MOZILLA_XPCOMRT_API)
class NrTcpSocketIpc : public NrSocketIpc,
                       public nsITCPSocketCallback {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITCPSOCKETCALLBACK

  explicit NrTcpSocketIpc(nsIThread* aThread);

  // Implementations of the NrSocketBase APIs
  virtual int create(nr_transport_addr *addr) override;
  virtual int sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to) override;
  virtual int recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from) override;
  virtual int getaddr(nr_transport_addr *addrp) override;
  virtual void close() override;
  virtual int connect(nr_transport_addr *addr) override;
  virtual int write(const void *msg, size_t len, size_t *written) override;
  virtual int read(void* buf, size_t maxlen, size_t *len) override;
  virtual int listen(int backlog) override;
  virtual int accept(nr_transport_addr *addrp, nr_socket **sockp) override;

private:
  class TcpSocketReadyRunner;
  DISALLOW_COPY_ASSIGN(NrTcpSocketIpc);
  virtual ~NrTcpSocketIpc();

  // Main thread executors of the NrSocketBase APIs
  void connect_i(const nsACString &remote_addr,
                 uint16_t remote_port,
                 const nsACString &local_addr,
                 uint16_t local_port,
                 const nsACString &tls_host);
  void write_i(nsAutoPtr<InfallibleTArray<uint8_t>> buf,
               uint32_t tracking_number);
  void close_i();

  static void release_child_i(dom::TCPSocketChild* aChild);

  // STS thread executor
  void message_sent_s(uint32_t bufferedAmount, uint32_t tracking_number);
  void recv_message_s(nr_tcp_message *msg);
  void update_state_s(NrSocketIpcState next_state);
  void maybe_post_socket_ready();

  // Accessed from UpdateReadyState (not sts_thread) to avoid sending
  // runnables when not needed
  NrSocketIpcState mirror_state_;

  // variables that can only be accessed on STS.
  NrSocketIpcState state_;
  std::queue<RefPtr<nr_tcp_message>> msg_queue_;
  uint32_t buffered_bytes_;
  uint32_t tracking_number_;
  std::deque<size_t> writes_in_flight_;

  // main thread.
  RefPtr<dom::TCPSocketChild> socket_child_;
};
#endif

int nr_netaddr_to_transport_addr(const net::NetAddr *netaddr,
                                 nr_transport_addr *addr,
                                 int protocol);
int nr_praddr_to_transport_addr(const PRNetAddr *praddr,
                                nr_transport_addr *addr,
                                int protocol, int keep);
int nr_transport_addr_get_addrstring_and_port(nr_transport_addr *addr,
                                              nsACString *host, int32_t *port);
}  // close namespace
#endif
