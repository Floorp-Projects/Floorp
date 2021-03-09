/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_DS_NSTHASHMAP_H_
#define XPCOM_DS_NSTHASHMAP_H_

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsBaseHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsHashtablesFwd.h"
#include <type_traits>

namespace mozilla::detail {
template <class KeyType>
struct nsKeyClass {
  // Prevent instantiation with an incomplete KeyType, as std::is_base_of_v
  // would have undefined behaviour then.
  static_assert(sizeof(KeyType) > 0,
                "KeyType must be a complete type (unless there's an explicit "
                "specialization for nsKeyClass<KeyType>)");
  static_assert(std::is_base_of_v<PLDHashEntryHdr, KeyType>,
                "KeyType must inherit from PLDHashEntryHdr (unless there's an "
                "explicit specialization for nsKeyClass<KeyType>)");

  using type = std::conditional_t<std::is_base_of_v<PLDHashEntryHdr, KeyType>,
                                  KeyType, void>;
};

template <typename KeyType>
struct nsKeyClass<KeyType*> {
  using type = nsPtrHashKey<KeyType>;
};

template <>
struct nsKeyClass<nsCString> {
  using type = nsCStringHashKey;
};

// This uses the case-sensitive hash key class, if you want the
// case-insensitive hash key, use nsStringCaseInsensitiveHashKey explicitly.
template <>
struct nsKeyClass<nsString> {
  using type = nsStringHashKey;
};

template <>
struct nsKeyClass<uint32_t> {
  using type = nsUint32HashKey;
};

template <>
struct nsKeyClass<uint64_t> {
  using type = nsUint64HashKey;
};

template <>
struct nsKeyClass<intptr_t> {
  using type = IntPtrHashKey;
};

template <>
struct nsKeyClass<nsCOMPtr<nsISupports>> {
  using type = nsISupportsHashKey;
};

template <typename T>
struct nsKeyClass<RefPtr<T>> {
  using type = nsRefPtrHashKey<T>;
};

template <>
struct nsKeyClass<nsID> {
  using type = nsIDHashKey;
};

}  // namespace mozilla::detail

// The actual definition of nsTHashMap is in nsHashtablesFwd.h, since it is a
// type alias.

#endif  // XPCOM_DS_NSTHASHMAP_H_
