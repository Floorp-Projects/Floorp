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
  // for |PRUnichar|

#include <string>
  // for |char_traits|

#include <iterator>
  // for |bidirectional_iterator_tag|


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


#define NS_DEF_1_STRING_COMPARISON_OPERATOR(comp, T1, T2) \
  template <class CharT>                        \
  inline                                        \
  PRBool                                        \
  operator comp( T1 lhs, T2 rhs )               \
    {                                           \
      return PRBool(Compare(lhs, rhs) comp 0);  \
    }

#define NS_DEF_STRING_COMPARISON_OPERATORS(T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(!=, T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(< , T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(<=, T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(==, T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(>=, T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(> , T1, T2)

#define NS_DEF_STRING_COMPARISONS(T) \
  NS_DEF_STRING_COMPARISON_OPERATORS(const T&, const CharT*) \
  NS_DEF_STRING_COMPARISON_OPERATORS(const CharT*, const T&)

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
          PRUint32                              mFragmentIdentifier;

          explicit
          ConstFragment( const basic_nsAReadableString<CharT>* aOwner = 0 )
              : mStart(0), mEnd(0), mOwningString(aOwner), mFragmentIdentifier(0)
            {
              // nothing else to do here
            }
        };

    public:
      enum FragmentRequest { kPrevFragment, kFirstFragment, kLastFragment, kNextFragment, kFragmentAt };

        // Damn!  Had to make |GetFragment| public because the compilers suck.  Should be protected.
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
                return *mPosition;
              }

            ConstIterator&
            operator++()
              {
                ++mPosition;
                normalize_forward();
                return *this;
              }

            ConstIterator
            operator++( int )
              {
                ConstIterator result(*this);
                ++mPosition;
                normalize_forward();
                return result;
              }

            ConstIterator&
            operator--()
              {
                normalize_backward();
                --mPosition;
                return *this;
              }

            ConstIterator
            operator--( int )
              {
                ConstIterator result(*this);
                normalize_backward();
                --mPosition;
                return result;
              }


              // Damn again!  Problems with templates made me implement comparisons as members.

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
          const CharT* startPos = GetFragment(fragment, kFragmentAt, max(0U, Length()-aOffset));
          return ConstIterator(fragment, startPos);
        }

    public:
      virtual ~basic_nsAReadableString<CharT>() { }

      virtual PRUint32 Length() const = 0;
      PRBool IsEmpty() const { return Length()==0; }

      // PRBool IsOrdered() const;



      PRBool IsUnicode() const { return PR_FALSE; }
        // ...but note specialization for |PRUnichar|, below

      const char* GetBuffer() const { return 0; }
      const PRUnichar* GetUnicode() const { return 0; }
        // ...but note specializations for |char| and |PRUnichar|, below

      // CharT operator[]( PRUint32 ) const;
      // CharT CharAt( PRUint32 ) const;
      // CharT First() const;
      // CharT Last() const;

      // void ToLowerCase( basic_nsAWritableString<CharT>& ) const;
      // void ToUpperCase( basic_nsAWritableString<CharT>& ) const;

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



        /*
          Normally you wouldn't declare these as members...

          ...explanation to come...
        */

      PRBool operator!=( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)!=0; }
      PRBool operator< ( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)< 0; }
      PRBool operator<=( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)<=0; }
      PRBool operator==( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)==0; }
      PRBool operator>=( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)>=0; }
      PRBool operator> ( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)> 0; }
  };

NS_DEF_STRING_COMPARISONS(basic_nsAReadableString<CharT>)



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
class basic_nsLiteralString
      : public basic_nsAReadableString<CharT>
  {
    typedef typename basic_nsAReadableString<CharT>::FragmentRequest  FragmentRequest;
    typedef typename basic_nsAWritableString<CharT>::ConstFragment    ConstFragment;

    protected:
      virtual const CharT* GetFragment( ConstFragment&, FragmentRequest, PRUint32 ) const;

    public:
    
        // Note: _not_ explicit
      basic_nsLiteralString( const CharT* aLiteral )
          : mStart(aLiteral),
            mEnd(mStart + char_traits<CharT>::length(mStart))
        {
          // nothing else to do here
        }

      basic_nsLiteralString( const CharT* aLiteral, PRUint32 aLength )
          : mStart(aLiteral)
            mEnd(mStart + aLength)
        {
          // nothing else to do here
        }

      virtual PRUint32 Length() const;

    private:
      const CharT* mStart;
      const CharT* mEnd;
  };

