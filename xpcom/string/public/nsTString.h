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
 *   Rick Gessner <rickg@netscape.com> (original author)
 *   Scott Collins <scc@mozilla.org>
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
   * This is the canonical null-terminated string class.  All subclasses
   * promise null-terminated storage.  Instances of this class allocate
   * strings on the heap.
   *
   * This class is also known as nsAFlat[C]String, where "flat" is used
   * to denote a null-terminated string.
   */
class nsTString_CharT : public nsTSubstring_CharT
  {
    public:

      typedef nsTString_CharT    self_type;

    public:

        /**
         * constructors
         */

      nsTString_CharT()
        : substring_type() {}

      explicit
      nsTString_CharT( char_type c )
        : substring_type()
        {
          Assign(c);
        }

      explicit
      nsTString_CharT( const char_type* data, size_type length = size_type(-1) )
        : substring_type()
        {
          Assign(data, length);
        }

      nsTString_CharT( const self_type& str )
        : substring_type()
        {
          Assign(str);
        }

      nsTString_CharT( const substring_tuple_type& tuple )
        : substring_type()
        {
          Assign(tuple);
        }

      explicit
      nsTString_CharT( const abstract_string_type& readable )
        : substring_type()
        {
          Assign(readable);
        }


        // |operator=| does not inherit, so we must define our own
      self_type& operator=( char_type c )                                                       { Assign(c);        return *this; }
      self_type& operator=( const char_type* data )                                             { Assign(data);     return *this; }
      self_type& operator=( const self_type& str )                                              { Assign(str);      return *this; }
      self_type& operator=( const substring_type& str )                                         { Assign(str);      return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                 { Assign(tuple);    return *this; }
      self_type& operator=( const abstract_string_type& readable )                              { Assign(readable); return *this; }


        /**
         * returns the null-terminated string
         */

      const char_type* get() const
        {
          return mData;
        }


        /**
         * returns character at specified index.
         *         
         * NOTE: unlike nsTSubstring::CharAt, this function allows you to index
         *       the null terminator character.
         */

      char_type CharAt( index_type i ) const
        {
          NS_ASSERTION(i <= mLength, "index exceeds allowable range");
          return mData[i];
        }

      char_type operator[]( index_type i ) const
        {
          return CharAt(i);
        }


#if MOZ_STRING_WITH_OBSOLETE_API


        /**
         *  Search for the given substring within this string.
         *  
         *  @param   aString is substring to be sought in this
         *  @param   aIgnoreCase selects case sensitivity
         *  @param   aOffset tells us where in this string to start searching
         *  @param   aCount tells us how far from the offset we are to search. Use
         *           -1 to search the whole string.
         *  @return  offset in string, or kNotFound
         */

      NS_COM PRInt32 Find( const nsCString& aString, PRBool aIgnoreCase=PR_FALSE, PRInt32 aOffset=0, PRInt32 aCount=-1 ) const;
      NS_COM PRInt32 Find( const char* aString, PRBool aIgnoreCase=PR_FALSE, PRInt32 aOffset=0, PRInt32 aCount=-1 ) const;

#ifdef CharT_is_PRUnichar
      NS_COM PRInt32 Find( const nsAFlatString& aString, PRInt32 aOffset=0, PRInt32 aCount=-1 ) const;
      NS_COM PRInt32 Find( const PRUnichar* aString, PRInt32 aOffset=0, PRInt32 aCount=-1 ) const;
#endif

        
        /**
         * This methods scans the string backwards, looking for the given string
         *
         * @param   aString is substring to be sought in this
         * @param   aIgnoreCase tells us whether or not to do caseless compare
         * @param   aOffset tells us where in this string to start searching.
         *          Use -1 to search from the end of the string.
         * @param   aCount tells us how many iterations to make starting at the
         *          given offset.
         * @return  offset in string, or kNotFound
         */

      NS_COM PRInt32 RFind( const nsCString& aString, PRBool aIgnoreCase=PR_FALSE, PRInt32 aOffset=-1, PRInt32 aCount=-1 ) const;
      NS_COM PRInt32 RFind( const char* aCString, PRBool aIgnoreCase=PR_FALSE, PRInt32 aOffset=-1, PRInt32 aCount=-1 ) const;

#ifdef CharT_is_PRUnichar
      NS_COM PRInt32 RFind( const nsAFlatString& aString, PRInt32 aOffset=-1, PRInt32 aCount=-1 ) const;
      NS_COM PRInt32 RFind( const PRUnichar* aString, PRInt32 aOffset=-1, PRInt32 aCount=-1 ) const;
#endif


        /**
         *  Search for given char within this string
         *  
         *  @param   aChar is the character to search for
         *  @param   aOffset tells us where in this strig to start searching
         *  @param   aCount tells us how far from the offset we are to search.
         *           Use -1 to search the whole string.
         *  @return  offset in string, or kNotFound
         */

      // PRInt32 FindChar( PRUnichar aChar, PRInt32 aOffset=0, PRInt32 aCount=-1 ) const;
      NS_COM PRInt32 RFindChar( PRUnichar aChar, PRInt32 aOffset=-1, PRInt32 aCount=-1 ) const;


        /**
         * This method searches this string for the first character found in
         * the given string.
         *
         * @param aString contains set of chars to be found
         * @param aOffset tells us where in this string to start searching
         *        (counting from left)
         * @return offset in string, or kNotFound
         */

      NS_COM PRInt32 FindCharInSet( const char* aString, PRInt32 aOffset=0 ) const;
      PRInt32 FindCharInSet( const self_type& aString, PRInt32 aOffset=0 ) const
        {
          return FindCharInSet(aString.get(), aOffset);
        }

#ifdef CharT_is_PRUnichar
      NS_COM PRInt32 FindCharInSet( const PRUnichar* aString, PRInt32 aOffset=0 ) const;
#endif


        /**
         * This method searches this string for the last character found in
         * the given string.
         *
         * @param aString contains set of chars to be found
         * @param aOffset tells us where in this string to start searching
         *        (counting from left)
         * @return offset in string, or kNotFound
         */

      NS_COM PRInt32 RFindCharInSet( const char_type* aString, PRInt32 aOffset=-1 ) const;
      PRInt32 RFindCharInSet( const self_type& aString, PRInt32 aOffset=-1 ) const
        {
          return RFindCharInSet(aString.get(), aOffset);
        }


        /**
         * Compares a given string to this string. 
         *
         * @param   aString is the string to be compared
         * @param   aIgnoreCase tells us how to treat case
         * @param   aCount tells us how many chars to compare
         * @return  -1,0,1
         */

#ifdef CharT_is_char
      NS_COM PRInt32 Compare( const char* aString, PRBool aIgnoreCase=PR_FALSE, PRInt32 aCount=-1 ) const;
#endif


        /**
         * Equality check between given string and this string.
         *
         * @param   aString is the string to check
         * @param   aIgnoreCase tells us how to treat case
         * @param   aCount tells us how many chars to compare
         * @return  boolean
         */
#ifdef CharT_is_char
      PRBool EqualsIgnoreCase( const char* aString, PRInt32 aCount=-1 ) const {
        return Compare(aString, PR_TRUE, aCount) == 0;
      }
#else
      PRBool EqualsIgnoreCase( const char* aString, PRInt32 aCount=-1 ) const;


        /**
         * Copies data from internal buffer onto given char* buffer
         *
         * NOTE: This only copies as many chars as will fit in given buffer (clips)
         * @param aBuf is the buffer where data is stored
         * @param aBuflength is the max # of chars to move to buffer
         * @param aOffset is the offset to copy from
         * @return ptr to given buffer
         */

      NS_COM char* ToCString( char* aBuf, PRUint32 aBufLength, PRUint32 aOffset=0 ) const;

#endif // !CharT_is_PRUnichar

        /**
         * Perform string to float conversion.
         *
         * @param   aErrorCode will contain error if one occurs
         * @return  float rep of string value
         */
      NS_COM float ToFloat( PRInt32* aErrorCode ) const;


        /**
         * Perform string to int conversion.
         * @param   aErrorCode will contain error if one occurs
         * @param   aRadix tells us which radix to assume; kAutoDetect tells us to determine the radix for you.
         * @return  int rep of string value, and possible (out) error code
         */
      NS_COM PRInt32 ToInteger( PRInt32* aErrorCode, PRUint32 aRadix=kRadix10 ) const;
      

        /**
         * |Left|, |Mid|, and |Right| are annoying signatures that seem better almost
         * any _other_ way than they are now.  Consider these alternatives
         * 
         * aWritable = aReadable.Left(17);   // ...a member function that returns a |Substring|
         * aWritable = Left(aReadable, 17);  // ...a global function that returns a |Substring|
         * Left(aReadable, 17, aWritable);   // ...a global function that does the assignment
         * 
         * as opposed to the current signature
         * 
         * aReadable.Left(aWritable, 17);    // ...a member function that does the assignment
         * 
         * or maybe just stamping them out in favor of |Substring|, they are just duplicate functionality
         *         
         * aWritable = Substring(aReadable, 0, 17);
         */

      NS_COM size_type Mid( self_type& aResult, PRUint32 aStartPos, PRUint32 aCount ) const;

      size_type Left( self_type& aResult, size_type aCount ) const
        {
          return Mid(aResult, 0, aCount);
        }

      size_type Right( self_type& aResult, size_type aCount ) const
        {
          aCount = NS_MIN(mLength, aCount);
          return Mid(aResult, mLength - aCount, aCount);
        }


        /**
         * Set a char inside this string at given index
         *
         * @param aChar is the char you want to write into this string
         * @param anIndex is the ofs where you want to write the given char
         * @return TRUE if successful
         */

      NS_COM PRBool SetCharAt( PRUnichar aChar, PRUint32 aIndex );


        /**
         *  These methods are used to remove all occurances of the
         *  characters found in aSet from this string.
         *  
         *  @param  aSet -- characters to be cut from this
         */
      NS_COM void StripChars( const char* aSet );


        /**
         *  This method is used to remove all occurances of aChar from this
         * string.
         *  
         *  @param  aChar -- char to be stripped
         *  @param  aOffset -- where in this string to start stripping chars
         */
         
      NS_COM void StripChar( char_type aChar, PRInt32 aOffset=0 );


        /**
         *  This method strips whitespace throughout the string.
         */
      NS_COM void StripWhitespace();


        /**
         *  swaps occurence of 1 string for another
         */

      NS_COM void ReplaceChar( char_type aOldChar, char_type aNewChar );
      NS_COM void ReplaceChar( const char* aSet, char_type aNewChar );
      NS_COM void ReplaceSubstring( const self_type& aTarget, const self_type& aNewValue);
      NS_COM void ReplaceSubstring( const char_type* aTarget, const char_type* aNewValue);


        /**
         *  This method trims characters found in aTrimSet from
         *  either end of the underlying string.
         *  
         *  @param   aSet -- contains chars to be trimmed from both ends
         *  @param   aEliminateLeading
         *  @param   aEliminateTrailing
         *  @param   aIgnoreQuotes -- if true, causes surrounding quotes to be ignored
         *  @return  this
         */
      NS_COM void Trim( const char* aSet, PRBool aEliminateLeading=PR_TRUE, PRBool aEliminateTrailing=PR_TRUE, PRBool aIgnoreQuotes=PR_FALSE );

        /**
         *  This method strips whitespace from string.
         *  You can control whether whitespace is yanked from start and end of
         *  string as well.
         *  
         *  @param   aEliminateLeading controls stripping of leading ws
         *  @param   aEliminateTrailing controls stripping of trailing ws
         */
      NS_COM void CompressWhitespace( PRBool aEliminateLeading=PR_TRUE, PRBool aEliminateTrailing=PR_TRUE );


        /**
         * assign/append/insert with _LOSSY_ conversion
         */

      NS_COM void AssignWithConversion( const nsTAString_IncompatibleCharT& aString );
      NS_COM void AssignWithConversion( const incompatible_char_type* aData, PRInt32 aLength=-1 );

      NS_COM void AppendWithConversion( const nsTAString_IncompatibleCharT& aString );
      NS_COM void AppendWithConversion( const incompatible_char_type* aData, PRInt32 aLength=-1 );

        /**
         * Append the given integer to this string 
         */
      NS_COM void AppendInt( PRInt32 aInteger, PRInt32 aRadix=kRadix10 ); //radix=8,10 or 16

        /**
         * Append the given unsigned integer to this string
         */
      inline void AppendInt( PRUint32 aInteger, PRInt32 aRadix = kRadix10 )
        {
          AppendInt(PRInt32(aInteger), aRadix);
        }

        /**
         * Append the given 64-bit integer to this string.
         * @param aInteger The integer to append
         * @param aRadix   The radix to use; can be 8, 10 or 16.
         */
      NS_COM void AppendInt( PRInt64 aInteger, PRInt32 aRadix=kRadix10 );

        /**
         * Append the given float to this string 
         */

      NS_COM void AppendFloat( double aFloat );


#endif // !MOZ_STRING_WITH_OBSOLETE_API


    protected:

      explicit
      nsTString_CharT( PRUint32 flags )
        : substring_type(flags) {}

        // allow subclasses to initialize fields directly
      nsTString_CharT( char_type* data, size_type length, PRUint32 flags )
        : substring_type(data, length, flags) {}
  };


class nsTFixedString_CharT : public nsTString_CharT
  {
    public:

      typedef nsTFixedString_CharT    self_type;
      typedef nsTFixedString_CharT    fixed_string_type;

    public:

        /**
         * @param data
         *        fixed-size buffer to be used by the string (the contents of
         *        this buffer may be modified by the string)
         * @param storageSize
         *        the size of the fixed buffer
         * @param length (optional)
         *        the length of the string already contained in the buffer
         */

      nsTFixedString_CharT( char_type* data, size_type storageSize )
        : string_type(data, char_traits::length(data), F_TERMINATED | F_FIXED | F_CLASS_FIXED)
        , mFixedCapacity(storageSize - 1)
        , mFixedBuf(data)
        {}

      nsTFixedString_CharT( char_type* data, size_type storageSize, size_type length )
        : string_type(data, length, F_TERMINATED | F_FIXED | F_CLASS_FIXED)
        , mFixedCapacity(storageSize - 1)
        , mFixedBuf(data)
        {
          // null-terminate
          mFixedBuf[length] = char_type(0);
        }

        // |operator=| does not inherit, so we must define our own
      self_type& operator=( char_type c )                                                       { Assign(c);        return *this; }
      self_type& operator=( const char_type* data )                                             { Assign(data);     return *this; }
      self_type& operator=( const substring_type& str )                                         { Assign(str);      return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                 { Assign(tuple);    return *this; }
      self_type& operator=( const abstract_string_type& readable )                              { Assign(readable); return *this; }

    protected:

      friend class nsTSubstring_CharT;

      size_type  mFixedCapacity;
      char_type *mFixedBuf;
  };


  /**
   * nsTAutoString_CharT
   *
   * Subclass of nsTString_CharT that adds support for stack-based string
   * allocation.  Do not allocate this class on the heap! ;-)
   */
class nsTAutoString_CharT : public nsTFixedString_CharT
  {
    public:

      typedef nsTAutoString_CharT    self_type;

    public:

        /**
         * constructors
         */

      nsTAutoString_CharT()
        : fixed_string_type(mStorage, kDefaultStorageSize, 0)
        {}

      explicit
      nsTAutoString_CharT( char_type c )
        : fixed_string_type(mStorage, kDefaultStorageSize, 0)
        {
          Assign(c);
        }

      explicit
      nsTAutoString_CharT( const char_type* data, size_type length = size_type(-1) )
        : fixed_string_type(mStorage, kDefaultStorageSize, 0)
        {
          Assign(data, length);
        }

      nsTAutoString_CharT( const self_type& str )
        : fixed_string_type(mStorage, kDefaultStorageSize, 0)
        {
          Assign(str);
        }

      explicit
      nsTAutoString_CharT( const substring_type& str )
        : fixed_string_type(mStorage, kDefaultStorageSize, 0)
        {
          Assign(str);
        }

      nsTAutoString_CharT( const substring_tuple_type& tuple )
        : fixed_string_type(mStorage, kDefaultStorageSize, 0)
        {
          Assign(tuple);
        }

      explicit
      nsTAutoString_CharT( const abstract_string_type& readable )
        : fixed_string_type(mStorage, kDefaultStorageSize, 0)
        {
          Assign(readable);
        }

        // |operator=| does not inherit, so we must define our own
      self_type& operator=( char_type c )                                                       { Assign(c);        return *this; }
      self_type& operator=( const char_type* data )                                             { Assign(data);     return *this; }
      self_type& operator=( const self_type& str )                                              { Assign(str);      return *this; }
      self_type& operator=( const substring_type& str )                                         { Assign(str);      return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                 { Assign(tuple);    return *this; }
      self_type& operator=( const abstract_string_type& readable )                              { Assign(readable); return *this; }

      enum { kDefaultStorageSize = 64 };

    private:

      char_type mStorage[kDefaultStorageSize];
  };


  /**
   * nsTXPIDLString extends nsTString such that:
   *
   *   (1) mData can be null
   *   (2) objects of this type can be automatically cast to |const CharT*|
   *   (3) getter_Copies method is supported to adopt data
   */
class nsTXPIDLString_CharT : public nsTString_CharT
  {
    public:

      typedef nsTXPIDLString_CharT    self_type;

    public:

      nsTXPIDLString_CharT()
        : string_type(NS_CONST_CAST(char_type*, char_traits::sEmptyBuffer), 0, F_TERMINATED | F_VOIDED) {}

        // copy-constructor required to avoid default
      nsTXPIDLString_CharT( const self_type& str )
        : string_type(NS_CONST_CAST(char_type*, char_traits::sEmptyBuffer), 0, F_TERMINATED | F_VOIDED)
        {
          Assign(str);
        }

        // return nsnull if we are voided
      const char_type* get() const
        {
          return (mFlags & F_VOIDED) ? nsnull : mData;
        }

        // this case operator is the reason why this class cannot just be a
        // typedef for nsTString
      operator const char_type*() const
        {
          return get();
        }

        // need this to diambiguous operator[int]
      char_type operator[]( PRInt32 i ) const
        {
          return CharAt(index_type(i));
        }

        // |operator=| does not inherit, so we must define our own
      self_type& operator=( char_type c )                                                       { Assign(c);        return *this; }
      self_type& operator=( const char_type* data )                                             { Assign(data);     return *this; }
      self_type& operator=( const self_type& str )                                              { Assign(str);      return *this; }
      self_type& operator=( const substring_type& str )                                         { Assign(str);      return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                 { Assign(tuple);    return *this; }
      self_type& operator=( const abstract_string_type& readable )                              { Assign(readable); return *this; }
  };


  /**
   * getter_Copies support for use with raw string out params:
   *
   *    NS_IMETHOD GetBlah(char**);
   *    
   *    void some_function()
   *    {
   *      nsXPIDLCString blah;
   *      GetBlah(getter_Copies(blah));
   *      // ...
   *    }
   */
class nsTGetterCopies_CharT
  {
    public:
      typedef CharT char_type;

      nsTGetterCopies_CharT(nsTXPIDLString_CharT& str)
        : mString(str), mData(nsnull) {}

      ~nsTGetterCopies_CharT()
        {
          mString.Adopt(mData); // OK if mData is null
        }

      operator char_type**()
        {
          return &mData;
        }

    private:
      nsTXPIDLString_CharT& mString;
      char_type*            mData;
  };

inline
nsTGetterCopies_CharT
getter_Copies( nsTXPIDLString_CharT& aString )
  {
    return nsTGetterCopies_CharT(aString);
  }


  /**
   * nsTAdoptingString extends nsTXPIDLString such that:
   *
   * (1) Adopt given string on construction or assignment, i.e. take
   * the value of what's given, and make what's given forget its
   * value. Note that this class violates constness in a few
   * places. Be careful!
   */
class nsTAdoptingString_CharT : public nsTXPIDLString_CharT
  {
    public:

      typedef nsTAdoptingString_CharT    self_type;

    public:

      explicit nsTAdoptingString_CharT() {}
      explicit nsTAdoptingString_CharT(char_type* str, size_type length = size_type(-1))
        {
          Adopt(str, length);
        }

        // copy-constructor required to adopt on copy. Note that this
        // will violate the constness of |str| in the operator=()
        // call. |str| will be truncated as a side-effect of this
        // constructor.
      nsTAdoptingString_CharT( const self_type& str )
        {
          *this = str;
        }

        // |operator=| does not inherit, so we must define our own
      self_type& operator=( const substring_type& str )                                         { Assign(str);      return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                 { Assign(tuple);    return *this; }
      self_type& operator=( const abstract_string_type& readable )                              { Assign(readable); return *this; }

        // Adopt(), if possible, when assigning to a self_type&. Note
        // that this violates the constness of str, str is always
        // truncated when this operator is called.
      NS_COM self_type& operator=( const self_type& str );

    private:
        // NOT TO BE IMPLEMENTED.
      self_type& operator=( const char_type* data );
      self_type& operator=( char_type* data );
  };

