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

#ifndef _nsAWritableString_h__
#define _nsAWritableString_h__

  // WORK IN PROGRESS

  // See also...
#include "nsAReadableString.h"


/*
  This file defines the abstract interfaces |nsAWritableString| and
  |nsAWritableCString|.

  |nsAWritableString| is a string of |PRUnichar|s.  |nsAWritableCString| (note the
  'C') is a string of |char|s.
*/

template <class CharT>
class basic_nsAWritableString
      : public basic_nsAReadableString<CharT>
    /*
      ...
    */
  {
    protected:
      typedef typename basic_nsAReadableString<CharT>::FragmentRequest  FragmentRequest;
      typedef typename basic_nsAReadableString<CharT>::ConstFragment    ConstFragment;

      struct Fragment
        {
          CharT* mStart;
          CharT* mEnd;

          basic_nsAWritableString<CharT>* mOwningString;
          void*                           mFragmentIdentifier;

          explicit
          Fragment( basic_nsAWritableString<CharT>* aOwner = 0 )
              : mStart(0), mEnd(0), mOwningString(aOwner), mFragmentIdentifier(0)
            {
              // nothing else to do here
            }
        };

    public:
      using basic_nsAReadableString<CharT>::GetFragment;
      virtual CharT* GetFragment( Fragment&, FragmentRequest, PRUint32 = 0 ) = 0;

      friend class Iterator;
      class Iterator
          : public std::bidirectional_iterator_tag
        {
            friend class basic_nsAWritableString<CharT>;

            Fragment  mFragment;
            CharT*    mPosition;

            void
            normalize_forward()
              {
                if ( mPosition == mFragment.mEnd )
                  if ( mFragment.mOwningString->GetFragment(mFragment, kNextFragment) )
                    mPosition = mFragment.mStart;
              }

            void
            normalize_backward()
              {
                if ( mPosition == mFragment.mStart )
                  if ( mFragment.mOwningString->GetFragment(mFragment, kPrevFragment) )
                    mPosition = mFragment.mEnd;
              }

            Iterator( Fragment& aFragment, CharT* aStartingPosition )
                : mFragment(aFragment), mPosition(aStartingPosition)
              {
                // nothing else to do here
              }

          public:
            // Iterator( const Iterator& ); ...use default copy-constructor
            // Iterator& operator=( const Iterator& ); ...use default copy-assignment operator

            
            CharT&
            operator*()
              {
                return *mPosition;
              }

            Iterator&
            operator++()
              {
                ++mPosition;
                normalize_forward();
                return *this;
              }

            Iterator
            operator++( int )
              {
                Iterator result(*this);
                ++mPosition;
                normalize_forward();
                return result;
              }

            Iterator&
            operator--()
              {
                normalize_backward();
                --mPosition;
                return *this;
              }

            Iterator
            operator--( int )
              {
                Iterator result(*this);
                normalize_backward();
                --mPosition;
                return result;
              }

            PRBool
            operator==( const ConstIterator& rhs )
              {
                return mPosition == rhs.mPosition;
              }

            PRBool
            operator!=( const ConstIterator& rhs )
              {
                return mPosition != rhs.mPosition;
              }
        };

    public:
      using basic_nsAReadableString<CharT>::Begin;

      Iterator
      Begin( PRUint32 aOffset = 0 )
        {
          Fragment fragment(this);
          CharT* startPos = GetFragment(fragment, kFragmentAt, aOffset);
          return Iterator(fragment, startPos);
        }

      using basic_nsAReadableString<CharT>::End;

      Iterator
      End( PRUint32 aOffset = 0 )
        {
          Fragment fragment(this);
          CharT* startPos = GetFragment(fragment, kFragmentAt, max(0U, Length()-aOffset));
          return Iterator(fragment, startPos);
        }

      virtual void SetCapacity( PRUint32 ) = 0;
      virtual void SetLength( PRUint32 ) = 0;


        // ...a synonym for |SetLength()|
      void
      Truncate( PRUint32 aNewLength=0 )
        {
          SetLength(aNewLength);
        }


      // virtual PRBool SetCharAt( char_type, index_type ) = 0;

      void ToLowerCase();
      void ToUpperCase();

      // void StripChars( const CharT* aSet );
      // void StripChar( ... );
      // void StripWhitespace();
      // void ReplaceChar( ... );
      // void ReplaceSubstring( ... );
      // void Trim( ... );
      // void CompressSet( ... );
      // void CompareWhitespace( ... );

      // Assign
      // Append( ... )
      // Insert

      // SetString
      // operator=( ... )
      // operator+=( ... )
  };

// operator>>
// getline (maybe)

typedef basic_nsAWritableString<PRUnichar> nsAWritableString;
typedef basic_nsAWritableString<char>      nsAWritableCString;

#endif // !defined(_nsAWritableString_h__)
