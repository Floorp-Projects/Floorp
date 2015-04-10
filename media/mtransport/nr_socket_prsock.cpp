/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
Modified version of nr_socket_local, adapted for NSPR
*/

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Original code from nICEr and nrappkit.

nICEr copyright:

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


nrappkit copyright:

   Copyright (C) 2001-2003, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


   ekr@rtfm.com  Thu Dec 20 20:14:49 2001
*/

#include <csi_platform.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>

#include "nspr.h"
#include "prerror.h"
#include "prio.h"
#include "prnetdb.h"

#include "mozilla/net/DNS.h"
#include "nsCOMPtr.h"
#include "nsASocketHandler.h"
#include "nsISocketTransportService.h"
#include "nsNetCID.h"
#include "nsISupportsImpl.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#include "runnable_utils.h"

extern "C" {
#include "nr_api.h"
#include "async_wait.h"
#include "nr_socket.h"
#include "nr_socket_local.h"
#include "stun_hint.h"
}
#include "nr_socket_prsock.h"
#include "simpletokenbucket.h"

// Implement the nsISupports ref counting
namespace mozilla {

static TimeStamp nr_socket_short_term_violation_time;
static TimeStamp nr_socket_long_term_violation_time;

TimeStamp NrSocketBase::short_term_violation_time() {
  return nr_socket_short_term_violation_time;
}

TimeStamp NrSocketBase::long_term_violation_time() {
  return nr_socket_long_term_violation_time;
}

// NrSocketBase implementation
// async_event APIs
int NrSocketBase::async_wait(int how, NR_async_cb cb, void *cb_arg,
                             char *function, int line) {
  uint16_t flag;

  switch (how) {
    case NR_ASYNC_WAIT_READ:
      flag = PR_POLL_READ;
      break;
    case NR_ASYNC_WAIT_WRITE:
      flag = PR_POLL_WRITE;
      break;
    default:
      return R_BAD_ARGS;
  }

  cbs_[how] = cb;
  cb_args_[how] = cb_arg;
  poll_flags_ |= flag;

  return 0;
}

int NrSocketBase::cancel(int how) {
  uint16_t flag;

  switch (how) {
    case NR_ASYNC_WAIT_READ:
      flag = PR_POLL_READ;
      break;
    case NR_ASYNC_WAIT_WRITE:
      flag = PR_POLL_WRITE;
      break;
    default:
      return R_BAD_ARGS;
  }

  poll_flags_ &= ~flag;

  return 0;
}

void NrSocketBase::fire_callback(int how) {
  // This can't happen unless we are armed because we only set
  // the flags if we are armed
  MOZ_ASSERT(cbs_[how]);

  // Now cancel so that we need to be re-armed. Note that
  // the re-arming probably happens in the callback we are
  // about to fire.
  cancel(how);

  cbs_[how](this, how, cb_args_[how]);
}

// NrSocket implementation
NS_IMPL_ISUPPORTS0(NrSocket)


// The nsASocket callbacks
void NrSocket::OnSocketReady(PRFileDesc *fd, int16_t outflags) {
  if (outflags & PR_POLL_READ)
    fire_callback(NR_ASYNC_WAIT_READ);
  if (outflags & PR_POLL_WRITE)
    fire_callback(NR_ASYNC_WAIT_WRITE);
}

void NrSocket::OnSocketDetached(PRFileDesc *fd) {
  ;  // TODO: Log?
}

void NrSocket::IsLocal(bool *aIsLocal) {
  // TODO(jesup): better check? Does it matter? (likely no)
  *aIsLocal = false;
}

// async_event APIs
int NrSocket::async_wait(int how, NR_async_cb cb, void *cb_arg,
                         char *function, int line) {
  int r = NrSocketBase::async_wait(how, cb, cb_arg, function, line);

  if (!r) {
    mPollFlags = poll_flags();
  }

  return r;
}

int NrSocket::cancel(int how) {
  int r = NrSocketBase::cancel(how);

  if (!r) {
    mPollFlags = poll_flags();
  }

  return r;
}

// Helper functions for addresses
static int nr_transport_addr_to_praddr(nr_transport_addr *addr,
  PRNetAddr *naddr)
  {
    int _status;

    memset(naddr, 0, sizeof(*naddr));

    switch(addr->protocol){
      case IPPROTO_TCP:
        break;
      case IPPROTO_UDP:
        break;
      default:
        ABORT(R_BAD_ARGS);
    }

    switch(addr->ip_version){
      case NR_IPV4:
        naddr->inet.family = PR_AF_INET;
        naddr->inet.port = addr->u.addr4.sin_port;
        naddr->inet.ip = addr->u.addr4.sin_addr.s_addr;
        break;
      case NR_IPV6:
#if 0
        naddr->ipv6.family = PR_AF_INET6;
        naddr->ipv6.port = addr->u.addr6.sin6_port;
#ifdef LINUX
        memcpy(naddr->ipv6.ip._S6_un._S6_u8,
               &addr->u.addr6.sin6_addr.__in6_u.__u6_addr8, 16);
#else
        memcpy(naddr->ipv6.ip._S6_un._S6_u8,
               &addr->u.addr6.sin6_addr.__u6_addr.__u6_addr8, 16);
#endif
#else
        // TODO: make IPv6 work
        ABORT(R_INTERNAL);
#endif
        break;
      default:
        ABORT(R_BAD_ARGS);
    }

    _status = 0;
  abort:
    return(_status);
  }

//XXX schien@mozilla.com: copy from PRNetAddrToNetAddr,
// should be removed after fix the link error in signaling_unittests
static int praddr_to_netaddr(const PRNetAddr *prAddr, net::NetAddr *addr)
{
  int _status;

  switch (prAddr->raw.family) {
    case PR_AF_INET:
      addr->inet.family = AF_INET;
      addr->inet.port = prAddr->inet.port;
      addr->inet.ip = prAddr->inet.ip;
      break;
    case PR_AF_INET6:
      addr->inet6.family = AF_INET6;
      addr->inet6.port = prAddr->ipv6.port;
      addr->inet6.flowinfo = prAddr->ipv6.flowinfo;
      memcpy(&addr->inet6.ip, &prAddr->ipv6.ip, sizeof(addr->inet6.ip.u8));
      addr->inet6.scope_id = prAddr->ipv6.scope_id;
      break;
    default:
      MOZ_ASSERT(false);
      ABORT(R_BAD_ARGS);
  }

  _status = 0;
abort:
  return(_status);
}

static int nr_transport_addr_to_netaddr(nr_transport_addr *addr,
  net::NetAddr *naddr)
{
  int r, _status;
  PRNetAddr praddr;

  if((r = nr_transport_addr_to_praddr(addr, &praddr))) {
    ABORT(r);
  }

  if((r = praddr_to_netaddr(&praddr, naddr))) {
    ABORT(r);
  }

  _status = 0;
abort:
  return(_status);
}

int nr_netaddr_to_transport_addr(const net::NetAddr *netaddr,
                                 nr_transport_addr *addr, int protocol)
  {
    int _status;
    int r;

    switch(netaddr->raw.family) {
      case AF_INET:
        if ((r = nr_ip4_port_to_transport_addr(ntohl(netaddr->inet.ip),
                                               ntohs(netaddr->inet.port),
                                               protocol, addr)))
          ABORT(r);
        break;
      case AF_INET6:
        ABORT(R_BAD_ARGS);
      default:
        MOZ_ASSERT(false);
        ABORT(R_BAD_ARGS);
    }
    _status=0;
  abort:
    return(_status);
  }

int nr_praddr_to_transport_addr(const PRNetAddr *praddr,
                                nr_transport_addr *addr, int protocol,
                                int keep)
  {
    int _status;
    int r;
    struct sockaddr_in ip4;

    switch(praddr->raw.family) {
      case PR_AF_INET:
        ip4.sin_family = PF_INET;
        ip4.sin_addr.s_addr = praddr->inet.ip;
        ip4.sin_port = praddr->inet.port;
        if ((r = nr_sockaddr_to_transport_addr((sockaddr *)&ip4,
                                               sizeof(ip4),
                                               protocol, keep,
                                               addr)))
          ABORT(r);
        break;
      case PR_AF_INET6:
#if 0
        r = nr_sockaddr_to_transport_addr((sockaddr *)&praddr->raw,
          sizeof(struct sockaddr_in6),IPPROTO_UDP,keep,addr);
        break;
#endif
        ABORT(R_BAD_ARGS);
      default:
        MOZ_ASSERT(false);
        ABORT(R_BAD_ARGS);
    }

    _status=0;
 abort:
    return(_status);
  }

/*
 * nr_transport_addr_get_addrstring_and_port
 * convert nr_transport_addr to IP address string and port number
 */
int nr_transport_addr_get_addrstring_and_port(nr_transport_addr *addr,
                                              nsACString *host, int32_t *port) {
  int r, _status;
  char addr_string[64];

  // We cannot directly use |nr_transport_addr.as_string| because it contains
  // more than ip address, therefore, we need to explicity convert it
  // from |nr_transport_addr_get_addrstring|.
  if ((r=nr_transport_addr_get_addrstring(addr, addr_string, sizeof(addr_string)))) {
    ABORT(r);
  }

  if ((r=nr_transport_addr_get_port(addr, port))) {
    ABORT(r);
  }

  *host = addr_string;

  _status=0;
abort:
  return(_status);
}

// nr_socket APIs (as member functions)
int NrSocket::create(nr_transport_addr *addr) {
  int r,_status;

  PRStatus status;
  PRNetAddr naddr;

  nsresult rv;
  nsCOMPtr<nsISocketTransportService> stservice =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (!NS_SUCCEEDED(rv)) {
    ABORT(R_INTERNAL);
  }

  if((r=nr_transport_addr_to_praddr(addr, &naddr)))
    ABORT(r);

  switch (addr->protocol) {
    case IPPROTO_UDP:
      if (!(fd_ = PR_NewUDPSocket())) {
        r_log(LOG_GENERIC,LOG_CRIT,"Couldn't create socket");
        ABORT(R_INTERNAL);
      }
      break;
    case IPPROTO_TCP:
      if (!(fd_ = PR_NewTCPSocket())) {
        r_log(LOG_GENERIC,LOG_CRIT,"Couldn't create socket");
        ABORT(R_INTERNAL);
      }
      break;
    default:
      ABORT(R_INTERNAL);
  }

  status = PR_Bind(fd_, &naddr);
  if (status != PR_SUCCESS) {
    r_log(LOG_GENERIC,LOG_CRIT,"Couldn't bind socket to address %s",
          addr->as_string);
    ABORT(R_INTERNAL);
  }

  r_log(LOG_GENERIC,LOG_DEBUG,"Creating socket %p with addr %s",
        fd_, addr->as_string);
  nr_transport_addr_copy(&my_addr_,addr);

  /* If we have a wildcard port, patch up the addr */
  if(nr_transport_addr_is_wildcard(addr)){
    status = PR_GetSockName(fd_, &naddr);
    if (status != PR_SUCCESS){
      r_log(LOG_GENERIC, LOG_CRIT, "Couldn't get sock name for socket");
      ABORT(R_INTERNAL);
    }

    if((r=nr_praddr_to_transport_addr(&naddr,&my_addr_,addr->protocol,1)))
      ABORT(r);
  }


  // Set nonblocking
  PRSocketOptionData option;
  option.option = PR_SockOpt_Nonblocking;
  option.value.non_blocking = PR_TRUE;
  status = PR_SetSocketOption(fd_, &option);
  if (status != PR_SUCCESS) {
    r_log(LOG_GENERIC, LOG_CRIT, "Couldn't make socket nonblocking");
    ABORT(R_INTERNAL);
  }

  // Remember our thread.
  ststhread_ = do_QueryInterface(stservice, &rv);
  if (!NS_SUCCEEDED(rv))
    ABORT(R_INTERNAL);

  // Finally, register with the STS
  rv = stservice->AttachSocket(fd_, this);
  if (!NS_SUCCEEDED(rv)) {
    ABORT(R_INTERNAL);
  }

  _status = 0;

abort:
  return(_status);
}

// This should be called on the STS thread.
int NrSocket::sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to) {
  ASSERT_ON_THREAD(ststhread_);
  int r,_status;
  PRNetAddr naddr;
  int32_t status;

  if ((r=nr_transport_addr_to_praddr(to, &naddr)))
    ABORT(r);

  if(fd_==nullptr)
    ABORT(R_EOD);

  if (nr_is_stun_request_message((UCHAR*)msg, len)) {
    // Global rate limiting for stun requests, to mitigate the ice hammer DoS
    // (see http://tools.ietf.org/html/draft-thomson-mmusic-ice-webrtc)

    // Tolerate rate of 8k/sec, for one second.
    static SimpleTokenBucket burst(16384*1, 16384);
    // Tolerate rate of 7.2k/sec over twenty seconds.
    static SimpleTokenBucket sustained(7372*20, 7372);

    // Check number of tokens in each bucket.
    if (burst.getTokens(UINT32_MAX) < len) {
      r_log(LOG_GENERIC, LOG_ERR,
                 "Short term global rate limit for STUN requests exceeded.");
#ifdef MOZILLA_INTERNAL_API
      nr_socket_short_term_violation_time = TimeStamp::Now();
#endif

// Bug 1013007
#if !EARLY_BETA_OR_EARLIER
      ABORT(R_WOULDBLOCK);
#else
      MOZ_ASSERT(false,
                 "Short term global rate limit for STUN requests exceeded. Go "
                 "bug bcampen@mozilla.com if you weren't intentionally "
                 "spamming ICE candidates, or don't know what that means.");
#endif
    }

    if (sustained.getTokens(UINT32_MAX) < len) {
      r_log(LOG_GENERIC, LOG_ERR,
                 "Long term global rate limit for STUN requests exceeded.");
#ifdef MOZILLA_INTERNAL_API
      nr_socket_long_term_violation_time = TimeStamp::Now();
#endif
// Bug 1013007
#if !EARLY_BETA_OR_EARLIER
      ABORT(R_WOULDBLOCK);
#else
      MOZ_ASSERT(false,
                 "Long term global rate limit for STUN requests exceeded. Go "
                 "bug bcampen@mozilla.com if you weren't intentionally "
                 "spamming ICE candidates, or don't know what that means.");
#endif
    }

    // Take len tokens from both buckets.
    // (not threadsafe, but no problem since this is only called from STS)
    burst.getTokens(len);
    sustained.getTokens(len);
  }

  // TODO: Convert flags?
  status = PR_SendTo(fd_, msg, len, flags, &naddr, PR_INTERVAL_NO_WAIT);
  if (status < 0 || (size_t)status != len) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
      ABORT(R_WOULDBLOCK);

    r_log(LOG_GENERIC, LOG_INFO, "Error in sendto %s", to->as_string);
    ABORT(R_IO_ERROR);
  }

  _status=0;
