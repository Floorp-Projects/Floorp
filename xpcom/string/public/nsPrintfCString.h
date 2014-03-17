/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
class nsPrintfCString : public nsFixedCString
{
  typedef nsCString string_type;

public:
  explicit nsPrintfCString( const char_type* format, ... )
    : nsFixedCString(mLocalBuffer, kLocalBufferSize, 0)
  {
    va_list ap;
    va_start(ap, format);
    AppendPrintf(format, ap);
    va_end(ap);
  }

private:
  static const uint32_t kLocalBufferSize = 16;
  char_type mLocalBuffer[kLocalBufferSize];
};

#endif // !defined(nsPrintfCString_h___)
