/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Helper routines for perfecthash.py.  Not to be used directly. */

#ifndef mozilla_PerfectHash_h
#define mozilla_PerfectHash_h

#include <type_traits>

namespace mozilla {
namespace perfecthash {

// 32-bit FNV offset basis and prime value.
// NOTE: Must match values in |perfecthash.py|
constexpr uint32_t FNV_OFFSET_BASIS = 0x811C9DC5;
constexpr uint32_t FNV_PRIME = 16777619;

/**
 * Basic FNV hasher function used by perfecthash. Generic over the unit type.
 */
template <typename C>
inline uint32_t Hash(uint32_t aBasis, const C* aKey, size_t aLen) {
  for (size_t i = 0; i < aLen; ++i) {
    aBasis =
        (aBasis ^ static_cast<std::make_unsigned_t<C>>(aKey[i])) * FNV_PRIME;
  }
  return aBasis;
}

/**
 * Helper method for getting the index from a perfect hash.
 * Called by code generated from |perfecthash.py|.
 */
template <typename C, typename Base, size_t NBases, typename Entry,
          size_t NEntries>
inline const Entry& Lookup(const C* aKey, size_t aLen,
                           const Base (&aTable)[NBases],
                           const Entry (&aEntries)[NEntries]) {
  uint32_t basis = aTable[Hash(FNV_OFFSET_BASIS, aKey, aLen) % NBases];
  return aEntries[Hash(basis, aKey, aLen) % NEntries];
}

}  // namespace perfecthash
}  // namespace mozilla

#endif  // !defined(mozilla_PerfectHash_h)