abort:
  return(_status);
}

int NrSocket::recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from) {
  ASSERT_ON_THREAD(ststhread_);
  int r,_status;
  PRNetAddr nfrom;
  int32_t status;

  status = PR_RecvFrom(fd_, buf, maxlen, flags, &nfrom, PR_INTERVAL_NO_WAIT);
  if (status <= 0) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
      ABORT(R_WOULDBLOCK);
    r_log(LOG_GENERIC, LOG_INFO, "Error in recvfrom");
    ABORT(R_IO_ERROR);
  }
  *len=status;

  if((r=nr_praddr_to_transport_addr(&nfrom,from,my_addr_.protocol,0)))
    ABORT(r);

  //r_log(LOG_GENERIC,LOG_DEBUG,"Read %d bytes from %s",*len,addr->as_string);

  _status=0;
abort:
  return(_status);
}

int NrSocket::getaddr(nr_transport_addr *addrp) {
  ASSERT_ON_THREAD(ststhread_);
  return nr_transport_addr_copy(addrp, &my_addr_);
}

// Close the socket so that the STS will detach and then kill it
void NrSocket::close() {
  ASSERT_ON_THREAD(ststhread_);
  mCondition = NS_BASE_STREAM_CLOSED;
}


int NrSocket::connect(nr_transport_addr *addr) {
  ASSERT_ON_THREAD(ststhread_);
  int r,_status;
  PRNetAddr naddr;
  int32_t status;

  if ((r=nr_transport_addr_to_praddr(addr, &naddr)))
    ABORT(r);

  if(!fd_)
    ABORT(R_EOD);

  // Note: this just means we tried to connect, not that we
  // are actually live.
  connect_invoked_ = true;
  status = PR_Connect(fd_, &naddr, PR_INTERVAL_NO_WAIT);

  if (status != PR_SUCCESS) {
    if (PR_GetError() == PR_IN_PROGRESS_ERROR)
      ABORT(R_WOULDBLOCK);

    ABORT(R_IO_ERROR);
  }

  _status=0;
abort:
  return(_status);
}


