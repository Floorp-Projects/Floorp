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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Scott Collins <scc@mozilla.org>
 *
 * Contributor(s):
 */

#ifndef nsAWritableString_h___
#define nsAWritableString_h___

  // See also...
#ifndef nsAReadableString_h___
#include "nsAReadableString.h"
#endif


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
//  : public bidirectional_iterator_tag
  {
    public:
      typedef ptrdiff_t                   difference_type;
      typedef CharT                       value_type;
      typedef CharT*                      pointer;
      typedef CharT&                      reference;
//    typedef bidirectional_iterator_tag  iterator_category;

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

      pointer
      get() const
        {
          return mPosition;
        }
      
      reference
      operator*() const
        {
          return *get();
        }

#if 0
        // An iterator really deserves this, but some compilers (notably IBM VisualAge for OS/2)
        //  don't like this when |CharT| is a type without members.
      pointer
      operator->() const
        {
          return get();
        }
#endif

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
              NS_ASSERTION(one_hop>0, "Infinite loop: can't advance a writable iterator beyond the end of a string");
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
              NS_ASSERTION(one_hop>0, "Infinite loop: can't advance (backward) a writable iterator beyond the end of a string");
              mPosition -= one_hop;
              normalize_backward();
              n -= one_hop;
            }

          return *this;
        }

      PRUint32
      write( const value_type* s, PRUint32 n )
        {
          NS_ASSERTION(size_forward() > 0, "You can't |write| into an |nsWritingIterator| with no space!");

          n = NS_MIN(n, PRUint32(size_forward()));
          nsCharTraits<value_type>::copy(mPosition, s, n);
          operator+=( difference_type(n) );
          return n;
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
          CharT* startPos = GetWritableFragment(fragment, kFragmentAt, NS_MAX(0U, this->Length()-aOffset));
          return nsWritingIterator<CharT>(fragment, startPos, *this);
        }


      virtual void SetCapacity( PRUint32 ) = 0;
      virtual void SetLength( PRUint32 ) = 0;


      void
      Truncate( PRUint32 aNewLength=0 )
        {
          NS_ASSERTION(aNewLength<=this->Length(), "Can't use |Truncate()| to make a string longer.");

          if ( aNewLength < this->Length() )
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
        // |Assign()|, |operator=()|
        //

      void Assign( const basic_nsAReadableString<CharT>& aReadable )                                { AssignFromReadable(aReadable); }
      void Assign( const nsPromiseReadable<CharT>& aReadable )                                      { AssignFromPromise(aReadable); }
      void Assign( const CharT* aPtr )                                                              { aPtr ? do_AssignFromElementPtr(aPtr) : SetLength(0); }
      void Assign( const CharT* aPtr, PRUint32 aLength )                                            { do_AssignFromElementPtrLength(aPtr, aLength); }
      void Assign( CharT aChar )                                                                    { do_AssignFromElement(aChar); }

      basic_nsAWritableString<CharT>& operator=( const basic_nsAReadableString<CharT>& aReadable )  { Assign(aReadable); return *this; }
      basic_nsAWritableString<CharT>& operator=( const nsPromiseReadable<CharT>& aReadable )        { Assign(aReadable); return *this; }
      basic_nsAWritableString<CharT>& operator=( const CharT* aPtr )                                { Assign(aPtr); return *this; }
      basic_nsAWritableString<CharT>& operator=( CharT aChar )                                      { Assign(aChar); return *this; }



        //
        // |Append()|, |operator+=()|
        //

      void Append( const basic_nsAReadableString<CharT>& aReadable )                                { AppendFromReadable(aReadable); }
      void Append( const nsPromiseReadable<CharT>& aReadable )                                      { AppendFromPromise(aReadable); }
      void Append( const CharT* aPtr )                                                              { if (aPtr) do_AppendFromElementPtr(aPtr); }
      void Append( const CharT* aPtr, PRUint32 aLength )                                            { do_AppendFromElementPtrLength(aPtr, aLength); }
      void Append( CharT aChar )                                                                    { do_AppendFromElement(aChar); }

      basic_nsAWritableString<CharT>& operator+=( const basic_nsAReadableString<CharT>& aReadable ) { Append(aReadable); return *this; }
      basic_nsAWritableString<CharT>& operator+=( const nsPromiseReadable<CharT>& aReadable )       { Append(aReadable); return *this; }
      basic_nsAWritableString<CharT>& operator+=( const CharT* aPtr )                               { Append(aPtr); return *this; }
      basic_nsAWritableString<CharT>& operator+=( CharT aChar )                                     { Append(aChar); return *this; }



        //
        // |Insert()|
        //  Note: I would really like to move the |atPosition| parameter to the front of the argument list
        //

      void Insert( const basic_nsAReadableString<CharT>& aReadable, PRUint32 atPosition )                               { InsertFromReadable(aReadable, atPosition); }
      void Insert( const nsPromiseReadable<CharT>& aReadable, PRUint32 atPosition )                                     { InsertFromPromise(aReadable, atPosition); }
      void Insert( const CharT* aPtr, PRUint32 atPosition )                                                             { if (aPtr) do_InsertFromElementPtr(aPtr, atPosition); }
      void Insert( const CharT* aPtr, PRUint32 atPosition, PRUint32 aLength )                                           { do_InsertFromElementPtrLength(aPtr, atPosition, aLength); }
      void Insert( CharT aChar, PRUint32 atPosition )                                                                   { do_InsertFromElement(aChar, atPosition); }



      virtual void Cut( PRUint32 cutStart, PRUint32 cutLength );



      void Replace( PRUint32 cutStart, PRUint32 cutLength, const basic_nsAReadableString<CharT>& aReadable )            { ReplaceFromReadable(cutStart, cutLength, aReadable); }
      void Replace( PRUint32 cutStart, PRUint32 cutLength, const nsPromiseReadable<CharT>& aReadable )                  { ReplaceFromPromise(cutStart, cutLength, aReadable); }

    private:
      typedef typename nsCharTraits<CharT>::incompatible_char_type incompatible_char_type;

        // NOT TO BE IMPLEMENTED
      void operator=  ( incompatible_char_type );
      void Assign     ( incompatible_char_type );
      void operator+= ( incompatible_char_type );
      void Append     ( incompatible_char_type );
      void Insert     ( incompatible_char_type, PRUint32 );
      

    protected:
              void AssignFromReadable( const basic_nsAReadableString<CharT>& );
              void AssignFromPromise( const basic_nsAReadableString<CharT>& );
      virtual void do_AssignFromReadable( const basic_nsAReadableString<CharT>& );
      virtual void do_AssignFromElementPtr( const CharT* );
      virtual void do_AssignFromElementPtrLength( const CharT*, PRUint32 );
      virtual void do_AssignFromElement( CharT );

              void AppendFromReadable( const basic_nsAReadableString<CharT>& );
              void AppendFromPromise( const basic_nsAReadableString<CharT>& );
      virtual void do_AppendFromReadable( const basic_nsAReadableString<CharT>& );
      virtual void do_AppendFromElementPtr( const CharT* );
      virtual void do_AppendFromElementPtrLength( const CharT*, PRUint32 );
      virtual void do_AppendFromElement( CharT );

              void InsertFromReadable( const basic_nsAReadableString<CharT>&, PRUint32 );
              void InsertFromPromise( const basic_nsAReadableString<CharT>&, PRUint32 );
      virtual void do_InsertFromReadable( const basic_nsAReadableString<CharT>&, PRUint32 );
      virtual void do_InsertFromElementPtr( const CharT*, PRUint32 );
      virtual void do_InsertFromElementPtrLength( const CharT*, PRUint32, PRUint32 );
      virtual void do_InsertFromElement( CharT, PRUint32 );

              void ReplaceFromReadable( PRUint32, PRUint32, const basic_nsAReadableString<CharT>& );
              void ReplaceFromPromise( PRUint32, PRUint32, const basic_nsAReadableString<CharT>& );
      virtual void do_ReplaceFromReadable( PRUint32, PRUint32, const basic_nsAReadableString<CharT>& );
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
    return lhs.get() == rhs.get();
  }

