/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

  /**
   * This is the canonical null-terminated string class.  All subclasses
   * promise null-terminated storage.  Instances of this class allocate
   * strings on the heap.
   *
   * NAMES:
   *   nsString for wide characters
   *   nsCString for narrow characters
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
      nsTString_CharT( const substring_type& readable )
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

      int32_t Find( const nsCString& aString, bool aIgnoreCase=false, int32_t aOffset=0, int32_t aCount=-1 ) const;
      int32_t Find( const char* aString, bool aIgnoreCase=false, int32_t aOffset=0, int32_t aCount=-1 ) const;

#ifdef CharT_is_PRUnichar
      int32_t Find( const nsAFlatString& aString, int32_t aOffset=0, int32_t aCount=-1 ) const;
      int32_t Find( const PRUnichar* aString, int32_t aOffset=0, int32_t aCount=-1 ) const;
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

      int32_t RFind( const nsCString& aString, bool aIgnoreCase=false, int32_t aOffset=-1, int32_t aCount=-1 ) const;
      int32_t RFind( const char* aCString, bool aIgnoreCase=false, int32_t aOffset=-1, int32_t aCount=-1 ) const;

#ifdef CharT_is_PRUnichar
      int32_t RFind( const nsAFlatString& aString, int32_t aOffset=-1, int32_t aCount=-1 ) const;
      int32_t RFind( const PRUnichar* aString, int32_t aOffset=-1, int32_t aCount=-1 ) const;
#endif


        /**
         *  Search for given char within this string
         *  
         *  @param   aChar is the character to search for
         *  @param   aOffset tells us where in this string to start searching
         *  @param   aCount tells us how far from the offset we are to search.
         *           Use -1 to search the whole string.
         *  @return  offset in string, or kNotFound
         */

      // int32_t FindChar( PRUnichar aChar, int32_t aOffset=0, int32_t aCount=-1 ) const;
      int32_t RFindChar( PRUnichar aChar, int32_t aOffset=-1, int32_t aCount=-1 ) const;


        /**
         * This method searches this string for the first character found in
         * the given string.
         *
         * @param aString contains set of chars to be found
         * @param aOffset tells us where in this string to start searching
         *        (counting from left)
         * @return offset in string, or kNotFound
         */

      int32_t FindCharInSet( const char* aString, int32_t aOffset=0 ) const;
      int32_t FindCharInSet( const self_type& aString, int32_t aOffset=0 ) const
        {
          return FindCharInSet(aString.get(), aOffset);
        }

#ifdef CharT_is_PRUnichar
      int32_t FindCharInSet( const PRUnichar* aString, int32_t aOffset=0 ) const;
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

      int32_t RFindCharInSet( const char_type* aString, int32_t aOffset=-1 ) const;
      int32_t RFindCharInSet( const self_type& aString, int32_t aOffset=-1 ) const
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
      int32_t Compare( const char* aString, bool aIgnoreCase=false, int32_t aCount=-1 ) const;
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
      bool EqualsIgnoreCase( const char* aString, int32_t aCount=-1 ) const {
        return Compare(aString, true, aCount) == 0;
      }
#else
      bool EqualsIgnoreCase( const char* aString, int32_t aCount=-1 ) const;