NS_DEF_STRING_COMPARISONS(basic_nsLiteralString<CharT>)

template <class CharT>
class nsPromiseConcatenation
      : public basic_nsAReadableString<CharT>
    /*
      ...not unlike RickG's original |nsSubsumeString| in _intent_.
    */
  {
    typedef typename basic_nsAReadableString<CharT>::FragmentRequest  FragmentRequest;
    typedef typename basic_nsAWritableString<CharT>::ConstFragment    ConstFragment;

    protected:
      virtual const CharT* GetFragment( ConstFragment&, FragmentRequest, PRUint32 ) const;

      static const int kLeftString = 0;
      static const int kRightString = 1;

      int
      current_string( const ConstFragment& aFragment ) const
        {
          return (aFragment.mFragmentIdentifier & mFragmentIdentifierMask) ? kRightString : kLeftString;
        }

      int
      use_left_string( ConstFragment& aFragment ) const
        {
          aFragment.mFragmentIdentifier &= ~mFragmentIdentifierMask;
          return kLeftString;
        }

      int
      use_right_string( ConstFragment& aFragment ) const
        {
          aFragment.mFragmentIdentifier |= mFragmentIdentifierMask;
          return kRightString;
        }

    public:
      nsPromiseConcatenation( const basic_nsAReadableString<CharT>& aLeftString, const basic_nsAReadableString<CharT>& aRightString, PRUint32 aMask = 1 )
          : mFragmentIdentifierMask(aMask)
        {
          mStrings[kLeftString] = &aLeftString;
          mStrings[kRightString] = &aRightString;
        }

      virtual PRUint32 Length() const;

      nsPromiseConcatenation<CharT> operator+( const basic_nsAReadableString<CharT>& rhs ) const;

    private:
      void operator+( const nsPromiseConcatenation<CharT>& ); // NOT TO BE IMPLEMENTED
        // making this |private| stops you from over parenthesizing concatenation expressions, e.g., |(A+B) + (C+D)|
        //  which would break the algorithm for distributing bits in the fragment identifier

    private:
      const basic_nsAReadableString<CharT>* mStrings[2];
      PRUint32 mFragmentIdentifierMask;
  };

NS_DEF_STRING_COMPARISONS(nsPromiseConcatenation<CharT>)

template <class CharT>
PRUint32
nsPromiseConcatenation<CharT>::Length() const
  {
    return mStrings[kLeftString]->Length() + mStrings[kRightString]->Length();
  }

template <class CharT>
const CharT*
nsPromiseConcatenation<CharT>::GetFragment( ConstFragment& aFragment, FragmentRequest aRequest, PRUint32 aPosition ) const
  {
    const int kLeftString   = 0;
    const int kRightString  = 1;

    int whichString;

    switch ( aRequest )
      {
        case kPrevFragment:
        case kNextFragment:
          whichString = current_string(aFragment);
          break;

        case kFirstFragment:
          whichString = use_left_string(aFragment);
          break;

        case kLastFragment:
          whichString = use_right_string(aFragment);
          break;

        case kFragmentAt:
          PRUint32 leftLength = mStrings[kLeftString]->Length();
          if ( aPosition < leftLength )
            whichString = use_left_string(aFragment);
          else
            {
              whichString = use_right_string(aFragment);
              aPosition -= leftLength;
            }
          break;
            
      }

    const CharT* result;
    bool done;
    do
      {
        done = true;
        result = mStrings[whichString]->GetFragment(aFragment, aRequest, aPosition);

        if ( !result )
          {
            done = false;
            if ( aRequest == kNextFragment && whichString == kLeftString )
              {
                aRequest = kFirstFragment;
                whichString = use_right_string(aFragment);
              }
            else if ( aRequest == kPrevFragment && whichString == kRightString )
              {
                aRequest = kLastFragment;
                whichString = use_left_string(aFragment);
              }
            else
              done = true;
          }
      }
    while ( !done );
    return result;
  }

