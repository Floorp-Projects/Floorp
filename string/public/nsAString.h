/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

#ifndef nsAString_h___
#define nsAString_h___

#ifndef nsStringFwd_h___
#include "nsStringFwd.h"
#endif

#ifndef nsPrivateSharableString_h___
#include "nsPrivateSharableString.h"
#endif

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
#endif

#ifndef nsStringIterator_h___
#include "nsStringIterator.h"
#endif


  /**
   *
   */

class NS_COM nsAString
    : public nsPrivateSharableString
  {
    public:
      typedef nsAString                     self_type;
      typedef nsAPromiseString              promise_type;
      typedef PRUnichar                     char_type;
      typedef char                          incompatible_char_type;


      typedef nsReadingIterator<char_type>  const_iterator;
      typedef nsWritingIterator<char_type>  iterator;

      typedef PRUint32                      size_type;
      typedef PRUint32                      index_type;




      // nsAString();                           // auto-generated default constructor OK (we're abstract anyway)
      // nsAString( const self_type& );         // auto-generated copy-constructor OK (again, only because we're abstract)
      virtual ~nsAString() { }                  // ...yes, I expect to be sub-classed

      inline const_iterator& BeginReading( const_iterator& ) const;
      inline const_iterator& EndReading( const_iterator& ) const;

      inline iterator& BeginWriting( iterator& );
      inline iterator& EndWriting( iterator& );

      virtual size_type Length() const = 0;
      PRBool IsEmpty() const { return Length() == 0; }

      inline PRBool Equals( const self_type& ) const;
      PRBool Equals( const char_type* ) const;

      virtual PRBool IsVoid() const;
      virtual void SetIsVoid( PRBool );
      
        /**
         * |CharAt|, |operator[]|, |First()|, and |Last()| are not guaranteed to be constant-time operations.
         * These signatures should be pushed down into interfaces that guarantee flat allocation.
         * Clients at _this_ level should always use iterators.
         */
      char_type  First() const;
      char_type  Last() const;

      size_type  CountChar( char_type ) const;


        /*
          |Left|, |Mid|, and |Right| are annoying signatures that seem better almost
          any _other_ way than they are now.  Consider these alternatives

            aWritable = aReadable.Left(17);   // ...a member function that returns a |Substring|
            aWritable = Left(aReadable, 17);  // ...a global function that returns a |Substring|
            Left(aReadable, 17, aWritable);   // ...a global function that does the assignment

          as opposed to the current signature

            aReadable.Left(aWritable, 17);    // ...a member function that does the assignment

          or maybe just stamping them out in favor of |Substring|, they are just duplicate functionality

            aWritable = Substring(aReadable, 0, 17);
        */
            
      size_type  Left( self_type&, size_type ) const;
      size_type  Mid( self_type&, PRUint32, PRUint32 ) const;
      size_type  Right( self_type&, size_type ) const;

      // Find( ... ) const;
      PRInt32 FindChar( char_type, index_type aOffset = 0 ) const;
      // FindCharInSet( ... ) const;
      // RFind( ... ) const;
      // RFindChar( ... ) const;
      // RFindCharInSet( ... ) const;

        /**
         * |SetCapacity| is not required to do anything; however, it can be used
         * as a hint to the implementation to reduce allocations.
         * |SetCapacity(0)| is a suggestion to discard all associated storage.
         */
      virtual void SetCapacity( size_type ) { }

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
      virtual void SetLength( size_type ) { }


      void
      Truncate( size_type aNewLength=0 )
        {
          NS_ASSERTION(aNewLength<=this->Length(), "Can't use |Truncate()| to make a string longer.");

          if ( aNewLength < this->Length() )
            SetLength(aNewLength);
        }


      // PRBool SetCharAt( char_type, index_type ) = 0;



      // void ToLowerCase();
      // void ToUpperCase();

      // void StripChars( const char_type* aSet );
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

      void Assign( const self_type& aReadable )                                                     { AssignFromReadable(aReadable); }
      inline void Assign( const promise_type& aReadable );
      void Assign( const char_type* aPtr )                                                          { aPtr ? do_AssignFromElementPtr(aPtr) : SetLength(0); }
      void Assign( const char_type* aPtr, size_type aLength )                                       { do_AssignFromElementPtrLength(aPtr, aLength); }
      void Assign( char_type aChar )                                                                { do_AssignFromElement(aChar); }

        // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      self_type& operator=( const self_type& aReadable )                                            { Assign(aReadable); return *this; }

      self_type& operator=( const promise_type& aReadable )                                         { Assign(aReadable); return *this; }
      self_type& operator=( const char_type* aPtr )                                                 { Assign(aPtr); return *this; }
      self_type& operator=( char_type aChar )                                                       { Assign(aChar); return *this; }



        //
        // |Append()|, |operator+=()|
        //

      void Append( const self_type& aReadable )                                                     { AppendFromReadable(aReadable); }
      inline void Append( const promise_type& aReadable );
      void Append( const char_type* aPtr )                                                          { if (aPtr) do_AppendFromElementPtr(aPtr); }
      void Append( const char_type* aPtr, size_type aLength )                                       { do_AppendFromElementPtrLength(aPtr, aLength); }
      void Append( char_type aChar )                                                                { do_AppendFromElement(aChar); }

      self_type& operator+=( const self_type& aReadable )                                           { Append(aReadable); return *this; }
      self_type& operator+=( const promise_type& aReadable )                                        { Append(aReadable); return *this; }
      self_type& operator+=( const char_type* aPtr )                                                { Append(aPtr); return *this; }
      self_type& operator+=( char_type aChar )                                                      { Append(aChar); return *this; }



        /**
         * The following index based routines need to be recast with iterators.
         */

        //
        // |Insert()|
        //  Note: I would really like to move the |atPosition| parameter to the front of the argument list
        //

      void Insert( const self_type& aReadable, index_type atPosition )                              { InsertFromReadable(aReadable, atPosition); }
      inline void Insert( const promise_type& aReadable, index_type atPosition );
      void Insert( const char_type* aPtr, index_type atPosition )                                   { if (aPtr) do_InsertFromElementPtr(aPtr, atPosition); }
      void Insert( const char_type* aPtr, index_type atPosition, size_type aLength )                { do_InsertFromElementPtrLength(aPtr, atPosition, aLength); }
      void Insert( char_type aChar, index_type atPosition )                                         { do_InsertFromElement(aChar, atPosition); }



      virtual void Cut( index_type cutStart, size_type cutLength );



      void Replace( index_type cutStart, size_type cutLength, const self_type& aReadable )          { ReplaceFromReadable(cutStart, cutLength, aReadable); }
//    void Replace( index_type cutStart, size_type cutLength, const promise_type& aReadable )       { ReplaceFromPromise(cutStart, cutLength, aReadable); }

    private:
        // NOT TO BE IMPLEMENTED
      index_type  CountChar( incompatible_char_type ) const;
      void operator=  ( incompatible_char_type );
      void Assign     ( incompatible_char_type );
      void operator+= ( incompatible_char_type );
      void Append     ( incompatible_char_type );
      void Insert     ( incompatible_char_type, index_type );
      

    protected:
              void AssignFromReadable( const self_type& );
              void AssignFromPromise( const self_type& );
      virtual void do_AssignFromReadable( const self_type& );
      virtual void do_AssignFromElementPtr( const char_type* );
      virtual void do_AssignFromElementPtrLength( const char_type*, size_type );
      virtual void do_AssignFromElement( char_type );

              void AppendFromReadable( const self_type& );
              void AppendFromPromise( const self_type& );
      virtual void do_AppendFromReadable( const self_type& );
      virtual void do_AppendFromElementPtr( const char_type* );
      virtual void do_AppendFromElementPtrLength( const char_type*, size_type );
      virtual void do_AppendFromElement( char_type );

              void InsertFromReadable( const self_type&, index_type );
              void InsertFromPromise( const self_type&, index_type );
      virtual void do_InsertFromReadable( const self_type&, index_type );
      virtual void do_InsertFromElementPtr( const char_type*, index_type );
      virtual void do_InsertFromElementPtrLength( const char_type*, index_type, size_type );
      virtual void do_InsertFromElement( char_type, index_type );

              void ReplaceFromReadable( index_type, size_type, const self_type& );
              void ReplaceFromPromise( index_type, size_type, const self_type& );
      virtual void do_ReplaceFromReadable( index_type, size_type, const self_type& );


//  protected:
    public:
      virtual const char_type* GetReadableFragment( nsReadableFragment<char_type>&, nsFragmentRequest, PRUint32 = 0 ) const = 0;
      virtual       char_type* GetWritableFragment( nsWritableFragment<char_type>&, nsFragmentRequest, PRUint32 = 0 ) = 0;
      virtual PRBool IsDependentOn( const self_type& aString ) const { return &aString == this; }
  };