int NrSocket::write(const void *msg, size_t len, size_t *written) {
  ASSERT_ON_THREAD(ststhread_);
  int _status;
  int32_t status;

  if (!connect_invoked_)
    ABORT(R_FAILED);

  status = PR_Write(fd_, msg, len);
  if (status < 0) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
      ABORT(R_WOULDBLOCK);
    ABORT(R_IO_ERROR);
  }

  *written = status;

  _status=0;
abort:
  return _status;
}

int NrSocket::read(void* buf, size_t maxlen, size_t *len) {
  ASSERT_ON_THREAD(ststhread_);
  int _status;
  int32_t status;

  if (!connect_invoked_)
    ABORT(R_FAILED);

  status = PR_Read(fd_, buf, maxlen);
  if (status < 0) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
      ABORT(R_WOULDBLOCK);
    ABORT(R_IO_ERROR);
  }
  if (status == 0)
    ABORT(R_EOD);

  *len = (size_t)status;  // Guaranteed to be > 0
  _status = 0;
abort:
  return(_status);
}

NS_IMPL_ISUPPORTS(NrSocketIpcProxy, nsIUDPSocketInternal)

nsresult
NrSocketIpcProxy::Init(const nsRefPtr<NrSocketIpc>& socket)
{
  nsresult rv;
  sts_thread_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "Failed to get STS thread");
    return rv;
  }

  socket_ = socket;
  return NS_OK;
}

