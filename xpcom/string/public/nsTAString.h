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

    public:

        // this acts like a virtual destructor
      NS_COM ~nsTAString_CharT();


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


        /**
         * BeginWriting/EndWriting can be used to get mutable access to the
         * string's underlying buffer.  EndWriting returns a pointer to the
         * end of the string's buffer.  This iterator API cannot be used to
         * grow a buffer.  Use SetLength to resize the string's buffer.
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


        /**
         * Length checking functions.  IsEmpty is a helper function to avoid
         * writing code like: |if (str.Length() == 0)|
         */

      NS_COM size_type Length() const;
      PRBool IsEmpty() const { return Length() == 0; }


        /**
         * String equality tests.  Pass a string comparator if you want to
         * control how the strings are compared.  By default, a binary
         * "case-sensitive" comparision is performed.
         */

      NS_COM PRBool Equals( const self_type& ) const;
      NS_COM PRBool Equals( const self_type&, const comparator_type& ) const;
      NS_COM PRBool Equals( const char_type* ) const;
      NS_COM PRBool Equals( const char_type*, const comparator_type& ) const;

        /**
         * An efficient comparison with ASCII that can be used even
         * for wide strings. Call this version when you know the
         * length of 'data'.
         */
      NS_COM PRBool EqualsASCII( const char* data, size_type len ) const;
        /**
         * An efficient comparison with ASCII that can be used even
         * for wide strings. Call this version when 'data' is
         * null-terminated.
         */
      NS_COM PRBool EqualsASCII( const char* data ) const;

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
      NS_COM PRBool LowerCaseEqualsASCII( const char* data, size_type len ) const;
      NS_COM PRBool LowerCaseEqualsASCII( const char* data ) const;

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

      NS_COM PRBool IsVoid() const;
      NS_COM void SetIsVoid( PRBool );


        /**
         * This method returns true if the string's underlying buffer is
         * null-terminated.  This should rarely be needed by applications.
         * The PromiseFlatTString method should be used to ensure that a
         * string's underlying buffer is null-terminated.
         */

      NS_COM PRBool IsTerminated() const;


        /**
         * These are contant time since nsTAString uses flat storage
         */
      NS_COM char_type First() const;
      NS_COM char_type Last() const;


        /**
         * Returns the number of occurances of the given character.
         */
      NS_COM size_type CountChar( char_type ) const;


        /**
         * Locates the offset of the first occurance of the character.  Pass a
         * non-zero offset to control where the search begins.
         */

      NS_COM PRInt32 FindChar( char_type, index_type offset = 0 ) const;


        /**
         * SetCapacity is not required to do anything; however, it can be used
         * as a hint to the implementation to reduce allocations.
         *
         * SetCapacity(0) is a suggestion to discard all associated storage.
         */
      NS_COM void SetCapacity( size_type );


        /**
         * XXX talk to dbaron about this comment.  we do need a method that
         * XXX allows someone to resize a string's buffer so that it can be
         * XXX populated using writing iterators.  SetLength seems to be the
         * XXX right method for the job, and we do use it in this capacity
         * XXX in certain places.
         *
         * SetLength is used in two ways:
         *   1) to |Cut| a suffix of the string;
         *   2) to prepare to |Append| or move characters around.
         *
         * External callers are not allowed to use |SetLength| in this
         * latter capacity, and should prefer |Truncate| for the former.
         * In other words, |SetLength| is deprecated for all use outside
         * of the string library and the internal use may at some point
         * be replaced as well.
         *
         * This distinction makes me think the two different uses should
         * be split into two distinct functions.
         */
      NS_COM void SetLength( size_type );


        /**
         * Can't use |Truncate| to make a string longer!
         */
      void Truncate( size_type aNewLength=0 )
        {
          NS_ASSERTION(aNewLength <= Length(), "Truncate cannot make string longer");
          SetLength(aNewLength);
        }


        /**
         * |Assign| and |operator=| make |this| equivalent to the string or
         * buffer given as an argument.  If possible, they do this by sharing
         * a reference counted buffer (see |nsTSubstring|).  If not, they copy
         * the buffer into their own buffer.
         */

      NS_COM void Assign( const self_type& readable );
      NS_COM void Assign( const substring_tuple_type& tuple );
      NS_COM void Assign( const char_type* data );
      NS_COM void Assign( const char_type* data, size_type length );
      NS_COM void Assign( char_type c );

      NS_COM void AssignASCII( const char* data, size_type length );
      NS_COM void AssignASCII( const char* data );

    // AssignLiteral must ONLY be applied to an actual literal string.
    // Do not attempt to use it with a regular char* pointer, or with a char
    // array variable. Use AssignASCII for those.
