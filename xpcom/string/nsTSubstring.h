/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#include "mozilla/Casting.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/Span.h"

#ifndef MOZILLA_INTERNAL_API
#error "Using XPCOM strings is limited to code linked into libxul."
#endif

/**
 * The base for string comparators
 */
class nsTStringComparator_CharT
{
public:
  typedef CharT char_type;

  nsTStringComparator_CharT()
  {
  }

  virtual int operator()(const char_type*, const char_type*,
                         uint32_t, uint32_t) const = 0;
};


/**
 * The default string comparator (case-sensitive comparision)
 */
class nsTDefaultStringComparator_CharT
  : public nsTStringComparator_CharT
{
public:
  typedef CharT char_type;

  nsTDefaultStringComparator_CharT()
  {
  }

  virtual int operator()(const char_type*, const char_type*,
                         uint32_t, uint32_t) const override;
};

class nsTSubstringSplitter_CharT;

namespace mozilla {
namespace detail {

/**
 * nsTStringRepr defines a string's memory layout and some accessor methods.
 * This class exists so that nsTLiteralString can avoid inheriting
 * nsTSubstring's destructor. All methods on this class must be const because
 * literal strings are not writable.
 *
 * This class is an implementation detail and should not be instantiated
 * directly, nor used in any way outside of the string code itself. It is
 * buried in a namespace to discourage its use in function parameters.
 * If you need to take a parameter, use [const] ns[C]Substring&.
 * If you need to instantiate a string, use ns[C]String or descendents.
 *
 * NAMES:
 *   nsStringRepr for wide characters
 *   nsCStringRepr for narrow characters
 *
 */
class nsTStringRepr_CharT
{
public:
  typedef mozilla::fallible_t                 fallible_t;

  typedef CharT                               char_type;

  typedef nsCharTraits<char_type>             char_traits;
  typedef char_traits::incompatible_char_type incompatible_char_type;

  typedef nsTStringRepr_CharT                 self_type;
  typedef self_type                           base_string_type;

  typedef nsTSubstring_CharT                  substring_type;
  typedef nsTSubstringTuple_CharT             substring_tuple_type;
  typedef nsTString_CharT                     string_type;

  typedef nsReadingIterator<char_type>        const_iterator;
  typedef nsWritingIterator<char_type>        iterator;

  typedef nsTStringComparator_CharT           comparator_type;

  typedef char_type*                          char_iterator;
  typedef const char_type*                    const_char_iterator;

  typedef uint32_t                            index_type;
  typedef uint32_t                            size_type;

  // These are only for internal use within the string classes:
  typedef StringDataFlags                     DataFlags;
  typedef StringClassFlags                    ClassFlags;

  /**
   * reading iterators
   */

  const_char_iterator BeginReading() const
  {
    return mData;
  }
  const_char_iterator EndReading() const
  {
    return mData + mLength;
  }

  /**
   * deprecated reading iterators
   */

  const_iterator& BeginReading(const_iterator& aIter) const
  {
    aIter.mStart = mData;
    aIter.mEnd = mData + mLength;
    aIter.mPosition = aIter.mStart;
    return aIter;
  }

  const_iterator& EndReading(const_iterator& aIter) const
  {
    aIter.mStart = mData;
    aIter.mEnd = mData + mLength;
    aIter.mPosition = aIter.mEnd;
    return aIter;
  }

  const_char_iterator& BeginReading(const_char_iterator& aIter) const
  {
    return aIter = mData;
  }

  const_char_iterator& EndReading(const_char_iterator& aIter) const
  {
    return aIter = mData + mLength;
  }

  /**
   * accessors
   */

  // returns pointer to string data (not necessarily null-terminated)
#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  char16ptr_t Data() const
#else
  const char_type* Data() const
#endif
  {
    return mData;
  }

  size_type Length() const
  {
    return mLength;
  }

  DataFlags GetDataFlags() const
  {
    return mDataFlags;
  }

  bool IsEmpty() const
  {
    return mLength == 0;
  }

  bool IsLiteral() const
  {
    return !!(mDataFlags & DataFlags::LITERAL);
  }

  bool IsVoid() const
  {
    return !!(mDataFlags & DataFlags::VOIDED);
  }

  bool IsTerminated() const
  {
    return !!(mDataFlags & DataFlags::TERMINATED);
  }

  char_type CharAt(index_type aIndex) const
  {
    NS_ASSERTION(aIndex < mLength, "index exceeds allowable range");
    return mData[aIndex];
  }

  char_type operator[](index_type aIndex) const
  {
    return CharAt(aIndex);
  }

  char_type First() const;

  char_type Last() const;

  size_type NS_FASTCALL CountChar(char_type) const;
  int32_t NS_FASTCALL FindChar(char_type, index_type aOffset = 0) const;

  inline bool Contains(char_type aChar) const
  {
    return FindChar(aChar) != kNotFound;
  }

  /**
   * equality
   */

  bool NS_FASTCALL Equals(const self_type&) const;
  bool NS_FASTCALL Equals(const self_type&, const comparator_type&) const;

