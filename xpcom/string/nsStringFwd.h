/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsStringFwd.h --- forward declarations for string classes */

#ifndef nsStringFwd_h
#define nsStringFwd_h

#include "nscore.h"

static constexpr int32_t kNotFound = -1;

namespace mozilla {
namespace detail {

template <typename T>
class nsTStringRepr;

using nsStringRepr = nsTStringRepr<char16_t>;
using nsCStringRepr = nsTStringRepr<char>;

}  // namespace detail
}  // namespace mozilla

static const size_t AutoStringDefaultStorageSize = 64;

template <typename T>
class nsTSubstring;
template <typename T>
class nsTSubstringTuple;
template <typename T>
class nsTString;
template <typename T, size_t N>
class nsTAutoStringN;
template <typename T>
class nsTDependentString;
template <typename T>
class nsTDependentSubstring;
template <typename T>
class nsTPromiseFlatString;
template <typename T>
class nsTLiteralString;
template <typename T>
class nsTSubstringSplitter;

template <typename T>
using nsTStringComparator = int (*)(const T*, const T*, size_t, size_t);

// The default string comparator (case-sensitive comparision)
template <typename T>
int nsTDefaultStringComparator(const T*, const T*, size_t, size_t);

// We define this version without a size param instead of providing a
// default value for N so that so there is a default typename that doesn't
// require angle brackets.
template <typename T>
using nsTAutoString = nsTAutoStringN<T, AutoStringDefaultStorageSize>;

// Double-byte (char16_t) string types.

using nsAString = nsTSubstring<char16_t>;
using nsSubstringTuple = nsTSubstringTuple<char16_t>;
using nsString = nsTString<char16_t>;
using nsAutoString = nsTAutoString<char16_t>;
template <size_t N>
using nsAutoStringN = nsTAutoStringN<char16_t, N>;
using nsDependentString = nsTDependentString<char16_t>;
using nsDependentSubstring = nsTDependentSubstring<char16_t>;
using nsPromiseFlatString = nsTPromiseFlatString<char16_t>;
using nsStringComparator = nsTStringComparator<char16_t>;
using nsLiteralString = nsTLiteralString<char16_t>;
using nsSubstringSplitter = nsTSubstringSplitter<char16_t>;

// Single-byte (char) string types.

using nsACString = nsTSubstring<char>;
using nsCSubstringTuple = nsTSubstringTuple<char>;
using nsCString = nsTString<char>;
using nsAutoCString = nsTAutoString<char>;
template <size_t N>
using nsAutoCStringN = nsTAutoStringN<char, N>;
using nsDependentCString = nsTDependentString<char>;
using nsDependentCSubstring = nsTDependentSubstring<char>;
using nsPromiseFlatCString = nsTPromiseFlatString<char>;
using nsCStringComparator = nsTStringComparator<char>;
using nsLiteralCString = nsTLiteralString<char>;
using nsCSubstringSplitter = nsTSubstringSplitter<char>;

#endif /* !defined(nsStringFwd_h) */
