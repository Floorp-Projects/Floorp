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

#ifndef nscore_h___
#include "nscore.h"
  // for |PRUnichar|
#endif

#ifndef _nsCharTraits_h__
#include "nsCharTraits.h"
#endif

#include <iterator>
  // for |bidirectional_iterator_tag|

#include <algorithm>
  // for |min|, |copy|


#ifdef HAVE_CPP_NAMESPACE_STD
using std::bidirectional_iterator_tag;
#endif

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

enum nsFragmentRequest { kPrevFragment, kFirstFragment, kLastFragment, kNextFragment, kFragmentAt };

template <class CharT>
struct nsReadableFragment
  {
    const CharT*  mStart;
    const CharT*  mEnd;
    PRUint32      mFragmentIdentifier;

    nsReadableFragment()
        : mStart(0), mEnd(0), mFragmentIdentifier(0)
      {
        // nothing else to do here
      }
  };

template <class CharT> class basic_nsAReadableString;

template <class CharT> class basic_nsAWritableString;
  // ...because we sometimes use them as `out' params


template <class CharT>
class nsReadingIterator
      : public bidirectional_iterator_tag
  {
    public:
      typedef ptrdiff_t                   difference_type;
      typedef CharT                       value_type;
      typedef const CharT*                pointer;
      typedef const CharT&                reference;
      typedef bidirectional_iterator_tag  iterator_category;

    private:
      friend class basic_nsAReadableString<CharT>;

      nsReadableFragment<CharT>             mFragment;
      const CharT*                          mPosition;
      const basic_nsAReadableString<CharT>* mOwningString;

      inline void normalize_forward();
      inline void normalize_backward();

      nsReadingIterator( const nsReadableFragment<CharT>& aFragment,
                         const CharT* aStartingPosition,
                         const basic_nsAReadableString<CharT>& aOwningString )
          : mFragment(aFragment),
            mPosition(aStartingPosition),
            mOwningString(&aOwningString)
        {
          // nothing else to do here
        }

    public:
      // nsReadingIterator( const nsReadingIterator<CharT>& ); ...use default copy-constructor
      // nsReadingIterator<CharT>& operator=( const nsReadingIterator<CharT>& ); ...use default copy-assignment operator

      
      CharT
      operator*() const
        {
          return *mPosition;
        }

      pointer
      operator->() const
        {
          return mPosition;
        }

      nsReadingIterator<CharT>&
      operator++()
        {
          ++mPosition;
          normalize_forward();
          return *this;
        }

      nsReadingIterator<CharT>
      operator++( int )
        {
          nsReadingIterator<CharT> result(*this);
          ++mPosition;
          normalize_forward();
          return result;
        }

      nsReadingIterator<CharT>&
      operator--()
        {
          normalize_backward();
          --mPosition;
          return *this;
        }

      nsReadingIterator<CharT>
      operator--( int )
        {
          nsReadingIterator<CharT> result(*this);
          normalize_backward();
          --mPosition;
          return result;
        }

      const nsReadableFragment<CharT>&
      fragment() const
        {
          return mFragment;
        }

      difference_type
      size_forward() const
        {
          return mFragment.mEnd - mPosition;
        }

      difference_type
      size_backward() const
        {
          return mPosition - mFragment.mStart;
        }

      nsReadingIterator<CharT>&
      operator+=( difference_type n )
        {
          if ( n < 0 )
            return operator-=(-n);

          while ( n )
            {
              difference_type one_hop = NS_MIN(n, size_forward());
              mPosition += one_hop;
              normalize_forward();
              n -= one_hop;
            }

          return *this;
        }

      nsReadingIterator<CharT>&
      operator-=( difference_type n )
        {
          if ( n < 0 )
            return operator+=(-n);

          while ( n )
            {
              difference_type one_hop = NS_MIN(n, size_backward());
              mPosition -= one_hop;
              normalize_backward();
              n -= one_hop;
            }

          return *this;
        }
  };


  //
  // nsAReadable[C]String
  //

