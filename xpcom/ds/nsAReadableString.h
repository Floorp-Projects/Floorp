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

#ifndef _nsAReadableString_h__
#define _nsAReadableString_h__

  // WORK IN PROGRESS

#include "nscore.h"
#include <iterator>


/*
  This file defines the abstract interfaces |nsAReadableString| and
  |nsAReadableCString| (the 'A' is for 'abstract', as opposed to the 'I' in
  [XP]COM interface names).

  These types are intended to be as source compatible as possible with the original
  definitions of |const nsString&| and |const nsCString&|, respectively.  In otherwords,
  these interfaces provide only non-mutating access to the underlying strings.  We
  split the these interfaces out from the mutating parts (see
  "nsAWritableString.h") because tests showed that we could exploit specialized
  implementations in some areas; we need an abstract interface to bring the whole
  family of strings together.

  |nsAReadableString| is a string of |PRUnichar|s.  |nsAReadableCString| (note the
  'C') is a string of |char|s.
*/


template <class CharT> class basic_nsAWritableString;
  // ...because we sometimes use them as `out' params

template <class CharT>
class basic_nsAReadableString
    /*
      ...
    */
  {
    protected:


      struct ConstFragment
        {
          const CharT* mStart;
          const CharT* mEnd;

          const basic_nsAReadableString<CharT>* mOwningString;
          void*                                 mFragmentIdentifier;

          explicit
          ConstFragment( const basic_nsAReadableString<CharT>* aOwner = 0 )
              : mStart(0), mEnd(0), mOwningString(aOwner), mFragmentIdentifier(0)
            {
              // nothing else to do here
            }
        };

    public:
      enum FragmentRequest { kPrevFragment, kFirstFragment, kLastFragment, kNextFragment, kFragmentAt };
      virtual const CharT* GetFragment( ConstFragment&, FragmentRequest, PRUint32 = 0 ) const = 0;

      friend class ConstIterator;
      class ConstIterator
            : public std::bidirectional_iterator_tag
        {
            friend class basic_nsAReadableString<CharT>;

            ConstFragment mFragment;
            const CharT*  mPosition;

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

            ConstIterator( const ConstFragment& aFragment, const CharT* aStartingPosition )
                : mFragment(aFragment), mPosition(aStartingPosition)
              {
                // nothing else to do here
              }

          public:
            // ConstIterator( const ConstIterator& ); ...use default copy-constructor
            // ConstIterator& operator=( const ConstIterator& ); ...use default copy-assignment operator

            
            CharT
            operator*()
              {
                normalize_forward();
                return *mPosition;
              }

            ConstIterator&
            operator++()
              {
                normalize_forward();
                ++mPosition;
                return *this;
              }

            ConstIterator
            operator++( int )
              {
                ConstIterator result(*this);
                normalize_forward();
                ++mPosition;
                return result;
              }

            ConstIterator&
            operator--()
              {
                normalize_backward();
                ++mPosition;
                return *this;
              }

            ConstIterator
            operator--( int )
              {
                ConstIterator result(*this);
                normalize_backward();
                ++mPosition;
                return result;
              }

            bool
            operator==( const ConstIterator& rhs )
              {
                return mPosition == rhs.mPosition;
              }

            bool
            operator!=( const ConstIterator& rhs )
              {
                return mPosition != rhs.mPosition;
              }
        };


    public:

      ConstIterator
      Begin( PRUint32 aOffset = 0 ) const
        {
          ConstFragment fragment(this);
          const CharT* startPos = GetFragment(fragment, kFragmentAt, aOffset);
          return ConstIterator(fragment, startPos);
        }

      ConstIterator
      End( PRUint32 aOffset = 0 ) const
        {
          ConstFragment fragment(this);
          const CharT* startPos = GetFragment(fragment, kFragmentAt, min(0U, Length()-aOffset));
          return ConstIterator(fragment, startPos);
        }

    public:
      virtual ~basic_nsAReadableString<CharT>() { }

      virtual PRUint32 Length() const = 0;
      PRBool IsEmpty() const { return Length()==0; }

      // PRBool IsOrdered() const;



      PRBool IsUnicode() const { return PR_FALSE; }
        // ...but note specialization for |PRUnichar|, below

      const CharT* GetBuffer() const { return 0; }
      const CharT* GetUnicode() const { return 0; }
        // ...but note specializations for |char| and |PRUnichar|, below

      // CharT operator[]( PRUint32 ) const;
      // CharT CharAt( PRUint32 ) const;
      // CharT First() const;
      // CharT Last() const;

      void ToLowerCase( basic_nsAWritableString<CharT>& ) const;
      void ToUpperCase( basic_nsAWritableString<CharT>& ) const;

      // PRUint32 CountChar( char_type ) const;

      // nsString* ToNewString() const; NO!  The right way to say this is
      // new nsString( fromAReadableString )

      // char* ToNewCString() const;
      // char* ToNewUTF8String() const;
      // PRUnichar* ToNewUnicode() const;
      // char* ToCString( char*, PRUint32, PRUint32 ) const;
      // double ToFLoat( PRInt32* aErrorCode ) const;
      // long ToInteger( PRInt32* aErrorCode, PRUint32 aRadix );

      PRUint32 Left( basic_nsAWritableString<CharT>&, PRUint32 ) const;
      PRUint32 Mid( basic_nsAWritableString<CharT>&, PRUint32, PRUint32 ) const;
      PRUint32 Right( basic_nsAWritableString<CharT>&, PRUint32 ) const;

      // PRUint32 BinarySearch( CharT ) const;
      // Find( ... ) const;
      // FindChar( ... ) const;
      // FindCharInSet( ... ) const;
      // RFind( ... ) const;
      // RFindChar( ... ) const;
      // RFindCharInSet( ... ) const;

      int Compare( const basic_nsAReadableString<CharT>& rhs ) const;
      // int Compare( const CharT*, const CharT* ) const;

      // Equals
      // EqualsIgnoreCase

      // IsASCII
      // IsSpace
      // IsAlpha
      // IsDigit
  };

