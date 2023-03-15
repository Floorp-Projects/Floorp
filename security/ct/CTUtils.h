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

struct DeleteHelper {
  void operator()(CERTSubjectPublicKeyInfo* value) {
    SECKEY_DestroySubjectPublicKeyInfo(value);
  }
  void operator()(PK11Context* value) { PK11_DestroyContext(value, true); }
  void operator()(PK11SlotInfo* value) { PK11_FreeSlot(value); }
  void operator()(SECKEYPublicKey* value) { SECKEY_DestroyPublicKey(value); }
  void operator()(SECItem* value) { SECITEM_FreeItem(value, true); }
};

template <class T>
struct MaybeDeleteHelper {
  void operator()(T* ptr) {
    if (ptr) {
      DeleteHelper del;
      del(ptr);
    }
  }
};

typedef std::unique_ptr<CERTSubjectPublicKeyInfo,
                        MaybeDeleteHelper<CERTSubjectPublicKeyInfo>>
    UniqueCERTSubjectPublicKeyInfo;
typedef std::unique_ptr<PK11Context, MaybeDeleteHelper<PK11Context>>
    UniquePK11Context;
typedef std::unique_ptr<PK11SlotInfo, MaybeDeleteHelper<PK11SlotInfo>>
    UniquePK11SlotInfo;
typedef std::unique_ptr<SECKEYPublicKey, MaybeDeleteHelper<SECKEYPublicKey>>
    UniqueSECKEYPublicKey;
typedef std::unique_ptr<SECItem, MaybeDeleteHelper<SECItem>> UniqueSECItem;

namespace mozilla {
namespace ct {

// Reads a TLS-encoded variable length unsigned integer from |in|.
// The integer is expected to be in big-endian order, which is used by TLS.
// Note: does not check if the output parameter overflows while reading.
// |length| indicates the size (in bytes) of the serialized integer.
inline static pkix::Result UncheckedReadUint(size_t length, pkix::Reader& in,
                                             uint64_t& out) {
  uint64_t result = 0;
  for (size_t i = 0; i < length; ++i) {
    uint8_t value;
    pkix::Result rv = in.Read(value);
    if (rv != pkix::Success) {
      return rv;
    }
    result = (result << 8) | value;
  }
  out = result;
  return pkix::Success;
}

// Performs overflow sanity checks and calls UncheckedReadUint.
template <size_t length, typename T>
pkix::Result ReadUint(pkix::Reader& in, T& out) {
  uint64_t value;
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");
  static_assert(length <= 8, "At most 8 byte integers can be read");
  static_assert(sizeof(T) >= length, "T must be able to hold <length> bytes");
  pkix::Result rv = UncheckedReadUint(length, in, value);
  if (rv != pkix::Success) {
    return rv;
  }
  out = static_cast<T>(value);
  return pkix::Success;
}

// Reads |length| bytes from |in|.
static inline pkix::Result ReadFixedBytes(size_t length, pkix::Reader& in,
                                          pkix::Input& out) {
  return in.Skip(length, out);
}

// Reads a length-prefixed variable amount of bytes from |in|, updating |out|
// on success. |prefixLength| indicates the number of bytes needed to represent
// the length.
template <size_t prefixLength>
pkix::Result ReadVariableBytes(pkix::Reader& in, pkix::Input& out) {
  size_t length;
  pkix::Result rv = ReadUint<prefixLength>(in, length);
  if (rv != pkix::Success) {
    return rv;
  }
  return ReadFixedBytes(length, in, out);
}

}  // namespace ct
}  // namespace mozilla

#endif  // CTUtils_h