#ifdef NS_DISABLE_LITERAL_TEMPLATE
      void AssignLiteral( const char* str )
                  { return AssignASCII(str); }
#else
      template<int N>
      void AssignLiteral( const char (&str)[N] )
                  { return AssignASCII(str, N-1); }
      template<int N>
      void AssignLiteral( char (&str)[N] )
                  { return AssignASCII(str, N-1); }
#endif

        // copy-assignment operator.  I must define my own if I don't want the compiler to make me one
      self_type& operator=( const self_type& readable )                                             { Assign(readable); return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                     { Assign(tuple); return *this; }
      self_type& operator=( const char_type* data )                                                 { Assign(data); return *this; }
      self_type& operator=( char_type c )                                                           { Assign(c); return *this; }



        /**
         * |Append|, |operator+=| are used to add characters to the end of this string.
         */ 

      NS_COM void Append( const self_type& readable );
      NS_COM void Append( const substring_tuple_type& tuple );
      NS_COM void Append( const char_type* data );
      NS_COM void Append( const char_type* data, size_type length );
      NS_COM void Append( char_type c );

      NS_COM void AppendASCII( const char* data, size_type length );
      NS_COM void AppendASCII( const char* data );

    // AppendLiteral must ONLY be applied to an actual literal string.
    // Do not attempt to use it with a regular char* pointer, or with a char
    // array variable. Use AppendASCII for those.
#ifdef NS_DISABLE_LITERAL_TEMPLATE
      void AppendLiteral( const char* str )
                  { return AppendASCII(str); }
#else
      template<int N>
      void AppendLiteral( const char (&str)[N] )
                  { return AppendASCII(str, N-1); }
      template<int N>
      void AppendLiteral( char (&str)[N] )
                  { return AppendASCII(str, N-1); }
#endif

      self_type& operator+=( const self_type& readable )                                            { Append(readable); return *this; }
      self_type& operator+=( const substring_tuple_type& tuple )                                    { Append(tuple); return *this; }
      self_type& operator+=( const char_type* data )                                                { Append(data); return *this; }
      self_type& operator+=( char_type c )                                                          { Append(c); return *this; }


        /**
         * |Insert| is used to add characters into this string at a given position.
         * NOTE: It's a shame the |pos| parameter isn't at the front of the arg list.
         */ 

      NS_COM void Insert( const self_type& readable, index_type pos );
      NS_COM void Insert( const substring_tuple_type& tuple, index_type pos );
      NS_COM void Insert( const char_type* data, index_type pos );
      NS_COM void Insert( const char_type* data, index_type pos, size_type length );
      NS_COM void Insert( char_type c, index_type pos );


        /**
         * |Cut| is used to remove a range of characters from this string.
         */

      NS_COM void Cut( index_type cutStart, size_type cutLength );

      
        /**
         * |Replace| is used overwrite a range of characters from this string.
         */

      NS_COM void Replace( index_type cutStart, size_type cutLength, const self_type& readable );
      NS_COM void Replace( index_type cutStart, size_type cutLength, const substring_tuple_type& readable );

      
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
      NS_COM size_type GetReadableBuffer( const char_type **data ) const;
      NS_COM size_type GetWritableBuffer(       char_type **data );

        /**
         * returns true if this tuple is dependent on (i.e., overlapping with)
         * the given char sequence.
         */
      PRBool IsDependentOn(const char_type *start, const char_type *end) const;

        /**
         * we can be converted to a const nsTSubstring (dependent on this)
         */
      const substring_type ToSubstring() const;

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
  };


NS_COM
int Compare( const nsTAString_CharT& lhs, const nsTAString_CharT& rhs, const nsTStringComparator_CharT& = nsTDefaultStringComparator_CharT() );


inline
PRBool operator!=( const nsTAString_CharT& lhs, const nsTAString_CharT& rhs )
  {
    return !lhs.Equals(rhs);
  }

inline
PRBool operator< ( const nsTAString_CharT& lhs, const nsTAString_CharT& rhs )
  {
    return Compare(lhs, rhs)< 0;
  }

inline
PRBool operator<=( const nsTAString_CharT& lhs, const nsTAString_CharT& rhs )
  {
    return Compare(lhs, rhs)<=0;
  }

inline
PRBool operator==( const nsTAString_CharT& lhs, const nsTAString_CharT& rhs )
  {
    return lhs.Equals(rhs);
  }

inline
PRBool operator>=( const nsTAString_CharT& lhs, const nsTAString_CharT& rhs )
  {
    return Compare(lhs, rhs)>=0;
  }

inline
PRBool operator> ( const nsTAString_CharT& lhs, const nsTAString_CharT& rhs )
  {
    return Compare(lhs, rhs)> 0;
  }