NrSocketIpcProxy::~NrSocketIpcProxy()
{
  // Send our ref to STS to be released
  RUN_ON_THREAD(sts_thread_,
                mozilla::WrapRelease(socket_.forget()),
                NS_DISPATCH_NORMAL);
}

// IUDPSocketInternal interfaces
// callback while error happened in UDP socket operation
NS_IMETHODIMP NrSocketIpcProxy::CallListenerError(const nsACString &message,
                                                  const nsACString &filename,
                                                  uint32_t line_number) {
  return socket_->CallListenerError(message, filename, line_number);
}

// callback while receiving UDP packet
NS_IMETHODIMP NrSocketIpcProxy::CallListenerReceivedData(const nsACString &host,
                                                         uint16_t port,
                                                         const uint8_t *data,
                                                         uint32_t data_length) {
  return socket_->CallListenerReceivedData(host, port, data, data_length);
}

// callback while UDP socket is opened
NS_IMETHODIMP NrSocketIpcProxy::CallListenerOpened() {
  return socket_->CallListenerOpened();
}

// callback while UDP socket is closed
NS_IMETHODIMP NrSocketIpcProxy::CallListenerClosed() {
  return socket_->CallListenerClosed();
}

// NrSocketIpc Implementation
NrSocketIpc::NrSocketIpc(const nsCOMPtr<nsIEventTarget> &main_thread)
    : err_(false),
      state_(NR_INIT),
      main_thread_(main_thread),
      monitor_("NrSocketIpc") {
}

