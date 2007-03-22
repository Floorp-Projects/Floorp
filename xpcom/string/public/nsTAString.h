/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_INTERNAL_API
#error Cannot use internal string classes without MOZILLA_INTERNAL_API defined. Use the frozen header nsStringAPI.h instead.
#endif

  /**
   * The base for string comparators
   */
class NS_COM nsTStringComparator_CharT
  {
    public:
      typedef CharT char_type;

      nsTStringComparator_CharT() {}

      virtual int operator()( const char_type*, const char_type*, PRUint32 length ) const = 0;
      virtual int operator()( char_type, char_type ) const = 0;
  };


  /**
   * The default string comparator (case-sensitive comparision)
   */
class NS_COM nsTDefaultStringComparator_CharT
    : public nsTStringComparator_CharT
  {
    public:
      typedef CharT char_type;

      nsTDefaultStringComparator_CharT() {}

      virtual int operator()( const char_type*, const char_type*, PRUint32 length ) const;
      virtual int operator()( char_type, char_type ) const;
  };


  /**
   * nsTAString is the most abstract class in the string hierarchy.
   *
   * In its original inception, nsTAString was designed to allow the data
   * storage for a string to be separated into multiple fragments.  This was
   * intended to enable lazy string flattening or avoid string flattening
   * altogether in some cases.  This abstraction, however, meant that every
   * single string operation (including simple operations such as IsEmpty() and
   * BeginReading()) required virtual function calls.  A virtual destructor was
   * also required.  This not only meant additional overhead for invoking
   * string methods but also added to additional codesize at every callsite (to
   * load the virtual function address).
   *
   * Today nsTAString exists mainly for backwards compatibility of the string
   * API.  It is restricted to representing a contiguous array of characters,
   * where the character array is not necessarily null-terminated.  Moreover,
   * since nsTAString's virtual function table was frozen for Mozilla 1.0,
   * nsTAString necessarily maintains ABI compatibility with older versions of
   * Gecko.  (nsTObsoleteAString provides that frozen ABI.  See
   * nsObsoleteAString.h for a description of how we solve the ABI
   * compatibility requirement while eliminating virtual function calls on
   * nsTAString.)
   *
   * XPIDL still generates C++ header files with references to nsTAStrings, so
   * nsTAString will still be heavily used in real code.
   *
   * If the opportunity to break ABI compatibility with Mozilla 1.0 were to
   * ever arise, our first move should be to make nsTAString equate to
   * nsTSubstring.  This may in fact be an option today for some Gecko-based
   * products.
   */