  bool NS_FASTCALL Equals(const substring_tuple_type& aTuple) const;
  bool NS_FASTCALL Equals(const substring_tuple_type& aTuple,
                          const comparator_type& aComp) const;

  bool NS_FASTCALL Equals(const char_type* aData) const;
  bool NS_FASTCALL Equals(const char_type* aData,
                          const comparator_type& aComp) const;

#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  bool NS_FASTCALL Equals(char16ptr_t aData) const
  {
    return Equals(static_cast<const char16_t*>(aData));
  }
  bool NS_FASTCALL Equals(char16ptr_t aData, const comparator_type& aComp) const
  {
    return Equals(static_cast<const char16_t*>(aData), aComp);
  }
#endif

  /**
   * An efficient comparison with ASCII that can be used even
   * for wide strings. Call this version when you know the
   * length of 'data'.
   */
  bool NS_FASTCALL EqualsASCII(const char* aData, size_type aLen) const;
  /**
   * An efficient comparison with ASCII that can be used even
   * for wide strings. Call this version when 'data' is
   * null-terminated.
   */
  bool NS_FASTCALL EqualsASCII(const char* aData) const;

  // EqualsLiteral must ONLY be applied to an actual literal string, or
  // a char array *constant* declared without an explicit size.
  // Do not attempt to use it with a regular char* pointer, or with a
  // non-constant char array variable. Use EqualsASCII for them.
  // The template trick to acquire the array length at compile time without
  // using a macro is due to Corey Kosak, with much thanks.
  template<int N>
  inline bool EqualsLiteral(const char (&aStr)[N]) const
  {
    return EqualsASCII(aStr, N - 1);
  }

  // The LowerCaseEquals methods compare the ASCII-lowercase version of
  // this string (lowercasing only ASCII uppercase characters) to some
  // ASCII/Literal string. The ASCII string is *not* lowercased for
  // you. If you compare to an ASCII or literal string that contains an
  // uppercase character, it is guaranteed to return false. We will
  // throw assertions too.
  bool NS_FASTCALL LowerCaseEqualsASCII(const char* aData,
                                        size_type aLen) const;
  bool NS_FASTCALL LowerCaseEqualsASCII(const char* aData) const;

  // LowerCaseEqualsLiteral must ONLY be applied to an actual
  // literal string, or a char array *constant* declared without an
  // explicit size.  Do not attempt to use it with a regular char*
  // pointer, or with a non-constant char array variable. Use
  // LowerCaseEqualsASCII for them.
  template<int N>
  inline bool LowerCaseEqualsLiteral(const char (&aStr)[N]) const
  {
    return LowerCaseEqualsASCII(aStr, N - 1);
  }

  /**
   * returns true if this string overlaps with the given string fragment.
   */
  bool IsDependentOn(const char_type* aStart, const char_type* aEnd) const
  {
    /**
     * if it _isn't_ the case that one fragment starts after the other ends,
     * or ends before the other starts, then, they conflict:
     *
     *   !(f2.begin >= f1.aEnd || f2.aEnd <= f1.begin)
     *
     * Simplified, that gives us:
     */
    return (aStart < (mData + mLength) && aEnd > mData);
  }

protected:
  nsTStringRepr_CharT() = delete; // Never instantiate directly

  constexpr
  nsTStringRepr_CharT(char_type* aData, size_type aLength,
                      DataFlags aDataFlags, ClassFlags aClassFlags)
    : mData(aData)
    , mLength(aLength)
    , mDataFlags(aDataFlags)
    , mClassFlags(aClassFlags)
  {
  }

  char_type* mData;
  size_type mLength;
  DataFlags mDataFlags;
  ClassFlags const mClassFlags;
};

} // namespace detail
} // namespace mozilla

/**
 * nsTSubstring is an abstract string class. From an API perspective, this
 * class is the root of the string class hierarchy. It represents a single
 * contiguous array of characters, which may or may not be null-terminated.
 * This type is not instantiated directly. A sub-class is instantiated
 * instead. For example, see nsTString.
 *
 * NAMES:
 *   nsAString for wide characters
 *   nsACString for narrow characters
 *
 */
class nsTSubstring_CharT : public mozilla::detail::nsTStringRepr_CharT
{
public:
  typedef nsTSubstring_CharT                  self_type;

  // this acts like a virtual destructor
  ~nsTSubstring_CharT()
  {
    Finalize();
  }

  /**
   * writing iterators
   */

  char_iterator BeginWriting()
  {
    if (!EnsureMutable()) {
      AllocFailed(mLength);
    }

    return mData;
  }

  char_iterator BeginWriting(const fallible_t&)
  {
    return EnsureMutable() ? mData : char_iterator(0);
  }

  char_iterator EndWriting()
  {
    if (!EnsureMutable()) {
      AllocFailed(mLength);
    }

    return mData + mLength;
  }

  char_iterator EndWriting(const fallible_t&)
  {
    return EnsureMutable() ? (mData + mLength) : char_iterator(0);
  }

