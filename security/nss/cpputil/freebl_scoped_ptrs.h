/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef freebl_scoped_ptrs_h__
#define freebl_scoped_ptrs_h__

#include <memory>
#include "blapi.h"

struct ScopedDelete {
  void operator()(CMACContext* ctx) { CMAC_Destroy(ctx, PR_TRUE); }
};

template <class T>
struct ScopedMaybeDelete {
  void operator()(T* ptr) {
    if (ptr) {
      ScopedDelete del;
      del(ptr);
    }
  }
};

#define SCOPED(x) typedef std::unique_ptr<x, ScopedMaybeDelete<x> > Scoped##x

SCOPED(CMACContext);

#undef SCOPED

#endif  // freebl_scoped_ptrs_h__
