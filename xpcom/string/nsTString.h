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
class nsTString : public nsTSubstring<T> {
 public:
  typedef nsTString<T> self_type;

  using repr_type = mozilla::detail::nsTStringRepr<T>;

#ifdef __clang__
  // bindgen w/ clang 3.9 at least chokes on a typedef, but using is okay.
  using typename nsTSubstring<T>::substring_type;
#else
  // On the other hand msvc chokes on the using statement. It seems others
  // don't care either way so we lump them in here.
  typedef typename nsTSubstring<T>::substring_type substring_type;
#endif

  typedef typename substring_type::fallible_t fallible_t;

  typedef typename substring_type::char_type char_type;
  typedef typename substring_type::char_traits char_traits;
  typedef
      typename substring_type::incompatible_char_type incompatible_char_type;

  typedef typename substring_type::substring_tuple_type substring_tuple_type;

  typedef typename substring_type::const_iterator const_iterator;
  typedef typename substring_type::iterator iterator;

  typedef typename substring_type::comparator_type comparator_type;

  typedef typename substring_type::const_char_iterator const_char_iterator;

  typedef typename substring_type::string_view string_view;

  typedef typename substring_type::index_type index_type;
  typedef typename substring_type::size_type size_type;

  // These are only for internal use within the string classes:
  typedef typename substring_type::DataFlags DataFlags;
  typedef typename substring_type::ClassFlags ClassFlags;

 public:
  /**
   * constructors
   */

  nsTString() : substring_type(ClassFlags::NULL_TERMINATED) {}

  explicit nsTString(const char_type* aData, size_type aLength = size_type(-1))
      : substring_type(ClassFlags::NULL_TERMINATED) {
    this->Assign(aData, aLength);
  }

  explicit nsTString(mozilla::Span<const char_type> aData)
      : nsTString(aData.Elements(), aData.Length()) {}

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  explicit nsTString(char16ptr_t aStr, size_type aLength = size_type(-1))
      : substring_type(ClassFlags::NULL_TERMINATED) {
    this->Assign(static_cast<const char16_t*>(aStr), aLength);
  }
#endif

  nsTString(const self_type& aStr)
      : substring_type(ClassFlags::NULL_TERMINATED) {
    this->Assign(aStr);
  }

  nsTString(self_type&& aStr) : substring_type(ClassFlags::NULL_TERMINATED) {
    this->Assign(std::move(aStr));
  }

  MOZ_IMPLICIT nsTString(const substring_tuple_type& aTuple)
      : substring_type(ClassFlags::NULL_TERMINATED) {
    this->Assign(aTuple);
  }

  explicit nsTString(const substring_type& aReadable)
      : substring_type(ClassFlags::NULL_TERMINATED) {
    this->Assign(aReadable);
  }

  explicit nsTString(substring_type&& aReadable)
      : substring_type(ClassFlags::NULL_TERMINATED) {
    this->Assign(std::move(aReadable));
  }

