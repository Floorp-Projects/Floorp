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

#ifndef nsBufferHandle_h___
#include "nsBufferHandle.h"
#endif

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
#endif

#ifndef nsStringIterator_h___
#include "nsStringIterator.h"
#endif

  /**
   * The base for wide string comparators
   * @status FROZEN
   */
class NS_COM nsStringComparator
  {
    public:
      typedef PRUnichar char_type;

      virtual int operator()( const char_type*, const char_type*, PRUint32 aLength ) const = 0;
      virtual int operator()( char_type, char_type ) const = 0;
  };

class NS_COM nsDefaultStringComparator
    : public nsStringComparator
  {
    public:
      virtual int operator()( const char_type*, const char_type*, PRUint32 aLength ) const;
      virtual int operator() ( char_type, char_type ) const;
  };


  /**
   * The base for narrow string comparators
   * @status FROZEN
   */
class NS_COM nsCStringComparator
  {
    public:
      typedef char char_type;

      virtual int operator()( const char_type*, const char_type*, PRUint32 aLength ) const = 0;
      virtual int operator() ( char_type, char_type ) const = 0;
  };

class NS_COM nsDefaultCStringComparator
    : public nsCStringComparator
  {
    public:
      virtual int operator()( const char_type*, const char_type*, PRUint32 aLength ) const;
      virtual int operator()( char_type, char_type ) const;
  };

  /**
   * |nsAC?String| is the most abstract class in the string hierarchy.
   * Strings implementing |nsAC?String| may be stored in multiple
   * fragments.  They need not be null-terminated and they may contain
   * embedded null characters.  They may be dependent objects that
   * depend on other strings.
   *
   * See also |nsASingleFragmentC?String| and |nsAFlatC?String|, the
   * other main abstract classes in the string hierarchy.
   *
   * @status FROZEN
   */

