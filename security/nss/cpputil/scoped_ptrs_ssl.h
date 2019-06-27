/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef scoped_ptrs_ssl_h__
#define scoped_ptrs_ssl_h__

#include <memory>
#include "sslexp.h"

struct ScopedDeleteSSL {
  void operator()(SSLAeadContext* ctx) { SSL_DestroyAead(ctx); }
  void operator()(SSLAntiReplayContext* ctx) {
    SSL_ReleaseAntiReplayContext(ctx);
  }
  void operator()(SSLResumptionTokenInfo* token) {
    SSL_DestroyResumptionTokenInfo(token);
  }
};

template <class T>
struct ScopedMaybeDeleteSSL {
  void operator()(T* ptr) {
    if (ptr) {
      ScopedDeleteSSL del;
      del(ptr);
    }
  }
};

#define SCOPED(x) typedef std::unique_ptr<x, ScopedMaybeDeleteSSL<x> > Scoped##x

SCOPED(SSLAeadContext);
SCOPED(SSLAntiReplayContext);
SCOPED(SSLResumptionTokenInfo);

#undef SCOPED

#endif  // scoped_ptrs_ssl_h__