  char_iterator& BeginWriting(char_iterator& aIter)
  {
    return aIter = BeginWriting();
  }

  char_iterator& BeginWriting(char_iterator& aIter, const fallible_t& aFallible)
  {
    return aIter = BeginWriting(aFallible);
  }

  char_iterator& EndWriting(char_iterator& aIter)
  {
    return aIter = EndWriting();
  }

  char_iterator& EndWriting(char_iterator& aIter, const fallible_t& aFallible)
  {
    return aIter = EndWriting(aFallible);
  }

  /**
   * deprecated writing iterators
   */

  iterator& BeginWriting(iterator& aIter)
  {
    char_type* data = BeginWriting();
    aIter.mStart = data;
    aIter.mEnd = data + mLength;
    aIter.mPosition = aIter.mStart;
    return aIter;
  }

  iterator& EndWriting(iterator& aIter)
  {
    char_type* data = BeginWriting();
    aIter.mStart = data;
    aIter.mEnd = data + mLength;
    aIter.mPosition = aIter.mEnd;
    return aIter;
  }

  /**
   * assignment
   */

  void NS_FASTCALL Assign(char_type aChar);
  MOZ_MUST_USE bool NS_FASTCALL Assign(char_type aChar, const fallible_t&);

  void NS_FASTCALL Assign(const char_type* aData);
  MOZ_MUST_USE bool NS_FASTCALL Assign(const char_type* aData,
                                       const fallible_t&);

  void NS_FASTCALL Assign(const char_type* aData, size_type aLength);
  MOZ_MUST_USE bool NS_FASTCALL Assign(const char_type* aData,
                                       size_type aLength, const fallible_t&);

  void NS_FASTCALL Assign(const self_type&);
  MOZ_MUST_USE bool NS_FASTCALL Assign(const self_type&, const fallible_t&);

  void NS_FASTCALL Assign(const substring_tuple_type&);
  MOZ_MUST_USE bool NS_FASTCALL Assign(const substring_tuple_type&,
                                       const fallible_t&);

#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  void Assign(char16ptr_t aData)
  {
    Assign(static_cast<const char16_t*>(aData));
  }

  void Assign(char16ptr_t aData, size_type aLength)
  {
    Assign(static_cast<const char16_t*>(aData), aLength);
  }

  MOZ_MUST_USE bool Assign(char16ptr_t aData, size_type aLength,
                           const fallible_t& aFallible)
  {
    return Assign(static_cast<const char16_t*>(aData), aLength,
                  aFallible);
  }
#endif

  void NS_FASTCALL AssignASCII(const char* aData, size_type aLength);
  MOZ_MUST_USE bool NS_FASTCALL AssignASCII(const char* aData,
                                            size_type aLength,
                                            const fallible_t&);

  void NS_FASTCALL AssignASCII(const char* aData)
  {
    AssignASCII(aData, mozilla::AssertedCast<size_type, size_t>(strlen(aData)));
  }
  MOZ_MUST_USE bool NS_FASTCALL AssignASCII(const char* aData,
                                            const fallible_t& aFallible)
  {
    return AssignASCII(aData,
                       mozilla::AssertedCast<size_type, size_t>(strlen(aData)),
                       aFallible);
  }

  // AssignLiteral must ONLY be applied to an actual literal string, or
  // a char array *constant* declared without an explicit size.
  // Do not attempt to use it with a regular char* pointer, or with a
  // non-constant char array variable. Use AssignASCII for those.
  // There are not fallible version of these methods because they only really
  // apply to small allocations that we wouldn't want to check anyway.
  template<int N>
  void AssignLiteral(const char_type (&aStr)[N])
  {
    AssignLiteral(aStr, N - 1);
  }
#ifdef CharT_is_PRUnichar
  template<int N>
  void AssignLiteral(const char (&aStr)[N])
  {
    AssignASCII(aStr, N - 1);
  }
#endif

  self_type& operator=(char_type aChar)
  {
    Assign(aChar);
    return *this;
  }
  self_type& operator=(const char_type* aData)
  {
    Assign(aData);
    return *this;
  }
#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  self_type& operator=(char16ptr_t aData)
  {
    Assign(aData);
    return *this;
  }
#endif
  self_type& operator=(const self_type& aStr)
  {
    Assign(aStr);
    return *this;
  }
  self_type& operator=(const substring_tuple_type& aTuple)
  {
    Assign(aTuple);
    return *this;
  }

  void NS_FASTCALL Adopt(char_type* aData, size_type aLength = size_type(-1));


  /**
   * buffer manipulation
   */

