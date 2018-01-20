/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#ifndef nsTSubstring_h
#define nsTSubstring_h

#include "mozilla/Casting.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/Span.h"

#include "nsTStringRepr.h"

#ifndef MOZILLA_INTERNAL_API
#error "Using XPCOM strings is limited to code linked into libxul."
#endif

template <typename T> class nsTSubstringSplitter;
template <typename T> class nsTString;

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
template <typename T>
class nsTSubstring : public mozilla::detail::nsTStringRepr<T>
{
public:
  typedef nsTSubstring<T> self_type;

  typedef nsTString<T> string_type;

  typedef typename mozilla::detail::nsTStringRepr<T> base_string_type;
  typedef typename base_string_type::substring_type substring_type;
  typedef typename base_string_type::literalstring_type literalstring_type;

  typedef typename base_string_type::fallible_t fallible_t;

  typedef typename base_string_type::char_type char_type;
  typedef typename base_string_type::char_traits char_traits;
  typedef typename base_string_type::incompatible_char_type incompatible_char_type;

  typedef typename base_string_type::substring_tuple_type substring_tuple_type;

  typedef typename base_string_type::const_iterator const_iterator;
  typedef typename base_string_type::iterator iterator;

  typedef typename base_string_type::comparator_type comparator_type;

  typedef typename base_string_type::char_iterator char_iterator;
  typedef typename base_string_type::const_char_iterator const_char_iterator;

  typedef typename base_string_type::index_type index_type;
  typedef typename base_string_type::size_type size_type;

  // These are only for internal use within the string classes:
  typedef typename base_string_type::DataFlags DataFlags;
  typedef typename base_string_type::ClassFlags ClassFlags;

  // this acts like a virtual destructor
  ~nsTSubstring()
  {
    Finalize();
  }

  /**
   * writing iterators
   */

  char_iterator BeginWriting()
  {
    if (!EnsureMutable()) {
      AllocFailed(base_string_type::mLength);
    }

    return base_string_type::mData;
  }

  char_iterator BeginWriting(const fallible_t&)
  {
    return EnsureMutable() ? base_string_type::mData : char_iterator(0);
  }

  char_iterator EndWriting()
  {
    if (!EnsureMutable()) {
      AllocFailed(base_string_type::mLength);
    }

    return base_string_type::mData + base_string_type::mLength;
  }

