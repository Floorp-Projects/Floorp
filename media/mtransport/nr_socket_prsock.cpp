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
#include <string>

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
#include "mozilla/SyncRunnable.h"
#include "nsTArray.h"
#include "mozilla/dom/TCPSocketBinding.h"
#include "nsITCPSocketCallback.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsISocketFilter.h"

#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#endif

#if defined(MOZILLA_INTERNAL_API)
// csi_platform.h deep in nrappkit defines LOG_INFO and LOG_WARNING
#ifdef LOG_INFO
#define LOG_TEMP_INFO LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_WARNING
#define LOG_TEMP_WARNING LOG_WARNING
#undef LOG_WARNING
#endif
#if defined(LOG_DEBUG)
#define LOG_TEMP_DEBUG LOG_DEBUG
#undef LOG_DEBUG
#endif
#undef strlcpy

#include "mozilla/dom/network/TCPSocketChild.h"

#ifdef LOG_TEMP_INFO
#define LOG_INFO LOG_TEMP_INFO
#endif
#ifdef LOG_TEMP_WARNING
#define LOG_WARNING LOG_TEMP_WARNING
#endif

#ifdef LOG_TEMP_DEBUG
#define LOG_DEBUG LOG_TEMP_DEBUG
#endif
#ifdef XP_WIN
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
// cloned from csi_platform.h.  Win32 doesn't like how we hide symbols
#define LOG_DEBUG 7
#endif
#endif


extern "C" {
#include "nr_api.h"
#include "async_wait.h"
#include "nr_socket.h"
#include "nr_socket_local.h"
#include "stun_hint.h"
}
#include "nr_socket_prsock.h"
#include "simpletokenbucket.h"
#include "test_nr_socket.h"

// Implement the nsISupports ref counting
namespace mozilla {

#if defined(MOZILLA_INTERNAL_API)
class SingletonThreadHolder final
{
private:
  ~SingletonThreadHolder()
  {
    r_log(LOG_GENERIC,LOG_DEBUG,"Deleting SingletonThreadHolder");
    MOZ_ASSERT(!mThread, "SingletonThreads should be Released and shut down before exit!");
    if (mThread) {
      mThread->Shutdown();
      mThread = nullptr;
    }
  }

  DISALLOW_COPY_ASSIGN(SingletonThreadHolder);

public:
  // Must be threadsafe for StaticRefPtr/ClearOnShutdown
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SingletonThreadHolder)

  explicit SingletonThreadHolder(const nsCSubstring& aName)
    : mName(aName)
  {
    mParentThread = NS_GetCurrentThread();
  }

  nsIThread* GetThread() {
    return mThread;
  }

  /*
   * Keep track of how many instances are using a SingletonThreadHolder.
   * When no one is using it, shut it down
   */
  MozExternalRefCountType AddUse()
  {
    MOZ_ASSERT(mParentThread == NS_GetCurrentThread());
    MOZ_ASSERT(int32_t(mUseCount) >= 0, "illegal refcnt");
    nsrefcnt count = ++mUseCount;
    if (count == 1) {
      // idle -> in-use
      nsresult rv = NS_NewNamedThread(mName, getter_AddRefs(mThread));
      MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv) && mThread,
                         "Should successfully create mtransport I/O thread");
      r_log(LOG_GENERIC,LOG_DEBUG,"Created wrapped SingletonThread %p",
            mThread.get());
    }
    r_log(LOG_GENERIC,LOG_DEBUG,"AddUse: %lu", (unsigned long) count);
    return count;
  }

  MozExternalRefCountType ReleaseUse()
  {
    MOZ_ASSERT(mParentThread == NS_GetCurrentThread());
    nsrefcnt count = --mUseCount;
    MOZ_ASSERT(int32_t(mUseCount) >= 0, "illegal refcnt");
    if (count == 0) {
      // in-use -> idle -- no one forcing it to remain instantiated
      r_log(LOG_GENERIC,LOG_DEBUG,"Shutting down wrapped SingletonThread %p",
            mThread.get());
      mThread->Shutdown();
      mThread = nullptr;
      // It'd be nice to use a timer instead...  But be careful of
      // xpcom-shutdown-threads in that case
    }
    r_log(LOG_GENERIC,LOG_DEBUG,"ReleaseUse: %lu", (unsigned long) count);
    return count;
  }

private:
  nsCString mName;
  nsAutoRefCnt mUseCount;
  nsCOMPtr<nsIThread> mParentThread;
  nsCOMPtr<nsIThread> mThread;
};

static StaticRefPtr<SingletonThreadHolder> sThread;

static void ClearSingletonOnShutdown()
{
  ClearOnShutdown(&sThread);
}
#endif

static nsIThread* GetIOThreadAndAddUse_s()
{
  // Always runs on STS thread!
#if defined(MOZILLA_INTERNAL_API)
  // We need to safely release this on shutdown to avoid leaks
  if (!sThread) {
    sThread = new SingletonThreadHolder(NS_LITERAL_CSTRING("mtransport"));
    NS_DispatchToMainThread(mozilla::WrapRunnableNM(&ClearSingletonOnShutdown));
  }
  // Mark that we're using the shared thread and need it to stick around
  sThread->AddUse();
  return sThread->GetThread();
#else
  static nsCOMPtr<nsIThread> sThread;
  if (!sThread) {
    (void) NS_NewNamedThread("mtransport", getter_AddRefs(sThread));
  }
  return sThread;
#endif
}

NrSocketIpc::NrSocketIpc(nsIEventTarget *aThread)
  : io_thread_(aThread)
{}

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
  if (outflags & PR_POLL_READ & poll_flags())
    fire_callback(NR_ASYNC_WAIT_READ);
  if (outflags & PR_POLL_WRITE & poll_flags())
    fire_callback(NR_ASYNC_WAIT_WRITE);
  if (outflags & (PR_POLL_ERR | PR_POLL_NVAL | PR_POLL_HUP))
    // TODO: Bug 946423: how do we notify the upper layers about this?
    close();
}

