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
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
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
    void*     mFragmentIdentifier;

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
      nsWritingIterator() { }
      // nsWritingIterator( const nsWritingIterator<CharT>& );                    // auto-generated copy-constructor OK
      // nsWritingIterator<CharT>& operator=( const nsWritingIterator<CharT>& );  // auto-generated copy-assignment operator OK

      inline void normalize_forward();
      inline void normalize_backward();

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
      advance( difference_type n )
        {
          while ( n > 0 )
            {
              difference_type one_hop = NS_MIN(n, size_forward());

              NS_ASSERTION(one_hop>0, "Infinite loop: can't advance a writing iterator beyond the end of a string");
                // perhaps I should |break| if |!one_hop|?

              mPosition += one_hop;
              normalize_forward();
              n -= one_hop;
            }

          while ( n < 0 )
            {
              normalize_backward();
              difference_type one_hop = NS_MAX(n, -size_backward());

              NS_ASSERTION(one_hop<0, "Infinite loop: can't advance (backward) a writing iterator beyond the end of a string");
                // perhaps I should |break| if |!one_hop|?

              mPosition += one_hop;
              n -= one_hop;
            }

          return *this;
        }

        /**
         * Really don't want to call these two operations |+=| and |-=|.
         * Would prefer a single function, e.g., |advance|, which doesn't imply a constant time operation.
         *
         * We'll get rid of these as soon as we can.
         */
      nsWritingIterator<CharT>&
      operator+=( difference_type n ) // deprecated
        {
          return advance(n);
        }

      nsWritingIterator<CharT>&
      operator-=( difference_type n ) // deprecated
        {
          return advance(-n);
        }

      PRUint32
      write( const value_type* s, PRUint32 n )
        {
          NS_ASSERTION(size_forward() > 0, "You can't |write| into an |nsWritingIterator| with no space!");

          n = NS_MIN(n, PRUint32(size_forward()));
          nsCharTraits<value_type>::move(mPosition, s, n);
          advance( difference_type(n) );
          return n;
        }
  };

#if 0
template <class CharT>
nsWritingIterator<CharT>&
nsWritingIterator<CharT>::advance( difference_type n )
  {
    while ( n > 0 )
      {
        difference_type one_hop = NS_MIN(n, size_forward());

        NS_ASSERTION(one_hop>0, "Infinite loop: can't advance a writing iterator beyond the end of a string");
          // perhaps I should |break| if |!one_hop|?

        mPosition += one_hop;
        normalize_forward();
        n -= one_hop;
      }

    while ( n < 0 )
      {
        normalize_backward();
        difference_type one_hop = NS_MAX(n, -size_backward());

        NS_ASSERTION(one_hop<0, "Infinite loop: can't advance (backward) a writing iterator beyond the end of a string");
          // perhaps I should |break| if |!one_hop|?

        mPosition += one_hop;
        n -= one_hop;
      }

    return *this;
  }
#endif

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
      typedef CharT                     char_type;
      typedef PRUint32                  size_type;
      typedef PRUint32                  index_type;

      typedef nsWritingIterator<CharT>  iterator;

      // basic_nsAWritableString();                                         // auto-generated default constructor OK (we're abstract anyway)
      // basic_nsAWritableString( const basic_nsAWritableString<CharT>& );  // auto-generated copy-constructor OK (again, only because we're abstract)
      // ~basic_nsAWritableString();                                        // auto-generated destructor OK
      // see below for copy-assignment operator

      virtual CharT* GetWritableFragment( nsWritableFragment<CharT>&, nsFragmentRequest, PRUint32 = 0 ) = 0;

        /**
         * Note: measure -- should the |BeginWriting| and |EndWriting| be |inline|?
         */
      nsWritingIterator<CharT>&
      BeginWriting( nsWritingIterator<CharT>& aResult )
        {
          aResult.mOwningString = this;
          GetWritableFragment(aResult.mFragment, kFirstFragment);
          aResult.normalize_forward();
          aResult.mPosition = aResult.mFragment.mStart;
          return aResult;
        }

        // deprecated
      nsWritingIterator<CharT>
      BeginWriting()
        {
          nsWritingIterator<CharT> result;
          return BeginWriting(result); // copies (since I return a value, not a reference)
        }


      nsWritingIterator<CharT>&
      EndWriting( nsWritingIterator<CharT>& aResult )
        {
          aResult.mOwningString = this;
          GetWritableFragment(aResult.mFragment, kLastFragment);
          aResult.mPosition = aResult.mFragment.mEnd;
          // must not |normalize_backward| as that would likely invalidate tests like |while ( first != last )|
          return aResult;
        }

        // deprecated
      nsWritingIterator<CharT>
      EndWriting()
        {
          nsWritingIterator<CharT> result;
          return EndWriting(result); // copies (since I return a value, not a reference)
        }


        /**
         * |SetCapacity| is not required to do anything; however, it can be used
         * as a hint to the implementation to reduce allocations.
         * |SetCapacity(0)| is a suggestion to discard all associated storage.
         */
      virtual void SetCapacity( PRUint32 ) { }

        /**
         * |SetLength| is used in two ways:
         *   1) to |Cut| a suffix of the string;
         *   2) to prepare to |Append| or move characters around.
         *
         * External callers are not allowed to use |SetLength| is this latter capacity.
         * Should this really be a public operation?
         * Additionally, your implementation of |SetLength| need not satisfy (2) if and only if you
         * override the |do_...| routines to not need this facility.
         *
         * This distinction makes me think the two different uses should be split into
         * two distinct functions.
         */
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

        // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      basic_nsAWritableString<CharT>& operator=( const basic_nsAWritableString<CharT>& aWritable )  { Assign(aWritable); return *this; }

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



        /**
         * The following index based routines need to be recast with iterators.
         */

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
    while ( mPosition == mFragment.mEnd
         && mOwningString->GetWritableFragment(mFragment, kNextFragment) )
      mPosition = mFragment.mStart;
  }

