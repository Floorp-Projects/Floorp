/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef scoped_ptrs_util_h__
#define scoped_ptrs_util_h__

#include <memory>
#include "pkcs11uri.h"
#include "secoid.h"

struct ScopedDelete {
  void operator()(SECAlgorithmID* id) { SECOID_DestroyAlgorithmID(id, true); }
  void operator()(SECItem* item) { SECITEM_FreeItem(item, true); }
  void operator()(PK11URI* uri) { PK11URI_DestroyURI(uri); }
  void operator()(PLArenaPool* arena) { PORT_FreeArena(arena, PR_FALSE); }
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

SCOPED(SECAlgorithmID);
SCOPED(SECItem);
SCOPED(PK11URI);
SCOPED(PLArenaPool);

#undef SCOPED

struct StackSECItem : public SECItem {
  StackSECItem() : SECItem({siBuffer, nullptr, 0}) {}
  ~StackSECItem() { SECITEM_FreeItem(this, PR_FALSE); }
};

#endif  // scoped_ptrs_util_h__