void NrSocket::OnSocketDetached(PRFileDesc *fd) {
  r_log(LOG_GENERIC, LOG_DEBUG, "Socket %p detached", fd);
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
        naddr->ipv6.family = PR_AF_INET6;
        naddr->ipv6.port = addr->u.addr6.sin6_port;
        naddr->ipv6.flowinfo = addr->u.addr6.sin6_flowinfo;
        memcpy(&naddr->ipv6.ip, &addr->u.addr6.sin6_addr, sizeof(in6_addr));
        naddr->ipv6.scope_id = addr->u.addr6.sin6_scope_id;
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
        if ((r = nr_ip6_port_to_transport_addr((in6_addr *)&netaddr->inet6.ip.u8,
                                               ntohs(netaddr->inet6.port),
                                               protocol, addr)))
          ABORT(r);
        break;
      default:
        MOZ_ASSERT(false);
        ABORT(R_BAD_ARGS);
    }
    _status = 0;
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
    struct sockaddr_in6 ip6;

    switch(praddr->raw.family) {
      case PR_AF_INET:
        ip4.sin_family = PF_INET;
        ip4.sin_addr.s_addr = praddr->inet.ip;
        ip4.sin_port = praddr->inet.port;
        if ((r = nr_sockaddr_to_transport_addr((sockaddr *)&ip4,
                                               protocol, keep,
                                               addr)))
          ABORT(r);
        break;
      case PR_AF_INET6:
        ip6.sin6_family = PF_INET6;
        ip6.sin6_port = praddr->ipv6.port;
        ip6.sin6_flowinfo = praddr->ipv6.flowinfo;
        memcpy(&ip6.sin6_addr, &praddr->ipv6.ip, sizeof(in6_addr));
        ip6.sin6_scope_id = praddr->ipv6.scope_id;
        if ((r = nr_sockaddr_to_transport_addr((sockaddr *)&ip6,protocol,keep,addr)))
          ABORT(r);
        break;
      default:
        MOZ_ASSERT(false);
        ABORT(R_BAD_ARGS);
    }

    _status = 0;
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

  _status = 0;
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
      if (!(fd_ = PR_OpenUDPSocket(naddr.raw.family))) {
        r_log(LOG_GENERIC,LOG_CRIT,"Couldn't create UDP socket, "
              "family=%d, err=%d", naddr.raw.family, PR_GetError());
        ABORT(R_INTERNAL);
      }
#ifdef XP_WIN
      if (!mozilla::IsWin8OrLater()) {
        // Increase default send and receive buffer sizes on <= Win7 to be able to
        // receive and send an unpaced HD (>= 720p = 1280x720 - I Frame ~ 21K size)
        // stream without losing packets.
        // Manual testing showed that 100K buffer size was not enough and the
        // packet loss dis-appeared with 256K buffer size.
        // See bug 1252769 for future improvements of this.
        PRSize min_buffer_size = 256 * 1024;
        PRSocketOptionData opt_rcvbuf;
        opt_rcvbuf.option = PR_SockOpt_RecvBufferSize;
        if ((status = PR_GetSocketOption(fd_, &opt_rcvbuf)) == PR_SUCCESS) {
          if (opt_rcvbuf.value.recv_buffer_size < min_buffer_size) {
            opt_rcvbuf.value.recv_buffer_size = min_buffer_size;
            if ((status = PR_SetSocketOption(fd_, &opt_rcvbuf)) != PR_SUCCESS) {
              r_log(LOG_GENERIC, LOG_CRIT,
                "Couldn't set socket receive buffer size: %d", status);
            }
          } else {
            r_log(LOG_GENERIC, LOG_INFO,
              "Socket receive buffer size is already: %d",
              opt_rcvbuf.value.recv_buffer_size);
          }
        } else {
          r_log(LOG_GENERIC, LOG_CRIT,
            "Couldn't get socket receive buffer size: %d", status);
        }
        PRSocketOptionData opt_sndbuf;
        opt_sndbuf.option = PR_SockOpt_SendBufferSize;
        if ((status = PR_GetSocketOption(fd_, &opt_sndbuf)) == PR_SUCCESS) {
          if (opt_sndbuf.value.recv_buffer_size < min_buffer_size) {
            opt_sndbuf.value.recv_buffer_size = min_buffer_size;
            if ((status = PR_SetSocketOption(fd_, &opt_sndbuf)) != PR_SUCCESS) {
              r_log(LOG_GENERIC, LOG_CRIT,
                "Couldn't set socket send buffer size: %d", status);
            }
          } else {
            r_log(LOG_GENERIC, LOG_INFO,
              "Socket send buffer size is already: %d",
              opt_sndbuf.value.recv_buffer_size);
          }
        } else {
          r_log(LOG_GENERIC, LOG_CRIT,
            "Couldn't get socket send buffer size: %d", status);
        }
      }
#endif
      break;
    case IPPROTO_TCP:
      if (!(fd_ = PR_OpenTCPSocket(naddr.raw.family))) {
        r_log(LOG_GENERIC,LOG_CRIT,"Couldn't create TCP socket, "
              "family=%d, err=%d", naddr.raw.family, PR_GetError());
        ABORT(R_INTERNAL);
      }
      // Set ReuseAddr for TCP sockets to enable having several
      // sockets bound to same local IP and port
      PRSocketOptionData opt_reuseaddr;
      opt_reuseaddr.option = PR_SockOpt_Reuseaddr;
      opt_reuseaddr.value.reuse_addr = PR_TRUE;
      status = PR_SetSocketOption(fd_, &opt_reuseaddr);
      if (status != PR_SUCCESS) {
        r_log(LOG_GENERIC, LOG_CRIT,
          "Couldn't set reuse addr socket option: %d", status);
        ABORT(R_INTERNAL);
      }
      // And also set ReusePort for platforms supporting this socket option
      PRSocketOptionData opt_reuseport;
      opt_reuseport.option = PR_SockOpt_Reuseport;
      opt_reuseport.value.reuse_port = PR_TRUE;
      status = PR_SetSocketOption(fd_, &opt_reuseport);
      if (status != PR_SUCCESS) {
        if (PR_GetError() != PR_OPERATION_NOT_SUPPORTED_ERROR) {
          r_log(LOG_GENERIC, LOG_CRIT,
            "Couldn't set reuse port socket option: %d", status);
          ABORT(R_INTERNAL);
        }
      }
      // Try to speedup packet delivery by disabling TCP Nagle
      PRSocketOptionData opt_nodelay;
      opt_nodelay.option = PR_SockOpt_NoDelay;
      opt_nodelay.value.no_delay = PR_TRUE;
      status = PR_SetSocketOption(fd_, &opt_nodelay);
      if (status != PR_SUCCESS) {
        r_log(LOG_GENERIC, LOG_WARNING,
          "Couldn't set Nodelay socket option: %d", status);
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
  PRSocketOptionData opt_nonblock;
  opt_nonblock.option = PR_SockOpt_Nonblocking;
  opt_nonblock.value.non_blocking = PR_TRUE;
  status = PR_SetSocketOption(fd_, &opt_nonblock);
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
    r_log(LOG_GENERIC, LOG_CRIT, "Couldn't attach socket to STS, rv=%u",
          static_cast<unsigned>(rv));
    ABORT(R_INTERNAL);
  }

  _status = 0;