  void NS_FASTCALL Replace(index_type aCutStart, size_type aCutLength,
                           char_type aChar);
  MOZ_MUST_USE bool NS_FASTCALL Replace(index_type aCutStart,
                                        size_type aCutLength,
                                        char_type aChar,
                                        const fallible_t&);
  void NS_FASTCALL Replace(index_type aCutStart, size_type aCutLength,
                           const char_type* aData,
                           size_type aLength = size_type(-1));
  MOZ_MUST_USE bool NS_FASTCALL Replace(index_type aCutStart,
                                        size_type aCutLength,
                                        const char_type* aData,
                                        size_type aLength,
                                        const fallible_t&);
  void Replace(index_type aCutStart, size_type aCutLength,
               const self_type& aStr)
  {
    Replace(aCutStart, aCutLength, aStr.Data(), aStr.Length());
  }
  MOZ_MUST_USE bool Replace(index_type aCutStart,
                            size_type aCutLength,
                            const self_type& aStr,
                            const fallible_t& aFallible)
  {
    return Replace(aCutStart, aCutLength, aStr.Data(), aStr.Length(),
                   aFallible);
  }
  void NS_FASTCALL Replace(index_type aCutStart, size_type aCutLength,
                           const substring_tuple_type& aTuple);

  void NS_FASTCALL ReplaceASCII(index_type aCutStart, size_type aCutLength,
                                const char* aData,
                                size_type aLength = size_type(-1));

  MOZ_MUST_USE bool NS_FASTCALL ReplaceASCII(index_type aCutStart, size_type aCutLength,
                                             const char* aData,
                                             size_type aLength,
                                             const fallible_t&);

  // ReplaceLiteral must ONLY be applied to an actual literal string.
  // Do not attempt to use it with a regular char* pointer, or with a char
  // array variable. Use Replace or ReplaceASCII for those.
  template<int N>
  void ReplaceLiteral(index_type aCutStart, size_type aCutLength,
                      const char_type (&aStr)[N])
  {
    ReplaceLiteral(aCutStart, aCutLength, aStr, N - 1);
  }

  void Append(char_type aChar)
  {
    Replace(mLength, 0, aChar);
  }
  MOZ_MUST_USE bool Append(char_type aChar, const fallible_t& aFallible)
  {
    return Replace(mLength, 0, aChar, aFallible);
  }
  void Append(const char_type* aData, size_type aLength = size_type(-1))
  {
    Replace(mLength, 0, aData, aLength);
  }
  MOZ_MUST_USE bool Append(const char_type* aData, size_type aLength,
                           const fallible_t& aFallible)
  {
    return Replace(mLength, 0, aData, aLength, aFallible);
  }

#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  void Append(char16ptr_t aData, size_type aLength = size_type(-1))
  {
    Append(static_cast<const char16_t*>(aData), aLength);
  }
#endif

  void Append(const self_type& aStr)
  {
    Replace(mLength, 0, aStr);
  }
  MOZ_MUST_USE bool Append(const self_type& aStr, const fallible_t& aFallible)
  {
    return Replace(mLength, 0, aStr, aFallible);
  }
  void Append(const substring_tuple_type& aTuple)
  {
    Replace(mLength, 0, aTuple);
  }

  void AppendASCII(const char* aData, size_type aLength = size_type(-1))
  {
    ReplaceASCII(mLength, 0, aData, aLength);
  }

  MOZ_MUST_USE bool AppendASCII(const char* aData, const fallible_t& aFallible)
  {
    return ReplaceASCII(mLength, 0, aData, size_type(-1), aFallible);
  }

  MOZ_MUST_USE bool AppendASCII(const char* aData, size_type aLength, const fallible_t& aFallible)
  {
    return ReplaceASCII(mLength, 0, aData, aLength, aFallible);
  }

  /**
   * Append a formatted string to the current string. Uses the
   * standard printf format codes.  This uses NSPR formatting, which will be
   * locale-aware for floating-point values.  You probably don't want to use
   * this with floating-point values as a result.
   */
  void AppendPrintf(const char* aFormat, ...) MOZ_FORMAT_PRINTF(2, 3);
  void AppendPrintf(const char* aFormat, va_list aAp) MOZ_FORMAT_PRINTF(2, 0);
  void AppendInt(int32_t aInteger)
  {
    AppendPrintf("%" PRId32, aInteger);
  }
  void AppendInt(int32_t aInteger, int aRadix)
  {
    if (aRadix == 10) {
      AppendPrintf("%" PRId32, aInteger);
    } else {
      AppendPrintf(aRadix == 8 ? "%" PRIo32 : "%" PRIx32,
                   static_cast<uint32_t>(aInteger));
    }
  }
  void AppendInt(uint32_t aInteger)
  {
    AppendPrintf("%" PRIu32, aInteger);
  }
  void AppendInt(uint32_t aInteger, int aRadix)
  {
    AppendPrintf(aRadix == 10 ? "%" PRIu32 : aRadix == 8 ? "%" PRIo32 : "%" PRIx32,
                 aInteger);
  }
  void AppendInt(int64_t aInteger)
  {
    AppendPrintf("%" PRId64, aInteger);
  }
  void AppendInt(int64_t aInteger, int aRadix)
  {
    if (aRadix == 10) {
      AppendPrintf("%" PRId64, aInteger);
    } else {
      AppendPrintf(aRadix == 8 ? "%" PRIo64 : "%" PRIx64,
                   static_cast<uint64_t>(aInteger));
    }
  }
  void AppendInt(uint64_t aInteger)
  {
    AppendPrintf("%" PRIu64, aInteger);
  }
  void AppendInt(uint64_t aInteger, int aRadix)
  {
    AppendPrintf(aRadix == 10 ? "%" PRIu64 : aRadix == 8 ? "%" PRIo64 : "%" PRIx64,
                 aInteger);
  }

