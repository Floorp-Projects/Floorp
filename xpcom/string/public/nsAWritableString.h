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


template <class CharT>
struct nsWritableFragment
  {
    CharT*    mStart;
    CharT*    mEnd;
    PRUint32  mFragmentIdentifier;

    nsWritableFragment()
        : mStart(0), mEnd(0), mFragmentIdentifier(0)
      {
        // nothing else to do here
      }
  };

template <class CharT> class basic_nsAWritableString;

template <class CharT>
class nsWritingIterator
    : public bidirectional_iterator_tag
  {
    public:
      typedef ptrdiff_t                   difference_type;
      typedef CharT                       value_type;
      typedef CharT*                      pointer;
      typedef CharT&                      reference;
      typedef bidirectional_iterator_tag  iterator_category;

    private:
      friend class basic_nsAWritableString<CharT>;

      nsWritableFragment<CharT>       mFragment;
      CharT*                          mPosition;
      basic_nsAWritableString<CharT>* mOwningString;

      inline void normalize_forward();
      inline void normalize_backward();

      nsWritingIterator( nsWritableFragment<CharT>& aFragment,
                         CharT* aStartingPosition,
                         basic_nsAWritableString<CharT>& aOwningString )
          : mFragment(aFragment),
            mPosition(aStartingPosition),
            mOwningString(&aOwningString)
        {
          // nothing else to do here
        }

    public:
      // nsWritingIterator( const nsWritingIterator<CharT>& ); ...use default copy-constructor
      // nsWritingIterator<CharT>& operator=( const nsWritingIterator<CharT>& ); ...use default copy-assignment operator

      
      reference
      operator*() const
        {
          return *mPosition;
        }

      pointer
      operator->() const
        {
          return mPosition;
        }

      nsWritingIterator<CharT>&
      operator++()
        {
          ++mPosition;
          normalize_forward();
          return *this;
        }

      nsWritingIterator<CharT>
      operator++( int )
        {
          nsWritingIterator<CharT> result(*this);
          ++mPosition;
          normalize_forward();
          return result;
        }

      nsWritingIterator<CharT>&
      operator--()
        {
          normalize_backward();
          --mPosition;
          return *this;
        }

      nsWritingIterator<CharT>
      operator--( int )
        {
          nsWritingIterator<CharT> result(*this);
          normalize_backward();
          --mPosition;
          return result;
        }

      const nsWritableFragment<CharT>&
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

      nsWritingIterator<CharT>&
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

      nsWritingIterator<CharT>&
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
    // friend class nsWritingIterator<CharT>;

    public:
      typedef PRUint32                  size_type;
      typedef PRUint32                  index_type;

      typedef nsWritingIterator<CharT>  iterator;

      virtual CharT* GetWritableFragment( nsWritableFragment<CharT>&, nsFragmentRequest, PRUint32 = 0 ) = 0;


      nsWritingIterator<CharT>
      BeginWriting( PRUint32 aOffset = 0 )
        {
          nsWritableFragment<CharT> fragment;
          CharT* startPos = GetWritableFragment(fragment, kFragmentAt, aOffset);
          return nsWritingIterator<CharT>(fragment, startPos, *this);
        }


      nsWritingIterator<CharT>
      EndWriting( PRUint32 aOffset = 0 )
        {
          nsWritableFragment<CharT> fragment;
          CharT* startPos = GetWritableFragment(fragment, kFragmentAt, NS_MAX(0U, Length()-aOffset));
          return nsWritingIterator<CharT>(fragment, startPos, *this);
        }


      virtual void SetCapacity( PRUint32 ) = 0;
      virtual void SetLength( PRUint32 ) = 0;


      void
      Truncate( PRUint32 aNewLength=0 )
        {
          NS_ASSERTION(aNewLength<=Length(), "Can't use |Truncate()| to make a string longer.");

          if ( aNewLength < Length() )
            SetLength(aNewLength);
        }


      // PRBool SetCharAt( char_type, index_type ) = 0;



      // void ToLowerCase();
      // void ToUpperCase();

      // void StripChars( const CharT* aSet );
      // void StripChar( ... );
      // void StripWhitespace();
      // void ReplaceChar( ... );
      // void ReplaceSubstring( ... );
      // void Trim( ... );
      // void CompressSet( ... );
      // void CompressWhitespace( ... );



        //
        // |operator=()|, |Assign()|
        //

      basic_nsAWritableString<CharT>& operator=( const basic_nsAReadableString<CharT>& aReadable )  { do_AssignFromReadable(aReadable); return *this; }
      basic_nsAWritableString<CharT>& operator=( const CharT* aPtr )                                { do_AssignFromElementPtr(aPtr); return *this; }
      basic_nsAWritableString<CharT>& operator=( CharT aChar )                                      { do_AssignFromElement(aChar); return *this; }

      void Assign( const basic_nsAReadableString<CharT>& aReadable )                                { do_AssignFromReadable(aReadable); }
//    void Assign( const nsReadingIterator<CharT>& aStart, const nsReadingIterator<CharT>& aEnd )   { do_AssignFromIterators(aStart, aEnd); }
      void Assign( const CharT* aPtr )                                                              { do_AssignFromElementPtr(aPtr); }
      void Assign( const CharT* aPtr, PRUint32 aLength )                                            { do_AssignFromElementPtrLength(aPtr, aLength); }
      void Assign( CharT aChar )                                                                    { do_AssignFromElement(aChar); }



        //
        // |operator+=()|, |Append()|
        //

      basic_nsAWritableString<CharT>& operator+=( const basic_nsAReadableString<CharT>& aReadable ) { do_AppendFromReadable(aReadable); return *this; }
      basic_nsAWritableString<CharT>& operator+=( const CharT* aPtr )                               { do_AppendFromElementPtr(aPtr); return *this; }
      basic_nsAWritableString<CharT>& operator+=( CharT aChar )                                     { do_AppendFromElement(aChar); return *this; }

      void Append( const basic_nsAReadableString<CharT>& aReadable )                                { do_AppendFromReadable(aReadable); }
//    void Append( const nsReadingIterator<CharT>& aStart, const nsReadingIterator<CharT>& aEnd )   { do_AppendFromIterators(aStart, aEnd); }
      void Append( const CharT* aPtr )                                                              { do_AppendFromElementPtr(aPtr); }
      void Append( const CharT* aPtr, PRUint32 aLength )                                            { do_AppendFromElementPtrLength(aPtr, aLength); }
      void Append( CharT aChar )                                                                    { do_AppendFromElement(aChar); }



        //
        // |Insert()|
        //  Note: I would really like to move the |atPosition| parameter to the front of the argument list
        //

      void Insert( const basic_nsAReadableString<CharT>& aReadable, PRUint32 atPosition )                               { do_InsertFromReadable(aReadable, atPosition); }
//    void Insert( const nsReadingIterator<CharT>& aStart, const nsReadingIterator<CharT>& aEnd, PRUint32 atPosition )  { do_InsertFromIterators(aStart, aEnd, atPosition); }
      void Insert( const CharT* aPtr, PRUint32 atPosition )                                                             { do_InsertFromElementPtr(aPtr, atPosition); }
      void Insert( const CharT* aPtr, PRUint32 atPosition, PRUint32 aLength )                                           { do_InsertFromElementPtrLength(aPtr, atPosition, aLength); }
      void Insert( CharT aChar, PRUint32 atPosition )                                                                   { do_InsertFromElement(aChar, atPosition); }



      virtual void Cut( PRUint32 cutStart, PRUint32 cutLength );



      void Replace( PRUint32 cutStart, PRUint32 cutLength, const basic_nsAReadableString<CharT>& );
      

    protected:
      virtual void do_AssignFromReadable( const basic_nsAReadableString<CharT>& );
//    virtual void do_AssignFromIterators( nsReadingIterator<CharT>, nsReadingIterator<CharT> );
      virtual void do_AssignFromElementPtr( const CharT* );
      virtual void do_AssignFromElementPtrLength( const CharT*, PRUint32 );
      virtual void do_AssignFromElement( CharT );

      virtual void do_AppendFromReadable( const basic_nsAReadableString<CharT>& );
//    virtual void do_AppendFromIterators( nsReadingIterator<CharT>, nsReadingIterator<CharT> );
      virtual void do_AppendFromElementPtr( const CharT* );
      virtual void do_AppendFromElementPtrLength( const CharT*, PRUint32 );
      virtual void do_AppendFromElement( CharT );

      virtual void do_InsertFromReadable( const basic_nsAReadableString<CharT>&, PRUint32 );
//    virtual void do_InsertFromIterators( nsReadingIterator<CharT>, nsReadingIterator<CharT>, PRUint32 );
      virtual void do_InsertFromElementPtr( const CharT*, PRUint32 );
      virtual void do_InsertFromElementPtrLength( const CharT*, PRUint32, PRUint32 );
      virtual void do_InsertFromElement( CharT, PRUint32 );
  };



  //
  // |nsWritingIterator|s
  //