template <class CharT>
class basic_nsAReadableString
    /*
      ...
    */
  {
    public:
      typedef PRUint32                  size_type;
      typedef PRUint32                  index_type;

      typedef nsReadingIterator<CharT>  const_iterator;



      virtual ~basic_nsAReadableString() { }
        // ...yes, I expect to be sub-classed.

      nsReadingIterator<CharT>  BeginReading( PRUint32 aOffset = 0 ) const;
      nsReadingIterator<CharT>  EndReading( PRUint32 aOffset = 0 ) const;

      virtual PRUint32  Length() const = 0;
      PRBool  IsEmpty() const;

      CharT  CharAt( PRUint32 ) const;
      CharT  operator[]( PRUint32 ) const;
      CharT  First() const;
      CharT  Last() const;

      PRUint32  CountChar( CharT ) const;

      PRUint32  Left( basic_nsAWritableString<CharT>&, PRUint32 ) const;
      PRUint32  Mid( basic_nsAWritableString<CharT>&, PRUint32, PRUint32 ) const;
      PRUint32  Right( basic_nsAWritableString<CharT>&, PRUint32 ) const;

      // Find( ... ) const;
      // FindChar( ... ) const;
      // FindCharInSet( ... ) const;
      // RFind( ... ) const;
      // RFindChar( ... ) const;
      // RFindCharInSet( ... ) const;


      int  Compare( const basic_nsAReadableString<CharT>& rhs ) const;
      int  Compare( const CharT* ) const;
      int  Compare( const CharT*, PRUint32 ) const;
//    int  Compare( CharT ) const;

        // |Equals()| is a synonym for |Compare()|
      PRBool  Equals( const basic_nsAReadableString<CharT>& rhs ) const;
      PRBool  Equals( const CharT* ) const;
      PRBool  Equals( const CharT*, PRUint32 ) const;
//    PRBool  Equals( CharT ) const;

        // Comparison operators are all synonyms for |Compare()|
      PRBool  operator!=( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)!=0; }
      PRBool  operator< ( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)< 0; }
      PRBool  operator<=( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)<=0; }
      PRBool  operator==( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)==0; }
      PRBool  operator>=( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)>=0; }
      PRBool  operator> ( const basic_nsAReadableString<CharT>& rhs ) const { return Compare(rhs)> 0; }


        /*
          Shouldn't be implemented because they're i18n sensitive.
          Let's leave them in |nsString| for now.
        */

      // ToLowerCase
      // ToUpperCase
      // EqualsIgnoreCase
      // IsASCII
      // IsSpace
      // IsAlpha
      // IsDigit
      // ToFloat
      // ToInteger

      // char* ToNewCString() const;
      // char* ToNewUTF8String() const;
      // PRUnichar* ToNewUnicode() const;
      // char* ToCString( char*, PRUint32, PRUint32 ) const;


        /*
          Shouldn't be implemented because it's wrong duplication.
          Let's leave it in |nsString| for now.
        */

      // nsString* ToNewString() const;
        // NO!  The right way to say this is |new nsString( fromAReadableString )|


        /*
          Shouldn't be implemented because they're not generally applicable.
          Let's leave them in |nsString| for now.
        */

      // IsOrdered
      // BinarySearch

    // protected:
      virtual const void* Implementation() const;
      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 = 0 ) const = 0;
  };

template <class CharT>
inline
void
nsReadingIterator<CharT>::normalize_forward()
  {
    if ( mPosition == mFragment.mEnd )
      if ( mOwningString->GetReadableFragment(mFragment, kNextFragment) )
        mPosition = mFragment.mStart;
  }

template <class CharT>
inline
void
nsReadingIterator<CharT>::normalize_backward()
  {
    if ( mPosition == mFragment.mStart )
      if ( mOwningString->GetReadableFragment(mFragment, kPrevFragment) )
        mPosition = mFragment.mEnd;
  }

template <class CharT>
inline
nsReadingIterator<CharT>
basic_nsAReadableString<CharT>::BeginReading( PRUint32 aOffset ) const
  {
    nsReadableFragment<CharT> fragment;
    const CharT* startPos = GetReadableFragment(fragment, kFragmentAt, aOffset);
    return nsReadingIterator<CharT>(fragment, startPos, *this);
  }

template <class CharT>
inline
nsReadingIterator<CharT>
basic_nsAReadableString<CharT>::EndReading( PRUint32 aOffset ) const
  {
    nsReadableFragment<CharT> fragment;
    const CharT* startPos = GetReadableFragment(fragment, kFragmentAt, NS_MAX(0U, Length()-aOffset));
    return nsReadingIterator<CharT>(fragment, startPos, *this);
  }