  /**
   * Append the given float to this string
   */
  void NS_FASTCALL AppendFloat(float aFloat);
  void NS_FASTCALL AppendFloat(double aFloat);
public:

  // AppendLiteral must ONLY be applied to an actual literal string.
  // Do not attempt to use it with a regular char* pointer, or with a char
  // array variable. Use Append or AppendASCII for those.
  template<int N>
  void AppendLiteral(const char_type (&aStr)[N])
  {
    ReplaceLiteral(mLength, 0, aStr, N - 1);
  }
#ifdef CharT_is_PRUnichar
  template<int N>
  void AppendLiteral(const char (&aStr)[N])
  {
    AppendASCII(aStr, N - 1);
  }

  template<int N>
  MOZ_MUST_USE bool AppendLiteral(const char (&aStr)[N], const fallible_t& aFallible)
  {
    return AppendASCII(aStr, N - 1, aFallible);
  }
#endif

  self_type& operator+=(char_type aChar)
  {
    Append(aChar);
    return *this;
  }
  self_type& operator+=(const char_type* aData)
  {
    Append(aData);
    return *this;
  }
#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  self_type& operator+=(char16ptr_t aData)
  {
    Append(aData);
    return *this;
  }
#endif
  self_type& operator+=(const self_type& aStr)
  {
    Append(aStr);
    return *this;
  }
  self_type& operator+=(const substring_tuple_type& aTuple)
  {
    Append(aTuple);
    return *this;
  }

  void Insert(char_type aChar, index_type aPos)
  {
    Replace(aPos, 0, aChar);
  }
  void Insert(const char_type* aData, index_type aPos,
              size_type aLength = size_type(-1))
  {
    Replace(aPos, 0, aData, aLength);
  }
#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  void Insert(char16ptr_t aData, index_type aPos,
              size_type aLength = size_type(-1))
  {
    Insert(static_cast<const char16_t*>(aData), aPos, aLength);
  }