class nsTAString_CharT
  {
    public:

      typedef CharT                                  char_type;
      typedef nsCharTraits<char_type>                char_traits;

      typedef char_traits::incompatible_char_type    incompatible_char_type;

      typedef nsTAString_CharT                       self_type;
      typedef nsTAString_CharT                       abstract_string_type;
      typedef nsTObsoleteAString_CharT               obsolete_string_type;
      typedef nsTSubstring_CharT                     substring_type;
      typedef nsTSubstringTuple_CharT                substring_tuple_type;

      typedef nsReadingIterator<char_type>           const_iterator;
      typedef nsWritingIterator<char_type>           iterator;

      typedef nsTStringComparator_CharT              comparator_type;

      typedef PRUint32                               size_type;
      typedef PRUint32                               index_type;

#ifdef MOZ_V1_STRING_ABI
    public:

        // this acts like a virtual destructor
      NS_COM NS_CONSTRUCTOR_FASTCALL ~nsTAString_CharT();


        /**
         * BeginReading/EndReading can be used to get immutable access to the
         * string's underlying buffer.  EndReading returns a pointer to the
         * end of the string's buffer.  nsReadableUtils.h provides a collection
         * of utility functions that work with these iterators.
         */

      inline const_iterator& BeginReading( const_iterator& iter ) const
        {
          size_type len = GetReadableBuffer(&iter.mStart);
          iter.mEnd = iter.mStart + len;
          iter.mPosition = iter.mStart;
          return iter;
        }

      inline const_iterator& EndReading( const_iterator& iter ) const
        {
          size_type len = GetReadableBuffer(&iter.mStart);
          iter.mEnd = iter.mStart + len;
          iter.mPosition = iter.mEnd;
          return iter;
        }

      inline const char_type* BeginReading() const
        {
          const char_type *b;
          GetReadableBuffer(&b);
          return b;
        }

      inline const char_type* EndReading() const
        {
          const char_type *b;
          size_type len = GetReadableBuffer(&b);
          return b + len;
        }


        /**
         * BeginWriting/EndWriting can be used to get mutable access to the
         * string's underlying buffer.  EndWriting returns a pointer to the
         * end of the string's buffer.
         *
         * If these methods are unable to return a mutable buffer, then they
         * will return a null iterator.
         */

      inline iterator& BeginWriting( iterator& iter )
        {
          size_type len = GetWritableBuffer(&iter.mStart);
          iter.mEnd = iter.mStart + len;
          iter.mPosition = iter.mStart;
          return iter;
        }

      inline iterator& EndWriting( iterator& iter )
        {
          size_type len = GetWritableBuffer(&iter.mStart);
          iter.mEnd = iter.mStart + len;
          iter.mPosition = iter.mEnd;
          return iter;
        }

      inline char_type* BeginWriting()
        {
          char_type *b;
          GetWritableBuffer(&b);
          return b;
        }

      inline char_type* EndWriting()
        {
          char_type *b;
          size_type len = GetWritableBuffer(&b);
          return b ? b + len : nsnull;
        }


        /**
         * Get a const pointer to the string's internal buffer.  The caller
         * MUST NOT modify the characters at the returned address.
         *
         * @returns The length of the buffer in characters.
         */
      inline size_type GetData( const char_type** data ) const
        {
          return GetReadableBuffer(data);
        }

        /**
         * Get a pointer to the string's internal buffer, optionally resizing
         * the buffer first.  If size_type(-1) is passed for newLen, then the
         * current length of the string is used.  The caller MAY modify the
         * characters at the returned address (up to but not exceeding the
         * length of the string).
         *
         * @returns The length of the buffer in characters or 0 if unable to
         * satisfy the request due to low-memory conditions.
         */
      inline size_type GetMutableData( char_type** data, size_type newLen = size_type(-1) )
        {
          return GetWritableBuffer(data, newLen);
        }


        /**
         * Length checking functions.  IsEmpty is a helper function to avoid
         * writing code like: |if (str.Length() == 0)|
         */

      NS_COM size_type NS_FASTCALL Length() const;
      PRBool IsEmpty() const { return Length() == 0; }


        /**
         * String equality tests.  Pass a string comparator if you want to
         * control how the strings are compared.  By default, a binary
         * "case-sensitive" comparision is performed.
         */

      NS_COM PRBool NS_FASTCALL Equals( const self_type& ) const;
      NS_COM PRBool NS_FASTCALL Equals( const self_type&, const comparator_type& ) const;
      NS_COM PRBool NS_FASTCALL Equals( const char_type* ) const;
      NS_COM PRBool NS_FASTCALL Equals( const char_type*, const comparator_type& ) const;

        /**
         * An efficient comparison with ASCII that can be used even
         * for wide strings. Call this version when you know the
         * length of 'data'.
         */
      NS_COM PRBool NS_FASTCALL EqualsASCII( const char* data, size_type len ) const;
        /**
         * An efficient comparison with ASCII that can be used even
         * for wide strings. Call this version when 'data' is
         * null-terminated.
         */
      NS_COM PRBool NS_FASTCALL EqualsASCII( const char* data ) const;

    // EqualsLiteral must ONLY be applied to an actual literal string.
    // Do not attempt to use it with a regular char* pointer, or with a char
    // array variable.
    // The template trick to acquire the array length at compile time without
    // using a macro is due to Corey Kosak, with much thanks.
#ifdef NS_DISABLE_LITERAL_TEMPLATE
      inline PRBool EqualsLiteral( const char* str ) const
        {
          return EqualsASCII(str);
        }
#else
      template<int N>
      inline PRBool EqualsLiteral( const char (&str)[N] ) const
        {
          return EqualsASCII(str, N-1);
        }
      template<int N>
      inline PRBool EqualsLiteral( char (&str)[N] ) const
        {
          const char* s = str;
          return EqualsASCII(s, N-1);
        }
#endif

    // The LowerCaseEquals methods compare the lower case version of
    // this string to some ASCII/Literal string. The ASCII string is
    // *not* lowercased for you. If you compare to an ASCII or literal
    // string that contains an uppercase character, it is guaranteed to
    // return false. We will throw assertions too.
      NS_COM PRBool NS_FASTCALL LowerCaseEqualsASCII( const char* data, size_type len ) const;
      NS_COM PRBool NS_FASTCALL LowerCaseEqualsASCII( const char* data ) const;

    // LowerCaseEqualsLiteral must ONLY be applied to an actual
    // literal string.  Do not attempt to use it with a regular char*
    // pointer, or with a char array variable. Use
    // LowerCaseEqualsASCII for them.
#ifdef NS_DISABLE_LITERAL_TEMPLATE
      inline PRBool LowerCaseEqualsLiteral( const char* str ) const
        {
          return LowerCaseEqualsASCII(str);
        }
#else
      template<int N>
      inline PRBool LowerCaseEqualsLiteral( const char (&str)[N] ) const
        {
          return LowerCaseEqualsASCII(str, N-1);
        }
      template<int N>
      inline PRBool LowerCaseEqualsLiteral( char (&str)[N] ) const
        {
          const char* s = str;
          return LowerCaseEqualsASCII(s, N-1);
        }
#endif

        /**
         * A string always references a non-null data pointer.  In some
         * applications (e.g., the DOM) it is necessary for a string class
         * to have some way to distinguish an empty string from a null (or
         * void) string.  These methods enable support for the concept of
         * a void string.
         */

      NS_COM PRBool NS_FASTCALL IsVoid() const;
      NS_COM void NS_FASTCALL SetIsVoid( PRBool );


        /**
         * This method returns true if the string's underlying buffer is
         * null-terminated.  This should rarely be needed by applications.
         * The PromiseFlatTString method should be used to ensure that a
         * string's underlying buffer is null-terminated.
         */

      NS_COM PRBool NS_FASTCALL IsTerminated() const;


        /**
         * These are contant time since nsTAString uses flat storage
         */
      NS_COM char_type NS_FASTCALL First() const;
      NS_COM char_type NS_FASTCALL Last() const;


        /**
         * Returns the number of occurrences of the given character.
         */
      NS_COM size_type NS_FASTCALL CountChar( char_type ) const;


        /**
         * Locates the offset of the first occurrence of the character.  Pass a
         * non-zero offset to control where the search begins.
         */

      NS_COM PRInt32 NS_FASTCALL FindChar( char_type, index_type offset = 0 ) const;


        /**
         * SetCapacity is not required to do anything; however, it can be used
         * as a hint to the implementation to reduce allocations.
         *
         * SetCapacity(0) is a suggestion to discard all associated storage.
         * 
         * The |newCapacity| value does not include room for the null
         * terminator.  The actual allocation size will be one unit larger
         * than the given value to make room for a null terminator.
         */
      NS_COM void NS_FASTCALL SetCapacity( size_type newCapacity );


        /**
         * SetLength is used to resize the string's internal buffer.  It has
         * the effect of moving the string's null-terminator to the offset
         * specified by |newLen|, and if the string's internal buffer was
         * previously readonly or dependent on some other string, then it will
         * be copied.  If there is insufficient memory to perform the resize,
         * then the string will not be modified.  If the length of the string
         * is reduced sufficiently, then the string's internal buffer may be
         * resized to something smaller.
         */
      NS_COM void NS_FASTCALL SetLength( size_type newLen );


        /**
         * Can't use |Truncate| to make a string longer!
         */
      void Truncate( size_type newLen = 0 )
        {
          NS_ASSERTION(newLen <= Length(), "Truncate cannot make string longer");
          SetLength(newLen);
        }


        /**
         * |Assign| and |operator=| make |this| equivalent to the string or
         * buffer given as an argument.  If possible, they do this by sharing
         * a reference counted buffer (see |nsTSubstring|).  If not, they copy
         * the buffer into their own buffer.
         */

      NS_COM void NS_FASTCALL Assign( const self_type& readable );
      NS_COM void NS_FASTCALL Assign( const substring_tuple_type& tuple );
      NS_COM void NS_FASTCALL Assign( const char_type* data );
      NS_COM void NS_FASTCALL Assign( const char_type* data, size_type length );
      NS_COM void NS_FASTCALL Assign( char_type c );

      NS_COM void NS_FASTCALL AssignASCII( const char* data, size_type length );
      NS_COM void NS_FASTCALL AssignASCII( const char* data );

    // AssignLiteral must ONLY be applied to an actual literal string.
    // Do not attempt to use it with a regular char* pointer, or with a char
    // array variable. Use AssignASCII for those.
#ifdef NS_DISABLE_LITERAL_TEMPLATE
      void AssignLiteral( const char* str )
                  { AssignASCII(str); }
#else
      template<int N>
      void AssignLiteral( const char (&str)[N] )
                  { AssignASCII(str, N-1); }
      template<int N>
      void AssignLiteral( char (&str)[N] )
                  { AssignASCII(str, N-1); }
#endif

        // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      self_type& operator=( const self_type& readable )                                             { Assign(readable); return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                     { Assign(tuple); return *this; }
      self_type& operator=( const char_type* data )                                                 { Assign(data); return *this; }
      self_type& operator=( char_type c )                                                           { Assign(c); return *this; }



        /**
         * |Append|, |operator+=| are used to add characters to the end of this string.
         */ 

      NS_COM void NS_FASTCALL Append( const self_type& readable );
      NS_COM void NS_FASTCALL Append( const substring_tuple_type& tuple );
      NS_COM void NS_FASTCALL Append( const char_type* data );
      NS_COM void NS_FASTCALL Append( const char_type* data, size_type length );
      NS_COM void NS_FASTCALL Append( char_type c );

      NS_COM void NS_FASTCALL AppendASCII( const char* data, size_type length );
      NS_COM void NS_FASTCALL AppendASCII( const char* data );

    // AppendLiteral must ONLY be applied to an actual literal string.
    // Do not attempt to use it with a regular char* pointer, or with a char
    // array variable. Use AppendASCII for those.
#ifdef NS_DISABLE_LITERAL_TEMPLATE
      void AppendLiteral( const char* str )
                  { AppendASCII(str); }
#else
      template<int N>
      void AppendLiteral( const char (&str)[N] )
                  { AppendASCII(str, N-1); }
      template<int N>
      void AppendLiteral( char (&str)[N] )
                  { AppendASCII(str, N-1); }
#endif

      self_type& operator+=( const self_type& readable )                                            { Append(readable); return *this; }
      self_type& operator+=( const substring_tuple_type& tuple )                                    { Append(tuple); return *this; }
      self_type& operator+=( const char_type* data )                                                { Append(data); return *this; }
      self_type& operator+=( char_type c )                                                          { Append(c); return *this; }


        /**
         * |Insert| is used to add characters into this string at a given position.
         * NOTE: It's a shame the |pos| parameter isn't at the front of the arg list.
         */ 

      NS_COM void NS_FASTCALL Insert( const self_type& readable, index_type pos );
      NS_COM void NS_FASTCALL Insert( const substring_tuple_type& tuple, index_type pos );
      NS_COM void NS_FASTCALL Insert( const char_type* data, index_type pos );
      NS_COM void NS_FASTCALL Insert( const char_type* data, index_type pos, size_type length );
      NS_COM void NS_FASTCALL Insert( char_type c, index_type pos );


        /**
         * |Cut| is used to remove a range of characters from this string.
         */

      NS_COM void NS_FASTCALL Cut( index_type cutStart, size_type cutLength );

      
        /**
         * |Replace| is used overwrite a range of characters from this string.
         */

      NS_COM void NS_FASTCALL Replace( index_type cutStart, size_type cutLength, const self_type& readable );
      NS_COM void NS_FASTCALL Replace( index_type cutStart, size_type cutLength, const substring_tuple_type& readable );

      
        /**
         * this is public to support automatic conversion of tuple to abstract
         * string, which is necessary to support our API.
         */
      nsTAString_CharT(const substring_tuple_type& tuple)
        : mVTable(obsolete_string_type::sCanonicalVTable)
        , mData(nsnull)
        , mLength(0)
        , mFlags(0)
        {
          Assign(tuple);
        }

    protected:

      friend class nsTSubstringTuple_CharT;

      // GCC 3.2 erroneously needs these (even though they are subclasses!)
      friend class nsTSubstring_CharT;
      friend class nsTDependentSubstring_CharT;
      friend class nsTPromiseFlatString_CharT;

        /**
         * the address of our virtual function table.  required for backwards
         * compatibility with Mozilla 1.0 frozen nsAC?String interface.
         */
      const void* mVTable;

        /**
         * these fields are "here" only when mVTable == sCanonicalVTable.
         *
         * they exist to support automatic construction of a nsTAString
         * from a nsTSubstringTuple.
         */
      char_type*  mData;
      size_type   mLength;
      PRUint32    mFlags;

        /**
         * nsTAString must be subclassed before it can be instantiated.
         */
      nsTAString_CharT(char_type* data, size_type length, PRUint32 flags)
        : mVTable(obsolete_string_type::sCanonicalVTable)
        , mData(data)
        , mLength(length)
        , mFlags(flags)
        {}

        /**
         * optional ctor for use by subclasses.
         *
         * NOTE: mData and mLength are intentionally left uninitialized.
         */
      explicit
      nsTAString_CharT(PRUint32 flags)
        : mVTable(obsolete_string_type::sCanonicalVTable)
        , mFlags(flags)
        {}

        /**
         * get pointer to internal string buffer (may not be null terminated).
         * return length of buffer.
         */
      NS_COM size_type NS_FASTCALL GetReadableBuffer( const char_type **data ) const;
      NS_COM size_type NS_FASTCALL GetWritableBuffer(       char_type **data, size_type newLen = size_type(-1) );

        /**
         * returns true if this tuple is dependent on (i.e., overlapping with)
         * the given char sequence.
         */
      PRBool NS_FASTCALL IsDependentOn(const char_type *start, const char_type *end) const;

        /**
         * we can be converted to a const nsTSubstring (dependent on this)
         */
      const substring_type NS_FASTCALL ToSubstring() const;

        /**
         * type cast helpers
         */

      const obsolete_string_type* AsObsoleteString() const
        {
          return NS_REINTERPRET_CAST(const obsolete_string_type*, this);
        }

      obsolete_string_type* AsObsoleteString()
        {
          return NS_REINTERPRET_CAST(obsolete_string_type*, this);
        }

      const substring_type* AsSubstring() const
        {
          return NS_REINTERPRET_CAST(const substring_type*, this);
        }

      substring_type* AsSubstring()
        {
          return NS_REINTERPRET_CAST(substring_type*, this);
        }

    private:

        // GCC 2.95.3, EGCS-2.91.66, Sun Workshop/Forte, and IBM VisualAge C++
        // require a public copy-constructor in order to support automatic
        // construction of a nsTAString from a nsTSubstringTuple.  I believe
        // enabling the default copy-constructor is harmless, but I do not want
        // it to be enabled by default because that might tempt people into
        // using it (where it would be invalid).
#if !defined(__SUNPRO_CC) && \
   !(defined(_AIX) && defined(__IBMCPP__)) && \
   (!defined(__GNUC__) || __GNUC__ > 2 || __GNUC_MINOR__ > 95)

        // NOT TO BE IMPLEMENTED
      nsTAString_CharT( const self_type& );

#endif

        // NOT TO BE IMPLEMENTED
      void operator=     ( incompatible_char_type );
      void Assign        ( incompatible_char_type );
      void operator+=    ( incompatible_char_type );
      void Append        ( incompatible_char_type );
      void Insert        ( incompatible_char_type, index_type );
#endif
  };
