/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTUtils_h
#define CTUtils_h

#include <memory>

#include "cryptohi.h"
#include "keyhi.h"
#include "keythi.h"
#include "pk11pub.h"
#include "mozpkix/Input.h"
#include "mozpkix/Result.h"

#define MOZILLA_CT_ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

struct DeleteHelper
{
  void operator()(CERTSubjectPublicKeyInfo* value)
  {
    SECKEY_DestroySubjectPublicKeyInfo(value);
  }
  void operator()(PK11Context* value) { PK11_DestroyContext(value, true); }
  void operator()(PK11SlotInfo* value) { PK11_FreeSlot(value); }
  void operator()(SECKEYPublicKey* value) { SECKEY_DestroyPublicKey(value); }
  void operator()(SECItem* value) { SECITEM_FreeItem(value, true); }
};

template <class T>
struct MaybeDeleteHelper
{
  void operator()(T* ptr)
  {
    if (ptr) {
      DeleteHelper del;
      del(ptr);
    }
  }
};

typedef std::unique_ptr<CERTSubjectPublicKeyInfo, MaybeDeleteHelper<CERTSubjectPublicKeyInfo>>
  UniqueCERTSubjectPublicKeyInfo;
typedef std::unique_ptr<PK11Context, MaybeDeleteHelper<PK11Context>>
  UniquePK11Context;
typedef std::unique_ptr<PK11SlotInfo, MaybeDeleteHelper<PK11SlotInfo>>
  UniquePK11SlotInfo;
typedef std::unique_ptr<SECKEYPublicKey, MaybeDeleteHelper<SECKEYPublicKey>>
  UniqueSECKEYPublicKey;
typedef std::unique_ptr<SECItem, MaybeDeleteHelper<SECItem>> UniqueSECItem;

namespace mozilla { namespace ct {

// Reads a TLS-encoded variable length unsigned integer from |in|.
// The integer is expected to be in big-endian order, which is used by TLS.
// Note: checks if the output parameter overflows while reading.
// |length| indicates the size (in bytes) of the serialized integer.
template <size_t length, typename T>
mozilla::pkix::Result ReadUint(mozilla::pkix::Reader& in, T& out);

// Reads a length-prefixed variable amount of bytes from |in|, updating |out|
// on success. |prefixLength| indicates the number of bytes needed to represent
// the length.
template <size_t prefixLength>
mozilla::pkix::Result ReadVariableBytes(mozilla::pkix::Reader& in,
                                        mozilla::pkix::Input& out);

} } // namespace mozilla::ct

#endif //CTUtils_h