#endif
  void Insert(const self_type& aStr, index_type aPos)
  {
    Replace(aPos, 0, aStr);
  }
  void Insert(const substring_tuple_type& aTuple, index_type aPos)
  {
    Replace(aPos, 0, aTuple);
  }

  // InsertLiteral must ONLY be applied to an actual literal string.
  // Do not attempt to use it with a regular char* pointer, or with a char
  // array variable. Use Insert for those.
  template<int N>
  void InsertLiteral(const char_type (&aStr)[N], index_type aPos)
  {
    ReplaceLiteral(aPos, 0, aStr, N - 1);
  }

  void Cut(index_type aCutStart, size_type aCutLength)
  {
    Replace(aCutStart, aCutLength, char_traits::sEmptyBuffer, 0);
  }

  nsTSubstringSplitter_CharT Split(const char_type aChar) const;

  /**
   * buffer sizing
   */

  /**
   * Attempts to set the capacity to the given size in number of
   * characters, without affecting the length of the string.
   * There is no need to include room for the null terminator: it is
   * the job of the string class.
   * Also ensures that the buffer is mutable.
   */
  void NS_FASTCALL SetCapacity(size_type aNewCapacity);
  MOZ_MUST_USE bool NS_FASTCALL SetCapacity(size_type aNewCapacity,
                                            const fallible_t&);

  void NS_FASTCALL SetLength(size_type aNewLength);
  MOZ_MUST_USE bool NS_FASTCALL SetLength(size_type aNewLength,
                                          const fallible_t&);

  void Truncate(size_type aNewLength = 0)
  {
    NS_ASSERTION(aNewLength <= mLength, "Truncate cannot make string longer");
    SetLength(aNewLength);
  }


  /**
   * buffer access
   */


  /**
   * Get a const pointer to the string's internal buffer.  The caller
   * MUST NOT modify the characters at the returned address.
   *
   * @returns The length of the buffer in characters.
   */
  inline size_type GetData(const char_type** aData) const
  {
    *aData = mData;
    return mLength;
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
  size_type GetMutableData(char_type** aData, size_type aNewLen = size_type(-1))
  {
    if (!EnsureMutable(aNewLen)) {
      AllocFailed(aNewLen == size_type(-1) ? mLength : aNewLen);
    }

    *aData = mData;
    return mLength;
  }

  size_type GetMutableData(char_type** aData, size_type aNewLen, const fallible_t&)
  {
    if (!EnsureMutable(aNewLen)) {
      *aData = nullptr;
      return 0;
    }

    *aData = mData;
    return mLength;
  }

#if defined(CharT_is_PRUnichar) && defined(MOZ_USE_CHAR16_WRAPPER)
  size_type GetMutableData(wchar_t** aData, size_type aNewLen = size_type(-1))
  {
    return GetMutableData(reinterpret_cast<char16_t**>(aData), aNewLen);
  }

  size_type GetMutableData(wchar_t** aData, size_type aNewLen,
                           const fallible_t& aFallible)
  {
    return GetMutableData(reinterpret_cast<char16_t**>(aData), aNewLen,
                          aFallible);
  }
#endif

  /**
   * Span integration
   */

  operator mozilla::Span<char_type>()
  {
    return mozilla::MakeSpan(BeginWriting(), Length());
  }

  operator mozilla::Span<const char_type>() const
  {
    return mozilla::MakeSpan(BeginReading(), Length());
  }

  void Append(mozilla::Span<const char_type> aSpan)
  {
    auto len = aSpan.Length();
    MOZ_RELEASE_ASSERT(len <= mozilla::MaxValue<size_type>::value);
    Append(aSpan.Elements(), len);
  }

  MOZ_MUST_USE bool Append(mozilla::Span<const char_type> aSpan,
                           const fallible_t& aFallible)
  {
    auto len = aSpan.Length();
    if (len > mozilla::MaxValue<size_type>::value) {
      return false;
    }
    return Append(aSpan.Elements(), len, aFallible);
  }

#if !defined(CharT_is_PRUnichar)
  operator mozilla::Span<uint8_t>()
  {
    return mozilla::MakeSpan(reinterpret_cast<uint8_t*>(BeginWriting()),
                             Length());
  }

  operator mozilla::Span<const uint8_t>() const
  {
    return mozilla::MakeSpan(reinterpret_cast<const uint8_t*>(BeginReading()),
                             Length());
  }

  void Append(mozilla::Span<const uint8_t> aSpan)
  {
    auto len = aSpan.Length();
    MOZ_RELEASE_ASSERT(len <= mozilla::MaxValue<size_type>::value);
    Append(reinterpret_cast<const char*>(aSpan.Elements()), len);
  }

  MOZ_MUST_USE bool Append(mozilla::Span<const uint8_t> aSpan,
                           const fallible_t& aFallible)
  {
    auto len = aSpan.Length();
    if (len > mozilla::MaxValue<size_type>::value) {
      return false;
    }
    return Append(
      reinterpret_cast<const char*>(aSpan.Elements()), len, aFallible);
  }
#endif

  /**
   * string data is never null, but can be marked void.  if true, the
   * string will be truncated.  @see nsTSubstring::IsVoid
   */

  void NS_FASTCALL SetIsVoid(bool);

  /**
   *  This method is used to remove all occurrences of aChar from this
   * string.
   *
   *  @param  aChar -- char to be stripped
   */

  void StripChar(char_type aChar);

  /**
   *  This method is used to remove all occurrences of aChars from this
   * string.
   *
   *  @param  aChars -- chars to be stripped
   */

  void StripChars(const char_type* aChars);

  /**
   * This method is used to remove all occurrences of some characters this
   * from this string.  The characters removed have the corresponding
   * entries in the bool array set to true; we retain all characters
   * with code beyond 127.
   * THE CALLER IS RESPONSIBLE for making sure the complete boolean
   * array, 128 entries, is properly initialized.
   *
   * See also: ASCIIMask class.
   *
   *  @param  aToStrip -- Array where each entry is true if the
   *          corresponding ASCII character is to be stripped.  All
   *          characters beyond code 127 are retained.  Note that this
   *          parameter is of ASCIIMaskArray type, but we expand the typedef
   *          to avoid having to include nsASCIIMask.h in this include file
   *          as it brings other includes.
   */
  void StripTaggedASCII(const std::array<bool, 128>& aToStrip);

  /**
   * A shortcut to strip \r and \n.
   */
  void StripCRLF();

  /**
   * If the string uses a shared buffer, this method
   * clears the pointer without releasing the buffer.
   */
  void ForgetSharedBuffer()
  {
    if (mDataFlags & DataFlags::SHARED) {
      SetToEmptyBuffer();
    }
  }

public:

  /**
   * this is public to support automatic conversion of tuple to string
   * base type, which helps avoid converting to nsTAString.
   */
  MOZ_IMPLICIT nsTSubstring_CharT(const substring_tuple_type& aTuple)
    : nsTStringRepr_CharT(nullptr, 0, DataFlags(0), ClassFlags(0))
  {
    Assign(aTuple);
  }

  size_t SizeOfExcludingThisIfUnshared(mozilla::MallocSizeOf aMallocSizeOf)
  const;
  size_t SizeOfIncludingThisIfUnshared(mozilla::MallocSizeOf aMallocSizeOf)
  const;

  /**
   * WARNING: Only use these functions if you really know what you are
   * doing, because they can easily lead to double-counting strings.  If
   * you do use them, please explain clearly in a comment why it's safe
   * and won't lead to double-counting.
   */
  size_t SizeOfExcludingThisEvenIfShared(mozilla::MallocSizeOf aMallocSizeOf)
  const;
  size_t SizeOfIncludingThisEvenIfShared(mozilla::MallocSizeOf aMallocSizeOf)
  const;

  template<class T>
  void NS_ABORT_OOM(T)
  {
    struct never {}; // a compiler-friendly way to do static_assert(false)
    static_assert(mozilla::IsSame<T, never>::value,
      "In string classes, use AllocFailed to account for sizeof(char_type). "
      "Use the global ::NS_ABORT_OOM if you really have a count of bytes.");
  }

  MOZ_ALWAYS_INLINE void AllocFailed(size_t aLength)
  {
    ::NS_ABORT_OOM(aLength * sizeof(char_type));
  }

protected:

  // default initialization
  nsTSubstring_CharT()
    : nsTStringRepr_CharT(char_traits::sEmptyBuffer, 0, DataFlags::TERMINATED,
                          ClassFlags(0))
  {
  }

  // copy-constructor, constructs as dependent on given object
  // (NOTE: this is for internal use only)
  nsTSubstring_CharT(const self_type& aStr)
    : nsTStringRepr_CharT(aStr.mData, aStr.mLength,
                          aStr.mDataFlags & (DataFlags::TERMINATED | DataFlags::VOIDED),
                          ClassFlags(0))
  {
  }

  // initialization with ClassFlags
  explicit nsTSubstring_CharT(ClassFlags aClassFlags)
    : nsTStringRepr_CharT(char_traits::sEmptyBuffer, 0, DataFlags::TERMINATED,
                          aClassFlags)
  {
  }

 /**
   * allows for direct initialization of a nsTSubstring object.
   */
  nsTSubstring_CharT(char_type* aData, size_type aLength,
                     DataFlags aDataFlags, ClassFlags aClassFlags)
// XXXbz or can I just include nscore.h and use NS_BUILD_REFCNT_LOGGING?
#if defined(DEBUG) || defined(FORCE_BUILD_REFCNT_LOGGING)
#define XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
    ;
#else
#undef XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
    : nsTStringRepr_CharT(aData, aLength, aDataFlags, aClassFlags)
  {
    MOZ_RELEASE_ASSERT(CheckCapacity(aLength), "String is too large.");
  }
#endif /* DEBUG || FORCE_BUILD_REFCNT_LOGGING */

  void SetToEmptyBuffer()
  {
    mData = char_traits::sEmptyBuffer;
    mLength = 0;
    mDataFlags = DataFlags::TERMINATED;
  }

  void SetData(char_type* aData, size_type aLength, DataFlags aDataFlags)
  {
    mData = aData;
    mLength = aLength;
    mDataFlags = aDataFlags;
  }

  /**
   * this function releases mData and does not change the value of
   * any of its member variables.  in other words, this function acts
   * like a destructor.
   */
  void NS_FASTCALL Finalize();

  /**
   * this function prepares mData to be mutated.
   *
   * @param aCapacity    specifies the required capacity of mData
   * @param aOldData     returns null or the old value of mData
   * @param aOldFlags    returns 0 or the old value of mDataFlags
   *
   * if mData is already mutable and of sufficient capacity, then this
   * function will return immediately.  otherwise, it will either resize
   * mData or allocate a new shared buffer.  if it needs to allocate a
   * new buffer, then it will return the old buffer and the corresponding
   * flags.  this allows the caller to decide when to free the old data.
   *
   * this function returns false if is unable to allocate sufficient
   * memory.
   *
   * XXX we should expose a way for subclasses to free old_data.
   */
  bool NS_FASTCALL MutatePrep(size_type aCapacity,
                              char_type** aOldData, DataFlags* aOldDataFlags);

  /**
   * this function prepares a section of mData to be modified.  if
   * necessary, this function will reallocate mData and possibly move
   * existing data to open up the specified section.
   *
   * @param aCutStart    specifies the starting offset of the section
   * @param aCutLength   specifies the length of the section to be replaced
   * @param aNewLength   specifies the length of the new section
   *
   * for example, suppose mData contains the string "abcdef" then
   *
   *   ReplacePrep(2, 3, 4);
   *
   * would cause mData to look like "ab____f" where the characters
   * indicated by '_' have an unspecified value and can be freely
   * modified.  this function will null-terminate mData upon return.
   *
   * this function returns false if is unable to allocate sufficient
   * memory.
   */
  MOZ_MUST_USE bool ReplacePrep(index_type aCutStart,
                                size_type aCutLength,
                                size_type aNewLength);

  MOZ_MUST_USE bool NS_FASTCALL ReplacePrepInternal(
    index_type aCutStart,
    size_type aCutLength,
    size_type aNewFragLength,
    size_type aNewTotalLength);

  /**
   * returns the number of writable storage units starting at mData.
   * the value does not include space for the null-terminator character.
   *
   * NOTE: this function returns 0 if mData is immutable (or the buffer
   *       is 0-sized).
   */
  size_type NS_FASTCALL Capacity() const;

  /**
   * this helper function can be called prior to directly manipulating
   * the contents of mData.  see, for example, BeginWriting.
   */
  MOZ_MUST_USE bool NS_FASTCALL EnsureMutable(
    size_type aNewLen = size_type(-1));

  /**
   * Checks if the given capacity is valid for this string type.
   */
  static MOZ_MUST_USE bool CheckCapacity(size_type aCapacity) {
    if (aCapacity > kMaxCapacity) {
      // Also assert for |aCapacity| equal to |size_type(-1)|, since we used to
      // use that value to flag immutability.
      NS_ASSERTION(aCapacity != size_type(-1), "Bogus capacity");
      return false;
    }

    return true;
  }

  void NS_FASTCALL ReplaceLiteral(index_type aCutStart, size_type aCutLength,
                                  const char_type* aData, size_type aLength);

  static const size_type kMaxCapacity;
public:

  // NOTE: this method is declared public _only_ for convenience for
  // callers who don't have access to the original nsLiteralString_CharT.
  void NS_FASTCALL AssignLiteral(const char_type* aData, size_type aLength);
};