  char_iterator EndWriting(const fallible_t&)
  {
    return EnsureMutable() ? (base_string_type::mData + base_string_type::mLength) : char_iterator(0);
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
   * Perform string to int conversion.
   * @param   aErrorCode will contain error if one occurs
   * @param   aRadix is the radix to use. Only 10 and 16 are supported.
   * @return  int rep of string value, and possible (out) error code
   */
  int32_t ToInteger(nsresult* aErrorCode, uint32_t aRadix = 10) const;

  /**
   * Perform string to 64-bit int conversion.
   * @param   aErrorCode will contain error if one occurs
   * @param   aRadix is the radix to use. Only 10 and 16 are supported.
   * @return  64-bit int rep of string value, and possible (out) error code
   */
  int64_t ToInteger64(nsresult* aErrorCode, uint32_t aRadix = 10) const;

  /**
   * deprecated writing iterators
   */

  iterator& BeginWriting(iterator& aIter)
  {
    char_type* data = BeginWriting();
    aIter.mStart = data;
    aIter.mEnd = data + base_string_type::mLength;
    aIter.mPosition = aIter.mStart;
    return aIter;
  }

  iterator& EndWriting(iterator& aIter)
  {
    char_type* data = BeginWriting();
    aIter.mStart = data;
    aIter.mEnd = data + base_string_type::mLength;
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

  void NS_FASTCALL Assign(self_type&&);
  MOZ_MUST_USE bool NS_FASTCALL Assign(self_type&&, const fallible_t&);

  // XXX(nika): GCC 4.9 doesn't correctly resolve calls to Assign a
  // nsLiteralCString into a nsTSubstring, due to a frontend bug. This explcit
  // Assign overload (and the corresponding constructor and operator= overloads)
  // are used to avoid this bug. Once we stop supporting GCC 4.9 we can remove
  // them.
  void NS_FASTCALL Assign(const literalstring_type&);

  void NS_FASTCALL Assign(const substring_tuple_type&);
  MOZ_MUST_USE bool NS_FASTCALL Assign(const substring_tuple_type&,
                                       const fallible_t&);

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void Assign(char16ptr_t aData)
  {
    Assign(static_cast<const char16_t*>(aData));
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void Assign(char16ptr_t aData, size_type aLength)
  {
    Assign(static_cast<const char16_t*>(aData), aLength);
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
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

  template<int N, typename Q = T, typename EnableIfChar16 = typename mozilla::Char16OnlyT<Q>>
  void AssignLiteral(const incompatible_char_type (&aStr)[N])
  {
    AssignASCII(aStr, N - 1);
  }

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
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
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
  self_type& operator=(self_type&& aStr)
  {
    Assign(mozilla::Move(aStr));
    return *this;
  }
  // NOTE(nika): gcc 4.9 workaround. Remove when support is dropped.
  self_type& operator=(const literalstring_type& aStr)
  {
    Assign(aStr);
    return *this;
  }
  self_type& operator=(const substring_tuple_type& aTuple)
  {
    Assign(aTuple);
    return *this;
  }

  // Adopt a heap-allocated char sequence for this string; is Voided if aData
  // is null. Useful for e.g. converting an strdup'd C string into an
  // nsCString. See also getter_Copies(), which is a useful wrapper.
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
    Replace(base_string_type::mLength, 0, aChar);
  }
  MOZ_MUST_USE bool Append(char_type aChar, const fallible_t& aFallible)
  {
    return Replace(base_string_type::mLength, 0, aChar, aFallible);
  }
  void Append(const char_type* aData, size_type aLength = size_type(-1))
  {
    Replace(base_string_type::mLength, 0, aData, aLength);
  }
  MOZ_MUST_USE bool Append(const char_type* aData, size_type aLength,
                           const fallible_t& aFallible)
  {
    return Replace(base_string_type::mLength, 0, aData, aLength, aFallible);
  }

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void Append(char16ptr_t aData, size_type aLength = size_type(-1))
  {
    Append(static_cast<const char16_t*>(aData), aLength);
  }
#endif

  void Append(const self_type& aStr)
  {
    Replace(base_string_type::mLength, 0, aStr);
  }
  MOZ_MUST_USE bool Append(const self_type& aStr, const fallible_t& aFallible)
  {
    return Replace(base_string_type::mLength, 0, aStr, aFallible);
  }
  void Append(const substring_tuple_type& aTuple)
  {
    Replace(base_string_type::mLength, 0, aTuple);
  }

  void AppendASCII(const char* aData, size_type aLength = size_type(-1))
  {
    ReplaceASCII(base_string_type::mLength, 0, aData, aLength);
  }

  MOZ_MUST_USE bool AppendASCII(const char* aData, const fallible_t& aFallible)
  {
    return ReplaceASCII(base_string_type::mLength, 0, aData, size_type(-1), aFallible);
  }

  MOZ_MUST_USE bool AppendASCII(const char* aData, size_type aLength, const fallible_t& aFallible)
  {
    return ReplaceASCII(base_string_type::mLength, 0, aData, aLength, aFallible);
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
    ReplaceLiteral(base_string_type::mLength, 0, aStr, N - 1);
  }

  // Only enable for T = char16_t
  template <int N, typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void AppendLiteral(const incompatible_char_type (&aStr)[N])
  {
    AppendASCII(aStr, N - 1);
  }

  // Only enable for T = char16_t
  template <int N, typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  MOZ_MUST_USE bool
  AppendLiteral(const incompatible_char_type (&aStr)[N], const fallible_t& aFallible)
  {
    return AppendASCII(aStr, N - 1, aFallible);
  }

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
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
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
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
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

  nsTSubstringSplitter<T> Split(const char_type aChar) const;

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
    NS_ASSERTION(aNewLength <= base_string_type::mLength, "Truncate cannot make string longer");
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
    *aData = base_string_type::mData;
    return base_string_type::mLength;
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
      AllocFailed(aNewLen == size_type(-1) ? base_string_type::mLength : aNewLen);
    }

    *aData = base_string_type::mData;
    return base_string_type::mLength;
  }

  size_type GetMutableData(char_type** aData, size_type aNewLen, const fallible_t&)
  {
    if (!EnsureMutable(aNewLen)) {
      *aData = nullptr;
      return 0;
    }

    *aData = base_string_type::mData;
    return base_string_type::mLength;
  }

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  size_type GetMutableData(wchar_t** aData, size_type aNewLen = size_type(-1))
  {
    return GetMutableData(reinterpret_cast<char16_t**>(aData), aNewLen);
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
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
    return mozilla::MakeSpan(BeginWriting(), base_string_type::Length());
  }

  operator mozilla::Span<const char_type>() const
  {
    return mozilla::MakeSpan(base_string_type::BeginReading(), base_string_type::Length());
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

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  operator mozilla::Span<uint8_t>()
  {
    return mozilla::MakeSpan(reinterpret_cast<uint8_t*>(BeginWriting()),
                             base_string_type::Length());
  }

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  operator mozilla::Span<const uint8_t>() const
  {
    return mozilla::MakeSpan(reinterpret_cast<const uint8_t*>(base_string_type::BeginReading()),
                             base_string_type::Length());
  }

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  void Append(mozilla::Span<const uint8_t> aSpan)
  {
    auto len = aSpan.Length();
    MOZ_RELEASE_ASSERT(len <= mozilla::MaxValue<size_type>::value);
    Append(reinterpret_cast<const char*>(aSpan.Elements()), len);
  }

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
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
    if (base_string_type::mDataFlags & DataFlags::SHARED) {
      SetToEmptyBuffer();
    }
  }

protected:
  void AssertValid()
  {
    MOZ_ASSERT(!(this->mClassFlags & ClassFlags::NULL_TERMINATED) ||
               (this->mDataFlags & DataFlags::TERMINATED),
               "String classes whose static type guarantees a null-terminated "
               "buffer must not be assigned a non-null-terminated buffer.");
  }

public:

  /**
   * this is public to support automatic conversion of tuple to string
   * base type, which helps avoid converting to nsTAString.
   */
  MOZ_IMPLICIT nsTSubstring(const substring_tuple_type& aTuple)
    : base_string_type(nullptr, 0, DataFlags(0), ClassFlags(0))
  {
    AssertValid();
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

  template<class N>
  void NS_ABORT_OOM(T)
  {
    struct never {}; // a compiler-friendly way to do static_assert(false)
    static_assert(mozilla::IsSame<N, never>::value,
      "In string classes, use AllocFailed to account for sizeof(char_type). "
      "Use the global ::NS_ABORT_OOM if you really have a count of bytes.");
  }

  MOZ_ALWAYS_INLINE void AllocFailed(size_t aLength)
  {
    ::NS_ABORT_OOM(aLength * sizeof(char_type));
  }

protected:

  // default initialization
  nsTSubstring()
    : base_string_type(char_traits::sEmptyBuffer, 0, DataFlags::TERMINATED,
                       ClassFlags(0))
  {
    AssertValid();
  }

  // copy-constructor, constructs as dependent on given object
  // (NOTE: this is for internal use only)
  nsTSubstring(const self_type& aStr)
    : base_string_type(aStr.base_string_type::mData, aStr.base_string_type::mLength,
                       aStr.base_string_type::mDataFlags & (DataFlags::TERMINATED | DataFlags::VOIDED),
                       ClassFlags(0))
  {
    AssertValid();
  }

  // initialization with ClassFlags
  explicit nsTSubstring(ClassFlags aClassFlags)
    : base_string_type(char_traits::sEmptyBuffer, 0, DataFlags::TERMINATED,
                       aClassFlags)
  {
    AssertValid();
  }

 /**
   * allows for direct initialization of a nsTSubstring object.
   */
  nsTSubstring(char_type* aData, size_type aLength,
               DataFlags aDataFlags, ClassFlags aClassFlags)
// XXXbz or can I just include nscore.h and use NS_BUILD_REFCNT_LOGGING?
#if defined(DEBUG) || defined(FORCE_BUILD_REFCNT_LOGGING)
#define XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
    ;
#else
#undef XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
    : base_string_type(aData, aLength, aDataFlags, aClassFlags)
  {
    AssertValid();
    MOZ_RELEASE_ASSERT(CheckCapacity(aLength), "String is too large.");
  }
#endif /* DEBUG || FORCE_BUILD_REFCNT_LOGGING */

  void SetToEmptyBuffer()
  {
    base_string_type::mData = char_traits::sEmptyBuffer;
    base_string_type::mLength = 0;
    base_string_type::mDataFlags = DataFlags::TERMINATED;
    AssertValid();
  }

  void SetData(char_type* aData, size_type aLength, DataFlags aDataFlags)
  {
    base_string_type::mData = aData;
    base_string_type::mLength = aLength;
    base_string_type::mDataFlags = aDataFlags;
    AssertValid();
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

extern template class nsTSubstring<char>;
extern template class nsTSubstring<char16_t>;

static_assert(sizeof(nsTSubstring<char>) ==
              sizeof(mozilla::detail::nsTStringRepr<char>),
              "Don't add new data fields to nsTSubstring_CharT. "
              "Add to nsTStringRepr<T> instead.");


// You should not need to instantiate this class directly.
// Use nsTSubstring::Split instead.
template <typename T>
class nsTSubstringSplitter
{
  typedef typename nsTSubstring<T>::size_type size_type;
  typedef typename nsTSubstring<T>::char_type char_type;

  class nsTSubstringSplit_Iter
  {
  public:
    nsTSubstringSplit_Iter(const nsTSubstringSplitter<T>& aObj,
                           size_type aPos)
      : mObj(aObj)
      , mPos(aPos)
    {
    }

    bool operator!=(const nsTSubstringSplit_Iter& other) const
    {
      return mPos != other.mPos;
    }

    const nsTDependentSubstring<T>& operator*() const;

    const nsTSubstringSplit_Iter& operator++()
    {
      ++mPos;
      return *this;
    }

  private:
    const nsTSubstringSplitter<T>& mObj;
    size_type mPos;
  };

private:
  const nsTSubstring<T>* const mStr;
  mozilla::UniquePtr<nsTDependentSubstring<T>[]> mArray;
  size_type mArraySize;
  const char_type mDelim;

public:
  nsTSubstringSplitter(const nsTSubstring<T>* aStr, char_type aDelim);

  nsTSubstringSplit_Iter begin() const
  {
    return nsTSubstringSplit_Iter(*this, 0);
  }

  nsTSubstringSplit_Iter end() const
  {
    return nsTSubstringSplit_Iter(*this, mArraySize);
  }

  const nsTDependentSubstring<T>& Get(const size_type index) const
  {
    MOZ_ASSERT(index < mArraySize);
    return mArray[index];
  }
};

extern template class nsTSubstringSplitter<char>;
extern template class nsTSubstringSplitter<char16_t>;

/**
 * Span integration
 */
namespace mozilla {

inline Span<char>
MakeSpan(nsTSubstring<char>& aString)
{
  return aString;
}

inline Span<const char>
MakeSpan(const nsTSubstring<char>& aString)
{
  return aString;
}

inline Span<char16_t>
MakeSpan(nsTSubstring<char16_t>& aString)
{
  return aString;
}

inline Span<const char16_t>
MakeSpan(const nsTSubstring<char16_t>& aString)
{
  return aString;
}


} // namespace mozilla

#endif
