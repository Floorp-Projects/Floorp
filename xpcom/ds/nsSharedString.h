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

#ifndef _nsSharedString_h__
#define _nsSharedString_h__

  // WORK IN PROGRESS

#include "nsAReadableString.h"

template <class CharT>
class basic_nsSharedString
      : public basic_nsAReadableString<CharT>
    /*
      ...
    */
  {
    private:
      ~basic_nsSharedString() { }
        // You can't sub-class me, or make an instance of me on the stack

      // operator delete

    public:

      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 ) const;

      virtual
      PRUint32
      Length() const
        {
          return mLength;
        }

      basic_nsSharedString( const basic_nsAReadableString<CharT>& aReadable )
        {
          mLength = aReadable.Length();
          copy(aReadable.BeginReading(), aReadable.EndReading(), mData+0);
        }

      nsrefcnt
      AddRef() const
        {
          return ++mRefCount;
        }

      nsrefcnt
      Release() const
        {
          nsrefcnt result = --mRefCount;
          if ( !mRefCount )
            delete this;
          return result;
        }

    private:
      mutable nsrefcnt  mRefCount;
      size_t            mLength;
      CharT             mData[1];
  };

NS_DEF_STRING_COMPARISONS(basic_nsSharedString<CharT>)

template <class CharT>
const CharT*
basic_nsSharedString<CharT>::GetReadableFragment( nsReadableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 anOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          aFragment.mEnd = (aFragment.mStart = mData) + mLength;
          return aFragment.mStart + anOffset;
        
        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }



template <class CharT>
class nsSharedStringPtr
  {
    public:
      // ...

    private:
      basic_nsSharedString<CharT>*  mPtr;
  };


template <class CharT>
basic_nsSharedString<CharT>*
new_nsSharedString( const basic_nsAReadableString<CharT>& aReadable )
  {
    void* in_buffer = operator new( sizeof(basic_nsSharedString<CharT>) + aReadable.Length()*sizeof(CharT) );
    return new (in_buffer) basic_nsSharedString<CharT>(aReadable);
  }


typedef basic_nsSharedString<PRUnichar>     nsSharedString;
typedef basic_nsSharedString<char>          nsSharedCString;


#endif // !defined(_nsSharedString_h__)
