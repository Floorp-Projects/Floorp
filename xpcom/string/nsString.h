/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsString_h___
#define nsString_h___

#include <ostream>

#include "mozilla/Attributes.h"

#include "nsStringFwd.h"

#include "nsAString.h"
#include "nsDependentSubstring.h"
#include "nsReadableUtils.h"

// enable support for the obsolete string API if not explicitly disabled
#ifndef MOZ_STRING_WITH_OBSOLETE_API
#  define MOZ_STRING_WITH_OBSOLETE_API 1
#endif

#include "nsTString.h"

static_assert(sizeof(char16_t) == 2, "size of char16_t must be 2");
static_assert(sizeof(nsString::char_type) == 2,
              "size of nsString::char_type must be 2");
static_assert(nsString::char_type(-1) > nsString::char_type(0),
              "nsString::char_type must be unsigned");
static_assert(sizeof(nsCString::char_type) == 1,
              "size of nsCString::char_type must be 1");

static_assert(sizeof(nsTLiteralString<char>) == sizeof(nsTString<char>),
              "nsLiteralCString can masquerade as nsCString, "
              "so they must have identical layout");

static_assert(sizeof(nsTLiteralString<char16_t>) == sizeof(nsTString<char16_t>),
              "nsTLiteralString can masquerade as nsString, "
              "so they must have identical layout");

/**
 * A helper class that converts a UTF-16 string to ASCII in a lossy manner
 */
class NS_LossyConvertUTF16toASCII : public nsAutoCString {
 public:
  explicit NS_LossyConvertUTF16toASCII(const char16ptr_t aString) {
    LossyAppendUTF16toASCII(mozilla::MakeStringSpan(aString), *this);
  }

  NS_LossyConvertUTF16toASCII(const char16ptr_t aString, size_t aLength) {
    LossyAppendUTF16toASCII(
        Substring(static_cast<const char16_t*>(aString), aLength), *this);
  }

  explicit NS_LossyConvertUTF16toASCII(const nsAString& aString) {
    LossyAppendUTF16toASCII(aString, *this);
  }

 private:
  // NOT TO BE IMPLEMENTED
  NS_LossyConvertUTF16toASCII(char) = delete;
};

class NS_ConvertASCIItoUTF16 : public nsAutoString {
 public:
  explicit NS_ConvertASCIItoUTF16(const char* aCString) {
    AppendASCIItoUTF16(mozilla::MakeStringSpan(aCString), *this);
  }

  NS_ConvertASCIItoUTF16(const char* aCString, size_t aLength) {
    AppendASCIItoUTF16(Substring(aCString, aLength), *this);
  }

  explicit NS_ConvertASCIItoUTF16(const nsACString& aCString) {
    AppendASCIItoUTF16(aCString, *this);
  }

  explicit NS_ConvertASCIItoUTF16(mozilla::Span<const char> aCString) {
    AppendASCIItoUTF16(aCString, *this);
  }

 private:
  // NOT TO BE IMPLEMENTED
  NS_ConvertASCIItoUTF16(char16_t) = delete;
};

/**
 * A helper class that converts a UTF-16 string to UTF-8
 */
class NS_ConvertUTF16toUTF8 : public nsAutoCString {
 public:
  explicit NS_ConvertUTF16toUTF8(const char16ptr_t aString) {
    AppendUTF16toUTF8(mozilla::MakeStringSpan(aString), *this);
  }

  NS_ConvertUTF16toUTF8(const char16ptr_t aString, size_t aLength) {
    AppendUTF16toUTF8(Substring(static_cast<const char16_t*>(aString), aLength),
                      *this);
  }

  explicit NS_ConvertUTF16toUTF8(const nsAString& aString) {
    AppendUTF16toUTF8(aString, *this);
  }

  explicit NS_ConvertUTF16toUTF8(mozilla::Span<const char16_t> aString) {
    AppendUTF16toUTF8(aString, *this);
  }

 private:
  // NOT TO BE IMPLEMENTED
  NS_ConvertUTF16toUTF8(char) = delete;
};

class NS_ConvertUTF8toUTF16 : public nsAutoString {
 public:
  explicit NS_ConvertUTF8toUTF16(const char* aCString) {
    AppendUTF8toUTF16(mozilla::MakeStringSpan(aCString), *this);
  }

  NS_ConvertUTF8toUTF16(const char* aCString, size_t aLength) {
    AppendUTF8toUTF16(Substring(aCString, aLength), *this);
  }

  explicit NS_ConvertUTF8toUTF16(const nsACString& aCString) {
    AppendUTF8toUTF16(aCString, *this);
  }

  explicit NS_ConvertUTF8toUTF16(mozilla::Span<const char> aCString) {
    AppendUTF8toUTF16(aCString, *this);
  }

 private:
  // NOT TO BE IMPLEMENTED
  NS_ConvertUTF8toUTF16(char16_t) = delete;
};

/**
 * Converts an integer (signed/unsigned, 32/64bit) to its decimal string
 * representation and returns it as an nsAutoCString/nsAutoString.
 */
template <typename T, typename U>
nsTAutoString<T> IntToTString(const U aInt, const int aRadix = 10) {
  nsTAutoString<T> string;
  string.AppendInt(aInt, aRadix);
  return string;
}

template <typename U>
nsAutoCString IntToCString(const U aInt, const int aRadix = 10) {
  return IntToTString<char>(aInt, aRadix);
}

template <typename U>
nsAutoString IntToString(const U aInt, const int aRadix = 10) {
  return IntToTString<char16_t>(aInt, aRadix);
}

// MOZ_DBG support

inline std::ostream& operator<<(std::ostream& aOut, const nsACString& aString) {
  aOut.write(aString.Data(), aString.Length());
  return aOut;
}

inline std::ostream& operator<<(std::ostream& aOut, const nsAString& aString) {
  return aOut << NS_ConvertUTF16toUTF8(aString);
}

// the following are included/declared for backwards compatibility
#include "nsDependentString.h"
#include "nsLiteralString.h"
#include "nsPromiseFlatString.h"

#endif  // !defined(nsString_h___)
