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
#include "nsXPCOM.h"
#include "runnable_utils.h"

extern "C" {
#include "nr_api.h"
#include "async_wait.h"
#include "nr_socket.h"
#include "nr_socket_local.h"
}
#include "nr_socket_prsock.h"

// Implement the nsISupports ref counting
namespace mozilla {

NS_IMPL_THREADSAFE_ISUPPORTS0(NrSocket)


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
  mPollFlags |= flag;

  return 0;
}

int NrSocket::cancel(int how) {
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

  mPollFlags &= ~flag;

  return 0;
}

void NrSocket::fire_callback(int how) {
  // This can't happen unless we are armed because we only set
  // the flags if we are armed
  MOZ_ASSERT(cbs_[how]);

  // Now cancel so that we need to be re-armed. Note that
  // the re-arming probably happens in the callback we are
  // about to fire.
  cancel(how);

  cbs_[how](this, how, cb_args_[how]);
}

// Helper functions for addresses
static int nr_transport_addr_to_praddr(nr_transport_addr *addr,
  PRNetAddr *naddr)
  {
    int _status;

    memset(naddr, 0, sizeof(*naddr));

    switch(addr->protocol){
      case IPPROTO_TCP:
        ABORT(R_INTERNAL); /* Can't happen for now */
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

int nr_netaddr_to_transport_addr(const net::NetAddr *netaddr,
  nr_transport_addr *addr)
  {
    int _status;
    int r;

    switch(netaddr->raw.family) {
      case AF_INET:
        if ((r = nr_ip4_port_to_transport_addr(ntohl(netaddr->inet.ip),
                                               ntohs(netaddr->inet.port),
                                               IPPROTO_UDP, addr)))
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
  nr_transport_addr *addr, int keep)
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
                                               IPPROTO_UDP, keep,
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

  if (!(fd_ = PR_NewUDPSocket())) {
    r_log(LOG_GENERIC,LOG_CRIT,"Couldn't create socket");
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

    if((r=nr_praddr_to_transport_addr(&naddr,&my_addr_,1)))
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

  // TODO: Convert flags?
  status = PR_SendTo(fd_, msg, len, flags, &naddr, PR_INTERVAL_NO_WAIT);
  if (status < 0 || (size_t)status != len) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
      ABORT(R_WOULDBLOCK);

    r_log_e(LOG_GENERIC, LOG_INFO, "Error in sendto %s", to->as_string);
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
    r_log_e(LOG_GENERIC,LOG_ERR,"Error in recvfrom");
    ABORT(R_IO_ERROR);
  }
  *len=status;

  if((r=nr_praddr_to_transport_addr(&nfrom,from,0)))
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

static nr_socket_vtbl nr_socket_local_vtbl={
  nr_socket_local_destroy,
  nr_socket_local_sendto,
  nr_socket_local_recvfrom,
  nr_socket_local_getfd,
  nr_socket_local_getaddr,
  nr_socket_local_close
};


int nr_socket_local_create(nr_transport_addr *addr, nr_socket **sockp) {
  NrSocket * sock = new NrSocket();
  int r, _status;

  r = sock->create(addr);
  if (r)
    ABORT(r);

  r = nr_socket_create_int(static_cast<void *>(sock), &nr_socket_local_vtbl, sockp);
  if (r)
    ABORT(r);

  // Add a reference so that we can delete it in destroy()
  sock->AddRef();

  _status =0;

abort:
  if (_status) {
    delete sock;
  }
  return _status;
}


static int nr_socket_local_destroy(void **objp) {
  if(!objp || !*objp)
    return 0;

  NrSocket *sock = static_cast<NrSocket *>(*objp);
  *objp=0;

  sock->close();  // Signal STS that we want not to listen
  sock->Release();  // Decrement the ref count

  return 0;
}

static int nr_socket_local_sendto(void *obj,const void *msg, size_t len,
                                  int flags, nr_transport_addr *addr) {
  NrSocket *sock = static_cast<NrSocket *>(obj);

  return sock->sendto(msg, len, flags, addr);
}

static int nr_socket_local_recvfrom(void *obj,void * restrict buf,
                                    size_t maxlen, size_t *len, int flags,
                                    nr_transport_addr *addr) {
  NrSocket *sock = static_cast<NrSocket *>(obj);

  return sock->recvfrom(buf, maxlen, len, flags, addr);
}

static int nr_socket_local_getfd(void *obj, NR_SOCKET *fd) {
  NrSocket *sock = static_cast<NrSocket *>(obj);

  *fd = sock;

  return 0;
}

static int nr_socket_local_getaddr(void *obj, nr_transport_addr *addrp) {
  NrSocket *sock = static_cast<NrSocket *>(obj);

  return sock->getaddr(addrp);
}


static int nr_socket_local_close(void *obj) {
  NrSocket *sock = static_cast<NrSocket *>(obj);

  sock->close();

  return 0;
}

// Implement async api
int NR_async_wait(NR_SOCKET sock, int how, NR_async_cb cb,void *cb_arg,
                  char *function,int line) {
  NrSocket *s = static_cast<NrSocket *>(sock);

  return s->async_wait(how, cb, cb_arg, function, line);
}

int NR_async_cancel(NR_SOCKET sock,int how) {
  NrSocket *s = static_cast<NrSocket *>(sock);

  return s->cancel(how);
}