class NS_COM nsACString
    : public nsPrivateSharableCString
  {
    public:
      typedef nsACString                    self_type;
      typedef nsAPromiseCString             promise_type;
      typedef char                          char_type;
      typedef PRUnichar                     incompatible_char_type;


      typedef nsReadingIterator<char_type>  const_iterator;
      typedef nsWritingIterator<char_type>  iterator;

      typedef PRUint32                      size_type;
      typedef PRUint32                      index_type;




      // nsACString();                          // auto-generated default constructor OK (we're abstract anyway)
      // nsACString( const self_type& );        // auto-generated copy-constructor OK (again, only because we're abstract)
      virtual ~nsACString() { }                 // ...yes, I expect to be sub-classed

      inline const_iterator& BeginReading( const_iterator& ) const;
      inline const_iterator& EndReading( const_iterator& ) const;

      inline iterator& BeginWriting( iterator& );
      inline iterator& EndWriting( iterator& );

      virtual size_type Length() const = 0;
      PRBool IsEmpty() const { return Length() == 0; }

      inline PRBool Equals( const self_type& ) const;
      PRBool Equals( const char_type* ) const;

      virtual PRBool IsVoid() const;
      virtual void SetIsVoid( PRBool );
      
        /**
         * |CharAt|, |operator[]|, |First()|, and |Last()| are not guaranteed to be constant-time operations.
         * These signatures should be pushed down into interfaces that guarantee flat allocation.
         * Clients at _this_ level should always use iterators.
         */
      char_type  First() const;
      char_type  Last() const;

      size_type  CountChar( char_type ) const;


        /*
          |Left|, |Mid|, and |Right| are annoying signatures that seem better almost
          any _other_ way than they are now.  Consider these alternatives

            aWritable = aReadable.Left(17);   // ...a member function that returns a |Substring|
            aWritable = Left(aReadable, 17);  // ...a global function that returns a |Substring|
            Left(aReadable, 17, aWritable);   // ...a global function that does the assignment

          as opposed to the current signature

            aReadable.Left(aWritable, 17);    // ...a member function that does the assignment

          or maybe just stamping them out in favor of |Substring|, they are just duplicate functionality

            aWritable = Substring(aReadable, 0, 17);
        */
            
      size_type  Left( self_type&, size_type ) const;
      size_type  Mid( self_type&, PRUint32, PRUint32 ) const;
      size_type  Right( self_type&, size_type ) const;

      // Find( ... ) const;
      PRInt32 FindChar( char_type, PRUint32 aOffset = 0 ) const;
      // FindCharInSet( ... ) const;
      // RFind( ... ) const;
      // RFindChar( ... ) const;
      // RFindCharInSet( ... ) const;

        /**
         * |SetCapacity| is not required to do anything; however, it can be used
         * as a hint to the implementation to reduce allocations.
         * |SetCapacity(0)| is a suggestion to discard all associated storage.
         */
      virtual void SetCapacity( size_type ) { }

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
      virtual void SetLength( size_type ) { }


      void
      Truncate( size_type aNewLength=0 )
        {
          NS_ASSERTION(aNewLength<=this->Length(), "Can't use |Truncate()| to make a string longer.");

          if ( aNewLength < this->Length() )
            SetLength(aNewLength);
        }


      // PRBool SetCharAt( char_type, index_type ) = 0;



      // void ToLowerCase();
      // void ToUpperCase();

      // void StripChars( const char_type* aSet );
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

      void Assign( const self_type& aReadable )                                                     { AssignFromReadable(aReadable); }
      inline void Assign( const promise_type& aReadable );
      void Assign( const char_type* aPtr )                                                          { aPtr ? do_AssignFromElementPtr(aPtr) : SetLength(0); }
      void Assign( const char_type* aPtr, size_type aLength )                                       { do_AssignFromElementPtrLength(aPtr, aLength); }
      void Assign( char_type aChar )                                                                { do_AssignFromElement(aChar); }

        // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      self_type& operator=( const self_type& aReadable )                                            { Assign(aReadable); return *this; }

      self_type& operator=( const promise_type& aReadable )                                         { Assign(aReadable); return *this; }
      self_type& operator=( const char_type* aPtr )                                                 { Assign(aPtr); return *this; }
      self_type& operator=( char_type aChar )                                                       { Assign(aChar); return *this; }



        //
        // |Append()|, |operator+=()|
        //

      void Append( const self_type& aReadable )                                                     { AppendFromReadable(aReadable); }
      inline void Append( const promise_type& aReadable );
      void Append( const char_type* aPtr )                                                          { if (aPtr) do_AppendFromElementPtr(aPtr); }
      void Append( const char_type* aPtr, size_type aLength )                                       { do_AppendFromElementPtrLength(aPtr, aLength); }
      void Append( char_type aChar )                                                                { do_AppendFromElement(aChar); }

      self_type& operator+=( const self_type& aReadable )                                           { Append(aReadable); return *this; }
      self_type& operator+=( const promise_type& aReadable )                                        { Append(aReadable); return *this; }
      self_type& operator+=( const char_type* aPtr )                                                { Append(aPtr); return *this; }
      self_type& operator+=( char_type aChar )                                                      { Append(aChar); return *this; }



        /**
         * The following index based routines need to be recast with iterators.
         */

        //
        // |Insert()|
        //  Note: I would really like to move the |atPosition| parameter to the front of the argument list
        //

      void Insert( const self_type& aReadable, index_type atPosition )                              { InsertFromReadable(aReadable, atPosition); }
      inline void Insert( const promise_type& aReadable, index_type atPosition );
      void Insert( const char_type* aPtr, index_type atPosition )                                   { if (aPtr) do_InsertFromElementPtr(aPtr, atPosition); }
      void Insert( const char_type* aPtr, index_type atPosition, size_type aLength )                { do_InsertFromElementPtrLength(aPtr, atPosition, aLength); }
      void Insert( char_type aChar, index_type atPosition )                                         { do_InsertFromElement(aChar, atPosition); }



      virtual void Cut( index_type cutStart, size_type cutLength );



      void Replace( index_type cutStart, size_type cutLength, const self_type& aReadable )          { ReplaceFromReadable(cutStart, cutLength, aReadable); }
//    void Replace( index_type cutStart, size_type cutLength, const promise_type& aReadable )       { ReplaceFromPromise(cutStart, cutLength, aReadable); }

    private:
        // NOT TO BE IMPLEMENTED
      index_type  CountChar( incompatible_char_type ) const;
      void operator=  ( incompatible_char_type );
      void Assign     ( incompatible_char_type );
      void operator+= ( incompatible_char_type );
      void Append     ( incompatible_char_type );
      void Insert     ( incompatible_char_type, index_type );
      

    protected:
              void AssignFromReadable( const self_type& );
              void AssignFromPromise( const self_type& );
      virtual void do_AssignFromReadable( const self_type& );
      virtual void do_AssignFromElementPtr( const char_type* );
      virtual void do_AssignFromElementPtrLength( const char_type*, size_type );
      virtual void do_AssignFromElement( char_type );

              void AppendFromReadable( const self_type& );
              void AppendFromPromise( const self_type& );
      virtual void do_AppendFromReadable( const self_type& );
      virtual void do_AppendFromElementPtr( const char_type* );
      virtual void do_AppendFromElementPtrLength( const char_type*, size_type );
      virtual void do_AppendFromElement( char_type );

              void InsertFromReadable( const self_type&, index_type );
              void InsertFromPromise( const self_type&, index_type );
      virtual void do_InsertFromReadable( const self_type&, index_type );
      virtual void do_InsertFromElementPtr( const char_type*, index_type );
      virtual void do_InsertFromElementPtrLength( const char_type*, index_type, size_type );
      virtual void do_InsertFromElement( char_type, index_type );

              void ReplaceFromReadable( index_type, size_type, const self_type& );
              void ReplaceFromPromise( index_type, size_type, const self_type& );
      virtual void do_ReplaceFromReadable( index_type, size_type, const self_type& );


//  protected:
    public:
      virtual const char_type* GetReadableFragment( nsReadableFragment<char_type>&, nsFragmentRequest, PRUint32 = 0 ) const = 0;
      virtual       char_type* GetWritableFragment( nsWritableFragment<char_type>&, nsFragmentRequest, PRUint32 = 0 ) = 0;
      virtual PRBool IsDependentOn( const self_type& aString ) const { return &aString == this; }
  };