abort:
  return(_status);
}

static int ShouldDrop(size_t len) {
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
    return R_WOULDBLOCK;
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
    return R_WOULDBLOCK;
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
  return 0;
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

  if (nr_is_stun_request_message((UCHAR*)msg, len) && ShouldDrop(len)) {
    ABORT(R_WOULDBLOCK);
  }

  // TODO: Convert flags?
  status = PR_SendTo(fd_, msg, len, flags, &naddr, PR_INTERVAL_NO_WAIT);
  if (status < 0 || (size_t)status != len) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
      ABORT(R_WOULDBLOCK);

    r_log(LOG_GENERIC, LOG_INFO, "Error in sendto %s: %d",
          to->as_string, PR_GetError());
    ABORT(R_IO_ERROR);
  }

  _status = 0;
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
    r_log(LOG_GENERIC, LOG_INFO, "Error in recvfrom: %d", (int)PR_GetError());
    ABORT(R_IO_ERROR);
  }
  *len = status;

  if((r=nr_praddr_to_transport_addr(&nfrom,from,my_addr_.protocol,0)))
    ABORT(r);

  //r_log(LOG_GENERIC,LOG_DEBUG,"Read %d bytes from %s",*len,addr->as_string);

  _status = 0;
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
  int32_t connect_status, getsockname_status;

  if ((r=nr_transport_addr_to_praddr(addr, &naddr)))
    ABORT(r);

  if(!fd_)
    ABORT(R_EOD);

  // Note: this just means we tried to connect, not that we
  // are actually live.
  connect_invoked_ = true;
  connect_status = PR_Connect(fd_, &naddr, PR_INTERVAL_NO_WAIT);
  if (connect_status != PR_SUCCESS) {
    if (PR_GetError() != PR_IN_PROGRESS_ERROR)
      ABORT(R_IO_ERROR);
  }

  // If our local address is wildcard, then fill in the
  // address now.
  if(nr_transport_addr_is_wildcard(&my_addr_)){
    getsockname_status = PR_GetSockName(fd_, &naddr);
    if (getsockname_status != PR_SUCCESS){
      r_log(LOG_GENERIC, LOG_CRIT, "Couldn't get sock name for socket");
      ABORT(R_INTERNAL);
    }

    if((r=nr_praddr_to_transport_addr(&naddr,&my_addr_,addr->protocol,1)))
      ABORT(r);
  }

  // Now return the WOULDBLOCK if needed.
  if (connect_status != PR_SUCCESS) {
    ABORT(R_WOULDBLOCK);
  }

  _status = 0;
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
    r_log(LOG_GENERIC, LOG_INFO, "Error in write");
    ABORT(R_IO_ERROR);
  }

  *written = status;

  _status = 0;
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
    r_log(LOG_GENERIC, LOG_INFO, "Error in read");
    ABORT(R_IO_ERROR);
  }
  if (status == 0)
    ABORT(R_EOD);

  *len = (size_t)status;  // Guaranteed to be > 0
  _status = 0;
abort:
  return(_status);
}

int NrSocket::listen(int backlog) {
  ASSERT_ON_THREAD(ststhread_);
  int32_t status;
  int _status;

  assert(fd_);
  status = PR_Listen(fd_, backlog);
  if (status != PR_SUCCESS) {
    r_log(LOG_GENERIC, LOG_CRIT, "%s: PR_GetError() == %d",
          __FUNCTION__, PR_GetError());
    ABORT(R_IO_ERROR);
  }

  _status = 0;
abort:
  return(_status);
}

int NrSocket::accept(nr_transport_addr *addrp, nr_socket **sockp) {
  ASSERT_ON_THREAD(ststhread_);
  int _status, r;
  PRStatus status;
  PRFileDesc *prfd;
  PRNetAddr nfrom;
  NrSocket *sock=nullptr;
  nsresult rv;
  PRSocketOptionData opt_nonblock, opt_nodelay;
  nsCOMPtr<nsISocketTransportService> stservice =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    ABORT(R_INTERNAL);
  }

  if(!fd_)
    ABORT(R_EOD);

  prfd = PR_Accept(fd_, &nfrom, PR_INTERVAL_NO_WAIT);

  if (!prfd) {
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
      ABORT(R_WOULDBLOCK);

    ABORT(R_IO_ERROR);
  }

  sock = new NrSocket();

  sock->fd_=prfd;
  nr_transport_addr_copy(&sock->my_addr_, &my_addr_);

  if((r=nr_praddr_to_transport_addr(&nfrom, addrp, my_addr_.protocol, 0)))
    ABORT(r);

  // Set nonblocking
  opt_nonblock.option = PR_SockOpt_Nonblocking;
  opt_nonblock.value.non_blocking = PR_TRUE;
  status = PR_SetSocketOption(prfd, &opt_nonblock);
  if (status != PR_SUCCESS) {
    r_log(LOG_GENERIC, LOG_CRIT,
      "Failed to make accepted socket nonblocking: %d", status);
    ABORT(R_INTERNAL);
  }
  // Disable TCP Nagle
  opt_nodelay.option = PR_SockOpt_NoDelay;
  opt_nodelay.value.no_delay = PR_TRUE;
  status = PR_SetSocketOption(prfd, &opt_nodelay);
  if (status != PR_SUCCESS) {
    r_log(LOG_GENERIC, LOG_WARNING,
      "Failed to set Nodelay on accepted socket: %d", status);
  }

  // Should fail only with OOM
  if ((r=nr_socket_create_int(static_cast<void *>(sock), sock->vtbl(), sockp)))
    ABORT(r);

  // Remember our thread.
  sock->ststhread_ = do_QueryInterface(stservice, &rv);
  if (NS_FAILED(rv))
    ABORT(R_INTERNAL);

  // Finally, register with the STS
  rv = stservice->AttachSocket(prfd, sock);
  if (NS_FAILED(rv)) {
    ABORT(R_INTERNAL);
  }

  sock->connect_invoked_ = true;

  // Add a reference so that we can delete it in destroy()
  sock->AddRef();
  _status = 0;
