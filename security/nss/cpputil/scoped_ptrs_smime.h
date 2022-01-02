/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef scoped_ptrs_smime_h__
#define scoped_ptrs_smime_h__

#include <memory>
#include "smime.h"

struct ScopedDeleteSmime {
  void operator()(NSSCMSMessage* id) { NSS_CMSMessage_Destroy(id); }
};

template <class T>
struct ScopedMaybeDeleteSmime {
  void operator()(T* ptr) {
    if (ptr) {
      ScopedDeleteSmime del;
      del(ptr);
    }
  }
};

#define SCOPED(x) \
  typedef std::unique_ptr<x, ScopedMaybeDeleteSmime<x> > Scoped##x

SCOPED(NSSCMSMessage);

#undef SCOPED

#endif  // scoped_ptrs_smime_h__