static_assert(sizeof(nsTSubstring_CharT) ==
              sizeof(mozilla::detail::nsTStringRepr_CharT),
              "Don't add new data fields to nsTSubstring_CharT. "
              "Add to nsTStringRepr_CharT instead.");

int NS_FASTCALL
Compare(const nsTSubstring_CharT::base_string_type& aLhs,
        const nsTSubstring_CharT::base_string_type& aRhs,
        const nsTStringComparator_CharT& = nsTDefaultStringComparator_CharT());


inline bool
operator!=(const nsTSubstring_CharT::base_string_type& aLhs,
           const nsTSubstring_CharT::base_string_type& aRhs)
{
  return !aLhs.Equals(aRhs);
}

inline bool
operator!=(const nsTSubstring_CharT::base_string_type& aLhs,
           const nsTSubstring_CharT::char_type* aRhs)
{
  return !aLhs.Equals(aRhs);
}

inline bool
operator<(const nsTSubstring_CharT::base_string_type& aLhs,
          const nsTSubstring_CharT::base_string_type& aRhs)
{
  return Compare(aLhs, aRhs) < 0;
}

inline bool
operator<=(const nsTSubstring_CharT::base_string_type& aLhs,
           const nsTSubstring_CharT::base_string_type& aRhs)
{
  return Compare(aLhs, aRhs) <= 0;
}

