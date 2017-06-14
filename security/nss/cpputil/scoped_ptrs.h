/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef scoped_ptrs_h__
#define scoped_ptrs_h__

#include <memory>
#include "cert.h"
#include "keyhi.h"
#include "pk11pub.h"
#include "pkcs11uri.h"

struct ScopedDelete {
  void operator()(CERTCertificate* cert) { CERT_DestroyCertificate(cert); }
  void operator()(CERTCertificateList* list) {
    CERT_DestroyCertificateList(list);
  }
  void operator()(CERTName* name) { CERT_DestroyName(name); }
  void operator()(CERTCertList* list) { CERT_DestroyCertList(list); }
  void operator()(CERTSubjectPublicKeyInfo* spki) {
    SECKEY_DestroySubjectPublicKeyInfo(spki);
  }
  void operator()(PK11SlotInfo* slot) { PK11_FreeSlot(slot); }
  void operator()(PK11SymKey* key) { PK11_FreeSymKey(key); }
  void operator()(PRFileDesc* fd) { PR_Close(fd); }
  void operator()(SECAlgorithmID* id) { SECOID_DestroyAlgorithmID(id, true); }
  void operator()(SECItem* item) { SECITEM_FreeItem(item, true); }
  void operator()(SECKEYPublicKey* key) { SECKEY_DestroyPublicKey(key); }
  void operator()(SECKEYPrivateKey* key) { SECKEY_DestroyPrivateKey(key); }
  void operator()(SECKEYPrivateKeyList* list) {
    SECKEY_DestroyPrivateKeyList(list);
  }
  void operator()(PK11URI* uri) { PK11URI_DestroyURI(uri); }
  void operator()(PLArenaPool* arena) { PORT_FreeArena(arena, PR_FALSE); }
  void operator()(PK11Context* context) { PK11_DestroyContext(context, true); }
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

SCOPED(CERTCertificate);
SCOPED(CERTCertificateList);
SCOPED(CERTCertList);
SCOPED(CERTName);
SCOPED(CERTSubjectPublicKeyInfo);
SCOPED(PK11SlotInfo);
SCOPED(PK11SymKey);
SCOPED(PRFileDesc);
SCOPED(SECAlgorithmID);
SCOPED(SECItem);
SCOPED(SECKEYPublicKey);
SCOPED(SECKEYPrivateKey);
SCOPED(SECKEYPrivateKeyList);
SCOPED(PK11URI);
SCOPED(PLArenaPool);
SCOPED(PK11Context);

#undef SCOPED

#endif  // scoped_ptrs_h__