class NS_COM nsAString
  {
    public:
      typedef PRUnichar                         char_type;
      typedef char                              incompatible_char_type;

      typedef nsBufferHandle<char_type>         buffer_handle_type;
      typedef nsConstBufferHandle<char_type>    const_buffer_handle_type;
      typedef nsSharedBufferHandle<char_type>   shared_buffer_handle_type;

      typedef nsReadableFragment<char_type>     const_fragment_type;
      typedef nsWritableFragment<char_type>     fragment_type;

      typedef nsAString                         self_type;
      typedef nsAString                         abstract_string_type;

      typedef nsReadingIterator<char_type>      const_iterator;
      typedef nsWritingIterator<char_type>      iterator;

      typedef PRUint32                          size_type;
      typedef PRUint32                          index_type;


    public:
      // nsAString();                           // auto-generated default constructor OK (we're abstract anyway)
      // nsAString( const self_type& );         // auto-generated copy-constructor OK (again, only because we're abstract)
      virtual ~nsAString() { }                  // ...yes, I expect to be sub-classed

      enum {
        kHasStackBuffer = 0x00000001  // Has and is using a buffer on
                                      // the stack (that could be copied
                                      // to the heap if
                                      // GetSharedBufferHandle() is
                                      // called).
      };
      virtual PRUint32                          GetImplementationFlags() const;

      virtual const        buffer_handle_type*  GetFlatBufferHandle()    const;
      virtual const        buffer_handle_type*  GetBufferHandle()        const;
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle()  const;

        /**
         * |GetBufferHandle()| will return either |0|, or a reasonable pointer.
         * The meaning of |0| is that the string points to a non-contiguous or else empty representation.
         * Otherwise |GetBufferHandle()| returns a handle that points to the single contiguous hunk of characters
         * that make up this string.
         */


      inline const_iterator& BeginReading( const_iterator& ) const;
      inline const_iterator& EndReading( const_iterator& ) const;

      inline iterator& BeginWriting( iterator& );
      inline iterator& EndWriting( iterator& );

      virtual size_type Length() const = 0;
      PRBool IsEmpty() const { return Length() == 0; }

      inline PRBool Equals( const abstract_string_type&, const nsStringComparator& = nsDefaultStringComparator() ) const;
      PRBool Equals( const char_type*, const nsStringComparator& = nsDefaultStringComparator() ) const;

      virtual PRBool IsVoid() const;
      virtual void SetIsVoid( PRBool );

        /**
         * |CharAt|, |operator[]|, |First()|, and |Last()| are not
         * guaranteed to be constant-time operations.  These signatures
         * should be pushed down into interfaces that guarantee flat
         * allocation.  (Right now |First| and |Last| are here but
         * |CharAt| and |operator[]| are on |nsASingleFragmentString|.)
         *
         * Clients at _this_ level should always use iterators.  For
         * example, to see if the n-th character is a '-':
         *
         *   nsAString::const_iterator iter;
         *   if ( *myString.BeginReading(iter).advance(n) == PRUnichar('-') )
         *     // do something...
         *   
         */
      char_type  First() const;
      char_type  Last() const;

      size_type  CountChar( char_type ) const;



      // Find( ... ) const;
      PRInt32 FindChar( char_type, index_type aOffset = 0 ) const;
      // FindCharInSet( ... ) const;
      // RFind( ... ) const;
      // RFindChar( ... ) const;
      // RFindCharInSet( ... ) const;

        /**
         * |SetCapacity| is not required to do anything; however, it can be
         * used as a hint to the implementation to reduce allocations.
         * |SetCapacity(0)| is a suggestion to discard all associated storage.
         */
      virtual void SetCapacity( size_type ) { }

        /**
         * |SetLength| is used in two ways:
         *   1) to |Cut| a suffix of the string;
         *   2) to prepare to |Append| or move characters around.
         *
         * External callers are not allowed to use |SetLength| is this
         * latter capacity, and should prefer |Truncate| for the former.
         * In other words, |SetLength| is deprecated for all use outside
         * of the string library and the internal use may at some point
         * be replaced as well.
         *
         * Should this really be a public operation?
         *
         * Additionally, your implementation of |SetLength| need not
         * satisfy (2) if and only if you override the |do_...| routines
         * to not need this facility.
         *
         * This distinction makes me think the two different uses should
         * be split into two distinct functions.
         */
      virtual void SetLength( size_type ) { }


      void
      Truncate( size_type aNewLength=0 )
        {
          NS_ASSERTION(aNewLength <= this->Length(),
                       "Can't use |Truncate()| to make a string longer.");

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



        /**
         * |Assign()| and |operator=()| make |this| equivalent to the
         * string or buffer given as an argument.  If possible, they do
         * this by sharing a refcounted buffer (see
         * |nsSharableC?String|, |nsXPIDLC?String|.  If not, they copy
         * the buffer into their own buffer.
         */

      void Assign( const self_type& aReadable )                                                     { do_AssignFromReadable(aReadable); }
      void Assign( const char_type* aPtr )                                                          { aPtr ? do_AssignFromElementPtr(aPtr) : SetLength(0); }
      void Assign( const char_type* aPtr, size_type aLength )                                       { do_AssignFromElementPtrLength(aPtr, aLength); }
      void Assign( char_type aChar )                                                                { do_AssignFromElement(aChar); }

        // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      self_type& operator=( const self_type& aReadable )                                            { Assign(aReadable); return *this; }

      self_type& operator=( const char_type* aPtr )                                                 { Assign(aPtr); return *this; }
      self_type& operator=( char_type aChar )                                                       { Assign(aChar); return *this; }



        //
        // |Append()|, |operator+=()|
        //

      void Append( const self_type& aReadable )                                                     { do_AppendFromReadable(aReadable); }
      void Append( const char_type* aPtr )                                                          { if (aPtr) do_AppendFromElementPtr(aPtr); }
      void Append( const char_type* aPtr, size_type aLength )                                       { do_AppendFromElementPtrLength(aPtr, aLength); }
      void Append( char_type aChar )                                                                { do_AppendFromElement(aChar); }

      self_type& operator+=( const self_type& aReadable )                                           { Append(aReadable); return *this; }
      self_type& operator+=( const char_type* aPtr )                                                { Append(aPtr); return *this; }
      self_type& operator+=( char_type aChar )                                                      { Append(aChar); return *this; }



        /**
         * The following index based routines need to be recast with iterators.
         */

        //
        // |Insert()|
        //  Note: I would really like to move the |atPosition| parameter to the front of the argument list
        //

      void Insert( const self_type& aReadable, index_type atPosition )                              { do_InsertFromReadable(aReadable, atPosition); }
      void Insert( const char_type* aPtr, index_type atPosition )                                   { if (aPtr) do_InsertFromElementPtr(aPtr, atPosition); }
      void Insert( const char_type* aPtr, index_type atPosition, size_type aLength )                { do_InsertFromElementPtrLength(aPtr, atPosition, aLength); }
      void Insert( char_type aChar, index_type atPosition )                                         { do_InsertFromElement(aChar, atPosition); }



      virtual void Cut( index_type cutStart, size_type cutLength );



      void Replace( index_type cutStart, size_type cutLength, const self_type& aReadable )          { do_ReplaceFromReadable(cutStart, cutLength, aReadable); }

    private:
        // NOT TO BE IMPLEMENTED
      index_type  CountChar( incompatible_char_type ) const;
      void operator=  ( incompatible_char_type );
      void Assign     ( incompatible_char_type );
      void operator+= ( incompatible_char_type );
      void Append     ( incompatible_char_type );
      void Insert     ( incompatible_char_type, index_type );
      

    protected:
              void UncheckedAssignFromReadable( const self_type& );
      virtual void do_AssignFromReadable( const self_type& );
      virtual void do_AssignFromElementPtr( const char_type* );
      virtual void do_AssignFromElementPtrLength( const char_type*, size_type );
      virtual void do_AssignFromElement( char_type );

              void UncheckedAppendFromReadable( const self_type& );
      virtual void do_AppendFromReadable( const self_type& );
      virtual void do_AppendFromElementPtr( const char_type* );
      virtual void do_AppendFromElementPtrLength( const char_type*, size_type );
      virtual void do_AppendFromElement( char_type );

              void UncheckedInsertFromReadable( const self_type&, index_type );
      virtual void do_InsertFromReadable( const self_type&, index_type );
      virtual void do_InsertFromElementPtr( const char_type*, index_type );
      virtual void do_InsertFromElementPtrLength( const char_type*, index_type, size_type );
      virtual void do_InsertFromElement( char_type, index_type );

              void UncheckedReplaceFromReadable( index_type, size_type, const self_type& );
      virtual void do_ReplaceFromReadable( index_type, size_type, const self_type& );


//  protected:
    public:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 = 0 ) const = 0;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 = 0 ) = 0;

      PRBool IsDependentOn( const self_type& aString ) const;
  };

  /**
   * @status FROZEN
   */

