/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef scoped_ptrs_h__
#define scoped_ptrs_h__

#include "keyhi.h"

namespace nss_test {

struct ScopedDelete {
  void operator()(PK11SlotInfo* slot) { PK11_FreeSlot(slot); }
  void operator()(SECItem* item) { SECITEM_FreeItem(item, true); }
  void operator()(PK11SymKey* key) { PK11_FreeSymKey(key); }
  void operator()(SECKEYPublicKey* key) { SECKEY_DestroyPublicKey(key); }
  void operator()(SECKEYPrivateKey* key) { SECKEY_DestroyPrivateKey(key); }
  void operator()(SECAlgorithmID* id) { SECOID_DestroyAlgorithmID(id, true); }
  void operator()(CERTSubjectPublicKeyInfo* spki) {
    SECKEY_DestroySubjectPublicKeyInfo(spki);
  }
};

template<class T>
struct ScopedMaybeDelete {
  void operator()(T* ptr) { if (ptr) { ScopedDelete del; del(ptr); } }
};

#define SCOPED(x) typedef std::unique_ptr<x, ScopedMaybeDelete<x> > Scoped ## x

SCOPED(PK11SlotInfo);
SCOPED(SECItem);
SCOPED(PK11SymKey);
SCOPED(SECKEYPublicKey);
SCOPED(SECKEYPrivateKey);
SCOPED(SECAlgorithmID);
SCOPED(CERTSubjectPublicKeyInfo);

#undef SCOPED

}  // namespace nss_test

#endif
