/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


// Original author: ekr@rtfm.com

// Some of this code is cut-and-pasted from nICEr. Copyright is:

/*
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

#include "nspr.h"
#include "prnetdb.h"

#include "mozilla/Assertions.h"

extern "C" {
#include "nr_api.h"
#include "async_timer.h"
#include "nr_resolver.h"
#include "transport_addr.h"
}

#include "nriceresolverfake.h"
#include "nr_socket_prsock.h"

namespace mozilla {

NrIceResolverFake::NrIceResolverFake() :
    vtbl_(new nr_resolver_vtbl), addrs_(), delay_ms_(100),
    allocated_resolvers_(0) {
  vtbl_->destroy = &NrIceResolverFake::destroy;
  vtbl_->resolve = &NrIceResolverFake::resolve;
  vtbl_->cancel = &NrIceResolverFake::cancel;
}

NrIceResolverFake::~NrIceResolverFake() {
  MOZ_ASSERT(allocated_resolvers_ == 0);
  delete vtbl_;
}


nr_resolver *NrIceResolverFake::AllocateResolver() {
  nr_resolver *resolver;

  int r = nr_resolver_create_int((void *)this,
                                 vtbl_, &resolver);
  MOZ_ASSERT(!r);
  if(r)
    return nullptr;

  ++allocated_resolvers_;

  return resolver;
}

void NrIceResolverFake::DestroyResolver() {
  --allocated_resolvers_;
}

int NrIceResolverFake::destroy(void **objp) {
  if (!objp || !*objp)
    return 0;

  NrIceResolverFake *fake = static_cast<NrIceResolverFake *>(*objp);
  *objp = nullptr;

  fake->DestroyResolver();

  return 0;
}

int NrIceResolverFake::resolve(void *obj,
                               nr_resolver_resource *resource,
                               int (*cb)(void *cb_arg,
                                         nr_transport_addr *addr),
                               void *cb_arg,
                               void **handle) {
  int r,_status;

  MOZ_ASSERT(obj);
  NrIceResolverFake *fake = static_cast<NrIceResolverFake *>(obj);

  MOZ_ASSERT(fake->allocated_resolvers_ > 0);

  PendingResolution *pending =
      new PendingResolution(fake,
                            resource->domain_name,
                            resource->port ? resource->port : 3478,
                            resource->transport_protocol ?
                            resource->transport_protocol :
                            IPPROTO_UDP,
                            resource->address_family,
                            cb, cb_arg);

  if ((r=NR_ASYNC_TIMER_SET(fake->delay_ms_,NrIceResolverFake::resolve_cb,
                            (void *)pending, &pending->timer_handle_))) {
    delete pending;
    ABORT(r);
  }
  *handle = pending;

  _status=0;
abort:
  return(_status);
}

void NrIceResolverFake::resolve_cb(NR_SOCKET s, int how, void *cb_arg) {
  MOZ_ASSERT(cb_arg);
  PendingResolution *pending = static_cast<PendingResolution *>(cb_arg);

  const PRNetAddr *addr=pending->resolver_->Resolve(pending->hostname_,
                                                    pending->address_family_);

  if (addr) {
    nr_transport_addr transport_addr;

    int r = nr_praddr_to_transport_addr(addr, &transport_addr,
                                        pending->transport_, 0);
    MOZ_ASSERT(!r);
    if (r)
      goto abort;

    r=nr_transport_addr_set_port(&transport_addr, pending->port_);
    MOZ_ASSERT(!r);
    if (r)
      goto abort;

    /* Fill in the address string */
    r=nr_transport_addr_fmt_addr_string(&transport_addr);
    MOZ_ASSERT(!r);
    if (r)
      goto abort;

    pending->cb_(pending->cb_arg_, &transport_addr);
    delete pending;
    return;
  }

abort:
  // Resolution failed.
  pending->cb_(pending->cb_arg_, nullptr);

  delete pending;
}

int NrIceResolverFake::cancel(void *obj, void *handle) {
  MOZ_ASSERT(obj);
  MOZ_ASSERT(static_cast<NrIceResolverFake *>(obj)->allocated_resolvers_ > 0);

  PendingResolution *pending = static_cast<PendingResolution *>(handle);

  NR_async_timer_cancel(pending->timer_handle_);
  delete pending;

  return(0);
}


}  // End of namespace mozilla