// IUDPSocketInternal interfaces
// callback while error happened in UDP socket operation
NS_IMETHODIMP NrSocketIpc::CallListenerError(const nsACString &message,
                                             const nsACString &filename,
                                             uint32_t line_number) {
  ASSERT_ON_THREAD(main_thread_);

  r_log(LOG_GENERIC, LOG_ERR, "UDP socket error:%s at %s:%d",
        message.BeginReading(), filename.BeginReading(), line_number );

  ReentrantMonitorAutoEnter mon(monitor_);
  err_ = true;
  monitor_.NotifyAll();

  return NS_OK;
}

// callback while receiving UDP packet
NS_IMETHODIMP NrSocketIpc::CallListenerReceivedData(const nsACString &host,
                                                    uint16_t port,
                                                    const uint8_t *data,
                                                    uint32_t data_length) {
  ASSERT_ON_THREAD(main_thread_);

  PRNetAddr addr;
  memset(&addr, 0, sizeof(addr));

  {
    ReentrantMonitorAutoEnter mon(monitor_);

    if (PR_SUCCESS != PR_StringToNetAddr(host.BeginReading(), &addr)) {
      err_ = true;
      MOZ_ASSERT(false, "Failed to convert remote host to PRNetAddr");
      return NS_OK;
    }

    // Use PR_IpAddrNull to avoid address being reset to 0.
    if (PR_SUCCESS != PR_SetNetAddr(PR_IpAddrNull, addr.raw.family, port, &addr)) {
      err_ = true;
      MOZ_ASSERT(false, "Failed to set port in PRNetAddr");
      return NS_OK;
    }
  }

  nsAutoPtr<DataBuffer> buf(new DataBuffer(data, data_length));
  RefPtr<nr_udp_message> msg(new nr_udp_message(addr, buf));

  RUN_ON_THREAD(sts_thread_,
                mozilla::WrapRunnable(nsRefPtr<NrSocketIpc>(this),
                                      &NrSocketIpc::recv_callback_s,
                                      msg),
                NS_DISPATCH_NORMAL);
  return NS_OK;
}

