/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Original Author:
 *   Scott Collins <scc@mozilla.org>
 *
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsPrintfCString.h"
#include <stdarg.h>
#include "prprf.h"


nsPrintfCString::nsPrintfCString( const char_type* format, ... )
    : mStart(mLocalBuffer),
      mLength(0)
  {
    va_list ap;

    size_t logical_capacity = kLocalBufferSize;
    size_t physical_capacity = logical_capacity + 1;

    va_start(ap, format);
    mLength = PR_vsnprintf(mStart, physical_capacity, format, ap);
    va_end(ap);
  }

nsPrintfCString::nsPrintfCString( size_t n, const char_type* format, ... )
    : mStart(mLocalBuffer),
      mLength(0)
  {
    va_list ap;

      // make sure there's at least |n| space
    size_t logical_capacity = kLocalBufferSize;
    if ( n > logical_capacity )
      {
        char_type* nonlocal_buffer = new char_type[n];

          // if we got something, use it
        if ( nonlocal_buffer )
          {
            mStart = nonlocal_buffer;
            logical_capacity = n;
          }
        // else, it's the error case ... we'll use what space we have
        //  (since we can't throw)
      }
    size_t physical_capacity = logical_capacity + 1;

    va_start(ap, format);
    mLength = PR_vsnprintf(mStart, physical_capacity, format, ap);
    va_end(ap);
  }

nsPrintfCString::~nsPrintfCString()
  {
    if ( mStart != mLocalBuffer )
      delete [] mStart;
  }

PRUint32
nsPrintfCString::Length() const
  {
    return mLength;
  }

#if 0
PRBool
nsPrintfCString::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
          aFragment.mFragmentIdentifier = this;
          // fall through
        case kThisFragment:
          aFragment.mStart = mStart;
          aFragment.mEnd = mStart + mLength;
          return PR_TRUE;

        default:
          return PR_FALSE;
      }
  }
#else
const nsPrintfCString::char_type*
nsPrintfCString::GetReadableFragment( const_fragment_type& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          aFragment.mEnd = (aFragment.mStart = mStart) + mLength;
          return mStart + aOffset;

        case kPrevFragment:
        case kNextFragment:
          default:
          return 0;
      }
  }
#endif

