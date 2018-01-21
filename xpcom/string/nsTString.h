/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#ifndef nsTString_h
#define nsTString_h

#include "nsTSubstring.h"

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
template <typename T>
class nsTString : public nsTSubstring<T>
{
public:

  typedef nsTString<T> self_type;

#ifdef __clang__
  // bindgen w/ clang 3.9 at least chokes on a typedef, but using is okay.
  using typename nsTSubstring<T>::substring_type;
#else
  // On the other hand msvc chokes on the using statement. It seems others
  // don't care either way so we lump them in here.
  typedef typename nsTSubstring<T>::substring_type substring_type;
#endif

  typedef typename substring_type::literalstring_type literalstring_type;

  typedef typename substring_type::fallible_t fallible_t;

  typedef typename substring_type::char_type char_type;
  typedef typename substring_type::char_traits char_traits;
  typedef typename substring_type::incompatible_char_type incompatible_char_type;

  typedef typename substring_type::substring_tuple_type substring_tuple_type;

  typedef typename substring_type::const_iterator const_iterator;
  typedef typename substring_type::iterator iterator;

  typedef typename substring_type::comparator_type comparator_type;

  typedef typename substring_type::char_iterator char_iterator;
  typedef typename substring_type::const_char_iterator const_char_iterator;

  typedef typename substring_type::index_type index_type;
  typedef typename substring_type::size_type size_type;

  // These are only for internal use within the string classes:
  typedef typename substring_type::DataFlags DataFlags;
  typedef typename substring_type::ClassFlags ClassFlags;

public:

  /**
   * constructors
   */

  nsTString()
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
  }

  explicit
  nsTString(const char_type* aData, size_type aLength = size_type(-1))
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
    this->Assign(aData, aLength);
  }

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  explicit
  nsTString(char16ptr_t aStr, size_type aLength = size_type(-1))
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
    this->Assign(static_cast<const char16_t*>(aStr), aLength);
  }