template <class CharT>
inline
void
nsWritingIterator<CharT>::normalize_forward()
  {
    if ( mPosition == mFragment.mEnd )
      if ( mOwningString->GetWritableFragment(mFragment, kNextFragment) )
        mPosition = mFragment.mStart;
  }

template <class CharT>
inline
void
nsWritingIterator<CharT>::normalize_backward()
  {
    if ( mPosition == mFragment.mStart )
      if ( mOwningString->GetWritableFragment(mFragment, kPrevFragment) )
        mPosition = mFragment.mEnd;
  }

template <class CharT>
inline
PRBool
operator==( const nsWritingIterator<CharT>& lhs, const nsWritingIterator<CharT>& rhs )
  {
    return lhs.operator->() == rhs.operator->();
  }



  //
  // |Assign()|
  //

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AssignFromReadable( const basic_nsAReadableString<CharT>& rhs )
  {
    SetLength(rhs.Length());
    copy_string(rhs.BeginReading(), rhs.EndReading(), BeginWriting());
  }

#if 0
template <class CharT>
void
basic_nsAWritableString<CharT>::do_AssignFromIterators( const nsReadingIterator<CharT>& aStart, const nsReadingIterator<CharT>& aEnd )
  {
    SetLength(distance(aStart, aEnd));
    copy_string(aStart, aEnd, BeginWriting());
  }