template <class CharT>
inline
PRBool
basic_nsAReadableString<CharT>::IsEmpty() const
  {
    return Length() == 0;
  }

template <class CharT>
inline
PRBool
basic_nsAReadableString<CharT>::Equals( const basic_nsAReadableString<CharT>& rhs ) const
  {
    return Compare(rhs) == 0;
  }

template <class CharT>
inline
PRBool
operator==( const nsReadingIterator<CharT>& lhs, const nsReadingIterator<CharT>& rhs )
  {
    return lhs.operator->() == rhs.operator->();
  }

#ifdef HAVE_CPP_UNAMBIGUOUS_STD_NOTEQUAL
template <class CharT>
inline
PRBool
operator!=( const nsReadingIterator<CharT>& lhs, const nsReadingIterator<CharT>& rhs )
  {
    return lhs.operator->() != rhs.operator->();
  }
#endif

#define NS_DEF_1_STRING_COMPARISON_OPERATOR(comp, T1, T2) \
  inline                                        \
  PRBool                                        \
  operator comp( T1 lhs, T2 rhs )               \
    {                                           \
      return PRBool(Compare(lhs, rhs) comp 0);  \
    }

#define NS_DEF_STRING_COMPARISON_OPERATORS(T1, T2) \
  template <class CharT> NS_DEF_1_STRING_COMPARISON_OPERATOR(!=, T1, T2) \
  template <class CharT> NS_DEF_1_STRING_COMPARISON_OPERATOR(< , T1, T2) \
  template <class CharT> NS_DEF_1_STRING_COMPARISON_OPERATOR(<=, T1, T2) \
  template <class CharT> NS_DEF_1_STRING_COMPARISON_OPERATOR(==, T1, T2) \
  template <class CharT> NS_DEF_1_STRING_COMPARISON_OPERATOR(>=, T1, T2) \
  template <class CharT> NS_DEF_1_STRING_COMPARISON_OPERATOR(> , T1, T2)

#define NS_DEF_NON_TEMPLATE_STRING_COMPARISON_OPERATORS(T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(!=, T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(< , T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(<=, T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(==, T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(>=, T1, T2) \
  NS_DEF_1_STRING_COMPARISON_OPERATOR(> , T1, T2)

#define NS_DEF_STRING_COMPARISONS(T) \
  NS_DEF_STRING_COMPARISON_OPERATORS(const T&, const CharT*) \
  NS_DEF_STRING_COMPARISON_OPERATORS(const CharT*, const T&)


NS_DEF_STRING_COMPARISONS(basic_nsAReadableString<CharT>)



template <class CharT>
const void*
basic_nsAReadableString<CharT>::Implementation() const
  {
    return 0;
  }



  /*
    Note: the following four functions, |CharAt|, |operator[]|, |First|, and |Last|, are implemented
    in the simplest reasonable scheme; by calling |GetReadableFragment| and resolving the pointer it
    returns.  The alternative is to force at least one of these methods to be |virtual|.  The ideal
    candidate for that change would be |CharAt|.

    This is something to measure in the context of how string classes are actually used.  In practice,
    do people extract a character at a time in performance critical places?  If so, can they use
    iterators instead?  If they must extract single characters, _and_ they can't use iterators, _and_
    it happens enough to notice, then we'll take the hit and make |CharAt| virtual.
  */

template <class CharT>
inline
CharT
basic_nsAReadableString<CharT>::CharAt( PRUint32 aIndex ) const
  {
      // ??? Is |CharAt()| supposed to be the 'safe' version?
    nsReadableFragment<CharT> fragment;
    return *GetReadableFragment(fragment, kFragmentAt, aIndex);
  }

template <class CharT>
inline
CharT
basic_nsAReadableString<CharT>::operator[]( PRUint32 aIndex ) const
  {
    return CharAt(aIndex);
  }

template <class CharT>
inline
CharT
basic_nsAReadableString<CharT>::First() const
  {
    return CharAt(0);
  }

template <class CharT>
inline
CharT
basic_nsAReadableString<CharT>::Last() const
  {
    return CharAt(Length()-1);
  }