#endif // !CharT_is_PRUnichar

        /**
         * Perform string to double-precision float conversion.
         *
         * @param   aErrorCode will contain error if one occurs
         * @return  double-precision float rep of string value
         */
      double ToDouble( nsresult* aErrorCode ) const;

        /**
         * Perform string to single-precision float conversion.
         *
         * @param   aErrorCode will contain error if one occurs
         * @return  single-precision float rep of string value
         */
      float ToFloat( nsresult* aErrorCode ) const {
        return (float)ToDouble(aErrorCode);
      }


        /**
         * Perform string to int conversion.
         * @param   aErrorCode will contain error if one occurs
         * @param   aRadix tells us which radix to assume; kAutoDetect tells us to determine the radix for you.
         * @return  int rep of string value, and possible (out) error code
         */
      int32_t ToInteger( nsresult* aErrorCode, uint32_t aRadix=kRadix10 ) const;

        /**
         * Perform string to 64-bit int conversion.
         * @param   aErrorCode will contain error if one occurs
         * @param   aRadix tells us which radix to assume; kAutoDetect tells us to determine the radix for you.
         * @return  64-bit int rep of string value, and possible (out) error code
         */
      int64_t ToInteger64( nsresult* aErrorCode, uint32_t aRadix=kRadix10 ) const;


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

      size_type Mid( self_type& aResult, uint32_t aStartPos, uint32_t aCount ) const;

      size_type Left( self_type& aResult, size_type aCount ) const
        {
          return Mid(aResult, 0, aCount);
        }

      size_type Right( self_type& aResult, size_type aCount ) const
        {
          aCount = XPCOM_MIN(mLength, aCount);
          return Mid(aResult, mLength - aCount, aCount);
        }


        /**
         * Set a char inside this string at given index
         *
         * @param aChar is the char you want to write into this string
         * @param anIndex is the ofs where you want to write the given char
         * @return TRUE if successful
         */

      bool SetCharAt( PRUnichar aChar, uint32_t aIndex );


        /**
         *  These methods are used to remove all occurrences of the
         *  characters found in aSet from this string.
         *  
         *  @param  aSet -- characters to be cut from this
         */
      void StripChars( const char* aSet );


        /**
         *  This method strips whitespace throughout the string.
         */
      void StripWhitespace();


        /**
         *  swaps occurence of 1 string for another
         */

      void ReplaceChar( char_type aOldChar, char_type aNewChar );
      void ReplaceChar( const char* aSet, char_type aNewChar );
      void ReplaceSubstring( const self_type& aTarget, const self_type& aNewValue);
      void ReplaceSubstring( const char_type* aTarget, const char_type* aNewValue);


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
      void Trim( const char* aSet, bool aEliminateLeading=true, bool aEliminateTrailing=true, bool aIgnoreQuotes=false );

        /**
         *  This method strips whitespace from string.
         *  You can control whether whitespace is yanked from start and end of
         *  string as well.
         *  
         *  @param   aEliminateLeading controls stripping of leading ws
         *  @param   aEliminateTrailing controls stripping of trailing ws
         */
      void CompressWhitespace( bool aEliminateLeading=true, bool aEliminateTrailing=true );


        /**
         * assign/append/insert with _LOSSY_ conversion
         */

      void AssignWithConversion( const nsTAString_IncompatibleCharT& aString );
      void AssignWithConversion( const incompatible_char_type* aData, int32_t aLength=-1 );