// callback while UDP socket is opened
NS_IMETHODIMP NrSocketIpc::CallListenerOpened() {
  ASSERT_ON_THREAD(main_thread_);
  ReentrantMonitorAutoEnter mon(monitor_);

  uint16_t port;
  if (NS_FAILED(socket_child_->GetLocalPort(&port))) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to get local port");
    return NS_OK;
  }

  nsAutoCString address;
  if(NS_FAILED(socket_child_->GetLocalAddress(address))) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to get local address");
    return NS_OK;
  }

  PRNetAddr praddr;
  if (PR_SUCCESS != PR_InitializeNetAddr(PR_IpAddrAny, port, &praddr)) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to set port in PRNetAddr");
    return NS_OK;
  }

  if (PR_SUCCESS != PR_StringToNetAddr(address.BeginReading(), &praddr)) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to convert local host to PRNetAddr");
    return NS_OK;
  }

  nr_transport_addr expected_addr;
  if(nr_transport_addr_copy(&expected_addr, &my_addr_)) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to copy my_addr_");
  }

  if (nr_praddr_to_transport_addr(&praddr, &my_addr_, IPPROTO_UDP, 1)) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to copy local host to my_addr_");
  }

  if (nr_transport_addr_cmp(&expected_addr, &my_addr_,
                            NR_TRANSPORT_ADDR_CMP_MODE_ADDR)) {
    err_ = true;
    MOZ_ASSERT(false, "Address of opened socket is not expected");
  }

  mon.NotifyAll();

  return NS_OK;
}

// callback while UDP socket is closed
NS_IMETHODIMP NrSocketIpc::CallListenerClosed() {
  ASSERT_ON_THREAD(main_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  MOZ_ASSERT(state_ == NR_CONNECTED || state_ == NR_CLOSING);
  state_ = NR_CLOSED;

  return NS_OK;
}

// nr_socket public APIs
int NrSocketIpc::create(nr_transport_addr *addr) {
  ASSERT_ON_THREAD(sts_thread_);

  int r, _status;
  nsresult rv;
  int32_t port;
  nsCString host;

  ReentrantMonitorAutoEnter mon(monitor_);

  if (state_ != NR_INIT) {
    ABORT(R_INTERNAL);
  }

  // Bug 950660: Remote TCP socket is not supported yet.
  if (NS_WARN_IF(addr->protocol != IPPROTO_UDP)) {
    MOZ_ASSERT(false, "NrSocket over TCP is not e10s ready, see Bug 950660");
    ABORT(R_INTERNAL);
  }

  sts_thread_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "Failed to get STS thread");
    ABORT(R_INTERNAL);
  }

  if ((r=nr_transport_addr_get_addrstring_and_port(addr, &host, &port))) {
    ABORT(r);
  }

  // wildcard address will be resolved at NrSocketIpc::CallListenerVoid
  if ((r=nr_transport_addr_copy(&my_addr_, addr))) {
    ABORT(r);
  }

  state_ = NR_CONNECTING;

  RUN_ON_THREAD(main_thread_,
                mozilla::WrapRunnable(nsRefPtr<NrSocketIpc>(this),
                                      &NrSocketIpc::create_m,
                                      host, static_cast<uint16_t>(port)),
                NS_DISPATCH_NORMAL);

  // Wait until socket creation complete.
  mon.Wait();

  if (err_) {
    ABORT(R_INTERNAL);
  }

  state_ = NR_CONNECTED;

  _status = 0;
abort:
  return(_status);
}