template <class CharT>
nsPromiseConcatenation<CharT>
nsPromiseConcatenation<CharT>::operator+( const basic_nsAReadableString<CharT>& rhs ) const
  {
    return nsPromiseConcatenation<CharT>(*this, rhs, mFragmentIdentifierMask<<1);
  }


template <class CharT>
class nsPromiseSubstring
      : public basic_nsAReadableString<CharT>
  {
    typedef typename basic_nsAReadableString<CharT>::FragmentRequest  FragmentRequest;
    typedef typename basic_nsAWritableString<CharT>::ConstFragment    ConstFragment;

    protected:
      virtual const CharT* GetFragment( ConstFragment&, FragmentRequest, PRUint32 ) const;

    public:
      nsPromiseSubstring( const basic_nsAReadableString<CharT>& aString, PRUint32 aStartPos, PRUint32 aLength )
          : mString(aString),
            mStartPos( min(aStartPos, aString.Length()) ),
            mLength( min(aLength, aString.Length()-mStartPos) )
        {
          // nothing else to do here
        }

      virtual PRUint32 Length() const;

    private:
      const basic_nsAReadableString<CharT>& mString;
      PRUint32 mStartPos;
      PRUint32 mLength;
  };

NS_DEF_STRING_COMPARISONS(nsPromiseSubstring<CharT>)

template <class CharT>
PRUint32
nsPromiseSubstring<CharT>::Length() const
  {
    return mLength;
  }

template <class CharT>
const CharT*
nsPromiseSubstring<CharT>::GetFragment( ConstFragment& aFragment, FragmentRequest aRequest, PRUint32 aPosition ) const
  {
    if ( aRequest == kFirstFragment )
      {
        aPosition = mStartPos;
        aRequest = kFragmentAt;
      }
    else if ( aRequest == kLastFragment )
      {
        aPosition = mLength + mStartPos;
        aRequest = kFragmentAt;
      }
    else if ( aRequest == kFragmentAt )
      aPosition += mStartPos;

    return mString.GetFragment(aFragment, aRequest, aPosition);
  }