abort:
  if (_status) {
    delete sock;
  }

  return(_status);
}

NS_IMPL_ISUPPORTS(NrUdpSocketIpcProxy, nsIUDPSocketInternal)

nsresult
NrUdpSocketIpcProxy::Init(const RefPtr<NrUdpSocketIpc>& socket)
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

NrUdpSocketIpcProxy::~NrUdpSocketIpcProxy()
{
  // Send our ref to STS to be released
  RUN_ON_THREAD(sts_thread_,
                mozilla::WrapRelease(socket_.forget()),
                NS_DISPATCH_NORMAL);
}

// IUDPSocketInternal interfaces
// callback while error happened in UDP socket operation
NS_IMETHODIMP NrUdpSocketIpcProxy::CallListenerError(const nsACString &message,
                                                     const nsACString &filename,
                                                     uint32_t line_number) {
  return socket_->CallListenerError(message, filename, line_number);
}

// callback while receiving UDP packet
NS_IMETHODIMP NrUdpSocketIpcProxy::CallListenerReceivedData(const nsACString &host,
                                                            uint16_t port,
                                                            const uint8_t *data,
                                                            uint32_t data_length) {
  return socket_->CallListenerReceivedData(host, port, data, data_length);
}

// callback while UDP socket is opened
NS_IMETHODIMP NrUdpSocketIpcProxy::CallListenerOpened() {
  return socket_->CallListenerOpened();
}

// callback while UDP socket is connected
NS_IMETHODIMP NrUdpSocketIpcProxy::CallListenerConnected() {
  return socket_->CallListenerConnected();
}

// callback while UDP socket is closed
NS_IMETHODIMP NrUdpSocketIpcProxy::CallListenerClosed() {
  return socket_->CallListenerClosed();
}

// NrUdpSocketIpc Implementation
NrUdpSocketIpc::NrUdpSocketIpc()
  : NrSocketIpc(GetIOThreadAndAddUse_s()),
    monitor_("NrUdpSocketIpc"),
    err_(false),
    state_(NR_INIT) {
}

NrUdpSocketIpc::~NrUdpSocketIpc()
{
  // also guarantees socket_child_ is released from the io_thread, and
  // tells the SingletonThreadHolder we're done with it

#if defined(MOZILLA_INTERNAL_API)
  // close(), but transfer the socket_child_ reference to die as well
  RUN_ON_THREAD(io_thread_,
                mozilla::WrapRunnableNM(&NrUdpSocketIpc::release_child_i,
                                        socket_child_.forget().take(),
                                        sts_thread_),
                NS_DISPATCH_NORMAL);
#endif
}

// IUDPSocketInternal interfaces
// callback while error happened in UDP socket operation
NS_IMETHODIMP NrUdpSocketIpc::CallListenerError(const nsACString &message,
                                                const nsACString &filename,
                                                uint32_t line_number) {
  ASSERT_ON_THREAD(io_thread_);

  r_log(LOG_GENERIC, LOG_ERR, "UDP socket error:%s at %s:%d this=%p",
        message.BeginReading(), filename.BeginReading(), line_number, (void*) this );

  ReentrantMonitorAutoEnter mon(monitor_);
  err_ = true;
  monitor_.NotifyAll();

  return NS_OK;
}