template <class CharT>
PRUint32
basic_nsAReadableString<CharT>::CountChar( CharT c ) const
  {
#if 1
    return PRUint32(count(BeginReading(), EndReading(), c));
#else
    PRUint32 result = 0;
    PRUint32 lengthToExamine = Length();

    nsReadingIterator<CharT> iter( BeginReading() );
    for (;;)
      {
        PRUint32 lengthToExamineInThisFragment = iter.size_forward();
        result += PRUint32(count(iter.operator->(), iter.operator->()+lengthToExamineInThisFragment, c));
        if ( !(lengthToExamine -= lengthToExamineInThisFragment) )
          return result;
        iter += lengthToExamineInThisFragment;
      }
#endif
  }


  /*
    Note: |Left()|, |Mid()|, and |Right()| could be modified to notice when they degenerate into copying the
    entire string, and call |Assign()| instead.  This would be a win when the underlying implementation of
    both strings could do buffer sharing.  This is _definitely_ something that should be measured before
    being implemented.
  */

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
    aLengthToCopy = NS_MIN(myLength, aLengthToCopy);
    aResult = Substring(*this, myLength-aLengthToCopy, aLengthToCopy);
    return aResult.Length();
  }



template <class CharT>
inline
int
basic_nsAReadableString<CharT>::Compare( const basic_nsAReadableString<CharT>& rhs ) const
  {
    return ::Compare(*this, rhs);
  }






  //
  // nsLiteral[C]String
  //

template <class CharT>
class basic_nsLiteralString
      : public basic_nsAReadableString<CharT>
    /*
      ...this class wraps a constant literal string and lets it act like an |nsAReadable...|.
      
      Use it like this:

        SomeFunctionTakingACString( nsLiteralCString("Hello, World!") );

      With some tweaking, I think I can make this work as well...

        SomeStringFunc( nsLiteralString( L"Hello, World!" ) );

      This class just holds a pointer.  If you don't supply the length, it must calculate it.
      No copying or allocations are performed.

      |const basic_nsLiteralString<CharT>&| appears frequently in interfaces because it
      allows the automatic conversion of a |CharT*|.
    */
  {
    protected:
      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 ) const;

    public:
    
        // Note: _not_ explicit
      basic_nsLiteralString( const CharT* aLiteral )
          : mStart(aLiteral),
            mEnd(mStart + nsCharTraits<CharT>::length(mStart))
        {
          // nothing else to do here
        }

      basic_nsLiteralString( const CharT* aLiteral, PRUint32 aLength )
          : mStart(aLiteral),
            mEnd(mStart + aLength)
        {
            // This is an annoying hack.  Callers should be fixed to use the other
            //  constructor if they don't really know the length.
          if ( aLength == PRUint32(-1) )
            mEnd = mStart + nsCharTraits<CharT>::length(mStart);
        }

      virtual PRUint32 Length() const;

    private:
      const CharT* mStart;
      const CharT* mEnd;
  };

NS_DEF_STRING_COMPARISONS(basic_nsLiteralString<CharT>)

template <class CharT>
const CharT*
basic_nsLiteralString<CharT>::GetReadableFragment( nsReadableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
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


// XXX Note that these are located here because some compilers are
// sensitive to the ordering of declarations with regard to templates.
template <class CharT>
inline
PRBool
basic_nsAReadableString<CharT>::Equals( const CharT* rhs ) const
  {
    return Compare(basic_nsLiteralString<CharT>(rhs)) == 0;
  }

template <class CharT>
inline
PRBool
basic_nsAReadableString<CharT>::Equals( const CharT* rhs, PRUint32 rhs_length ) const
  {
    return Compare(basic_nsLiteralString<CharT>(rhs, rhs_length)) == 0;
  }

template <class CharT>
inline
int
basic_nsAReadableString<CharT>::Compare( const CharT* rhs ) const
  {
    return ::Compare(*this, NS_STATIC_CAST(basic_nsAReadableString<CharT>, basic_nsLiteralString<CharT>(rhs)));
  }

template <class CharT>
inline
int
basic_nsAReadableString<CharT>::Compare( const CharT* rhs, PRUint32 rhs_length ) const
  {
    return ::Compare(*this, NS_STATIC_CAST(basic_nsAReadableString<CharT>, basic_nsLiteralString<CharT>(rhs, rhs_length)));
  }



  //
  // nsLiteralChar, nsLiteralPRUnichar
  //

template <class CharT>
class basic_nsLiteralChar
      : public basic_nsAReadableString<CharT>
  {
    protected:
      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 ) const;

    public:

      basic_nsLiteralChar( CharT aChar )
          : mChar(aChar)
        {
          // nothing else to do here
        }

      virtual
      PRUint32
      Length() const
        {
          return 1;
        }

    private:
      CharT  mChar;
  };

