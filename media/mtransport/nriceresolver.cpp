/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


// Original authors: jib@mozilla.com, ekr@rtfm.com

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

#include "logging.h"
#include "nspr.h"
#include "prnetdb.h"

#include "mozilla/Assertions.h"

extern "C" {
#include "nr_api.h"
#include "async_timer.h"
#include "nr_resolver.h"
#include "transport_addr.h"
}

#include "mozilla/net/DNS.h" // TODO(jib@mozilla.com) down here because bug 848578
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIDNSService.h"
#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"
#include "nriceresolver.h"
#include "nr_socket_prsock.h"
#include "mtransport/runnable_utils.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

NrIceResolver::NrIceResolver() :
    vtbl_(new nr_resolver_vtbl())
#ifdef DEBUG
    , allocated_resolvers_(0)
#endif
{
  vtbl_->destroy = &NrIceResolver::destroy;
  vtbl_->resolve = &NrIceResolver::resolve;
  vtbl_->cancel = &NrIceResolver::cancel;
}

NrIceResolver::~NrIceResolver() {
  MOZ_ASSERT(!allocated_resolvers_);
  delete vtbl_;
}

nsresult NrIceResolver::Init() {
  nsresult rv;

  sts_thread_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  dns_ = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Could not acquire DNS service");
  }
  return rv;
}

nr_resolver *NrIceResolver::AllocateResolver() {
  nr_resolver *resolver;

  int r = nr_resolver_create_int((void *)this, vtbl_, &resolver);
  MOZ_ASSERT(!r);
  if(r) {
    MOZ_MTLOG(ML_ERROR, "nr_resolver_create_int failed");
    return nullptr;
  }
  // We must be available to allocators until they all call DestroyResolver,
  // because allocators may (and do) outlive the originator of NrIceResolver.
  AddRef();
#ifdef DEBUG
  ++allocated_resolvers_;
#endif
  return resolver;
}

void NrIceResolver::DestroyResolver() {
#ifdef DEBUG
  --allocated_resolvers_;
#endif
  // Undoes Addref in AllocateResolver so the NrIceResolver can be freed.
  Release();
}

int NrIceResolver::destroy(void **objp) {
  if (!objp || !*objp)
    return 0;
  NrIceResolver *resolver = static_cast<NrIceResolver *>(*objp);
  *objp = 0;
  resolver->DestroyResolver();
  return 0;
}

int NrIceResolver::resolve(void *obj,
                           nr_resolver_resource *resource,
                           int (*cb)(void *cb_arg, nr_transport_addr *addr),
                           void *cb_arg,
                           void **handle) {
  MOZ_ASSERT(obj);
  return static_cast<NrIceResolver *>(obj)->resolve(resource, cb, cb_arg, handle);
}

int NrIceResolver::resolve(nr_resolver_resource *resource,
                           int (*cb)(void *cb_arg, nr_transport_addr *addr),
                           void *cb_arg,
                           void **handle) {
  int _status;
  MOZ_ASSERT(allocated_resolvers_ > 0);
  ASSERT_ON_THREAD(sts_thread_);
  nsCOMPtr<PendingResolution> pr;

  if (resource->transport_protocol != IPPROTO_UDP) {
    MOZ_MTLOG(ML_ERROR, "Only UDP is supported.");
    ABORT(R_NOT_FOUND);
  }
  pr = new PendingResolution(sts_thread_, resource->port? resource->port : 3478,
                             cb, cb_arg);
  if (NS_FAILED(dns_->AsyncResolve(nsAutoCString(resource->domain_name),
                                   nsIDNSService::RESOLVE_DISABLE_IPV6, pr,
                                   sts_thread_, getter_AddRefs(pr->request_)))) {
    MOZ_MTLOG(ML_ERROR, "AsyncResolve failed.");
    ABORT(R_NOT_FOUND);
  }
  // Because the C API offers no "finished" method to release the handle we
  // return, we cannot return the request we got from AsyncResolve directly.
  //
  // Instead, we return an addref'ed reference to PendingResolution itself,
  // which in turn holds the request and coordinates between cancel and
  // OnLookupComplete to release it only once.
  *handle = pr.forget().get();

  _status=0;
abort:
  return _status;
}

nsresult NrIceResolver::PendingResolution::OnLookupComplete(
    nsICancelable *request, nsIDNSRecord *record, nsresult status) {
  ASSERT_ON_THREAD(thread_);
  // First check if we've been canceled. This is single-threaded on the STS
  // thread, but cancel() cannot guarantee this event isn't on the queue.
  if (!canceled_) {
    nr_transport_addr *cb_addr = nullptr;
    nr_transport_addr ta;
    // TODO(jib@mozilla.com): Revisit when we do TURN.
    if (NS_SUCCEEDED(status)) {
      net::NetAddr na;
      if (NS_SUCCEEDED(record->GetNextAddr(port_, &na))) {
        MOZ_ALWAYS_TRUE (nr_netaddr_to_transport_addr(&na, &ta) == 0);
        cb_addr = &ta;
      }
    }
    cb_(cb_arg_, cb_addr);
    Release();
  }
  return NS_OK;
}

int NrIceResolver::cancel(void *obj, void *handle) {
  MOZ_ALWAYS_TRUE(obj);
  MOZ_ASSERT(handle);
  ASSERT_ON_THREAD(static_cast<NrIceResolver *>(obj)->sts_thread_);
  return static_cast<PendingResolution *>(handle)->cancel();
}

int NrIceResolver::PendingResolution::cancel() {
  request_->Cancel (NS_ERROR_ABORT);
  canceled_ = true; // in case OnLookupComplete is already on event queue.
  Release();
  return 0;
}

NS_IMPL_ISUPPORTS1(NrIceResolver::PendingResolution, nsIDNSListener);
}  // End of namespace mozilla