#include "nsAPromiseString.h"

inline
void
nsAString::Assign( const nsAPromiseString& aReadable )
  {
    AssignFromPromise(aReadable);
  }

inline
void
nsAString::Append( const nsAPromiseString& aReadable )
  {
    AppendFromPromise(aReadable);
  }

inline
void
nsAString::Insert( const nsAPromiseString& aReadable, index_type atPosition )
  {
    InsertFromPromise(aReadable, atPosition);
  }

inline
void
nsACString::Assign( const nsAPromiseCString& aReadable )
  {
    AssignFromPromise(aReadable);
  }

inline
void
nsACString::Append( const nsAPromiseCString& aReadable )
  {
    AppendFromPromise(aReadable);
  }

inline
void
nsACString::Insert( const nsAPromiseCString& aReadable, index_type atPosition )
  {
    InsertFromPromise(aReadable, atPosition);
  }


  /**
   * Note: measure -- should the |Begin...| and |End...| be |inline|?
   */
inline
nsAString::const_iterator&
nsAString::BeginReading( const_iterator& aResult ) const
  {
    aResult.mOwningString = this;
    GetReadableFragment(aResult.mFragment, kFirstFragment);
    aResult.mPosition = aResult.mFragment.mStart;
    aResult.normalize_forward();
    return aResult;
  }

