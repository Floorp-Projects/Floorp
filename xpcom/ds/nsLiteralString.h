/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 */

#ifndef _nsLiteralString_h__
#define _nsLiteralString_h__

  // WORK IN PROGRESS

#include "nsAReadableString.h"
#include <string>

template <class CharT>
class basic_nsLiteralString
      : public basic_nsAReadableString<CharT>
  {
    typedef typename basic_nsAReadableString<CharT>::FragmentRequest  FragmentRequest;
    typedef typename basic_nsAWritableString<CharT>::ConstFragment    ConstFragment;

    protected:
      virtual const CharT* GetFragment( ConstFragment&, FragmentRequest, PRUint32 ) const;

    public:
      basic_nsLiteralString( const CharT* );
      virtual PRUint32 Length() const;
      // ...

    private:
      const CharT* mStart;
      const CharT* mEnd;
  };

template <class CharT>
basic_nsLiteralString<CharT>::basic_nsLiteralString( const CharT* aStringLiteral )
    : mStart(aStringLiteral)
  {
    mEnd = mStart + char_traits<CharT>::length(mStart);
  }

template <class CharT>
const CharT*
basic_nsLiteralString<CharT>::GetFragment( ConstFragment& aFragment, FragmentRequest aRequest, PRUint32 aOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          aFragment.mStart = mStart;
          aFragment.mEnd = mEnd;
          return mStart + aOffset;
        
        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }

template <class CharT>
PRUint32
basic_nsLiteralString<CharT>::Length() const
  {
    return PRUint32(mEnd - mStart);
  }



typedef basic_nsLiteralString<PRUnichar>  nsLiteralString;
typedef basic_nsLiteralString<char>       nsLiteralCString;

#endif // !defined(_nsLiteralString_h__)