// callback while receiving UDP packet
NS_IMETHODIMP NrUdpSocketIpc::CallListenerReceivedData(const nsACString &host,
                                                       uint16_t port,
                                                       const uint8_t *data,
                                                       uint32_t data_length) {
  ASSERT_ON_THREAD(io_thread_);

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
                mozilla::WrapRunnable(RefPtr<NrUdpSocketIpc>(this),
                                      &NrUdpSocketIpc::recv_callback_s,
                                      msg),
                NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult NrUdpSocketIpc::SetAddress() {
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

  if (!nr_transport_addr_is_wildcard(&expected_addr) &&
      nr_transport_addr_cmp(&expected_addr, &my_addr_,
                            NR_TRANSPORT_ADDR_CMP_MODE_ADDR)) {
    err_ = true;
    MOZ_ASSERT(false, "Address of opened socket is not expected");
  }

  return NS_OK;
}

// callback while UDP socket is opened
NS_IMETHODIMP NrUdpSocketIpc::CallListenerOpened() {
  ASSERT_ON_THREAD(io_thread_);
  ReentrantMonitorAutoEnter mon(monitor_);

  r_log(LOG_GENERIC, LOG_DEBUG, "UDP socket opened this=%p", (void*) this);
  nsresult rv = SetAddress();
  if (NS_FAILED(rv)) {
    return rv;
  }

  mon.NotifyAll();

  return NS_OK;
}

// callback while UDP socket is connected
NS_IMETHODIMP NrUdpSocketIpc::CallListenerConnected() {
  ASSERT_ON_THREAD(io_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  r_log(LOG_GENERIC, LOG_DEBUG, "UDP socket connected this=%p", (void*) this);
  MOZ_ASSERT(state_ == NR_CONNECTED);

  nsresult rv = SetAddress();
  if (NS_FAILED(rv)) {
    mon.NotifyAll();
    return rv;
  }

  r_log(LOG_GENERIC, LOG_INFO, "Exit UDP socket connected");
  mon.NotifyAll();

  return NS_OK;
}

// callback while UDP socket is closed
NS_IMETHODIMP NrUdpSocketIpc::CallListenerClosed() {
  ASSERT_ON_THREAD(io_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  r_log(LOG_GENERIC, LOG_DEBUG, "UDP socket closed this=%p", (void*) this);
  MOZ_ASSERT(state_ == NR_CONNECTED || state_ == NR_CLOSING);
  state_ = NR_CLOSED;

  return NS_OK;
}

//
// NrSocketBase methods.
//
int NrUdpSocketIpc::create(nr_transport_addr *addr) {
  ASSERT_ON_THREAD(sts_thread_);

  int r, _status;
  nsresult rv;
  int32_t port;
  nsCString host;

  ReentrantMonitorAutoEnter mon(monitor_);

  if (state_ != NR_INIT) {
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

  // wildcard address will be resolved at NrUdpSocketIpc::CallListenerVoid
  if ((r=nr_transport_addr_copy(&my_addr_, addr))) {
    ABORT(r);
  }

  state_ = NR_CONNECTING;

  RUN_ON_THREAD(io_thread_,
                mozilla::WrapRunnable(RefPtr<NrUdpSocketIpc>(this),
                                      &NrUdpSocketIpc::create_i,
                                      host, static_cast<uint16_t>(port)),
                NS_DISPATCH_NORMAL);

  // Wait until socket creation complete.
  mon.Wait();

  if (err_) {
    close();
    ABORT(R_INTERNAL);
  }

  state_ = NR_CONNECTED;

  _status = 0;
abort:
  return(_status);
}

int NrUdpSocketIpc::sendto(const void *msg, size_t len, int flags,
                        nr_transport_addr *to) {
  ASSERT_ON_THREAD(sts_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  //If send err happened before, simply return the error.
  if (err_) {
    return R_IO_ERROR;
  }

  if (state_ != NR_CONNECTED) {
    return R_INTERNAL;
  }

  int r;
  net::NetAddr addr;
  if ((r=nr_transport_addr_to_netaddr(to, &addr))) {
    return r;
  }

  if (nr_is_stun_request_message((UCHAR*)msg, len) && ShouldDrop(len)) {
    return R_WOULDBLOCK;
  }

  nsAutoPtr<DataBuffer> buf(new DataBuffer(static_cast<const uint8_t*>(msg), len));

  RUN_ON_THREAD(io_thread_,
                mozilla::WrapRunnable(RefPtr<NrUdpSocketIpc>(this),
                                      &NrUdpSocketIpc::sendto_i,
                                      addr, buf),
                NS_DISPATCH_NORMAL);
  return 0;
}

void NrUdpSocketIpc::close() {
  r_log(LOG_GENERIC, LOG_DEBUG, "NrUdpSocketIpc::close()");

  ASSERT_ON_THREAD(sts_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);
  state_ = NR_CLOSING;

  RUN_ON_THREAD(io_thread_,
                mozilla::WrapRunnable(RefPtr<NrUdpSocketIpc>(this),
                                      &NrUdpSocketIpc::close_i),
                NS_DISPATCH_NORMAL);

  //remove all enqueued messages
  std::queue<RefPtr<nr_udp_message> > empty;
  std::swap(received_msgs_, empty);
}

int NrUdpSocketIpc::recvfrom(void *buf, size_t maxlen, size_t *len, int flags,
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

int NrUdpSocketIpc::getaddr(nr_transport_addr *addrp) {
  ASSERT_ON_THREAD(sts_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  if (state_ != NR_CONNECTED) {
    return R_INTERNAL;
  }

  return nr_transport_addr_copy(addrp, &my_addr_);
}

int NrUdpSocketIpc::connect(nr_transport_addr *addr) {
  int r,_status;
  int32_t port;
  nsCString host;

  ReentrantMonitorAutoEnter mon(monitor_);
  r_log(LOG_GENERIC, LOG_DEBUG, "NrUdpSocketIpc::connect(%s) this=%p", addr->as_string,
        (void*) this);

  if ((r=nr_transport_addr_get_addrstring_and_port(addr, &host, &port))) {
    ABORT(r);
  }

  RUN_ON_THREAD(io_thread_,
                mozilla::WrapRunnable(RefPtr<NrUdpSocketIpc>(this),
                                      &NrUdpSocketIpc::connect_i,
                                      host, static_cast<uint16_t>(port)),
                NS_DISPATCH_NORMAL);

  // Wait until connect() completes.
  mon.Wait();

  r_log(LOG_GENERIC, LOG_DEBUG, "NrUdpSocketIpc::connect this=%p completed err_ = %s",
        (void*) this, err_ ? "true" : "false");

  if (err_) {
    ABORT(R_INTERNAL);
  }

  _status = 0;
abort:
  return _status;
}

int NrUdpSocketIpc::write(const void *msg, size_t len, size_t *written) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

int NrUdpSocketIpc::read(void* buf, size_t maxlen, size_t *len) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

int NrUdpSocketIpc::listen(int backlog) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

int NrUdpSocketIpc::accept(nr_transport_addr *addrp, nr_socket **sockp) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

// IO thread executors
void NrUdpSocketIpc::create_i(const nsACString &host, const uint16_t port) {
  ASSERT_ON_THREAD(io_thread_);

  uint32_t minBuffSize = 0;
  nsresult rv;
  nsCOMPtr<nsIUDPSocketChild> socketChild = do_CreateInstance("@mozilla.org/udp-socket-child;1", &rv);
  if (NS_FAILED(rv)) {
    ReentrantMonitorAutoEnter mon(monitor_);
    err_ = true;
    MOZ_ASSERT(false, "Failed to create UDPSocketChild");
    return;
  }

  // This can spin the event loop; don't do that with the monitor held
  socketChild->SetBackgroundSpinsEvents();

  ReentrantMonitorAutoEnter mon(monitor_);
  if (!socket_child_) {
    socket_child_ = socketChild;
    socket_child_->SetFilterName(nsCString(NS_NETWORK_SOCKET_FILTER_HANDLER_STUN_SUFFIX));
  } else {
    socketChild = nullptr;
  }

  RefPtr<NrUdpSocketIpcProxy> proxy(new NrUdpSocketIpcProxy);
  rv = proxy->Init(this);
  if (NS_FAILED(rv)) {
    err_ = true;
    mon.NotifyAll();
    return;
  }

#ifdef XP_WIN
  if (!mozilla::IsWin8OrLater()) {
    // Increase default receive and send buffer size on <= Win7 to be able to
    // receive and send an unpaced HD (>= 720p = 1280x720 - I Frame ~ 21K size)
    // stream without losing packets.
    // Manual testing showed that 100K buffer size was not enough and the
    // packet loss dis-appeared with 256K buffer size.
    // See bug 1252769 for future improvements of this.
    minBuffSize = 256 * 1024;
  }
#endif
  // XXX bug 1126232 - don't use null Principal!
  if (NS_FAILED(socket_child_->Bind(proxy, nullptr, host, port,
                                    /* reuse = */ false,
                                    /* loopback = */ false,
                                    /* recv buffer size */ minBuffSize,
                                    /* send buffer size */ minBuffSize))) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to create UDP socket");
    mon.NotifyAll();
    return;
  }
}

void NrUdpSocketIpc::connect_i(const nsACString &host, const uint16_t port) {
  ASSERT_ON_THREAD(io_thread_);
  nsresult rv;
  ReentrantMonitorAutoEnter mon(monitor_);

  RefPtr<NrUdpSocketIpcProxy> proxy(new NrUdpSocketIpcProxy);
  rv = proxy->Init(this);
  if (NS_FAILED(rv)) {
    err_ = true;
    mon.NotifyAll();
    return;
  }

  if (NS_FAILED(socket_child_->Connect(proxy, host, port))) {
    err_ = true;
    MOZ_ASSERT(false, "Failed to connect UDP socket");
    mon.NotifyAll();
    return;
  }
}


void NrUdpSocketIpc::sendto_i(const net::NetAddr &addr, nsAutoPtr<DataBuffer> buf) {
  ASSERT_ON_THREAD(io_thread_);

  ReentrantMonitorAutoEnter mon(monitor_);

  if (!socket_child_) {
    MOZ_ASSERT(false);
    err_ = true;
    return;
  }
  if (NS_FAILED(socket_child_->SendWithAddress(&addr,
                                               buf->data(),
                                               buf->len()))) {
    err_ = true;
  }
}

void NrUdpSocketIpc::close_i() {
  ASSERT_ON_THREAD(io_thread_);

  if (socket_child_) {
    socket_child_->Close();
    socket_child_ = nullptr;
  }
}

#if defined(MOZILLA_INTERNAL_API)
// close(), but transfer the socket_child_ reference to die as well
// static
void NrUdpSocketIpc::release_child_i(nsIUDPSocketChild* aChild,
                                     nsCOMPtr<nsIEventTarget> sts_thread) {
  RefPtr<nsIUDPSocketChild> socket_child_ref =
    already_AddRefed<nsIUDPSocketChild>(aChild);
  if (socket_child_ref) {
    socket_child_ref->Close();
  }
  // Tell SingletonThreadHolder we're done with it
  RUN_ON_THREAD(sts_thread,
                mozilla::WrapRunnableNM(&NrUdpSocketIpc::release_use_s),
                NS_DISPATCH_NORMAL);
}

void NrUdpSocketIpc::release_use_s() {
  sThread->ReleaseUse();
}
#endif

void NrUdpSocketIpc::recv_callback_s(RefPtr<nr_udp_message> msg) {
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

#if defined(MOZILLA_INTERNAL_API)
// TCPSocket.
class NrTcpSocketIpc::TcpSocketReadyRunner: public Runnable
{
public:
  explicit TcpSocketReadyRunner(NrTcpSocketIpc *sck)
    : socket_(sck) {}

  NS_IMETHOD Run() override {
    socket_->maybe_post_socket_ready();
    return NS_OK;
  }

private:
  RefPtr<NrTcpSocketIpc> socket_;
};


NS_IMPL_ISUPPORTS(NrTcpSocketIpc,
                  nsITCPSocketCallback)

NrTcpSocketIpc::NrTcpSocketIpc(nsIThread* aThread)
  : NrSocketIpc(static_cast<nsIEventTarget*>(aThread)),
    mirror_state_(NR_INIT),
    state_(NR_INIT),
    buffered_bytes_(0),
    tracking_number_(0) {
}

NrTcpSocketIpc::~NrTcpSocketIpc()
{
  // also guarantees socket_child_ is released from the io_thread

  // close(), but transfer the socket_child_ reference to die as well
  RUN_ON_THREAD(io_thread_,
                mozilla::WrapRunnableNM(&NrTcpSocketIpc::release_child_i,
                                        socket_child_.forget().take(),
                                        sts_thread_),
                NS_DISPATCH_NORMAL);
}

//
// nsITCPSocketCallback methods
//
NS_IMETHODIMP NrTcpSocketIpc::UpdateReadyState(uint32_t aReadyState) {
  NrSocketIpcState temp = NR_INIT;
  switch (static_cast<dom::TCPReadyState>(aReadyState)) {
    case dom::TCPReadyState::Connecting:
      temp = NR_CONNECTING;
      break;
    case dom::TCPReadyState::Open:
      temp = NR_CONNECTED;
      break;
    case dom::TCPReadyState::Closing:
      temp = NR_CLOSING;
      break;
    case dom::TCPReadyState::Closed:
      temp = NR_CLOSED;
      break;
    default:
      MOZ_ASSERT(false, "Invalid ReadyState");
      return NS_OK;
  }
  if (mirror_state_ != temp) {
    mirror_state_ = temp;
    RUN_ON_THREAD(sts_thread_,
                  mozilla::WrapRunnable(RefPtr<NrTcpSocketIpc>(this),
                                        &NrTcpSocketIpc::update_state_s,
                                        temp),
                  NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

NS_IMETHODIMP NrTcpSocketIpc::UpdateBufferedAmount(uint32_t buffered_amount,
                                                   uint32_t tracking_number) {
  RUN_ON_THREAD(sts_thread_,
                mozilla::WrapRunnable(RefPtr<NrTcpSocketIpc>(this),
                                      &NrTcpSocketIpc::message_sent_s,
                                      buffered_amount,
                                      tracking_number),
                NS_DISPATCH_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP NrTcpSocketIpc::FireDataArrayEvent(const nsAString& aType,
                                                 const InfallibleTArray<uint8_t>& buffer) {
  // Called when we received data.
  uint8_t *buf = const_cast<uint8_t*>(buffer.Elements());

  nsAutoPtr<DataBuffer> data_buf(new DataBuffer(buf, buffer.Length()));
  RefPtr<nr_tcp_message> msg = new nr_tcp_message(data_buf);

  RUN_ON_THREAD(sts_thread_,
                mozilla::WrapRunnable(RefPtr<NrTcpSocketIpc>(this),
                                      &NrTcpSocketIpc::recv_message_s,
                                      msg),
                NS_DISPATCH_NORMAL);
  return NS_OK;
}

NS_IMETHODIMP NrTcpSocketIpc::FireErrorEvent(const nsAString &type,
                                             const nsAString &name) {
  r_log(LOG_GENERIC, LOG_ERR,
        "Error from TCPSocketChild: type: %s, name: %s",
        NS_LossyConvertUTF16toASCII(type).get(), NS_LossyConvertUTF16toASCII(name).get());
  socket_child_ = nullptr;

  mirror_state_ = NR_CLOSED;
  RUN_ON_THREAD(sts_thread_,
                mozilla::WrapRunnable(RefPtr<NrTcpSocketIpc>(this),
                                      &NrTcpSocketIpc::update_state_s,
                                      NR_CLOSED),
                NS_DISPATCH_NORMAL);

  return NS_OK;
}

// methods of nsITCPSocketCallback that we are not going to implement.

NS_IMETHODIMP NrTcpSocketIpc::FireDataStringEvent(const nsAString &type,
                                                  const nsACString &data) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP NrTcpSocketIpc::FireEvent(const nsAString &type) {
  // XXX support type.mData == 'close' at least
  return NS_ERROR_NOT_IMPLEMENTED;
}

//
// NrSocketBase methods.
//
int NrTcpSocketIpc::create(nr_transport_addr *addr) {
  int r, _status;
  nsresult rv;
  int32_t port;
  nsCString host;

  if (state_ != NR_INIT) {
    ABORT(R_INTERNAL);
  }

  sts_thread_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "Failed to get STS thread");
    ABORT(R_INTERNAL);
  }

  // Sanity check
  if ((r=nr_transport_addr_get_addrstring_and_port(addr, &host, &port))) {
    ABORT(r);
  }

  if ((r=nr_transport_addr_copy(&my_addr_, addr))) {
    ABORT(r);
  }

  _status = 0;
abort:
  return(_status);
}

int NrTcpSocketIpc::sendto(const void *msg, size_t len,
                           int flags, nr_transport_addr *to) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

int NrTcpSocketIpc::recvfrom(void * buf, size_t maxlen,
                             size_t *len, int flags,
                             nr_transport_addr *from) {
  MOZ_ASSERT(false);
  return R_INTERNAL;
}

int NrTcpSocketIpc::getaddr(nr_transport_addr *addrp) {
  ASSERT_ON_THREAD(sts_thread_);
  return nr_transport_addr_copy(addrp, &my_addr_);
}

void NrTcpSocketIpc::close() {
  ASSERT_ON_THREAD(sts_thread_);

  if (state_ == NR_CLOSED || state_ == NR_CLOSING) {
    return;
  }

  state_ = NR_CLOSING;

  RUN_ON_THREAD(io_thread_,
                mozilla::WrapRunnable(RefPtr<NrTcpSocketIpc>(this),
                                      &NrTcpSocketIpc::close_i),
                NS_DISPATCH_NORMAL);

  //remove all enqueued messages
  std::queue<RefPtr<nr_tcp_message>> empty;
  std::swap(msg_queue_, empty);
}

int NrTcpSocketIpc::connect(nr_transport_addr *addr) {
  nsCString remote_addr, local_addr;
  int32_t remote_port, local_port;
  int r, _status;
  if ((r=nr_transport_addr_get_addrstring_and_port(addr,
                                                   &remote_addr,
                                                   &remote_port))) {
    ABORT(r);
  }

  if ((r=nr_transport_addr_get_addrstring_and_port(&my_addr_,
                                                   &local_addr,
                                                   &local_port))) {
    MOZ_ASSERT(false); // shouldn't fail as it was sanity-checked in ::create()
    ABORT(r);
  }

  state_ = mirror_state_ = NR_CONNECTING;
  RUN_ON_THREAD(io_thread_,
                mozilla::WrapRunnable(RefPtr<NrTcpSocketIpc>(this),
                             &NrTcpSocketIpc::connect_i,
                             remote_addr,
                             static_cast<uint16_t>(remote_port),
                             local_addr,
                             static_cast<uint16_t>(local_port)),
                NS_DISPATCH_NORMAL);

  // Make caller wait for ready to write.
  _status = R_WOULDBLOCK;
 abort:
  return _status;
}

int NrTcpSocketIpc::write(const void *msg, size_t len, size_t *written) {
  ASSERT_ON_THREAD(sts_thread_);
  int _status = 0;
  if (state_ != NR_CONNECTED) {
    ABORT(R_FAILED);
  }

  if (buffered_bytes_ + len >= nsITCPSocketCallback::BUFFER_SIZE) {
    ABORT(R_WOULDBLOCK);
  }

  buffered_bytes_ += len;
  {
    InfallibleTArray<uint8_t>* arr = new InfallibleTArray<uint8_t>();
    arr->AppendElements(static_cast<const uint8_t*>(msg), len);
    // keep track of un-acknowleged writes by tracking number.
    writes_in_flight_.push_back(len);
    RUN_ON_THREAD(io_thread_,
                  mozilla::WrapRunnable(RefPtr<NrTcpSocketIpc>(this),
                                        &NrTcpSocketIpc::write_i,
                                        nsAutoPtr<InfallibleTArray<uint8_t>>(arr),
                                        ++tracking_number_),
                  NS_DISPATCH_NORMAL);
  }
  *written = len;
 abort:
  return _status;
}

int NrTcpSocketIpc::read(void* buf, size_t maxlen, size_t *len) {
  int _status = 0;
  if (state_ != NR_CONNECTED) {
    ABORT(R_FAILED);
  }

  if (msg_queue_.size() == 0) {
    ABORT(R_WOULDBLOCK);
  }

  {
    RefPtr<nr_tcp_message> msg(msg_queue_.front());
    size_t consumed_len = std::min(maxlen, msg->unread_bytes());
    memcpy(buf, msg->reading_pointer(), consumed_len);
    if (consumed_len < msg->unread_bytes()) {
      // There is still something left in buffer.
      msg->read_bytes += consumed_len;
    } else {
      msg_queue_.pop();
    }
    *len = consumed_len;
  }

 abort:
  return _status;
}

int NrTcpSocketIpc::listen(int backlog) {
  return R_INTERNAL;
}

int NrTcpSocketIpc::accept(nr_transport_addr *addrp, nr_socket **sockp) {
  return R_INTERNAL;
}

void NrTcpSocketIpc::connect_i(const nsACString &remote_addr,
                               uint16_t remote_port,
                               const nsACString &local_addr,
                               uint16_t local_port) {
  ASSERT_ON_THREAD(io_thread_);
  mirror_state_ = NR_CONNECTING;

  dom::TCPSocketChild* child = new dom::TCPSocketChild(NS_ConvertUTF8toUTF16(remote_addr), remote_port);
  socket_child_ = child;

  // Bug 1285330: put filtering back in here

  // XXX remove remote!
  socket_child_->SendWindowlessOpenBind(this,
                                        remote_addr, remote_port,
                                        local_addr, local_port,
                                        /* use ssl */ false,
                                        /* reuse addr port */ true);
}

void NrTcpSocketIpc::write_i(nsAutoPtr<InfallibleTArray<uint8_t>> arr,
                             uint32_t tracking_number) {
  ASSERT_ON_THREAD(io_thread_);
  if (!socket_child_) {
    return;
  }
  socket_child_->SendSendArray(*arr, tracking_number);
}

void NrTcpSocketIpc::close_i() {
  ASSERT_ON_THREAD(io_thread_);
  mirror_state_ = NR_CLOSING;
  if (!socket_child_) {
    return;
  }
  socket_child_->SendClose();
}

// close(), but transfer the socket_child_ reference to die as well
// static
void NrTcpSocketIpc::release_child_i(dom::TCPSocketChild* aChild,
                                     nsCOMPtr<nsIEventTarget> sts_thread) {
  RefPtr<dom::TCPSocketChild> socket_child_ref =
    already_AddRefed<dom::TCPSocketChild>(aChild);
  if (socket_child_ref) {
    socket_child_ref->SendClose();
  }
  // io_thread_ is MainThread, so no use to release
}

void NrTcpSocketIpc::message_sent_s(uint32_t buffered_amount,
                                    uint32_t tracking_number) {
  ASSERT_ON_THREAD(sts_thread_);

  size_t num_unacked_writes = tracking_number_ - tracking_number;
  while (writes_in_flight_.size() > num_unacked_writes) {
    writes_in_flight_.pop_front();
  }

  for (size_t unacked_write_len : writes_in_flight_) {
    buffered_amount += unacked_write_len;
  }

  r_log(LOG_GENERIC, LOG_ERR,
        "UpdateBufferedAmount: (tracking %u): %u, waiting: %s",
        tracking_number, buffered_amount,
        (poll_flags() & PR_POLL_WRITE) ? "yes" : "no");

  buffered_bytes_ = buffered_amount;
  maybe_post_socket_ready();
}

void NrTcpSocketIpc::recv_message_s(nr_tcp_message *msg) {
  ASSERT_ON_THREAD(sts_thread_);
  msg_queue_.push(msg);
  maybe_post_socket_ready();
}

void NrTcpSocketIpc::update_state_s(NrSocketIpcState next_state) {
  ASSERT_ON_THREAD(sts_thread_);
  // only allow valid transitions
  switch (state_) {
    case NR_CONNECTING:
      if (next_state == NR_CONNECTED) {
        state_ = NR_CONNECTED;
        maybe_post_socket_ready();
      } else {
        state_ = next_state; // all states are valid from CONNECTING
      }
      break;
    case NR_CONNECTED:
      if (next_state != NR_CONNECTING) {
        state_ = next_state;
      }
      break;
    case NR_CLOSING:
      if (next_state == NR_CLOSED) {
        state_ = next_state;
      }
      break;
    case NR_CLOSED:
      break;
    default:
      MOZ_CRASH("update_state_s while in illegal state");
  }
}

void NrTcpSocketIpc::maybe_post_socket_ready() {
  bool has_event = false;
  if (state_ == NR_CONNECTED) {
    if (poll_flags() & PR_POLL_WRITE) {
      // This effectively polls via the event loop until the
      // NR_ASYNC_WAIT_WRITE is no longer armed.
      if (buffered_bytes_ < nsITCPSocketCallback::BUFFER_SIZE) {
        r_log(LOG_GENERIC, LOG_INFO, "Firing write callback (%u)",
              (uint32_t)buffered_bytes_);
        fire_callback(NR_ASYNC_WAIT_WRITE);
        has_event = true;
      }
    }
    if (poll_flags() & PR_POLL_READ) {
      if (msg_queue_.size()) {
        r_log(LOG_GENERIC, LOG_INFO, "Firing read callback (%u)",
              (uint32_t)msg_queue_.size());
        fire_callback(NR_ASYNC_WAIT_READ);
        has_event = true;
      }
    }
  }

  // If any event has been posted, we post a runnable to see
  // if the events have to be posted again.
  if (has_event) {
    RefPtr<TcpSocketReadyRunner> runnable = new TcpSocketReadyRunner(this);
    NS_DispatchToCurrentThread(runnable);
  }
}
#endif

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
static int nr_socket_local_listen(void *obj, int backlog);
static int nr_socket_local_accept(void *obj, nr_transport_addr *addrp,
                                  nr_socket **sockp);

static nr_socket_vtbl nr_socket_local_vtbl={
  2,
  nr_socket_local_destroy,
  nr_socket_local_sendto,
  nr_socket_local_recvfrom,
  nr_socket_local_getfd,
  nr_socket_local_getaddr,
  nr_socket_local_connect,
  nr_socket_local_write,
  nr_socket_local_read,
  nr_socket_local_close,
  nr_socket_local_listen,
  nr_socket_local_accept
};

/* static */
int
NrSocketBase::CreateSocket(nr_transport_addr *addr, RefPtr<NrSocketBase> *sock)
{
  int r, _status;

  // create IPC bridge for content process
  if (XRE_IsParentProcess()) {
    *sock = new NrSocket();
  } else {
    switch (addr->protocol) {
      case IPPROTO_UDP:
        *sock = new NrUdpSocketIpc();
        break;
      case IPPROTO_TCP:
#if defined(MOZILLA_INTERNAL_API)
        {
          nsCOMPtr<nsIThread> main_thread;
          NS_GetMainThread(getter_AddRefs(main_thread));
          *sock = new NrTcpSocketIpc(main_thread.get());
        }
#else
        ABORT(R_REJECTED);
#endif
        break;
    }
  }

  r = (*sock)->create(addr);
  if (r)
    ABORT(r);

  _status = 0;
abort:
  if (_status) {
    *sock = nullptr;
  }
  return _status;
}

int nr_socket_local_create(void *obj, nr_transport_addr *addr, nr_socket **sockp) {
  RefPtr<NrSocketBase> sock;
  int r, _status;

  r = NrSocketBase::CreateSocket(addr, &sock);
  if (r) {
    ABORT(r);
  }

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
  *objp = 0;

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
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  return sock->write(msg, len, written);
}

static int nr_socket_local_read(void *obj, void * restrict buf, size_t maxlen,
                                size_t *len) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  return sock->read(buf, maxlen, len);
}

static int nr_socket_local_connect(void *obj, nr_transport_addr *addr) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  return sock->connect(addr);
}

static int nr_socket_local_listen(void *obj, int backlog) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  return sock->listen(backlog);
}

static int nr_socket_local_accept(void *obj, nr_transport_addr *addrp,
                                  nr_socket **sockp) {
  NrSocketBase *sock = static_cast<NrSocketBase *>(obj);

  return sock->accept(addrp, sockp);
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