inline
nsAString::const_iterator&
nsAString::EndReading( const_iterator& aResult ) const
  {
    aResult.mOwningString = this;
    GetReadableFragment(aResult.mFragment, kLastFragment);
    aResult.mPosition = aResult.mFragment.mEnd;
    // must not |normalize_backward| as that would likely invalidate tests like |while ( first != last )|
    return aResult;
  }

inline
nsAString::iterator&
nsAString::BeginWriting( iterator& aResult )
  {
    aResult.mOwningString = this;
    GetWritableFragment(aResult.mFragment, kFirstFragment);
    aResult.mPosition = aResult.mFragment.mStart;
    aResult.normalize_forward();
    return aResult;
  }


inline
nsAString::iterator&
nsAString::EndWriting( iterator& aResult )
  {
    aResult.mOwningString = this;
    GetWritableFragment(aResult.mFragment, kLastFragment);
    aResult.mPosition = aResult.mFragment.mEnd;
    // must not |normalize_backward| as that would likely invalidate tests like |while ( first != last )|
    return aResult;
  }

class NS_COM nsStringComparator
  {
    public:
      virtual int operator()( const PRUnichar*, const PRUnichar*, PRUint32 aLength ) const = 0;
  };

class NS_COM nsDefaultStringComparator
    : public nsStringComparator
  {
    public:
      virtual int operator()( const PRUnichar*, const PRUnichar*, PRUint32 aLength ) const;
  };

