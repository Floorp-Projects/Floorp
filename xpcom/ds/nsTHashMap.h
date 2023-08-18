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
#include "nsAtomHashKeys.h"
#include "nsHashtablesFwd.h"
#include <type_traits>

namespace mozilla::detail {
template <class KeyType>
struct nsKeyClass<
    KeyType, std::enable_if_t<sizeof(KeyType) &&
                              std::is_base_of_v<PLDHashEntryHdr, KeyType>>> {
  using type = KeyType;
};

template <typename KeyType>
struct nsKeyClass<KeyType*> {
  using type = nsPtrHashKey<KeyType>;
};

template <>
struct nsKeyClass<nsAtom*> {
  using type = nsWeakAtomHashKey;
};

template <typename Ret, typename... Args>
struct nsKeyClass<Ret (*)(Args...)> {
  using type = nsFuncPtrHashKey<Ret (*)(Args...)>;
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

template <typename KeyType>
struct nsKeyClass<KeyType, std::enable_if_t<std::is_integral_v<KeyType> ||
                                            std::is_enum_v<KeyType>>> {
  using type = nsIntegralHashKey<KeyType>;
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
struct nsKeyClass<RefPtr<nsAtom>> {
  using type = nsAtomHashKey;
};

template <>
struct nsKeyClass<nsID> {
  using type = nsIDHashKey;
};

}  // namespace mozilla::detail

// The actual definition of nsTHashMap is in nsHashtablesFwd.h, since it is a
// type alias.

#endif  // XPCOM_DS_NSTHASHMAP_H_