NS_SPECIALIZE_TEMPLATE
inline
PRBool
basic_nsAReadableString<PRUnichar>::IsUnicode() const
  {
    return PR_TRUE;
  }

NS_SPECIALIZE_TEMPLATE
inline
const PRUnichar*
basic_nsAReadableString<PRUnichar>::GetUnicode() const
    // DEPRECATED: use the iterators instead
  {
    ConstFragment fragment;
    GetFragment(fragment, kFirstFragment);
    return fragment.mStart;
  }

NS_SPECIALIZE_TEMPLATE
inline
const char*
basic_nsAReadableString<char>::GetBuffer() const
    // DEPRECATED: use the iterators instead
  {
    ConstFragment fragment;
    GetFragment(fragment, kFirstFragment);
    return fragment.mStart;
  }


template <class CharT>
class do_ToLowerCase : unary_function<CharT, CharT>
  {
    // std::locale loc;
    // std::ctype<CharT> ct;

    public:
      // do_ToLowerCase() : ct( use_facet< std::ctype<CharT> >(loc) ) { }
   
      CharT
      operator()( CharT aChar )
        {
          // return ct.tolower(aChar);
          return CharT(std::tolower(aChar));
        }
  };

template <class CharT>
void
basic_nsAReadableString<CharT>::ToLowerCase( basic_nsAWritableString<CharT>& aResult ) const
  {
    aResult.SetLength(Length());
    std::transform(Begin(), End(), aResult.Begin(), do_ToLowerCase<CharT>());
  }

template <class CharT>
class do_ToUpperCase : unary_function<CharT, CharT>
  {
    // std::locale loc;
    // std::ctype<CharT> ct;

    public:
      // do_ToUpperCase() : ct( use_facet< std::ctype<CharT> >(loc) ) { }
   
      CharT
      operator()( CharT aChar )
        {
          // return ct.toupper(aChar);
          return CharT(std::toupper(aChar));
        }
  };