template <class CharT>
inline
PRBool
operator!=( const nsWritingIterator<CharT>& lhs, const nsWritingIterator<CharT>& rhs )
  {
    return lhs.get() != rhs.get();
  }



  //
  // |Assign()|
  //

template <class CharT>
void
basic_nsAWritableString<CharT>::AssignFromReadable( const basic_nsAReadableString<CharT>& rhs )
  {
    if ( NS_STATIC_CAST(const basic_nsAReadableString<CharT>*, this) != &rhs )
      do_AssignFromReadable(rhs);
    // else, self-assign is a no-op
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::AssignFromPromise( const basic_nsAReadableString<CharT>& aReadable )
    /*
      ...this function is only called when a promise that somehow references |this| is assigned _into_ |this|.
      E.g.,
      
        ... writable& w ...
        ... readable& r ...
        
        w = r + w;

      In this example, you can see that unless the characters promised by |w| in |r+w| are resolved before
      anything starts getting copied into |w|, there will be trouble.  They will be overritten by the contents
      of |r| before being retrieved to be appended.

      We could have a really tricky solution where we tell the promise to resolve _just_ the data promised
      by |this|, but this should be a rare case, since clients with more local knowledge will know that, e.g.,
      in the case above, |Insert| could have special behavior with significantly better performance.  Since
      it's a rare case anyway, we should just do the simplest thing that could possibly work, resolve the
      entire promise.  If we measure and this turns out to show up on performance radar, we then have the
      option to fix either the callers or this mechanism.
    */
  {
    if ( !aReadable.Promises(*this) )
      do_AssignFromReadable(aReadable);
    else
      {
        PRUint32 length = aReadable.Length();
        CharT* buffer = new CharT[length];
        if ( buffer )
          {
            // Note: not exception safe.  We need something to manage temporary buffers like this

            copy_string(aReadable.BeginReading(), aReadable.EndReading(), buffer);
            do_AssignFromElementPtrLength(buffer, length);
            delete buffer;
          }
        // else assert?
      }
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AssignFromReadable( const basic_nsAReadableString<CharT>& aReadable )
  {
    SetLength(0);
    SetLength(aReadable.Length());
      // first setting the length to |0| avoids copying characters only to be overwritten later
      //  in the case where the implementation decides to re-allocate

    copy_string(aReadable.BeginReading(), aReadable.EndReading(), BeginWriting());
  }

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
basic_nsAWritableString<CharT>::AppendFromReadable( const basic_nsAReadableString<CharT>& aReadable )
  {
    if ( NS_STATIC_CAST(const basic_nsAReadableString<CharT>*, this) != &aReadable )
      do_AppendFromReadable(aReadable);
    else
      AppendFromPromise(aReadable);
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::AppendFromPromise( const basic_nsAReadableString<CharT>& aReadable )
  {
    if ( !aReadable.Promises(*this) )
      do_AppendFromReadable(aReadable);
    else
      {
        PRUint32 length = aReadable.Length();
        CharT* buffer = new CharT[length];
        if ( buffer )
          {
            copy_string(aReadable.BeginReading(), aReadable.EndReading(), buffer);
            do_AppendFromElementPtrLength(buffer, length);
            delete buffer;
          }
        // else assert?
      }
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_AppendFromReadable( const basic_nsAReadableString<CharT>& aReadable )
  {
    PRUint32 oldLength = this->Length();
    SetLength(oldLength + aReadable.Length());
    copy_string(aReadable.BeginReading(), aReadable.EndReading(), BeginWriting(oldLength));
  }

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
    PRUint32 oldLength = this->Length();
    SetLength(oldLength+1);
    nsWritableFragment<CharT> fragment;
    *GetWritableFragment(fragment, kFragmentAt, oldLength) = aChar;
  }



  //
  // |Insert()|
  //

template <class CharT>
void
basic_nsAWritableString<CharT>::InsertFromReadable( const basic_nsAReadableString<CharT>& aReadable, PRUint32 atPosition )
  {
    if ( NS_STATIC_CAST(const basic_nsAReadableString<CharT>*, this) != &aReadable )
      do_InsertFromReadable(aReadable, atPosition);
    else
      InsertFromPromise(aReadable, atPosition);
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::InsertFromPromise( const basic_nsAReadableString<CharT>& aReadable, PRUint32 atPosition )
  {
    if ( !aReadable.Promises(*this) )
      do_InsertFromReadable(aReadable, atPosition);
    else
      {
        PRUint32 length = aReadable.Length();
        CharT* buffer = new CharT[length];
        if ( buffer )
          {
            copy_string(aReadable.BeginReading(), aReadable.EndReading(), buffer);
            do_InsertFromElementPtrLength(buffer, atPosition, length);
            delete buffer;
          }
        // else assert
      }
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_InsertFromReadable( const basic_nsAReadableString<CharT>& aReadable, PRUint32 atPosition )
  {
    PRUint32 oldLength = this->Length();
    SetLength(oldLength + aReadable.Length());
    if ( atPosition < oldLength )
      copy_string_backward(this->BeginReading(atPosition), this->BeginReading(oldLength), EndWriting());
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
    PRUint32 myLength = this->Length();
    cutLength = NS_MIN(cutLength, myLength-cutStart);
    PRUint32 cutEnd = cutStart + cutLength;
    if ( cutEnd < myLength )
      copy_string(this->BeginReading(cutEnd), this->EndReading(), BeginWriting(cutStart));
    SetLength(myLength-cutLength);
  }



  //
  // |Replace()|
  //

template <class CharT>
void
basic_nsAWritableString<CharT>::ReplaceFromReadable( PRUint32 cutStart, PRUint32 cutLength, const basic_nsAReadableString<CharT>& aReplacement )
  {
    if ( NS_STATIC_CAST(const basic_nsAReadableString<CharT>*, this) != &aReplacement )
      do_ReplaceFromReadable(cutStart, cutLength, aReplacement);
    else
      ReplaceFromPromise(cutStart, cutLength, aReplacement);
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::ReplaceFromPromise( PRUint32 cutStart, PRUint32 cutLength, const basic_nsAReadableString<CharT>& aReadable )
  {
    if ( !aReadable.Promises(*this) )
      do_ReplaceFromReadable(cutStart, cutLength, aReadable);
    else
      {
        PRUint32 length = aReadable.Length();
        CharT* buffer = new CharT[length];
        if ( buffer )
          {
            copy_string(aReadable.BeginReading(), aReadable.EndReading(), buffer);
            do_ReplaceFromReadable(cutStart, cutLength, basic_nsLiteralString<CharT>(buffer, length));
            delete buffer;
          }
        // else assert?
      }
  }

template <class CharT>
void
basic_nsAWritableString<CharT>::do_ReplaceFromReadable( PRUint32 cutStart, PRUint32 cutLength, const basic_nsAReadableString<CharT>& aReplacement )
  {
    PRUint32 oldLength = this->Length();

    cutStart = NS_MIN(cutStart, oldLength);
    cutLength = NS_MIN(cutLength, oldLength-cutStart);
    PRUint32 cutEnd = cutStart + cutLength;

    PRUint32 replacementLength = aReplacement.Length();
    PRUint32 replacementEnd = cutStart + replacementLength;

    PRUint32 newLength = oldLength - cutLength + replacementLength;

    if ( cutLength > replacementLength )
      copy_string(this->BeginReading(cutEnd), this->EndReading(), BeginWriting(replacementEnd));
    SetLength(newLength);
    if ( cutLength < replacementLength )
      copy_string_backward(this->BeginReading(cutEnd), this->BeginReading(oldLength), BeginWriting(replacementEnd));

    copy_string(aReplacement.BeginReading(), aReplacement.EndReading(), BeginWriting(cutStart));
  }



  //
  // Types
  //

typedef basic_nsAWritableString<PRUnichar>  nsAWritableString;
typedef basic_nsAWritableString<char>       nsAWritableCString;

#endif // !defined(nsAWritableString_h___)
