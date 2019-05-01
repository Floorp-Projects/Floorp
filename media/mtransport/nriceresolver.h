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

#ifndef nriceresolver_h__
#define nriceresolver_h__

#include <map>
#include <string>
#include "nspr.h"
#include "prnetdb.h"
#include "nsIDNSService.h"
#include "nsIDNSListener.h"
#include "nsICancelable.h"
#include "nricectx.h"

typedef struct nr_resolver_ nr_resolver;
typedef struct nr_resolver_vtbl_ nr_resolver_vtbl;
typedef struct nr_transport_addr_ nr_transport_addr;
typedef struct nr_resolver_resource_ nr_resolver_resource;

namespace mozilla {

class NrIceResolver {
 private:
  ~NrIceResolver();

 public:
  NrIceResolver();

  nsresult Init();
  nr_resolver* AllocateResolver();
  void DestroyResolver();
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NrIceResolver)

  int resolve(nr_resolver_resource* resource,
              int (*cb)(void* cb_arg, nr_transport_addr* addr), void* cb_arg,
              void** handle);

 private:
  // Implementations of vtbl functions
  static int destroy(void** objp);
  static int resolve(void* obj, nr_resolver_resource* resource,
                     int (*cb)(void* cb_arg, nr_transport_addr* addr),
                     void* cb_arg, void** handle);
  static void resolve_cb(NR_SOCKET s, int how, void* cb_arg);
  static int cancel(void* obj, void* handle);

  class PendingResolution : public nsIDNSListener {
   public:
    PendingResolution(nsIEventTarget* thread, uint16_t port, int transport,
                      int (*cb)(void* cb_arg, nr_transport_addr* addr),
                      void* cb_arg)
        : thread_(thread),
          port_(port),
          transport_(transport),
          cb_(cb),
          cb_arg_(cb_arg) {}
    NS_IMETHOD OnLookupComplete(nsICancelable* request, nsIDNSRecord* record,
                                nsresult status) override;
    NS_IMETHOD OnLookupByTypeComplete(nsICancelable* request,
                                      nsIDNSByTypeRecord* res,
                                      nsresult status) override {
      return NS_OK;
    }
    int cancel();
    nsCOMPtr<nsICancelable> request_;
    NS_DECL_THREADSAFE_ISUPPORTS

   private:
    virtual ~PendingResolution(){};
    nsCOMPtr<nsIEventTarget> thread_;
    uint16_t port_;
    int transport_;
    int (*cb_)(void* cb_arg, nr_transport_addr* addr);
    void* cb_arg_;
  };

  nr_resolver_vtbl* vtbl_;
  nsCOMPtr<nsIEventTarget> sts_thread_;
  nsCOMPtr<nsIDNSService> dns_;
#ifdef DEBUG
  int allocated_resolvers_;
#endif
};

}  // End of namespace mozilla
#endif