template <class CharT>
void
basic_nsAReadableString<CharT>::ToUpperCase( basic_nsAWritableString<CharT>& aResult ) const
  {
    aResult.SetLength(Length());
    std::transform(Begin(), End(), aResult.Begin(), do_ToUpperCase<CharT>());
  }

template <class CharT>
PRUint32
basic_nsAReadableString<CharT>::Left( basic_nsAWritableString<CharT>& aResult, PRUint32 aLengthToCopy ) const
  {
    aLengthToCopy = min(aLengthToCopy, Length());
    aResult.SetLength(aLengthToCopy);
    std::copy(Begin(), Begin(aLengthToCopy), aResult.Begin());
    return aLengthToCopy;
  }

template <class CharT>
PRUint32
basic_nsAReadableString<CharT>::Mid( basic_nsAWritableString<CharT>& aResult, PRUint32 aStartPos, PRUint32 aLengthToCopy ) const
  {
    aStartPos = min(aStartPos, Length());
    aLengthToCopy = min(aLengthToCopy, Length()-aStartPos);
    aResult.SetLength(aLengthToCopy);
    std::copy(Begin(aStartPos), Begin(aStartPos+aLengthToCopy), aResult.Begin());
    return aLengthToCopy;
  }

template <class CharT>
PRUint32
basic_nsAReadableString<CharT>::Right( basic_nsAWritableString<CharT>& aResult, PRUint32 aLengthToCopy ) const
  {
    aLengthToCopy = min(aLengthToCopy, Length());
    aResult.SetLength(aLengthToCopy);
    std::copy(End(aLengthToCopy), End(), aResult.Begin());
    return aLengthToCopy;
  }

template <class CharT>
inline
int
basic_nsAReadableString<CharT>::Compare( const basic_nsAReadableString<CharT>& rhs ) const
  {
    // ...
  }

// readable != readable
// readable != CharT*
// CharT* != readable

// readable < readable
// readable < CharT*
// CharT* < readable

// readable <= readable
// readable <= CharT*
// CharT* <= readable

// readable == readable
// readable == CharT*
// CharT* == readable

// readable >= readable
// readable >= CharT*
// CharT* >= readable

// readable > readable
// readable > CharT*
// CharT* > readable


  /*
    How shall we provide |operator+()|?

    What would it return?  It has to return a stack based object, because the client will
    not be given an opportunity to handle memory management in an expression like

      myWritableString = stringA + stringB + stringC;

    ...so the `obvious' answer of returning a new |nsSharedString| is no good.  We could
    return an |nsString|, if that name were in scope here, though there's no telling what the client
    will really want to do with the result.  What might be better, though,
    is to return a `promise' to concatenate some strings...

    By making |nsConcatString| inherit from readable strings, we automatically handle
    assignment and other interesting uses within writable strings, plus we drastically reduce
    the number of cases we have to write |operator+()| for.  The cost is extra temporary concat strings
    in the evaluation of strings of '+'s, e.g., |A + B + C + D|, and that we have to do some work
    to implement the virtual functions of readables.
  */

template <class CharT>
class nsConcatString
      : public basic_nsAReadableString<CharT>
    /*
      ...not unlike RickG's original |nsSubsumeString| in intent.
    */
  {
    public:

    // ...
  };

// readable + readable --> concat
// readable + CharT* --> concat
// CharT* + readable --> concat

template <class CharT, class TraitsT>
basic_ostream<CharT, class TraitsT>&
operator<<( basic_ostream<CharT, TraitsT>& os, const basic_nsAReadableString<CharT>& s )
  {
    std::copy(s.Begin(), s.End(), ostream_iterator<CharT, CharT, TraitsT>(os));
    return os;
  }

typedef basic_nsAReadableString<PRUnichar>  nsAReadableString;
typedef basic_nsAReadableString<char>       nsAReadableCString;

#endif // !defined(_nsAReadableString_h__)