class NS_COM nsACString
  {
    public:
      typedef char                              char_type;
      typedef PRUnichar                         incompatible_char_type;

      typedef nsBufferHandle<char_type>         buffer_handle_type;
      typedef nsConstBufferHandle<char_type>    const_buffer_handle_type;
      typedef nsSharedBufferHandle<char_type>   shared_buffer_handle_type;

      typedef nsReadableFragment<char_type>     const_fragment_type;
      typedef nsWritableFragment<char_type>     fragment_type;

      typedef nsACString                        self_type;
      typedef nsACString                        abstract_string_type;

      typedef nsReadingIterator<char_type>      const_iterator;
      typedef nsWritingIterator<char_type>      iterator;

      typedef PRUint32                          size_type;
      typedef PRUint32                          index_type;




      // nsACString();                          // auto-generated default constructor OK (we're abstract anyway)
      // nsACString( const self_type& );        // auto-generated copy-constructor OK (again, only because we're abstract)
      virtual ~nsACString() { }                 // ...yes, I expect to be sub-classed

      enum {
        kHasStackBuffer = 0x00000001  // Has and is using a buffer on
                                      // the stack (that could be copied
                                      // to the heap if
                                      // GetSharedBufferHandle() is
                                      // called).
      };
      virtual PRUint32                          GetImplementationFlags() const;

      virtual const        buffer_handle_type*  GetFlatBufferHandle()    const;
      virtual const        buffer_handle_type*  GetBufferHandle()        const;
      virtual const shared_buffer_handle_type*  GetSharedBufferHandle()  const;

        /**
         * |GetBufferHandle()| will return either |0|, or a reasonable pointer.
         * The meaning of |0| is that the string points to a non-contiguous or else empty representation.
         * Otherwise |GetBufferHandle()| returns a handle that points to the single contiguous hunk of characters
         * that make up this string.
         */


      inline const_iterator& BeginReading( const_iterator& ) const;
      inline const_iterator& EndReading( const_iterator& ) const;

      inline iterator& BeginWriting( iterator& );
      inline iterator& EndWriting( iterator& );

      virtual size_type Length() const = 0;
      PRBool IsEmpty() const { return Length() == 0; }

      inline PRBool Equals( const abstract_string_type&, const nsCStringComparator& = nsDefaultCStringComparator() ) const;
      PRBool Equals( const char_type*, const nsCStringComparator& = nsDefaultCStringComparator() ) const;

      virtual PRBool IsVoid() const;
      virtual void SetIsVoid( PRBool );

        /**
         * |CharAt|, |operator[]|, |First()|, and |Last()| are not
         * guaranteed to be constant-time operations.  These signatures
         * should be pushed down into interfaces that guarantee flat
         * allocation.  (Right now |First| and |Last| are here but
         * |CharAt| and |operator[]| are on |nsASingleFragmentString|.)
         *
         * Clients at _this_ level should always use iterators.  For
         * example, to see if the n-th character is a '-':
         *
         *   nsACString::const_iterator iter;
         *   if ( *myString.BeginReading(iter).advance(n) == '-' )
         *     // do something...
         *   
         */
      char_type  First() const;
      char_type  Last() const;

      size_type  CountChar( char_type ) const;


      // Find( ... ) const;
      PRInt32 FindChar( char_type, index_type aOffset = 0 ) const;
      // FindCharInSet( ... ) const;
      // RFind( ... ) const;
      // RFindChar( ... ) const;
      // RFindCharInSet( ... ) const;

        /**
         * |SetCapacity| is not required to do anything; however, it can be
         * used as a hint to the implementation to reduce allocations.
         * |SetCapacity(0)| is a suggestion to discard all associated storage.
         */
      virtual void SetCapacity( size_type ) { }

        /**
         * |SetLength| is used in two ways:
         *   1) to |Cut| a suffix of the string;
         *   2) to prepare to |Append| or move characters around.
         *
         * External callers are not allowed to use |SetLength| is this
         * latter capacity, and should prefer |Truncate| for the former.
         * In other words, |SetLength| is deprecated for all use outside
         * of the string library and the internal use may at some point
         * be replaced as well.
         *
         * Should this really be a public operation?
         *
         * Additionally, your implementation of |SetLength| need not
         * satisfy (2) if and only if you override the |do_...| routines
         * to not need this facility.
         *
         * This distinction makes me think the two different uses should
         * be split into two distinct functions.
         */
      virtual void SetLength( size_type ) { }


      void
      Truncate( size_type aNewLength=0 )
        {
          NS_ASSERTION(aNewLength <= this->Length(),
                       "Can't use |Truncate()| to make a string longer.");

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



        /**
         * |Assign()| and |operator=()| make |this| equivalent to the
         * string or buffer given as an argument.  If possible, they do
         * this by sharing a refcounted buffer (see
         * |nsSharableC?String|, |nsXPIDLC?String|.  If not, they copy
         * the buffer into their own buffer.
         */

      void Assign( const self_type& aReadable )                                                     { do_AssignFromReadable(aReadable); }
      void Assign( const char_type* aPtr )                                                          { aPtr ? do_AssignFromElementPtr(aPtr) : SetLength(0); }
      void Assign( const char_type* aPtr, size_type aLength )                                       { do_AssignFromElementPtrLength(aPtr, aLength); }
      void Assign( char_type aChar )                                                                { do_AssignFromElement(aChar); }

        // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      self_type& operator=( const self_type& aReadable )                                            { Assign(aReadable); return *this; }

      self_type& operator=( const char_type* aPtr )                                                 { Assign(aPtr); return *this; }
      self_type& operator=( char_type aChar )                                                       { Assign(aChar); return *this; }



        //
        // |Append()|, |operator+=()|
        //

      void Append( const self_type& aReadable )                                                     { do_AppendFromReadable(aReadable); }
      void Append( const char_type* aPtr )                                                          { if (aPtr) do_AppendFromElementPtr(aPtr); }
      void Append( const char_type* aPtr, size_type aLength )                                       { do_AppendFromElementPtrLength(aPtr, aLength); }
      void Append( char_type aChar )                                                                { do_AppendFromElement(aChar); }

      self_type& operator+=( const self_type& aReadable )                                           { Append(aReadable); return *this; }
      self_type& operator+=( const char_type* aPtr )                                                { Append(aPtr); return *this; }
      self_type& operator+=( char_type aChar )                                                      { Append(aChar); return *this; }



        /**
         * The following index based routines need to be recast with iterators.
         */

        //
        // |Insert()|
        //  Note: I would really like to move the |atPosition| parameter to the front of the argument list
        //

      void Insert( const self_type& aReadable, index_type atPosition )                              { do_InsertFromReadable(aReadable, atPosition); }
      void Insert( const char_type* aPtr, index_type atPosition )                                   { if (aPtr) do_InsertFromElementPtr(aPtr, atPosition); }
      void Insert( const char_type* aPtr, index_type atPosition, size_type aLength )                { do_InsertFromElementPtrLength(aPtr, atPosition, aLength); }
      void Insert( char_type aChar, index_type atPosition )                                         { do_InsertFromElement(aChar, atPosition); }



      virtual void Cut( index_type cutStart, size_type cutLength );



      void Replace( index_type cutStart, size_type cutLength, const self_type& aReadable )          { do_ReplaceFromReadable(cutStart, cutLength, aReadable); }

    private:
        // NOT TO BE IMPLEMENTED
      index_type  CountChar( incompatible_char_type ) const;
      void operator=  ( incompatible_char_type );
      void Assign     ( incompatible_char_type );
      void operator+= ( incompatible_char_type );
      void Append     ( incompatible_char_type );
      void Insert     ( incompatible_char_type, index_type );
      

    protected:
              void UncheckedAssignFromReadable( const self_type& );
      virtual void do_AssignFromReadable( const self_type& );
      virtual void do_AssignFromElementPtr( const char_type* );
      virtual void do_AssignFromElementPtrLength( const char_type*, size_type );
      virtual void do_AssignFromElement( char_type );

              void UncheckedAppendFromReadable( const self_type& );
      virtual void do_AppendFromReadable( const self_type& );
      virtual void do_AppendFromElementPtr( const char_type* );
      virtual void do_AppendFromElementPtrLength( const char_type*, size_type );
      virtual void do_AppendFromElement( char_type );

              void UncheckedInsertFromReadable( const self_type&, index_type );
      virtual void do_InsertFromReadable( const self_type&, index_type );
      virtual void do_InsertFromElementPtr( const char_type*, index_type );
      virtual void do_InsertFromElementPtrLength( const char_type*, index_type, size_type );
      virtual void do_InsertFromElement( char_type, index_type );

              void UncheckedReplaceFromReadable( index_type, size_type, const self_type& );
      virtual void do_ReplaceFromReadable( index_type, size_type, const self_type& );


//  protected:
    public:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 = 0 ) const = 0;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 = 0 ) = 0;

      PRBool IsDependentOn( const abstract_string_type& aString ) const;
  };

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

NS_COM int Compare( const nsAString& lhs, const nsAString& rhs, const nsStringComparator& = nsDefaultStringComparator() );

inline
PRBool
nsAString::Equals( const abstract_string_type& rhs, const nsStringComparator& aComparator ) const
  {
    return Length()==rhs.Length() && Compare(*this, rhs, aComparator)==0;
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


class NS_COM nsCaseInsensitiveCStringComparator
    : public nsCStringComparator
  {
    public:
      virtual int operator()( const char_type*, const char_type*, PRUint32 aLength ) const;
      virtual int operator()( char_type, char_type ) const;
  };

NS_COM int Compare( const nsACString& lhs, const nsACString& rhs, const nsCStringComparator& = nsDefaultCStringComparator() );

inline
PRBool
nsACString::Equals( const abstract_string_type& rhs, const nsCStringComparator& aComparator ) const
  {
    return Length()==rhs.Length() && Compare(*this, rhs, aComparator)==0;
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

  // Once you've got strings, you shouldn't need to do anything else to have concatenation
#ifndef XPCOM_GLUE
#ifndef nsDependentConcatenation_h___
#include "nsDependentConcatenation.h"
#endif
#endif 

#endif // !defined(nsAString_h___)