class NS_COM nsCaseInsensitiveStringComparator
    : public nsStringComparator
  {
    public:
      virtual int operator()( const PRUnichar*, const PRUnichar*, PRUint32 aLength ) const;
  };

NS_COM int Compare( const nsAString& lhs, const nsAString& rhs, const nsStringComparator& = nsDefaultStringComparator() );

inline
PRBool
nsAString::Equals( const self_type& rhs ) const
  {
    return Length()==rhs.Length() && Compare(*this, rhs)==0;
  }

inline
PRBool
operator!=( const nsAString& lhs, const nsAString& rhs )
  {
    return !lhs.Equals(rhs);
  }

inline
PRBool
operator< ( const nsAString& lhs, const nsAString& rhs )
  {
    return Compare(lhs, rhs)< 0;
  }

inline
PRBool
operator<=( const nsAString& lhs, const nsAString& rhs )
  {
    return Compare(lhs, rhs)<=0;
  }

inline
PRBool
operator==( const nsAString& lhs, const nsAString& rhs )
  {
    return lhs.Equals(rhs);
  }

inline
PRBool
operator>=( const nsAString& lhs, const nsAString& rhs )
  {
    return Compare(lhs, rhs)>=0;
  }

inline
PRBool
operator> ( const nsAString& lhs, const nsAString& rhs )
  {
    return Compare(lhs, rhs)> 0;
  }