template <class CharT>
nsPromiseSubstring<CharT>
Substring( const basic_nsAReadableString<CharT>& aString, PRUint32 aStartPos, PRUint32 aSubstringLength )
  {
    return nsPromiseSubstring<CharT>(aString, aStartPos, aSubstringLength);
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



template <class CharT>
PRUint32
basic_nsAReadableString<CharT>::Left( basic_nsAWritableString<CharT>& aResult, PRUint32 aLengthToCopy ) const
  {
    aResult = Substring(*this, 0, aLengthToCopy);
    return aResult.Length();
  }

template <class CharT>
PRUint32
basic_nsAReadableString<CharT>::Mid( basic_nsAWritableString<CharT>& aResult, PRUint32 aStartPos, PRUint32 aLengthToCopy ) const
  {
    aResult = Substring(*this, aStartPos, aLengthToCopy);
    return aResult.Length();
  }

template <class CharT>
PRUint32
basic_nsAReadableString<CharT>::Right( basic_nsAWritableString<CharT>& aResult, PRUint32 aLengthToCopy ) const
  {
    PRUint32 myLength = Length();
    aLengthToCopy = min(myLength, aLengthToCopy);
    aResult = Substring(*this, myLength-aLengthToCopy, aLengthToCopy);
    return aResult.Length();
  }

template <class CharT>
int
Compare( const basic_nsAReadableString<CharT>& lhs, const basic_nsAReadableString<CharT>& rhs )
  {
      /*
        If this turns out to be too slow (after measurement), there are two important modifications
          1) chunky iterators
          2) use char_traits<T>::compare
      */

    PRUint32 lLength = lhs.Length();
    PRUint32 rLength = rhs.Length();
    int result = 0;
    if ( lLength < rLength )
      result = -1;
    else if ( lLength > rLength )
      result = 1;

    PRUint32 lengthToCompare = min(lLength, rLength);

    typedef typename basic_nsAReadableString<CharT>::ConstIterator ConstIterator;
    ConstIterator lPos = lhs.Begin();
    ConstIterator lEnd = lhs.Begin(lengthToCompare);
    ConstIterator rPos = rhs.Begin();

    while ( lPos != lEnd )
      {
        if ( *lPos < *rPos )
          return -1;
        if ( *rPos < *lPos )
          return 1;

        ++lPos;
        ++rPos;
      }

    return result;
  }

template <class CharT>
int
Compare( const basic_nsAReadableString<CharT>& lhs, const CharT* rhs )
  {
    return Compare(lhs, basic_nsLiteralString<CharT>(rhs));
  }

template <class CharT>
int
Compare( const CharT* lhs, const basic_nsAReadableString<CharT>& rhs )
  {
    return Compare(basic_nsLiteralString<CharT>(lhs), rhs);
  }

template <class CharT>
inline
int
basic_nsAReadableString<CharT>::Compare( const basic_nsAReadableString<CharT>& rhs ) const
  {
    return ::Compare(*this, rhs);
  }



  /*
    How shall we provide |operator+()|?

    What would it return?  It has to return a stack based object, because the client will
    not be given an opportunity to handle memory management in an expression like

      myWritableString = stringA + stringB + stringC;

    ...so the `obvious' answer of returning a new |nsSharedString| is no good.  We could
    return an |nsString|, if that name were in scope here, though there's no telling what the client
    will really want to do with the result.  What might be better, though,
    is to return a `promise' to concatenate some strings...

    By making |nsPromiseConcatenation| inherit from readable strings, we automatically handle
    assignment and other interesting uses within writable strings, plus we drastically reduce
    the number of cases we have to write |operator+()| for.  The cost is extra temporary concat strings
    in the evaluation of strings of '+'s, e.g., |A + B + C + D|, and that we have to do some work
    to implement the virtual functions of readables.
  */

template <class CharT>
nsPromiseConcatenation<CharT>
operator+( const basic_nsAReadableString<CharT>& lhs, const basic_nsAReadableString<CharT>& rhs )
  {
    return nsPromiseConcatenation<CharT>(lhs, rhs);
  }

template <class CharT>
nsPromiseConcatenation<CharT>
operator+( const basic_nsAReadableString<CharT>& lhs, const basic_nsLiteralString<CharT>& rhs )
  {
    return nsPromiseConcatenation<CharT>(lhs, rhs);
  }

template <class CharT>
nsPromiseConcatenation<CharT>
operator+( const basic_nsLiteralString<CharT>& lhs, const basic_nsAReadableString<CharT>& rhs )
  {
    return nsPromiseConcatenation<CharT>(lhs, rhs);
  }

template <class CharT>
nsPromiseConcatenation<CharT>
operator+( const basic_nsLiteralString<CharT>& lhs, const basic_nsLiteralString<CharT>& rhs )
  {
    return nsPromiseConcatenation<CharT>(lhs, rhs);
  }




template <class CharT, class TraitsT>
basic_ostream<CharT, class TraitsT>&
operator<<( basic_ostream<CharT, TraitsT>& os, const basic_nsAReadableString<CharT>& s )
  {
    std::copy(s.Begin(), s.End(), ostream_iterator<CharT, CharT, TraitsT>(os));
    return os;
  }

typedef basic_nsAReadableString<PRUnichar>  nsAReadableString;
typedef basic_nsAReadableString<char>       nsAReadableCString;

typedef basic_nsLiteralString<PRUnichar>    nsLiteralString;
typedef basic_nsLiteralString<char>         nsLiteralCString;

#endif // !defined(_nsAReadableString_h__)
