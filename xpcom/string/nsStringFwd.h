/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsStringFwd.h --- forward declarations for string classes */

#ifndef nsStringFwd_h
#define nsStringFwd_h

#include "nscore.h"

namespace mozilla {
namespace detail {

class nsStringRepr;
class nsCStringRepr;

} // namespace detail
} // namespace mozilla

static const size_t AutoStringDefaultStorageSize = 64;

// Double-byte (char16_t) string types.
class nsAString;
class nsSubstringTuple;
class nsString;
template<size_t N> class nsAutoStringN;
using nsAutoString = nsAutoStringN<AutoStringDefaultStorageSize>;
class nsDependentString;
class nsDependentSubstring;
class nsPromiseFlatString;
class nsStringComparator;
class nsDefaultStringComparator;

// Single-byte (char) string types.
class nsACString;
class nsCSubstringTuple;
class nsCString;
template<size_t N> class nsAutoCStringN;
using nsAutoCString = nsAutoCStringN<AutoStringDefaultStorageSize>;
class nsDependentCString;
class nsDependentCSubstring;
class nsPromiseFlatCString;
class nsCStringComparator;
class nsDefaultCStringComparator;

#endif /* !defined(nsStringFwd_h) */
