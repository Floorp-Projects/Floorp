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
   * nsTSubstring
   *
   * The base string type.  This type is not instantiated directly.  A sub-
   * class is instantiated instead.  For example, see nsTString.
   *
   * This type works like nsTAString except that it does not have the ABI
   * requirements of that interface.  Like nsTAString, nsTSubstring
   * represents a single contiguous array of characters that may or may not
   * be null-terminated.
   *
   * Many of the accessors on nsTSubstring are inlined as an optimization.
   *
   * This class is also known as "nsASingleFragmentC?String".
   */
class nsTSubstring_CharT : public nsTAString_CharT
  {
    public:

      typedef nsTSubstring_CharT    self_type;
      typedef nsTString_CharT       string_type;

      typedef char_type*            char_iterator;
      typedef const char_type*      const_char_iterator;

    public:

        /**
         * reading iterators
         */

      const_char_iterator BeginReading() const { return mData; }
      const_char_iterator EndReading() const { return mData + mLength; }

        /**
         * deprecated reading iterators
         */

      const_iterator& BeginReading( const_iterator& iter ) const
        {
          iter.mStart = mData;
          iter.mEnd = mData + mLength;
          iter.mPosition = iter.mStart;
          return iter;
        }

      const_iterator& EndReading( const_iterator& iter ) const
        {
          iter.mStart = mData;
          iter.mEnd = mData + mLength;
          iter.mPosition = iter.mEnd;
          return iter;
        }

      const_char_iterator& BeginReading( const_char_iterator& iter ) const
        {
          return iter = mData;
        }

      const_char_iterator& EndReading( const_char_iterator& iter ) const
        {
          return iter = mData + mLength;
        }


        /**
         * writing iterators
         */
      
      char_iterator BeginWriting() { EnsureMutable(); return mData; }
      char_iterator EndWriting() { EnsureMutable(); return mData + mLength; }

        /**
         * deprecated writing iterators
         */
      
      iterator& BeginWriting( iterator& iter )
        {
          EnsureMutable();
          iter.mStart = mData;
          iter.mEnd = mData + mLength;
          iter.mPosition = iter.mStart;
          return iter;
        }

      iterator& EndWriting( iterator& iter )
        {
          EnsureMutable();
          iter.mStart = mData;
          iter.mEnd = mData + mLength;
          iter.mPosition = iter.mEnd;
          return iter;
        }

      char_iterator& BeginWriting( char_iterator& iter )
        {
          EnsureMutable();
          return iter = mData;
        }

      char_iterator& EndWriting( char_iterator& iter )
        {
          EnsureMutable();
          return iter = mData + mLength;
        }


        /**
         * accessors
         */

        // returns pointer to string data (not necessarily null-terminated)
      const char_type *Data() const
        {
          return mData;
        }

      size_type Length() const
        {
          return mLength;
        }

      PRBool IsEmpty() const
        {
          return mLength == 0;
        }

      PRBool IsVoid() const
        {
          return mFlags & F_VOIDED;
        }

      PRBool IsTerminated() const
        {
          return mFlags & F_TERMINATED;
        }

      char_type CharAt( index_type i ) const
        {
          NS_ASSERTION(i < mLength, "index exceeds allowable range");
          return mData[i];
        }

      char_type operator[]( index_type i ) const
        {
          return CharAt(i);
        }

      char_type First() const
        {
          NS_ASSERTION(mLength > 0, "|First()| called on an empty string");
          return mData[0];
        }

      inline
      char_type Last() const
        {
          NS_ASSERTION(mLength > 0, "|Last()| called on an empty string");
          return mData[mLength - 1];
        }

      NS_COM size_type CountChar( char_type ) const;
      NS_COM PRInt32 FindChar( char_type, index_type offset = 0 ) const;


        /**
         * equality
         */

      NS_COM PRBool Equals( const self_type& ) const;
      NS_COM PRBool Equals( const self_type&, const comparator_type& ) const;

      NS_COM PRBool Equals( const abstract_string_type& readable ) const;
      NS_COM PRBool Equals( const abstract_string_type& readable, const comparator_type& comp ) const;

      NS_COM PRBool Equals( const char_type* data ) const;
      NS_COM PRBool Equals( const char_type* data, const comparator_type& comp ) const;

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
         * assignment
         */

      void Assign( char_type c )                                                                { Assign(&c, 1); }
      NS_COM void Assign( const char_type* data, size_type length = size_type(-1) );
      NS_COM void Assign( const self_type& );
      NS_COM void Assign( const substring_tuple_type& );
      NS_COM void Assign( const abstract_string_type& );

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

      self_type& operator=( char_type c )                                                       { Assign(c);        return *this; }
      self_type& operator=( const char_type* data )                                             { Assign(data);     return *this; }
      self_type& operator=( const self_type& str )                                              { Assign(str);      return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                 { Assign(tuple);    return *this; }
      self_type& operator=( const abstract_string_type& readable )                              { Assign(readable); return *this; }

      NS_COM void Adopt( char_type* data, size_type length = size_type(-1) );


        /**
         * buffer manipulation
         */

             void Replace( index_type cutStart, size_type cutLength, char_type c )               { Replace(cutStart, cutLength, &c, 1); }
      NS_COM void Replace( index_type cutStart, size_type cutLength, const char_type* data, size_type length = size_type(-1) );
             void Replace( index_type cutStart, size_type cutLength, const self_type& str )      { Replace(cutStart, cutLength, str.Data(), str.Length()); }
      NS_COM void Replace( index_type cutStart, size_type cutLength, const substring_tuple_type& tuple );
      NS_COM void Replace( index_type cutStart, size_type cutLength, const abstract_string_type& readable );

      NS_COM void ReplaceASCII( index_type cutStart, size_type cutLength, const char* data, size_type length = size_type(-1) );

      void Append( char_type c )                                                                 { Replace(mLength, 0, c); }
      void Append( const char_type* data, size_type length = size_type(-1) )                     { Replace(mLength, 0, data, length); }
      void Append( const self_type& str )                                                        { Replace(mLength, 0, str); }
      void Append( const substring_tuple_type& tuple )                                           { Replace(mLength, 0, tuple); }
      void Append( const abstract_string_type& readable )                                        { Replace(mLength, 0, readable); }

      void AppendASCII( const char* data, size_type length = size_type(-1) )                     { ReplaceASCII(mLength, 0, data, length); }

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

      self_type& operator+=( char_type c )                                                       { Append(c);        return *this; }
      self_type& operator+=( const char_type* data )                                             { Append(data);     return *this; }
      self_type& operator+=( const self_type& str )                                              { Append(str);      return *this; }
      self_type& operator+=( const substring_tuple_type& tuple )                                 { Append(tuple);    return *this; }
      self_type& operator+=( const abstract_string_type& readable )                              { Append(readable); return *this; }

      void Insert( char_type c, index_type pos )                                                 { Replace(pos, 0, c); }
      void Insert( const char_type* data, index_type pos, size_type length = size_type(-1) )     { Replace(pos, 0, data, length); }
      void Insert( const self_type& str, index_type pos )                                        { Replace(pos, 0, str); }
      void Insert( const substring_tuple_type& tuple, index_type pos )                           { Replace(pos, 0, tuple); }
      void Insert( const abstract_string_type& readable, index_type pos )                        { Replace(pos, 0, readable); }

      void Cut( index_type cutStart, size_type cutLength )                                       { Replace(cutStart, cutLength, char_traits::sEmptyBuffer, 0); }


        /**
         * buffer sizing
         */

      NS_COM void SetCapacity( size_type capacity );

      NS_COM void SetLength( size_type );

      void Truncate( size_type newLength = 0 )
        {
          NS_ASSERTION(newLength <= mLength, "Truncate cannot make string longer");
          SetLength(newLength);
        }


        /**
         * string data is never null, but can be marked void.  if true, the
         * string will be truncated.  @see nsTSubstring::IsVoid
         */

      NS_COM void SetIsVoid( PRBool );


    public:

        /**
         * this is public to support automatic conversion of tuple to string
         * base type, which helps avoid converting to nsTAString.
         */
      nsTSubstring_CharT(const substring_tuple_type& tuple)
        : abstract_string_type(nsnull, 0, F_NONE)
        {
          Assign(tuple);
        }

    protected:

      friend class nsTObsoleteAStringThunk_CharT;
      friend class nsTAString_CharT;
      friend class nsTSubstringTuple_CharT;

      // XXX GCC 3.4 needs this :-(
      friend class nsTPromiseFlatString_CharT;

        // default initialization 
      nsTSubstring_CharT()
        : abstract_string_type(
              NS_CONST_CAST(char_type*, char_traits::sEmptyBuffer), 0, F_TERMINATED) {}

        // allow subclasses to initialize fields directly
      nsTSubstring_CharT( char_type *data, size_type length, PRUint32 flags )
        : abstract_string_type(data, length, flags) {}

        // version of constructor that leaves mData and mLength uninitialized
      explicit
      nsTSubstring_CharT( PRUint32 flags )
        : abstract_string_type(flags) {}

        // copy-constructor, constructs as dependent on given object
        // (NOTE: this is for internal use only)
      nsTSubstring_CharT( const self_type& str )
        : abstract_string_type(
              str.mData, str.mLength, str.mFlags & (F_TERMINATED | F_VOIDED)) {}

        /**
         * this function releases mData and does not change the value of
         * any of its member variables.  inotherwords, this function acts
         * like a destructor.
         */
      void Finalize();

        /**
         * this function prepares mData to be mutated.
         *
         * @param capacity     specifies the required capacity of mData  
         * @param old_data     returns null or the old value of mData
         * @param old_flags    returns 0 or the old value of mFlags
         *
         * if mData is already mutable and of sufficient capacity, then this
         * function will return immediately.  otherwise, it will either resize
         * mData or allocate a new shared buffer.  if it needs to allocate a
         * new buffer, then it will return the old buffer and the corresponding
         * flags.  this allows the caller to decide when to free the old data.
         *
         * XXX we should expose a way for subclasses to free old_data.
         */
      PRBool MutatePrep( size_type capacity, char_type** old_data, PRUint32* old_flags );

        /**
         * this function prepares a section of mData to be modified.  if
         * necessary, this function will reallocate mData and possibly move
         * existing data to open up the specified section.
         *
         * @param cutStart     specifies the starting offset of the section
         * @param cutLength    specifies the length of the section to be replaced
         * @param newLength    specifies the length of the new section
         *
         * for example, suppose mData contains the string "abcdef" then
         * 
         *   ReplacePrep(2, 3, 4);
         *
         * would cause mData to look like "ab____f" where the characters
         * indicated by '_' have an unspecified value and can be freely
         * modified.  this function will null-terminate mData upon return.
         */
      void ReplacePrep( index_type cutStart, size_type cutLength, size_type newLength );

        /**
         * returns the number of writable storage units starting at mData.
         * the value does not include space for the null-terminator character.
         *
         * NOTE: this function returns size_type(-1) if mData is immutable.
         */
      size_type Capacity() const;

        /**
         * this helper function can be called prior to directly manipulating
         * the contents of mData.  see, for example, BeginWriting.
         */
      NS_COM void EnsureMutable();

        /**
         * returns true if this string overlaps with the given string fragment.
         */
      PRBool IsDependentOn( const char_type *start, const char_type *end ) const
        {
          /**
           * if it _isn't_ the case that one fragment starts after the other ends,
           * or ends before the other starts, then, they conflict:
           * 
           *   !(f2.begin >= f1.end || f2.end <= f1.begin)
           * 
           * Simplified, that gives us:
           */
          return ( start < (mData + mLength) && end > mData );
        }

        /**
         * this helper function stores the specified dataFlags in mFlags
         */
      void SetDataFlags(PRUint32 dataFlags)
        {
          NS_ASSERTION((dataFlags & 0xFFFF0000) == 0, "bad flags");
          mFlags = dataFlags | (mFlags & 0xFFFF0000);
        }

    public:

      // mFlags is a bitwise combination of the following flags.  the meaning
      // and interpretation of these flags is an implementation detail.
      // 
      // NOTE: these flags are declared public _only_ for convenience inside
      // the string implementation.
      
      enum
        {
          F_NONE         = 0,       // no flags

          // data flags are in the lower 16-bits
          F_TERMINATED   = 1 << 0,  // IsTerminated returns true
          F_VOIDED       = 1 << 1,  // IsVoid returns true
          F_SHARED       = 1 << 2,  // mData points to a heap-allocated, shared buffer
          F_OWNED        = 1 << 3,  // mData points to a heap-allocated, raw buffer
          F_FIXED        = 1 << 4,  // mData points to a fixed-size writable, dependent buffer

          // class flags are in the upper 16-bits
          F_CLASS_FIXED  = 1 << 16   // indicates that |this| is of type nsTFixedString
        };

      //
      // Some terminology:
      //
      //   "dependent buffer"    A dependent buffer is one that the string class
      //                         does not own.  The string class relies on some
      //                         external code to ensure the lifetime of the
      //                         dependent buffer.
      //
      //   "shared buffer"       A shared buffer is one that the string class
      //                         allocates.  When it allocates a shared string
      //                         buffer, it allocates some additional space at
      //                         the beginning of the buffer for additional 
      //                         fields, including a reference count and a 
      //                         buffer length.  See nsStringHeader.
      //                         
      //   "adopted buffer"      An adopted buffer is a raw string buffer
      //                         allocated on the heap (using nsMemory::Alloc)
      //                         of which the string class subsumes ownership.
      //
      // Some comments about the string flags:
      //
      //   F_SHARED, F_OWNED, and F_FIXED are all mutually exlusive.  They
      //   indicate the allocation type of mData.  If none of these flags
      //   are set, then the string buffer is dependent.
      //
      //   F_SHARED, F_OWNED, or F_FIXED imply F_TERMINATED.  This is because
      //   the string classes always allocate null-terminated buffers, and
      //   non-terminated substrings are always dependent.
      //
      //   F_VOIDED implies F_TERMINATED, and moreover it implies that mData
      //   points to char_traits::sEmptyBuffer.  Therefore, F_VOIDED is
      //   mutually exclusive with F_SHARED, F_OWNED, and F_FIXED.
      //
  };
