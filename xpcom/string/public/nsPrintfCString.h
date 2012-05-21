/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintfCString_h___
#define nsPrintfCString_h___
 
#ifndef nsString_h___
#include "nsString.h"
#endif

/**
 * |nsPrintfCString| is syntactic sugar for a printf used as a
 * |const nsACString| with a small builtin autobuffer. In almost all cases
 * it is better to just use AppendPrintf. Example usage:
 *
 *   NS_WARNING(nsPrintfCString("Unexpected value: %f", 13.917).get());
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
      enum { kLocalBufferSize=15 };
      // ought to be large enough for most things ... a |long long| needs at most 20 (so you'd better ask)
      //  pinkerton suggests 7.  We should measure and decide what's appropriate

      char_type  mLocalBuffer[ kLocalBufferSize ];
  };

#endif // !defined(nsPrintfCString_h___)