template <class CharT>
const CharT*
basic_nsLiteralChar<CharT>::GetReadableFragment( nsReadableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const
  {
    switch ( aRequest )
      {
        case kFirstFragment:
        case kLastFragment:
        case kFragmentAt:
          aFragment.mEnd = (aFragment.mStart = &mChar) + 1;
          return aFragment.mStart + aOffset;
        
        case kPrevFragment:
        case kNextFragment:
        default:
          return 0;
      }
  }






  //
  // nsPromiseConcatenation
  //

template <class CharT>
class nsPromiseConcatenation
      : public basic_nsAReadableString<CharT>
    /*
      NOT FOR USE BY HUMANS

      Instances of this class only exist as anonymous temporary results from |operator+()|.
      This is the machinery that makes string concatenation efficient.  No allocations or
      character copies are required unless and until a final assignment is made.  It works
      its magic by overriding and forwarding calls to |GetReadableFragment()|.

      Note: |nsPromiseConcatenation| imposes some limits on string concatenation with |operator+()|.
        - no more than 33 strings, e.g., |s1 + s2 + s3 + ... s32 + s33|
        - left to right evaluation is required ... do not use parentheses to override this

      In practice, neither of these is onerous.  Parentheses do not change the semantics of the
      concatenation, only the order in which the result is assembled ... so there's no reason
      for a user to need to control it.  Too many strings summed together can easily be worked
      around with an intermediate assignment.  I wouldn't have the parentheses limitation if I
      assigned the identifier mask starting at the top, the first time anybody called
      |GetReadableFragment()|.
    */
  {
    protected:
      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 ) const;

      enum { kLeftString, kRightString };

      int
      GetCurrentStringFromFragment( const nsReadableFragment<CharT>& aFragment ) const
        {
          return (aFragment.mFragmentIdentifier & mFragmentIdentifierMask) ? kRightString : kLeftString;
        }

      int
      SetLeftStringInFragment( nsReadableFragment<CharT>& aFragment ) const
        {
          aFragment.mFragmentIdentifier &= ~mFragmentIdentifierMask;
          return kLeftString;
        }

      int
      SetRightStringInFragment( nsReadableFragment<CharT>& aFragment ) const
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
nsPromiseConcatenation<CharT>::GetReadableFragment( nsReadableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aPosition ) const
  {
    int whichString;

      // based on the request, pick which string we will forward the |GetReadableFragment()| call into

    switch ( aRequest )
      {
        case kPrevFragment:
        case kNextFragment:
          whichString = GetCurrentStringFromFragment(aFragment);
          break;

        case kFirstFragment:
          whichString = SetLeftStringInFragment(aFragment);
          break;

        case kLastFragment:
          whichString = SetRightStringInFragment(aFragment);
          break;

        case kFragmentAt:
          PRUint32 leftLength = mStrings[kLeftString]->Length();
          if ( aPosition < leftLength )
            whichString = SetLeftStringInFragment(aFragment);
          else
            {
              whichString = SetRightStringInFragment(aFragment);
              aPosition -= leftLength;
            }
          break;
            
      }

    const CharT* result;
    bool done;
    do
      {
        done = true;
        result = mStrings[whichString]->GetReadableFragment(aFragment, aRequest, aPosition);

        if ( !result )
          {
            done = false;
            if ( aRequest == kNextFragment && whichString == kLeftString )
              {
                aRequest = kFirstFragment;
                whichString = SetRightStringInFragment(aFragment);
              }
            else if ( aRequest == kPrevFragment && whichString == kRightString )
              {
                aRequest = kLastFragment;
                whichString = SetLeftStringInFragment(aFragment);
              }
            else
              done = true;
          }
      }
    while ( !done );
    return result;
  }

template <class CharT>
inline
nsPromiseConcatenation<CharT>
nsPromiseConcatenation<CharT>::operator+( const basic_nsAReadableString<CharT>& rhs ) const
  {
    return nsPromiseConcatenation<CharT>(*this, rhs, mFragmentIdentifierMask<<1);
  }






  //
  // nsPromiseSubstring
  //

template <class CharT>
class nsPromiseSubstring
      : public basic_nsAReadableString<CharT>
    /*
      NOT FOR USE BY HUMANS (mostly)

      ...not unlike |nsPromiseConcatenation|.  Instances of this class exist only as anonymous
      temporary results from |Substring()|.  Like |nsPromiseConcatenation|, this class only
      holds a pointer, no string data of its own.  It does its magic by overriding and forwarding
      calls to |GetReadableFragment()|.
    */
  {
    protected:
      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 ) const;

    public:
      nsPromiseSubstring( const basic_nsAReadableString<CharT>& aString, PRUint32 aStartPos, PRUint32 aLength )
          : mString(aString),
            mStartPos( NS_MIN(aStartPos, aString.Length()) ),
            mLength( NS_MIN(aLength, aString.Length()-mStartPos) )
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
nsPromiseSubstring<CharT>::GetReadableFragment( nsReadableFragment<CharT>& aFragment, nsFragmentRequest aRequest, PRUint32 aPosition ) const
  {
      // Offset any request for a specific position (First, Last, At) by our
      //  substrings startpos within the owning string

    if ( aRequest == kFirstFragment )
      {
        aPosition = mStartPos;
        aRequest = kFragmentAt;
      }
    else if ( aRequest == kLastFragment )
      {
        aPosition = mStartPos + mLength;
        aRequest = kFragmentAt;
      }
    else if ( aRequest == kFragmentAt )
      aPosition += mStartPos;

    return mString.GetReadableFragment(aFragment, aRequest, aPosition);
  }






  //
  // Global functions
  //

template <class InputIterator, class OutputIterator>
OutputIterator
copy_string( InputIterator first, InputIterator last, OutputIterator result )
  {
    while ( first != last )
      {
        PRUint32 lengthToCopy = PRUint32( NS_MIN(first.size_forward(), result.size_forward()) );
        if ( first.fragment().mStart == last.fragment().mStart )
          lengthToCopy = NS_MIN(lengthToCopy, PRUint32(last.operator->() - first.operator->()));

        NS_ASSERTION(lengthToCopy, "|copy_string| will never terminate");

        nsCharTraits<typename InputIterator::value_type>::copy(result.operator->(), first.operator->(), lengthToCopy);

        first += PRInt32(lengthToCopy);
        result += PRInt32(lengthToCopy);
      }

    return result;
  }

template <class InputIterator, class CharT>
CharT*
copy_string( InputIterator first, InputIterator last, CharT* result )
  {
    while ( first != last )
      {
        PRUint32 lengthToCopy = PRUint32(first.size_forward());
        if ( first.fragment().mStart == last.fragment().mStart )
          lengthToCopy = NS_MIN(lengthToCopy, PRUint32(last.operator->() - first.operator->()));

        NS_ASSERTION(lengthToCopy, "|copy_string| will never terminate");

        nsCharTraits<CharT>::copy(result, first.operator->(), lengthToCopy);

        first += PRInt32(lengthToCopy);
        result += PRInt32(lengthToCopy);
      }

    return result;
  }

template <class InputIterator, class OutputIterator>
OutputIterator
copy_string_backward( InputIterator first, InputIterator last, OutputIterator result )
  {
    while ( first != last )
      {
        PRUint32 lengthToCopy = PRUint32( NS_MIN(last.size_backward(), result.size_backward()) );
        if ( first.fragment().mStart == last.fragment().mStart )
          lengthToCopy = NS_MIN(lengthToCopy, PRUint32(last.operator->() - first.operator->()));

        NS_ASSERTION(lengthToCopy, "|copy_string_backward| will never terminate");

        nsCharTraits<typename InputIterator::value_type>::move(result.operator->()-lengthToCopy, last.operator->()-lengthToCopy, lengthToCopy);

        last -= PRInt32(lengthToCopy);
        result -= PRInt32(lengthToCopy);
      }

    return result;
  }

template <class CharT>
inline
PRBool
SameImplementation( const basic_nsAReadableString<CharT>& lhs, const basic_nsAReadableString<CharT>& rhs )
  {
    const void* imp_tag = lhs.Implementation();
    return imp_tag && (imp_tag==rhs.Implementation());
  }

template <class CharT>
nsPromiseSubstring<CharT>
Substring( const basic_nsAReadableString<CharT>& aString, PRUint32 aStartPos, PRUint32 aSubstringLength )
  {
    return nsPromiseSubstring<CharT>(aString, aStartPos, aSubstringLength);
  }

template <class CharT>
int
Compare( const basic_nsAReadableString<CharT>& lhs, const basic_nsAReadableString<CharT>& rhs )
  {
    if ( &lhs == &rhs )
      return 0;

    PRUint32 lLength = lhs.Length();
    PRUint32 rLength = rhs.Length();
    PRUint32 lengthToCompare = NS_MIN(lLength, rLength);

    nsReadingIterator<CharT> leftIter( lhs.BeginReading() );
    nsReadingIterator<CharT> rightIter( rhs.BeginReading() );

    for (;;)
      {
        PRUint32 lengthAvailable = PRUint32( NS_MIN(leftIter.size_forward(), rightIter.size_forward()) );

        if ( lengthAvailable > lengthToCompare )
          lengthAvailable = lengthToCompare;
        
        if ( int result = nsCharTraits<CharT>::compare(leftIter.operator->(), rightIter.operator->(), lengthAvailable) )
          return result;

        if ( !(lengthToCompare -= lengthAvailable) )
          break;

        leftIter += PRInt32(lengthAvailable);
        rightIter += PRInt32(lengthAvailable);
      }

    if ( lLength < rLength )
      return -1;
    else if ( rLength < lLength )
      return 1;
    else
      return 0;
  }

template <class CharT>
inline
int
Compare( const basic_nsAReadableString<CharT>& lhs, const CharT* rhs )
  {
    return Compare(lhs, basic_nsLiteralString<CharT>(rhs));
  }

template <class CharT>
inline
int
Compare( const CharT* lhs, const basic_nsAReadableString<CharT>& rhs )
  {
    return Compare(basic_nsLiteralString<CharT>(lhs), rhs);
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
inline
nsPromiseConcatenation<CharT>
operator+( const basic_nsAReadableString<CharT>& lhs, const basic_nsAReadableString<CharT>& rhs )
  {
    return nsPromiseConcatenation<CharT>(lhs, rhs);
  }

template <class CharT>
inline
nsPromiseConcatenation<CharT>
operator+( const basic_nsAReadableString<CharT>& lhs, const basic_nsLiteralString<CharT>& rhs )
  {
    return nsPromiseConcatenation<CharT>(lhs, rhs);
  }

template <class CharT>
inline
nsPromiseConcatenation<CharT>
operator+( const basic_nsLiteralString<CharT>& lhs, const basic_nsAReadableString<CharT>& rhs )
  {
    return nsPromiseConcatenation<CharT>(lhs, rhs);
  }

template <class CharT>
inline
nsPromiseConcatenation<CharT>
operator+( const basic_nsLiteralString<CharT>& lhs, const basic_nsLiteralString<CharT>& rhs )
  {
    return nsPromiseConcatenation<CharT>(lhs, rhs);
  }



#if 0
#ifdef STANDALONE_STRING_TESTS
template <class CharT, class TraitsT>
basic_ostream<CharT, TraitsT>&
operator<<( basic_ostream<CharT, TraitsT>& os, const basic_nsAReadableString<CharT>& s )
  {
    copy(s.BeginReading(), s.EndReading(), ostream_iterator<CharT, CharT, TraitsT>(os));
    return os;
  }
#endif
#endif

typedef basic_nsAReadableString<PRUnichar>  nsAReadableString;
typedef basic_nsAReadableString<char>       nsAReadableCString;

typedef basic_nsLiteralString<PRUnichar>    nsLiteralString;
typedef basic_nsLiteralString<char>         nsLiteralCString;

#define NS_LITERAL_STRING(s)  nsLiteralString(s, sizeof(s)/sizeof(wchar_t))
#define NS_LITERAL_CSTRING(s) nsLiteralCString(s, sizeof(s))

typedef basic_nsLiteralChar<char>       nsLiteralChar;
typedef basic_nsLiteralChar<PRUnichar>  nsLiteralPRUnichar;


#endif // !defined(_nsAReadableString_h__)
