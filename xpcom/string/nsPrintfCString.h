/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintfCString_h___
#define nsPrintfCString_h___

#include "nsString.h"

/**
 * nsPrintfCString lets you create a nsCString using a printf-style format
 * string.  For example:
 *
 *   NS_WARNING(nsPrintfCString("Unexpected value: %f", 13.917).get());
 *
 * nsPrintfCString has a small built-in auto-buffer.  For larger strings, it
 * will allocate on the heap.
 *
 * See also nsCString::AppendPrintf().
 */
class nsPrintfCString : public nsAutoCStringN<16> {
  typedef nsCString string_type;

 public:
  explicit nsPrintfCString(const char_type* aFormat, ...)
      MOZ_FORMAT_PRINTF(2, 3) {
    va_list ap;
    va_start(ap, aFormat);
    AppendVprintf(aFormat, ap);
    va_end(ap);
  }
};

/**
 *
 *
 * nsVPrintfCString is like nsPrinfCString but is created using vprintf style
 * args. This is useful for functions that have already received variadic
 * arguments and want to create a nsPrintfCString. For example:
 *
 * void LogToSeveralLocations(const char* aFormat,...) {
 *   va_list ap;
 *   va_start(ap, aFormat);
 *   nsPrintfCString logString(aFormat, ap);
 *   va_end(ap);
 *   // Use logString
 *  }
 *
 * See also nsCString::AppendVprintf().
 */

class nsVprintfCString : public nsAutoCStringN<16> {
  typedef nsCString string_type;

 public:
  nsVprintfCString(const char_type* aFormat, va_list aArgs)
      MOZ_FORMAT_PRINTF(2, 0) {
    AppendVprintf(aFormat, aArgs);
  }
};

#endif  // !defined(nsPrintfCString_h___)