int NrSocketIpc::sendto(const void *msg, size_t len, int flags,
                        nr_transport_addr *to) {
  ASSERT_ON_THREAD(sts_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  //If send err happened before, simply return the error.
  if (err_) {
    return R_IO_ERROR;
  }

  if (!socket_child_) {
    return R_EOD;
  }

  if (state_ != NR_CONNECTED) {
    return R_INTERNAL;
  }

  int r;
  net::NetAddr addr;
  if ((r=nr_transport_addr_to_netaddr(to, &addr))) {
    return r;
  }

  nsAutoPtr<DataBuffer> buf(new DataBuffer(static_cast<const uint8_t*>(msg), len));

  RUN_ON_THREAD(main_thread_,
                mozilla::WrapRunnable(nsRefPtr<NrSocketIpc>(this),
                                      &NrSocketIpc::sendto_m,
                                      addr, buf),
                NS_DISPATCH_NORMAL);
  return 0;
}

void NrSocketIpc::close() {
  ASSERT_ON_THREAD(sts_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);
  state_ = NR_CLOSING;

  RUN_ON_THREAD(main_thread_,
                mozilla::WrapRunnable(nsRefPtr<NrSocketIpc>(this),
                                      &NrSocketIpc::close_m),
                NS_DISPATCH_NORMAL);

  //remove all enqueued messages
  std::queue<RefPtr<nr_udp_message> > empty;
  std::swap(received_msgs_, empty);
}

int NrSocketIpc::recvfrom(void *buf, size_t maxlen, size_t *len, int flags,
                          nr_transport_addr *from) {
  ASSERT_ON_THREAD(sts_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  int r, _status;
  uint32_t consumed_len;

  *len = 0;

  if (state_ != NR_CONNECTED) {
    ABORT(R_INTERNAL);
  }

  if (received_msgs_.empty()) {
    ABORT(R_WOULDBLOCK);
  }

  {
    RefPtr<nr_udp_message> msg(received_msgs_.front());

    received_msgs_.pop();

    if ((r=nr_praddr_to_transport_addr(&msg->from, from, IPPROTO_UDP, 0))) {
      err_ = true;
      MOZ_ASSERT(false, "Get bogus address for received UDP packet");
      ABORT(r);
    }

    consumed_len = std::min(maxlen, msg->data->len());
    if (consumed_len < msg->data->len()) {
      r_log(LOG_GENERIC, LOG_DEBUG, "Partial received UDP packet will be discard");
    }

    memcpy(buf, msg->data->data(), consumed_len);
    *len = consumed_len;
  }

  _status = 0;
abort:
  return(_status);
}

int NrSocketIpc::getaddr(nr_transport_addr *addrp) {
  ASSERT_ON_THREAD(sts_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  if (state_ != NR_CONNECTED) {
    return R_INTERNAL;
  }

  return nr_transport_addr_copy(addrp, &my_addr_);
}

int NrSocketIpc::connect(nr_transport_addr *addr) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

int NrSocketIpc::write(const void *msg, size_t len, size_t *written) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

int NrSocketIpc::read(void* buf, size_t maxlen, size_t *len) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

// Main thread executors
void NrSocketIpc::create_m(const nsACString &host, const uint16_t port) {
  ASSERT_ON_THREAD(main_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  nsresult rv;
  nsCOMPtr<nsIUDPSocketChild> socketChild = do_CreateInstance("@mozilla.org/udp-socket-child;1", &rv);
  if (NS_FAILED(rv)) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to create UDPSocketChild");
    mon.NotifyAll();
    return;
  }

  socket_child_ = new nsMainThreadPtrHolder<nsIUDPSocketChild>(socketChild);
  socket_child_->SetFilterName(nsCString("stun"));

  nsRefPtr<NrSocketIpcProxy> proxy(new NrSocketIpcProxy);
  rv = proxy->Init(this);
  if (NS_FAILED(rv)) {
    err_ = true;
    mon.NotifyAll();
    return;
  }

  if (NS_FAILED(socket_child_->Bind(proxy, nullptr, host, port,
                                    /* reuse = */ false,
                                    /* loopback = */ false))) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to create UDP socket");
    mon.NotifyAll();
    return;
  }
}

void NrSocketIpc::sendto_m(const net::NetAddr &addr, nsAutoPtr<DataBuffer> buf) {
  ASSERT_ON_THREAD(main_thread_);

  MOZ_ASSERT(socket_child_);

  ReentrantMonitorAutoEnter mon(monitor_);

  if (NS_FAILED(socket_child_->SendWithAddress(&addr,
                                               buf->data(),
                                               buf->len()))) {
    err_ = true;
  }
}

void NrSocketIpc::close_m() {
  ASSERT_ON_THREAD(main_thread_);

  if (socket_child_) {
    socket_child_->Close();
    socket_child_ = nullptr;
  }
}

void NrSocketIpc::recv_callback_s(RefPtr<nr_udp_message> msg) {
  ASSERT_ON_THREAD(sts_thread_);

  {
    ReentrantMonitorAutoEnter mon(monitor_);
    if (state_ != NR_CONNECTED) {
      return;
    }
  }

  //enqueue received message
  received_msgs_.push(msg);

  if ((poll_flags() & PR_POLL_READ)) {
    fire_callback(NR_ASYNC_WAIT_READ);
  }
}

}  // close namespace


using namespace mozilla;

// Bridge to the nr_socket interface
static int nr_socket_local_destroy(void **objp);
static int nr_socket_local_sendto(void *obj,const void *msg, size_t len,
                                  int flags, nr_transport_addr *to);
static int nr_socket_local_recvfrom(void *obj,void * restrict buf,
  size_t maxlen, size_t *len, int flags, nr_transport_addr *from);
static int nr_socket_local_getfd(void *obj, NR_SOCKET *fd);
static int nr_socket_local_getaddr(void *obj, nr_transport_addr *addrp);
static int nr_socket_local_close(void *obj);
static int nr_socket_local_connect(void *sock, nr_transport_addr *addr);
static int nr_socket_local_write(void *obj,const void *msg, size_t len,
                                 size_t *written);
static int nr_socket_local_read(void *obj,void * restrict buf, size_t maxlen,
                                size_t *len);

static nr_socket_vtbl nr_socket_local_vtbl={
  1,
  nr_socket_local_destroy,
  nr_socket_local_sendto,
  nr_socket_local_recvfrom,
  nr_socket_local_getfd,
  nr_socket_local_getaddr,
  nr_socket_local_connect,
  nr_socket_local_write,
  nr_socket_local_read,
  nr_socket_local_close
};

int nr_socket_local_create(nr_transport_addr *addr, nr_socket **sockp) {
  RefPtr<NrSocketBase> sock;

  // create IPC bridge for content process
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    sock = new NrSocket();
  } else {
    nsCOMPtr<nsIThread> main_thread;
    NS_GetMainThread(getter_AddRefs(main_thread));
    sock = new NrSocketIpc(main_thread.get());
  }

  int r, _status;

  r = sock->create(addr);
  if (r)
    ABORT(r);

  r = nr_socket_create_int(static_cast<void *>(sock),
                           sock->vtbl(), sockp);
  if (r)
    ABORT(r);

  _status = 0;

  {
    // We will release this reference in destroy(), not exactly the normal
    // ownership model, but it is what it is.
    NrSocketBase* dummy = sock.forget().take();
    (void)dummy;
  }

abort:
  return _status;
}