inline bool
operator==(const nsTSubstring_CharT::base_string_type& aLhs,
           const nsTSubstring_CharT::base_string_type& aRhs)
{
  return aLhs.Equals(aRhs);
}

inline bool
operator==(const nsTSubstring_CharT::base_string_type& aLhs,
           const nsTSubstring_CharT::char_type* aRhs)
{
  return aLhs.Equals(aRhs);
}


inline bool
operator>=(const nsTSubstring_CharT::base_string_type& aLhs,
           const nsTSubstring_CharT::base_string_type& aRhs)
{
  return Compare(aLhs, aRhs) >= 0;
}

inline bool
operator>(const nsTSubstring_CharT::base_string_type& aLhs,
          const nsTSubstring_CharT::base_string_type& aRhs)
{
  return Compare(aLhs, aRhs) > 0;
}

// You should not need to instantiate this class directly.
// Use nsTSubstring::Split instead.
class nsTSubstringSplitter_CharT
{
  typedef nsTSubstring_CharT::size_type size_type;
  typedef nsTSubstring_CharT::char_type char_type;

  class nsTSubstringSplit_Iter
  {
  public:
    nsTSubstringSplit_Iter(const nsTSubstringSplitter_CharT& aObj,
                           size_type aPos)
      : mObj(aObj)
      , mPos(aPos)
    {
    }

    bool operator!=(const nsTSubstringSplit_Iter& other) const
    {
      return mPos != other.mPos;
    }

    const nsTDependentSubstring_CharT& operator*() const;

    const nsTSubstringSplit_Iter& operator++()
    {
      ++mPos;
      return *this;
    }

  private:
    const nsTSubstringSplitter_CharT& mObj;
    size_type mPos;
  };

private:
  const nsTSubstring_CharT* const mStr;
  mozilla::UniquePtr<nsTDependentSubstring_CharT[]> mArray;
  size_type mArraySize;
  const char_type mDelim;

public:
  nsTSubstringSplitter_CharT(const nsTSubstring_CharT* aStr, char_type aDelim);

  nsTSubstringSplit_Iter begin() const
  {
    return nsTSubstringSplit_Iter(*this, 0);
  }

  nsTSubstringSplit_Iter end() const
  {
    return nsTSubstringSplit_Iter(*this, mArraySize);
  }

  const nsTDependentSubstring_CharT& Get(const size_type index) const
  {
    MOZ_ASSERT(index < mArraySize);
    return mArray[index];
  }
};

/**
 * Span integration
 */
namespace mozilla {

inline Span<CharT>
MakeSpan(nsTSubstring_CharT& aString)
{
  return aString;
}

inline Span<const CharT>
MakeSpan(const nsTSubstring_CharT& aString)
{
  return aString;
}

} // namespace mozilla