inline
nsAString::size_type
nsAString::Left( nsAString& aResult, size_type aLengthToCopy ) const
  {
    return Mid(aResult, 0, aLengthToCopy);
  }


  /**
   *
   */

inline
nsACString::const_iterator&
nsACString::BeginReading( const_iterator& aResult ) const
  {
    aResult.mOwningString = this;
    GetReadableFragment(aResult.mFragment, kFirstFragment);
    aResult.mPosition = aResult.mFragment.mStart;
    aResult.normalize_forward();
    return aResult;
  }

inline
nsACString::const_iterator&
nsACString::EndReading( const_iterator& aResult ) const
  {
    aResult.mOwningString = this;
    GetReadableFragment(aResult.mFragment, kLastFragment);
    aResult.mPosition = aResult.mFragment.mEnd;
    // must not |normalize_backward| as that would likely invalidate tests like |while ( first != last )|
    return aResult;
  }

inline
nsACString::iterator&
nsACString::BeginWriting( iterator& aResult )
  {
    aResult.mOwningString = this;
    GetWritableFragment(aResult.mFragment, kFirstFragment);
    aResult.mPosition = aResult.mFragment.mStart;
    aResult.normalize_forward();
    return aResult;
  }


inline
nsACString::iterator&
nsACString::EndWriting( iterator& aResult )
  {
    aResult.mOwningString = this;
    GetWritableFragment(aResult.mFragment, kLastFragment);
    aResult.mPosition = aResult.mFragment.mEnd;
    // must not |normalize_backward| as that would likely invalidate tests like |while ( first != last )|
    return aResult;
  }


class NS_COM nsCStringComparator
  {
    public:
      virtual int operator()( const char*, const char*, PRUint32 aLength ) const = 0;
  };

class NS_COM nsDefaultCStringComparator
    : public nsCStringComparator
  {
    public:
      virtual int operator()( const char*, const char*, PRUint32 aLength ) const;
  };

class NS_COM nsCaseInsensitiveCStringComparator
    : public nsCStringComparator
  {
    public:
      virtual int operator()( const char*, const char*, PRUint32 aLength ) const;
  };

NS_COM int Compare( const nsACString& lhs, const nsACString& rhs, const nsCStringComparator& = nsDefaultCStringComparator() );

inline
PRBool
nsACString::Equals( const self_type& rhs ) const
  {
    return Length()==rhs.Length() && Compare(*this, rhs)==0;
  }

inline
PRBool
operator!=( const nsACString& lhs, const nsACString& rhs )
  {
    return !lhs.Equals(rhs);
  }

inline
PRBool
operator< ( const nsACString& lhs, const nsACString& rhs )
  {
    return Compare(lhs, rhs)< 0;
  }

inline
PRBool
operator<=( const nsACString& lhs, const nsACString& rhs )
  {
    return Compare(lhs, rhs)<=0;
  }

inline
PRBool
operator==( const nsACString& lhs, const nsACString& rhs )
  {
    return lhs.Equals(rhs);
  }

inline
PRBool
operator>=( const nsACString& lhs, const nsACString& rhs )
  {
    return Compare(lhs, rhs)>=0;
  }

inline
PRBool
operator> ( const nsACString& lhs, const nsACString& rhs )
  {
    return Compare(lhs, rhs)> 0;
  }

inline
nsACString::size_type
nsACString::Left( nsACString& aResult, size_type aLengthToCopy ) const
  {
    return Mid(aResult, 0, aLengthToCopy);
  }

  // Once you've got strings, you shouldn't need to do anything else to have concatenation
#ifndef nsDependentConcatenation_h___
#include "nsDependentConcatenation.h"
#endif

#endif // !defined(nsAString_h___)