#endif

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AssignFromElementPtr( const CharT* aPtr )
  {
    do_AssignFromReadable(basic_nsLiteralString<CharT>(aPtr));
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AssignFromElementPtrLength( const CharT* aPtr, PRUint32 aLength )
  {
    do_AssignFromReadable(basic_nsLiteralString<CharT>(aPtr, aLength));
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AssignFromElement( CharT aChar )
  {
    do_AssignFromReadable(basic_nsLiteralChar<CharT>(aChar));
  }



  //
  // |Append()|
  //

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AppendFromReadable( const basic_nsAReadableString<CharT>& rhs )
  {
    PRUint32 oldLength = Length();
    SetLength(oldLength + rhs.Length());
    copy_string(rhs.BeginReading(), rhs.EndReading(), BeginWriting(oldLength));
  }

#if 0
template <class CharT>
void
basic_nsAWritableString<CharT>::do_AppendFromIterators( const nsReadingIterator<CharT>& aStart, const nsReadingIterator<CharT>& aEnd )
  {
    PRUint32 oldLength = Length();
    SetLength(oldLength + distance(aStart, aEnd));
    copy_string(aStart, aEnd, BeginWriting(oldLength));
  }
#endif

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AppendFromElementPtr( const CharT* aChar )
  {
    do_AppendFromReadable(basic_nsLiteralString<CharT>(aChar));
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AppendFromElementPtrLength( const CharT* aChar, PRUint32 aLength )
  {
    do_AppendFromReadable(basic_nsLiteralString<CharT>(aChar, aLength));
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AppendFromElement( CharT aChar )
  {
    do_AppendFromReadable(basic_nsLiteralChar<CharT>(aChar));
  }



  //
  // |Insert()|
  //

template <class CharT>
void
basic_nsAWritableString<CharT>::do_InsertFromReadable( const basic_nsAReadableString<CharT>& aReadable, PRUint32 atPosition )
  {
    PRUint32 oldLength = Length();
    SetLength(oldLength + aReadable.Length());
    if ( atPosition < oldLength )
      copy_string_backward(BeginReading(atPosition), BeginReading(oldLength), EndWriting());
    else
      atPosition = oldLength;
    copy_string(aReadable.BeginReading(), aReadable.EndReading(), BeginWriting(atPosition));
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_InsertFromElementPtr( const CharT* aPtr, PRUint32 atPosition )
  {
    do_InsertFromReadable(basic_nsLiteralString<CharT>(aPtr), atPosition);
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_InsertFromElementPtrLength( const CharT* aPtr, PRUint32 atPosition, PRUint32 aLength )
  {
    do_InsertFromReadable(basic_nsLiteralString<CharT>(aPtr, aLength), atPosition);
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_InsertFromElement( CharT aChar, PRUint32 atPosition )
  {
    do_InsertFromReadable(basic_nsLiteralChar<CharT>(aChar), atPosition);
  }



  //
  // |Cut()|
  //

template <class CharT>
void
basic_nsAWritableString<CharT>::Cut( PRUint32 cutStart, PRUint32 cutLength )
  {
    copy_string(BeginReading(cutStart+cutLength), EndReading(), BeginWriting(cutStart));
    SetLength(Length()-cutLength);
  }



  //
  // |Replace()|
  //

template <class CharT>
void
basic_nsAWritableString<CharT>::Replace( PRUint32 cutStart, PRUint32 cutLength, const basic_nsAReadableString<CharT>& aReplacement )
  {
    PRUint32 oldLength = Length();

    cutStart = NS_MIN(cutStart, oldLength);
    cutLength = NS_MIN(cutLength, oldLength-cutStart);
    PRUint32 cutEnd = cutStart + cutLength;

    PRUint32 replacementLength = aReplacement.Length();
    PRUint32 replacementEnd = cutStart + replacementLength;

    PRUint32 newLength = oldLength - cutLength + replacementLength;

    if ( cutLength > replacementLength )
      copy_string(BeginReading(cutEnd), EndReading(), BeginWriting(replacementEnd));
    SetLength(newLength);
    if ( cutLength < replacementLength )
      copy_string_backward(BeginReading(cutEnd), BeginReading(oldLength), BeginWriting(replacementEnd));

    copy_string(aReplacement.BeginReading(), aReplacement.EndReading(), BeginWriting(cutStart));
  }



  //
  // Types
  //

typedef basic_nsAWritableString<PRUnichar>  nsAWritableString;
typedef basic_nsAWritableString<char>       nsAWritableCString;

#endif // !defined(_nsAWritableString_h__)