static int nr_socket_local_destroy(void **objp) {
  if(!objp || !*objp)
    return 0;

  NrSocketBase *sock = static_cast<NrSocketBase *>(*objp);
  *objp=0;

  sock->close();  // Signal STS that we want not to listen
  sock->Release();  // Decrement the ref count

  return 0;
}

static int nr_socket_local_sendto(void *obj,const void *msg, size_t len,
                                  int flags, nr_transport_addr *addr) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  return sock->sendto(msg, len, flags, addr);
}

static int nr_socket_local_recvfrom(void *obj,void * restrict buf,
                                    size_t maxlen, size_t *len, int flags,
                                    nr_transport_addr *addr) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  return sock->recvfrom(buf, maxlen, len, flags, addr);
}

static int nr_socket_local_getfd(void *obj, NR_SOCKET *fd) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  *fd = sock;

  return 0;
}

static int nr_socket_local_getaddr(void *obj, nr_transport_addr *addrp) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  return sock->getaddr(addrp);
}


static int nr_socket_local_close(void *obj) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  sock->close();

  return 0;
}

static int nr_socket_local_write(void *obj, const void *msg, size_t len,
                                 size_t *written) {
  NrSocket *sock = static_cast<NrSocket *>(obj);

  return sock->write(msg, len, written);
}

static int nr_socket_local_read(void *obj, void * restrict buf, size_t maxlen,
                                size_t *len) {
  NrSocket *sock = static_cast<NrSocket *>(obj);

  return sock->read(buf, maxlen, len);
}

static int nr_socket_local_connect(void *obj, nr_transport_addr *addr) {
  NrSocket *sock = static_cast<NrSocket *>(obj);

  return sock->connect(addr);
}

// Implement async api
int NR_async_wait(NR_SOCKET sock, int how, NR_async_cb cb,void *cb_arg,
                  char *function,int line) {
  NrSocketBase *s = static_cast<NrSocketBase *>(sock);

  return s->async_wait(how, cb, cb_arg, function, line);
}

int NR_async_cancel(NR_SOCKET sock,int how) {
  NrSocketBase *s = static_cast<NrSocketBase *>(sock);

  return s->cancel(how);
}

nr_socket_vtbl* NrSocketBase::vtbl() {
  return &nr_socket_local_vtbl;
}

