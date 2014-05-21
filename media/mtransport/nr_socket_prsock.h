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

#include "databuffer.h"
#include "m_cpp_utils.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"

// Stub declaration for nICEr type
typedef struct nr_socket_vtbl_ nr_socket_vtbl;

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

   // Implementations of the async_event APIs
  virtual int async_wait(int how, NR_async_cb cb, void *cb_arg,
                         char *function, int line);
  virtual int cancel(int how);

  // nsISupport reference counted interface
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) = 0;

  uint32_t poll_flags() {
    return poll_flags_;
  }

  virtual nr_socket_vtbl *vtbl();  // To access in test classes.

  static TimeStamp short_term_violation_time();
  static TimeStamp long_term_violation_time();

protected:
  void fire_callback(int how);

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
  virtual ~NrSocket() {
    if (fd_)
      PR_Close(fd_);
  }

  // Implement nsASocket
  virtual void OnSocketReady(PRFileDesc *fd, int16_t outflags);
  virtual void OnSocketDetached(PRFileDesc *fd);
  virtual void IsLocal(bool *aIsLocal);
  virtual uint64_t ByteCountSent() { return 0; }
  virtual uint64_t ByteCountReceived() { return 0; }

  // nsISupports methods
  NS_DECL_THREADSAFE_ISUPPORTS

  // Implementations of the async_event APIs
  virtual int async_wait(int how, NR_async_cb cb, void *cb_arg,
                         char *function, int line);
  virtual int cancel(int how);


  // Implementations of the nr_socket APIs
  virtual int create(nr_transport_addr *addr); // (really init, but it's called create)
  virtual int sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to);
  virtual int recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from);
  virtual int getaddr(nr_transport_addr *addrp);
  virtual void close();
  virtual int connect(nr_transport_addr *addr);
  virtual int write(const void *msg, size_t len, size_t *written);
  virtual int read(void* buf, size_t maxlen, size_t *len);

private:
  DISALLOW_COPY_ASSIGN(NrSocket);

  PRFileDesc *fd_;
  nsCOMPtr<nsIEventTarget> ststhread_;
};

struct nr_udp_message {
  nr_udp_message(const PRNetAddr &from, nsAutoPtr<DataBuffer> &data)
      : from(from), data(data) {
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nr_udp_message);

  PRNetAddr from;
  nsAutoPtr<DataBuffer> data;

private:
  DISALLOW_COPY_ASSIGN(nr_udp_message);
};

class NrSocketIpc : public NrSocketBase,
                    public nsIUDPSocketInternal {
public:

  enum NrSocketIpcState {
    NR_INIT,
    NR_CONNECTING,
    NR_CONNECTED,
    NR_CLOSING,
    NR_CLOSED,
  };

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETINTERNAL

  NrSocketIpc(const nsCOMPtr<nsIEventTarget> &main_thread);
  virtual ~NrSocketIpc() {};

  // Implementations of the NrSocketBase APIs
  virtual int create(nr_transport_addr *addr);
  virtual int sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to);
  virtual int recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from);
  virtual int getaddr(nr_transport_addr *addrp);
  virtual void close();
  virtual int connect(nr_transport_addr *addr);
  virtual int write(const void *msg, size_t len, size_t *written);
  virtual int read(void* buf, size_t maxlen, size_t *len);

private:
  DISALLOW_COPY_ASSIGN(NrSocketIpc);

  // Main thread executors of the NrSocketBase APIs
  void create_m(const nsACString &host, const uint16_t port);
  void sendto_m(const net::NetAddr &addr, nsAutoPtr<DataBuffer> buf);
  void close_m();
  // STS thread executor
  void recv_callback_s(RefPtr<nr_udp_message> msg);

  bool err_;
  NrSocketIpcState state_;
  std::queue<RefPtr<nr_udp_message> > received_msgs_;

  nsCOMPtr<nsIUDPSocketChild> socket_child_;
  nsCOMPtr<nsIEventTarget> sts_thread_;
  const nsCOMPtr<nsIEventTarget> main_thread_;
  ReentrantMonitor monitor_;
};

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