  // |operator=| does not inherit, so we must define our own
  self_type& operator=(char_type aChar) {
    this->Assign(aChar);
    return *this;
  }
  self_type& operator=(const char_type* aData) {
    this->Assign(aData);
    return *this;
  }
  self_type& operator=(const self_type& aStr) {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(self_type&& aStr) {
    this->Assign(std::move(aStr));
    return *this;
  }
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  self_type& operator=(const char16ptr_t aStr) {
    this->Assign(static_cast<const char16_t*>(aStr));
    return *this;
  }
#endif
  self_type& operator=(const substring_type& aStr) {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(substring_type&& aStr) {
    this->Assign(std::move(aStr));
    return *this;
  }
  self_type& operator=(const substring_tuple_type& aTuple) {
    this->Assign(aTuple);
    return *this;
  }

  /**
   * returns the null-terminated string
   */

  template <typename U, typename Dummy>
  struct raw_type {
    typedef const U* type;
  };
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Dummy>
  struct raw_type<char16_t, Dummy> {
    typedef char16ptr_t type;
  };
#endif

  MOZ_NO_DANGLING_ON_TEMPORARIES typename raw_type<T, int>::type get() const {
    return this->mData;
  }

  /**
   * returns character at specified index.
   *
   * NOTE: unlike nsTSubstring::CharAt, this function allows you to index
   *       the null terminator character.
   */

  char_type CharAt(index_type aIndex) const {
    MOZ_ASSERT(aIndex <= this->Length(), "index exceeds allowable range");
    return this->mData[aIndex];
  }

  char_type operator[](index_type aIndex) const { return CharAt(aIndex); }

  /**
   * Perform string to double-precision float conversion.
   *
   * @param   aErrorCode will contain error if one occurs
   * @return  double-precision float rep of string value
   */
  double ToDouble(nsresult* aErrorCode) const {
    return ToDouble(/* aAllowTrailingChars = */ false, aErrorCode);
  }

  /**
   * Perform string to single-precision float conversion.
   *
   * @param   aErrorCode will contain error if one occurs
   * @return  single-precision float rep of string value
   */
  float ToFloat(nsresult* aErrorCode) const {
    return float(ToDouble(aErrorCode));
  }

  /**
   * Similar to above ToDouble and ToFloat but allows trailing characters that
   * are not converted.
   */
  double ToDoubleAllowTrailingChars(nsresult* aErrorCode) const {
    return ToDouble(/* aAllowTrailingChars = */ true, aErrorCode);
  }
  float ToFloatAllowTrailingChars(nsresult* aErrorCode) const {
    return float(ToDoubleAllowTrailingChars(aErrorCode));
  }

  /**
   * Set a char inside this string at given index
   *
   * @param aChar is the char you want to write into this string
   * @param anIndex is the ofs where you want to write the given char
   * @return TRUE if successful
   */
  bool SetCharAt(char16_t aChar, index_type aIndex);

  /**
   * Allow this string to be bound to a character buffer
   * until the string is rebound or mutated; the caller
   * must ensure that the buffer outlives the string.
   */
  void Rebind(const char_type* aData, size_type aLength);

  /**
   * verify restrictions for dependent strings
   */
  void AssertValidDependentString() {
    MOZ_ASSERT(this->mData, "nsTDependentString must wrap a non-NULL buffer");
    MOZ_DIAGNOSTIC_ASSERT(this->mData[substring_type::mLength] == 0,
                          "nsTDependentString must wrap only null-terminated "
                          "strings.  You are probably looking for "
                          "nsTDependentSubstring.");
  }

 protected:
  // allow subclasses to initialize fields directly
  nsTString(char_type* aData, size_type aLength, DataFlags aDataFlags,
            ClassFlags aClassFlags)
      : substring_type(aData, aLength, aDataFlags,
                       aClassFlags | ClassFlags::NULL_TERMINATED) {}

  friend const nsTString<char>& VoidCString();
  friend const nsTString<char16_t>& VoidString();

  double ToDouble(bool aAllowTrailingChars, nsresult* aErrorCode) const;

  // Used by Null[C]String.
  explicit nsTString(DataFlags aDataFlags)
      : substring_type(char_traits::sEmptyBuffer, 0,
                       aDataFlags | DataFlags::TERMINATED,
                       ClassFlags::NULL_TERMINATED) {}
};

template <>
double nsTString<char>::ToDouble(bool aAllowTrailingChars,
                                 nsresult* aErrorCode) const;
template <>
double nsTString<char16_t>::ToDouble(bool aAllowTrailingChars,
                                     nsresult* aErrorCode) const;

extern template class nsTString<char>;
extern template class nsTString<char16_t>;

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
template <typename T, size_t N>
class MOZ_NON_MEMMOVABLE nsTAutoStringN : public nsTString<T> {
 public:
  typedef nsTAutoStringN<T, N> self_type;

  typedef nsTString<T> base_string_type;
  typedef typename base_string_type::string_type string_type;
  typedef typename base_string_type::char_type char_type;
  typedef typename base_string_type::char_traits char_traits;
  typedef typename base_string_type::substring_type substring_type;
  typedef typename base_string_type::size_type size_type;
  typedef typename base_string_type::substring_tuple_type substring_tuple_type;

  // These are only for internal use within the string classes:
  typedef typename base_string_type::DataFlags DataFlags;
  typedef typename base_string_type::ClassFlags ClassFlags;
  typedef typename base_string_type::LengthStorage LengthStorage;

 public:
  /**
   * constructors
   */

  nsTAutoStringN()
      : string_type(mStorage, 0, DataFlags::TERMINATED | DataFlags::INLINE,
                    ClassFlags::INLINE),
        mInlineCapacity(N - 1) {
    // null-terminate
    mStorage[0] = char_type(0);
  }

  explicit nsTAutoStringN(char_type aChar) : self_type() {
    this->Assign(aChar);
  }

  explicit nsTAutoStringN(const char_type* aData,
                          size_type aLength = size_type(-1))
      : self_type() {
    this->Assign(aData, aLength);
  }

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  explicit nsTAutoStringN(char16ptr_t aData, size_type aLength = size_type(-1))
      : self_type(static_cast<const char16_t*>(aData), aLength) {}
#endif

  nsTAutoStringN(const self_type& aStr) : self_type() { this->Assign(aStr); }

  nsTAutoStringN(self_type&& aStr) : self_type() {
    this->Assign(std::move(aStr));
  }

  explicit nsTAutoStringN(const substring_type& aStr) : self_type() {
    this->Assign(aStr);
  }

  explicit nsTAutoStringN(substring_type&& aStr) : self_type() {
    this->Assign(std::move(aStr));
  }

  MOZ_IMPLICIT nsTAutoStringN(const substring_tuple_type& aTuple)
      : self_type() {
    this->Assign(aTuple);
  }

  // |operator=| does not inherit, so we must define our own
  self_type& operator=(char_type aChar) {
    this->Assign(aChar);
    return *this;
  }
  self_type& operator=(const char_type* aData) {
    this->Assign(aData);
    return *this;
  }
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  self_type& operator=(char16ptr_t aStr) {
    this->Assign(aStr);
    return *this;
  }
#endif
  self_type& operator=(const self_type& aStr) {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(self_type&& aStr) {
    this->Assign(std::move(aStr));
    return *this;
  }
  self_type& operator=(const substring_type& aStr) {
    this->Assign(aStr);
    return *this;
  }
  self_type& operator=(substring_type&& aStr) {
    this->Assign(std::move(aStr));
    return *this;
  }
  self_type& operator=(const substring_tuple_type& aTuple) {
    this->Assign(aTuple);
    return *this;
  }

  static const size_t kStorageSize = N;

 protected:
  friend class nsTSubstring<T>;

  const LengthStorage mInlineCapacity;

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
template <class E>
class nsTArrayElementTraits;
template <typename T>
class nsTArrayElementTraits<nsTAutoString<T>> {
 public:
  template <class A>
  struct Dont_Instantiate_nsTArray_of;
  template <class A>
  struct Instead_Use_nsTArray_of;

  static Dont_Instantiate_nsTArray_of<nsTAutoString<T>>* Construct(
      Instead_Use_nsTArray_of<nsTString<T>>* aE) {
    return 0;
  }
  template <class A>
  static Dont_Instantiate_nsTArray_of<nsTAutoString<T>>* Construct(
      Instead_Use_nsTArray_of<nsTString<T>>* aE, const A& aArg) {
    return 0;
  }
  template <class... Args>
  static Dont_Instantiate_nsTArray_of<nsTAutoString<T>>* Construct(
      Instead_Use_nsTArray_of<nsTString<T>>* aE, Args&&... aArgs) {
    return 0;
  }
  static Dont_Instantiate_nsTArray_of<nsTAutoString<T>>* Destruct(
      Instead_Use_nsTArray_of<nsTString<T>>* aE) {
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
class MOZ_STACK_CLASS nsTGetterCopies {
 public:
  typedef T char_type;

  explicit nsTGetterCopies(nsTSubstring<T>& aStr)
      : mString(aStr), mData(nullptr) {}

  ~nsTGetterCopies() {
    mString.Adopt(mData);  // OK if mData is null
  }

  operator char_type**() { return &mData; }

 private:
  nsTSubstring<T>& mString;
  char_type* mData;
};

// See the comment above nsTGetterCopies_CharT for how to use this.
template <typename T>
inline nsTGetterCopies<T> getter_Copies(nsTSubstring<T>& aString) {
  return nsTGetterCopies<T>(aString);
}

#endif