#endif

  nsTString(const self_type& aStr)
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
    this->Assign(aStr);
  }

  nsTString(self_type&& aStr)
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
    this->Assign(mozilla::Move(aStr));
  }

  MOZ_IMPLICIT nsTString(const substring_tuple_type& aTuple)
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
    this->Assign(aTuple);
  }

  explicit
  nsTString(const substring_type& aReadable)
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
    this->Assign(aReadable);
  }

  explicit
  nsTString(substring_type&& aReadable)
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
    this->Assign(mozilla::Move(aReadable));
  }

  // NOTE(nika): gcc 4.9 workaround. Remove when support is dropped.
  explicit
  nsTString(const literalstring_type& aReadable)
    : substring_type(ClassFlags::NULL_TERMINATED)
  {
    this->Assign(aReadable);
  }


  // |operator=| does not inherit, so we must define our own
  self_type& operator=(char_type aChar)
  {
    this->Assign(aChar);
    return *this;
  }
  self_type& operator=(const char_type* aData)
  {
    this->Assign(aData);
    return *this;
  }
  self_type& operator=(const self_type& aStr)
  {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(self_type&& aStr)
  {
    this->Assign(mozilla::Move(aStr));
    return *this;
  }
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  self_type& operator=(const char16ptr_t aStr)
  {
    this->Assign(static_cast<const char16_t*>(aStr));
    return *this;
  }
#endif
  self_type& operator=(const substring_type& aStr)
  {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(substring_type&& aStr)
  {
    this->Assign(mozilla::Move(aStr));
    return *this;
  }
  // NOTE(nika): gcc 4.9 workaround. Remove when support is dropped.
  self_type& operator=(const literalstring_type& aStr)
  {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(const substring_tuple_type& aTuple)
  {
    this->Assign(aTuple);
    return *this;
  }

  /**
   * returns the null-terminated string
   */

  template <typename U, typename Dummy> struct raw_type { typedef const U* type; };
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Dummy> struct raw_type<char16_t, Dummy> { typedef char16ptr_t type; };
#endif

  MOZ_NO_DANGLING_ON_TEMPORARIES typename raw_type<T, int>::type get() const
  {
    return this->mData;
  }


  /**
   * returns character at specified index.
   *
   * NOTE: unlike nsTSubstring::CharAt, this function allows you to index
   *       the null terminator character.
   */

  char_type CharAt(index_type aIndex) const
  {
    NS_ASSERTION(aIndex <= this->mLength, "index exceeds allowable range");
    return this->mData[aIndex];
  }

  char_type operator[](index_type aIndex) const
  {
    return CharAt(aIndex);
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

  int32_t Find(const nsTString<char>& aString, bool aIgnoreCase = false,
               int32_t aOffset = 0, int32_t aCount = -1) const;
  int32_t Find(const char* aString, bool aIgnoreCase = false,
               int32_t aOffset = 0, int32_t aCount = -1) const;

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  int32_t Find(const self_type& aString, int32_t aOffset = 0,
               int32_t aCount = -1) const;
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  int32_t Find(const char_type* aString, int32_t aOffset = 0,
               int32_t aCount = -1) const;
#ifdef MOZ_USE_CHAR16_WRAPPER
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  int32_t Find(char16ptr_t aString, int32_t aOffset = 0,
               int32_t aCount = -1) const
  {
    return Find(static_cast<const char16_t*>(aString), aOffset, aCount);
  }
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

  // Case aIgnoreCase option only with char versions
  int32_t RFind(const nsTString<char>& aString, bool aIgnoreCase = false,
                int32_t aOffset = -1, int32_t aCount = -1) const;
  int32_t RFind(const char* aCString, bool aIgnoreCase = false,
                int32_t aOffset = -1, int32_t aCount = -1) const;

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  int32_t RFind(const self_type& aString, int32_t aOffset = -1,
                int32_t aCount = -1) const;
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  int32_t RFind(const char_type* aString, int32_t aOffset = -1,
                int32_t aCount = -1) const;


  /**
   *  Search for given char within this string
   *
   *  @param   aChar is the character to search for
   *  @param   aOffset tells us where in this string to start searching
   *  @param   aCount tells us how far from the offset we are to search.
   *           Use -1 to search the whole string.
   *  @return  offset in string, or kNotFound
   */

  // int32_t FindChar( char16_t aChar, int32_t aOffset=0, int32_t aCount=-1 ) const;
  int32_t RFindChar(char16_t aChar, int32_t aOffset = -1,
                    int32_t aCount = -1) const;


  /**
   * This method searches this string for the first character found in
   * the given string.
   *
   * @param aString contains set of chars to be found
   * @param aOffset tells us where in this string to start searching
   *        (counting from left)
   * @return offset in string, or kNotFound
   */

  int32_t FindCharInSet(const char_type* aString, int32_t aOffset = 0) const;
  int32_t FindCharInSet(const self_type& aString, int32_t aOffset = 0) const
  {
    return FindCharInSet(aString.get(), aOffset);
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  int32_t FindCharInSet(const char* aSet, int32_t aOffset = 0) const;


  /**
   * This method searches this string for the last character found in
   * the given string.
   *
   * @param aString contains set of chars to be found
   * @param aOffset tells us where in this string to start searching
   *        (counting from left)
   * @return offset in string, or kNotFound
   */

  int32_t RFindCharInSet(const char_type* aString, int32_t aOffset = -1) const;
  int32_t RFindCharInSet(const self_type& aString, int32_t aOffset = -1) const
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
  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  int32_t Compare(const char_type* aString, bool aIgnoreCase = false,
                  int32_t aCount = -1) const;


  /**
   * Equality check between given string and this string.
   *
   * @param   aString is the string to check
   * @param   aIgnoreCase tells us how to treat case
   * @param   aCount tells us how many chars to compare
   * @return  boolean
   */
  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  bool EqualsIgnoreCase(const char_type* aString, int32_t aCount = -1) const
  {
    return Compare(aString, true, aCount) == 0;
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  bool EqualsIgnoreCase(const incompatible_char_type* aString, int32_t aCount = -1) const;

  /**
   * Perform string to double-precision float conversion.
   *
   * @param   aErrorCode will contain error if one occurs
   * @return  double-precision float rep of string value
   */
  double ToDouble(nsresult* aErrorCode) const;

  /**
   * Perform string to single-precision float conversion.
   *
   * @param   aErrorCode will contain error if one occurs
   * @return  single-precision float rep of string value
   */
  float ToFloat(nsresult* aErrorCode) const;

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

  size_type Mid(self_type& aResult, index_type aStartPos, size_type aCount) const;

  size_type Left(self_type& aResult, size_type aCount) const
  {
    return Mid(aResult, 0, aCount);
  }

  size_type Right(self_type& aResult, size_type aCount) const
  {
    aCount = XPCOM_MIN(this->mLength, aCount);
    return Mid(aResult, this->mLength - aCount, aCount);
  }


  /**
   * Set a char inside this string at given index
   *
   * @param aChar is the char you want to write into this string
   * @param anIndex is the ofs where you want to write the given char
   * @return TRUE if successful
   */

  bool SetCharAt(char16_t aChar, uint32_t aIndex);


  /**
   *  These methods are used to remove all occurrences of the
   *  characters found in aSet from this string.
   *
   *  @param  aSet -- characters to be cut from this
   */
  void StripChars(const char_type* aSet);

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  bool StripChars(const incompatible_char_type* aSet, const fallible_t&);

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void StripChars(const incompatible_char_type* aSet);

  /**
   *  This method strips whitespace throughout the string.
   */
  void StripWhitespace();
  bool StripWhitespace(const fallible_t&);


  /**
   *  swaps occurence of 1 string for another
   */

  void ReplaceChar(char_type aOldChar, char_type aNewChar);
  void ReplaceChar(const char_type* aSet, char_type aNewChar);

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void ReplaceChar(const char* aSet, char16_t aNewChar);

  /**
   * Replace all occurrences of aTarget with aNewValue.
   * The complexity of this function is O(n+m), n being the length of the string
   * and m being the length of aNewValue.
   */
  void ReplaceSubstring(const self_type& aTarget, const self_type& aNewValue);
  void ReplaceSubstring(const char_type* aTarget, const char_type* aNewValue);
  MOZ_MUST_USE bool ReplaceSubstring(const self_type& aTarget,
                                     const self_type& aNewValue,
                                     const fallible_t&);
  MOZ_MUST_USE bool ReplaceSubstring(const char_type* aTarget,
                                     const char_type* aNewValue,
                                     const fallible_t&);


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
  void Trim(const char* aSet, bool aEliminateLeading = true,
            bool aEliminateTrailing = true, bool aIgnoreQuotes = false);

  /**
   *  This method strips whitespace from string.
   *  You can control whether whitespace is yanked from start and end of
   *  string as well.
   *
   *  @param   aEliminateLeading controls stripping of leading ws
   *  @param   aEliminateTrailing controls stripping of trailing ws
   */
  void CompressWhitespace(bool aEliminateLeading = true,
                          bool aEliminateTrailing = true);

#endif // !MOZ_STRING_WITH_OBSOLETE_API

  /**
   * Allow this string to be bound to a character buffer
   * until the string is rebound or mutated; the caller
   * must ensure that the buffer outlives the string.
   */
  void Rebind(const char_type* aData, size_type aLength);

  /**
   * verify restrictions for dependent strings
   */
  void AssertValidDependentString()
  {
    NS_ASSERTION(this->mData, "nsTDependentString must wrap a non-NULL buffer");
    NS_ASSERTION(this->mLength != size_type(-1), "nsTDependentString has bogus length");
    NS_ASSERTION(this->mData[substring_type::mLength] == 0,
                 "nsTDependentString must wrap only null-terminated strings. "
                 "You are probably looking for nsTDependentSubstring.");
  }


protected:

  // allow subclasses to initialize fields directly
  nsTString(char_type* aData, size_type aLength, DataFlags aDataFlags,
            ClassFlags aClassFlags)
    : substring_type(aData, aLength, aDataFlags,
                     aClassFlags | ClassFlags::NULL_TERMINATED)
  {
  }

  friend const nsTString<char>& VoidCString();
  friend const nsTString<char16_t>& VoidString();

  // Used by Null[C]String.
  explicit nsTString(DataFlags aDataFlags)
    : substring_type(char_traits::sEmptyBuffer, 0,
                     aDataFlags | DataFlags::TERMINATED,
                     ClassFlags::NULL_TERMINATED)
  {}

  struct Segment {
    uint32_t mBegin, mLength;
    Segment(uint32_t aBegin, uint32_t aLength)
      : mBegin(aBegin)
      , mLength(aLength)
    {}
  };
};

// TODO(erahm): Do something with ToDouble so that we can extern the
// nsTString templates.
//extern template class nsTString<char>;
//extern template class nsTString<char16_t>;

/**
 * nsTAutoStringN
 *
 * Subclass of nsTString that adds support for stack-based string
 * allocation.  It is normally not a good idea to use this class on the
 * heap, because it will allocate space which may be wasted if the string
 * it contains is significantly smaller or any larger than 64 characters.
 *
 * NAMES:
 *   nsAutoStringN / nsTAutoString for wide characters
 *   nsAutoCStringN / nsTAutoCString for narrow characters
 */
template<typename T, size_t N>
class MOZ_NON_MEMMOVABLE nsTAutoStringN : public nsTString<T>
{
public:

  typedef nsTAutoStringN<T, N> self_type;

  typedef nsTString<T> base_string_type;
  typedef typename base_string_type::string_type string_type;
  typedef typename base_string_type::char_type char_type;
  typedef typename base_string_type::char_traits char_traits;
  typedef typename base_string_type::substring_type substring_type;
  typedef typename base_string_type::size_type size_type;
  typedef typename base_string_type::substring_tuple_type substring_tuple_type;
  typedef typename base_string_type::literalstring_type literalstring_type;

  // These are only for internal use within the string classes:
  typedef typename base_string_type::DataFlags DataFlags;
  typedef typename base_string_type::ClassFlags ClassFlags;

public:

  /**
   * constructors
   */

  nsTAutoStringN()
    : string_type(mStorage, 0, DataFlags::TERMINATED | DataFlags::INLINE,
                  ClassFlags::INLINE)
    , mInlineCapacity(N - 1)
  {
    // null-terminate
    mStorage[0] = char_type(0);
  }

  explicit
  nsTAutoStringN(char_type aChar)
    : self_type()
  {
    this->Assign(aChar);
  }

  explicit
  nsTAutoStringN(const char_type* aData, size_type aLength = size_type(-1))
    : self_type()
  {
    this->Assign(aData, aLength);
  }

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  explicit
  nsTAutoStringN(char16ptr_t aData, size_type aLength = size_type(-1))
    : self_type(static_cast<const char16_t*>(aData), aLength)
  {
  }
#endif

  nsTAutoStringN(const self_type& aStr)
    : self_type()
  {
    this->Assign(aStr);
  }

  nsTAutoStringN(self_type&& aStr)
    : self_type()
  {
    this->Assign(mozilla::Move(aStr));
  }

  explicit
  nsTAutoStringN(const substring_type& aStr)
    : self_type()
  {
    this->Assign(aStr);
  }

  explicit
  nsTAutoStringN(substring_type&& aStr)
    : self_type()
  {
    this->Assign(mozilla::Move(aStr));
  }

  // NOTE(nika): gcc 4.9 workaround. Remove when support is dropped.
  explicit
  nsTAutoStringN(const literalstring_type& aStr)
    : self_type()
  {
    this->Assign(aStr);
  }

  MOZ_IMPLICIT nsTAutoStringN(const substring_tuple_type& aTuple)
    : self_type()
  {
    this->Assign(aTuple);
  }

  // |operator=| does not inherit, so we must define our own
  self_type& operator=(char_type aChar)
  {
    this->Assign(aChar);
    return *this;
  }
  self_type& operator=(const char_type* aData)
  {
    this->Assign(aData);
    return *this;
  }
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  self_type& operator=(char16ptr_t aStr)
  {
    this->Assign(aStr);
    return *this;
  }
#endif
  self_type& operator=(const self_type& aStr)
  {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(self_type&& aStr)
  {
    this->Assign(mozilla::Move(aStr));
    return *this;
  }
  self_type& operator=(const substring_type& aStr)
  {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(substring_type&& aStr)
  {
    this->Assign(mozilla::Move(aStr));
    return *this;
  }
  // NOTE(nika): gcc 4.9 workaround. Remove when support is dropped.
  self_type& operator=(const literalstring_type& aStr)
  {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(const substring_tuple_type& aTuple)
  {
    this->Assign(aTuple);
    return *this;
  }

  static const size_t kStorageSize = N;

protected:
  friend class nsTSubstring<T>;

  size_type mInlineCapacity;

private:
  char_type mStorage[N];
};

// Externs for the most common nsTAutoStringN variations.
extern template class nsTAutoStringN<char, 64>;
extern template class nsTAutoStringN<char16_t, 64>;

//
// nsAutoString stores pointers into itself which are invalidated when an
// nsTArray is resized, so nsTArray must not be instantiated with nsAutoString
// elements!
//
template<class E> class nsTArrayElementTraits;
template<typename T>
class nsTArrayElementTraits<nsTAutoString<T>>
{
public:
  template<class A> struct Dont_Instantiate_nsTArray_of;
  template<class A> struct Instead_Use_nsTArray_of;

  static Dont_Instantiate_nsTArray_of<nsTAutoString<T>>*
  Construct(Instead_Use_nsTArray_of<nsTString<T>>* aE)
  {
    return 0;
  }
  template<class A>
  static Dont_Instantiate_nsTArray_of<nsTAutoString<T>>*
  Construct(Instead_Use_nsTArray_of<nsTString<T>>* aE, const A& aArg)
  {
    return 0;
  }
  static Dont_Instantiate_nsTArray_of<nsTAutoString<T>>*
  Destruct(Instead_Use_nsTArray_of<nsTString<T>>* aE)
  {
    return 0;
  }
};

/**
 * getter_Copies support for adopting raw string out params that are
 * heap-allocated, e.g.:
 *
 *    char* gStr;
 *    void GetBlah(char** aStr)
 *    {
 *      *aStr = strdup(gStr);
 *    }
 *
 *    // This works, but is clumsy.
 *    void Inelegant()
 *    {
 *      char* buf;
 *      GetBlah(&buf);
 *      nsCString str;
 *      str.Adopt(buf);
 *      // ...
 *    }
 *
 *    // This is nicer.
 *    void Elegant()
 *    {
 *      nsCString str;
 *      GetBlah(getter_Copies(str));
 *      // ...
 *    }
 */
template <typename T>
class MOZ_STACK_CLASS nsTGetterCopies
{
public:
  typedef T char_type;

  explicit nsTGetterCopies(nsTSubstring<T>& aStr)
    : mString(aStr)
    , mData(nullptr)
  {
  }

  ~nsTGetterCopies()
  {
    mString.Adopt(mData); // OK if mData is null
  }

  operator char_type**()
  {
    return &mData;
  }

private:
  nsTSubstring<T>& mString;
  char_type* mData;
};

// See the comment above nsTGetterCopies_CharT for how to use this.
template <typename T>
inline nsTGetterCopies<T>
getter_Copies(nsTSubstring<T>& aString)
{
  return nsTGetterCopies<T>(aString);
}

#endif
