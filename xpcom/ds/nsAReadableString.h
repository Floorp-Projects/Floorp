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

      enum FragmentRequest { kPrevFragment, kFirstFragment, kLastFragment, kNextFragment };

      struct ConstFragment
        {
          const CharT* mStart;
          const CharT* mEnd;

          basic_nsAReadableString<CharT>* mOwningString;
          void*                           mFragmentIdentifier;
        };

      virtual PRBool GetFragment( ConstFragment&, FragmentRequest ) const = 0;

    public:

      friend class ConstIterator;
      class ConstIterator
            : public std::bidirectional_iterator_tag
        {
            friend class basic_nsAReadableString<CharT>;

            ConstFragment mCurrentFragment;

          public:
            // ConstIterator( const ConstIterator& ); ...use default copy-constructor

            
        };


    public:
      typedef unsigned long size_type;

    public:
      virtual ~basic_nsAReadableString<CharT>() { }

      virtual size_type Length() const = 0;
      PRBool IsEmpty() const { return Length()==0; }
      PRBool IsUnicode() const { return PR_FALSE; }

      // PRBool IsOrdered() const;




      const CharT*
      GetBuffer() const
          // DEPRECATED: use the iterators instead
        {
          ConstFragment fragment;
          GetFragment(fragment, kFirstFragment);
          return fragment.mStart;
        }

      const CharT*
      GetUnicode() const
          // DEPRECATED: use the iterators instead
        {
          ConstFragment fragment;
          GetNextFragment(fragment, kFirstFragment);
          return fragment.mStart;
        }

      // CharT operator[]( size_type ) const;
      // CharT CharAt( size_type ) const;
      // CharT First() const;
      // CharT Last() const;

      void ToLowerCase( basic_nsAWritableString<CharT>& ) const;
      void ToUpperCase( basic_nsAWritableString<CharT>& ) const;

      // size_type CountChar( char_type ) const;

      // nsString* ToNewString() const; NO!  The right way to say this is
      // new nsString( fromAReadableString )

      // char* ToNewCString() const;
      // char* ToNewUTF8String() const;
      // PRUnichar* ToNewUnicode() const;
      // char* ToCString( char*, size_type, size_type ) const;
      // double ToFLoat( PRInt32* aErrorCode ) const;
      // long ToInteger( PRInt32* aErrorCode, PRUint32 aRadix );

      size_type Left( basic_nsAWritableString<CharT>&, size_type ) const;
      size_type Mid( basic_nsAWritableString<CharT>&, size_type, size_type ) const;
      size_type Right( basic_nsAWritableString<CharT>&, size_type ) const;

      // size_type BinarySearch( CharT ) const;
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
basic_nsAReadableString<PRUnichar>::GetBuffer() const
  {
    return 0;
  }

NS_SPECIALIZE_TEMPLATE
inline
const char*
basic_nsAReadableString<char>::GetUnicode() const
  {
    return 0;
  }


template <class CharT>
class do_ToLowerCase : unary_function<CharT, CharT>
  {
    std::locale loc;
    std::ctype<CharT> ct;

    public:
      do_ToLowerCase() : ct( use_facet< std::ctype<CharT> >(loc) ) { }
   
      CharT
      operator()( CharT aChar )
        {
          return ct.tolower(aChar);
        }
  };

template <class CharT>
void
basic_nsAReadableString<CharT>::ToLowerCase( basic_nsAWritableString<CharT>& aResult ) const
  {
    aResult.SetLength(Length());
    std::transform(begin(), end(), aResult.begin(), do_ToLowerCase<CharT>());
  }

template <class CharT>
class do_ToUpperCase : unary_function<CharT, CharT>
  {
    std::locale loc;
    std::ctype<CharT> ct;

    public:
      do_ToUpperCase() : ct( use_facet< std::ctype<CharT> >(loc) ) { }
   
      CharT
      operator()( CharT aChar )
        {
          return ct.toupper(aChar);
        }
  };

template <class CharT>
void
basic_nsAReadableString<CharT>::ToUpperCase( basic_nsAWritableString<CharT>& aResult ) const
  {
    aResult.SetLength(Length());
    std::transform(begin(), end(), aResult.begin(), do_ToUpperCase<CharT>());
  }

template <class CharT>
typename basic_nsAReadableString<CharT>::size_type
basic_nsAReadableString<CharT>::Left( basic_nsAWritableString<CharT>& aResult, size_type aLengthToCopy ) const
  {
    aLengthToCopy = min(aLengthToCopy, Length());
    aResult.SetLength(aLengthToCopy);
    std::copy(begin(), begin()+aLengthToCopy, aResult.begin());
  }

template <class CharT>
typename basic_nsAReadableString<CharT>::size_type
basic_nsAReadableString<CharT>::Mid( basic_nsAWritableString<CharT>& aResult, size_type aStartPos, size_type aLengthToCopy ) const
  {
    aStartPos = min(aStartPos, Length());
    aLengthToCopy = min(aLengthToCopy, Length()-aStartPos);
    aResult.SetLength(aLengthToCopy);
    std::copy(begin()+aStartPos, begin()+(aStartPos+aLengthToCopy), aResult.begin());
  }

template <class CharT>
typename basic_nsAReadableString<CharT>::size_type
basic_nsAReadableString<CharT>::Right( basic_nsAWritableString<CharT>& aResult, size_type aLengthToCopy ) const
  {
    aLengthToCopy = min(aLengthToCopy, Length());
    aResult.SetLength(aLengthToCopy);
    std::copy(end()-aLengthToCopy, end(), aResult.begin());
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

// operator<<

typedef basic_nsAReadableString<PRUnichar>  nsAReadableString;
typedef basic_nsAReadableString<char>       nsAReadableCString;

#endif // !defined(_nsAReadableString_h__)
