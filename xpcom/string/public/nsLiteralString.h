/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLiteralString_h___
#define nsLiteralString_h___

#ifndef nscore_h___
#include "nscore.h"
#endif

#ifndef nsDependentString_h___
#include "nsDependentString.h"
#endif

#include "mozilla/Char16.h"

namespace mozilla {
namespace internal {

// This is the same as sizeof(c) - 1, except it won't compile if c isn't a
// string literal.  This ensures that NS_LITERAL_CSTRING doesn't compile if you
// pass it a char* (or something else for that matter).
template<int n>
inline uint32_t LiteralStringLength(const char (&c)[n])
{
  return n - 1;
}

template<int n>
inline uint32_t LiteralWStringLength(const char16_t (&c)[n])
{
  return n - 1;
}

} // namespace internal
} // namespace mozilla

#define NS_MULTILINE_LITERAL_STRING(s)          nsDependentString(reinterpret_cast<const nsAString::char_type*>(s), mozilla::internal::LiteralWStringLength(s))
#define NS_MULTILINE_LITERAL_STRING_INIT(n,s)   n(reinterpret_cast<const nsAString::char_type*>(s), mozilla::internal::LiteralWStringLength(s))
#define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  const nsDependentString n(reinterpret_cast<const nsAString::char_type*>(s), mozilla::internal::LiteralWStringLength(s))
typedef nsDependentString nsLiteralString;

#define NS_LITERAL_STRING(s)                      static_cast<const nsAFlatString&>(NS_MULTILINE_LITERAL_STRING(MOZ_UTF16(s)))
#define NS_LITERAL_STRING_INIT(n,s)               NS_MULTILINE_LITERAL_STRING_INIT(n, MOZ_UTF16(s))
#define NS_NAMED_LITERAL_STRING(n,s)              NS_NAMED_MULTILINE_LITERAL_STRING(n, MOZ_UTF16(s))

#define NS_LITERAL_CSTRING(s)                     static_cast<const nsDependentCString&>(nsDependentCString(s, mozilla::internal::LiteralStringLength(s)))
#define NS_LITERAL_CSTRING_INIT(n,s)              n(s, mozilla::internal::LiteralStringLength(s))
#define NS_NAMED_LITERAL_CSTRING(n,s)             const nsDependentCString n(s, mozilla::internal::LiteralStringLength(s))

typedef nsDependentCString nsLiteralCString;

#endif /* !defined(nsLiteralString_h___) */