#endif // !MOZ_STRING_WITH_OBSOLETE_API

      void Rebind( const char_type* data, size_type length );

        /**
         * verify restrictions for dependent strings
         */
      void AssertValidDepedentString()
        {
          NS_ASSERTION(mData, "nsTDependentString must wrap a non-NULL buffer");
          NS_ASSERTION(mLength != size_type(-1), "nsTDependentString has bogus length");
          NS_ASSERTION(mData[mLength] == 0, "nsTDependentString must wrap only null-terminated strings");
        }


    protected:

      explicit
      nsTString_CharT( uint32_t flags )
        : substring_type(flags) {}

        // allow subclasses to initialize fields directly
      nsTString_CharT( char_type* data, size_type length, uint32_t flags )
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
        : string_type(data, uint32_t(char_traits::length(data)), F_TERMINATED | F_FIXED | F_CLASS_FIXED)
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

    protected:

      friend class nsTSubstring_CharT;

      size_type  mFixedCapacity;
      char_type *mFixedBuf;
  };


  /**
   * nsTAutoString_CharT
   *
   * Subclass of nsTString_CharT that adds support for stack-based string
   * allocation.  It is normally not a good idea to use this class on the
   * heap, because it will allocate space which may be wasted if the string
   * it contains is significantly smaller or any larger than 64 characters.
   *
   * NAMES:
   *   nsAutoString for wide characters
   *   nsAutoCString for narrow characters
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

        // |operator=| does not inherit, so we must define our own
      self_type& operator=( char_type c )                                                       { Assign(c);        return *this; }
      self_type& operator=( const char_type* data )                                             { Assign(data);     return *this; }
      self_type& operator=( const self_type& str )                                              { Assign(str);      return *this; }
      self_type& operator=( const substring_type& str )                                         { Assign(str);      return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                 { Assign(tuple);    return *this; }

      enum { kDefaultStorageSize = 64 };

    private:

      char_type mStorage[kDefaultStorageSize];
  };


  //
  // nsAutoString stores pointers into itself which are invalidated when an
  // nsTArray is resized, so nsTArray must not be instantiated with nsAutoString
  // elements!
  //
  template<class E> class nsTArrayElementTraits;
  template<>
  class nsTArrayElementTraits<nsTAutoString_CharT> {
    public:
      template<class A> struct Dont_Instantiate_nsTArray_of;
      template<class A> struct Instead_Use_nsTArray_of;

      static Dont_Instantiate_nsTArray_of<nsTAutoString_CharT> *
      Construct(Instead_Use_nsTArray_of<nsTString_CharT> *e) {
        return 0;
      }
      template<class A>
      static Dont_Instantiate_nsTArray_of<nsTAutoString_CharT> *
      Construct(Instead_Use_nsTArray_of<nsTString_CharT> *e,
                const A &arg) {
        return 0;
      }
      static Dont_Instantiate_nsTArray_of<nsTAutoString_CharT> *
      Destruct(Instead_Use_nsTArray_of<nsTString_CharT> *e) {
        return 0;
      }
  };

  /**
   * nsTXPIDLString extends nsTString such that:
   *
   *   (1) mData can be null
   *   (2) objects of this type can be automatically cast to |const CharT*|
   *   (3) getter_Copies method is supported to adopt data allocated with
   *       NS_Alloc, such as "out string" parameters in XPIDL.
   *
   * NAMES:
   *   nsXPIDLString for wide characters
   *   nsXPIDLCString for narrow characters
   */
class nsTXPIDLString_CharT : public nsTString_CharT
  {
    public:

      typedef nsTXPIDLString_CharT    self_type;

    public:

      nsTXPIDLString_CharT()
        : string_type(char_traits::sEmptyBuffer, 0, F_TERMINATED | F_VOIDED) {}

        // copy-constructor required to avoid default
      nsTXPIDLString_CharT( const self_type& str )
        : string_type(char_traits::sEmptyBuffer, 0, F_TERMINATED | F_VOIDED)
        {
          Assign(str);
        }

        // return nullptr if we are voided
      const char_type* get() const
        {
          return (mFlags & F_VOIDED) ? nullptr : mData;
        }

        // this case operator is the reason why this class cannot just be a
        // typedef for nsTString
      operator const char_type*() const
        {
          return get();
        }

        // need this to diambiguous operator[int]
      char_type operator[]( int32_t i ) const
        {
          return CharAt(index_type(i));
        }

        // |operator=| does not inherit, so we must define our own
      self_type& operator=( char_type c )                                                       { Assign(c);        return *this; }
      self_type& operator=( const char_type* data )                                             { Assign(data);     return *this; }
      self_type& operator=( const self_type& str )                                              { Assign(str);      return *this; }
      self_type& operator=( const substring_type& str )                                         { Assign(str);      return *this; }
      self_type& operator=( const substring_tuple_type& tuple )                                 { Assign(tuple);    return *this; }
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
class MOZ_STACK_CLASS nsTGetterCopies_CharT
  {
    public:
      typedef CharT char_type;

      nsTGetterCopies_CharT(nsTSubstring_CharT& str)
        : mString(str), mData(nullptr) {}

      ~nsTGetterCopies_CharT()
        {
          mString.Adopt(mData); // OK if mData is null
        }

      operator char_type**()
        {
          return &mData;
        }

    private:
      nsTSubstring_CharT&      mString;
      char_type*            mData;
  };

inline
nsTGetterCopies_CharT
getter_Copies( nsTSubstring_CharT& aString )
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

        // Adopt(), if possible, when assigning to a self_type&. Note
        // that this violates the constness of str, str is always
        // truncated when this operator is called.
      self_type& operator=( const self_type& str );

    private:
      self_type& operator=( const char_type* data ) MOZ_DELETE;
      self_type& operator=( char_type* data ) MOZ_DELETE;
  };