template <class CharT>
inline
void
nsWritingIterator<CharT>::normalize_backward()
  {
    while ( mPosition == mFragment.mStart
         && mOwningString->GetWritableFragment(mFragment, kPrevFragment) )
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

            nsReadingIterator<CharT> fromBegin, fromEnd;
            CharT* toBegin = buffer;
            copy_string(aReadable.BeginReading(fromBegin), aReadable.EndReading(fromEnd), toBegin);
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

    nsReadingIterator<CharT> fromBegin, fromEnd;
    nsWritingIterator<CharT> toBegin;
    copy_string(aReadable.BeginReading(fromBegin), aReadable.EndReading(fromEnd), BeginWriting(toBegin));
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
            nsReadingIterator<CharT> fromBegin, fromEnd;
            CharT* toBegin = buffer;
            copy_string(aReadable.BeginReading(fromBegin), aReadable.EndReading(fromEnd), toBegin);
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

    nsReadingIterator<CharT> fromBegin, fromEnd;
    nsWritingIterator<CharT> toBegin;
    copy_string(aReadable.BeginReading(fromBegin), aReadable.EndReading(fromEnd), BeginWriting(toBegin).advance( PRInt32(oldLength) ) );
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
    do_AppendFromReadable(basic_nsLiteralChar<CharT>(aChar));
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
            nsReadingIterator<CharT> fromBegin, fromEnd;
            CharT* toBegin = buffer;
            copy_string(aReadable.BeginReading(fromBegin), aReadable.EndReading(fromEnd), toBegin);
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

    nsReadingIterator<CharT> fromBegin, fromEnd;
    nsWritingIterator<CharT> toBegin;
    if ( atPosition < oldLength )
      copy_string_backward(this->BeginReading(fromBegin).advance(PRInt32(atPosition)), this->BeginReading(fromEnd).advance(PRInt32(oldLength)), EndWriting(toBegin));
    else
      atPosition = oldLength;
    copy_string(aReadable.BeginReading(fromBegin), aReadable.EndReading(fromEnd), BeginWriting(toBegin).advance(PRInt32(atPosition)));
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

    nsReadingIterator<CharT> fromBegin, fromEnd;
    nsWritingIterator<CharT> toBegin;
    if ( cutEnd < myLength )
      copy_string(this->BeginReading(fromBegin).advance(PRInt32(cutEnd)), this->EndReading(fromEnd), BeginWriting(toBegin).advance(PRInt32(cutStart)));
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
            nsReadingIterator<CharT> fromBegin, fromEnd;
            CharT* toBegin = buffer;
            copy_string(aReadable.BeginReading(fromBegin), aReadable.EndReading(fromEnd), toBegin);
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

    nsReadingIterator<CharT> fromBegin, fromEnd;
    nsWritingIterator<CharT> toBegin;
    if ( cutLength > replacementLength )
      copy_string(this->BeginReading(fromBegin).advance(PRInt32(cutEnd)), this->EndReading(fromEnd), BeginWriting(toBegin).advance(PRInt32(replacementEnd)));
    SetLength(newLength);
    if ( cutLength < replacementLength )
      copy_string_backward(this->BeginReading(fromBegin).advance(PRInt32(cutEnd)), this->BeginReading(fromEnd).advance(PRInt32(oldLength)), BeginWriting(toBegin).advance(PRInt32(replacementEnd)));

    copy_string(aReplacement.BeginReading(fromBegin), aReplacement.EndReading(fromEnd), BeginWriting(toBegin).advance(PRInt32(cutStart)));
  }



  //
  // Types
  //

typedef basic_nsAWritableString<PRUnichar>  nsAWritableString;
typedef basic_nsAWritableString<char>       nsAWritableCString;

#endif // !defined(nsAWritableString_h___)
