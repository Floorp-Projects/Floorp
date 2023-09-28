/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#ifndef nsTSubstring_h
#define nsTSubstring_h

#include <iterator>
#include <type_traits>

#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Span.h"
#include "mozilla/Try.h"
#include "mozilla/Unused.h"

#include "nsTStringRepr.h"

#ifndef MOZILLA_INTERNAL_API
#  error "Using XPCOM strings is limited to code linked into libxul."
#endif

// The max number of logically uninitialized code units to
// fill with a marker byte or to mark as unintialized for
// memory checking. (Limited to avoid quadratic behavior.)
const size_t kNsStringBufferMaxPoison = 16;

class nsStringBuffer;
template <typename T>
class nsTSubstringSplitter;
template <typename T>
class nsTString;
template <typename T>
class nsTSubstring;

namespace mozilla {

/**
 * This handle represents permission to perform low-level writes
 * the storage buffer of a string in a manner that's aware of the
 * actual capacity of the storage buffer allocation and that's
 * cache-friendly in the sense that the writing of zero terminator
 * for C compatibility can happen in linear memory access order
 * (i.e. the zero terminator write takes place after writing
 * new content to the string as opposed to the zero terminator
 * write happening first causing a non-linear memory write for
 * cache purposes).
 *
 * If you requested a prefix to be preserved when starting
 * or restarting the bulk write, the prefix is present at the
 * start of the buffer exposed by this handle as Span or
 * as a raw pointer, and it's your responsibility to start
 * writing after after the preserved prefix (which you
 * presumably wanted not to overwrite since you asked for
 * it to be preserved).
 *
 * In a success case, you must call Finish() with the new
 * length of the string. In failure cases, it's OK to return
 * early from the function whose local variable this handle is.
 * The destructor of this class takes care of putting the
 * string in a valid and mostly harmless state in that case
 * by setting the value of a non-empty string to a single
 * REPLACEMENT CHARACTER or in the case of nsACString that's
 * too short for a REPLACEMENT CHARACTER to fit, an ASCII
 * SUBSTITUTE.
 *
 * You must not allow this handle to outlive the string you
 * obtained it from.
 *
 * You must not access the string you obtained this handle
 * from in any way other than through this handle until
 * you call Finish() on the handle or the handle goes out
 * of scope.
 *
 * Once you've called Finish(), you must not call any
 * methods on this handle and must not use values previously
 * obtained.
 *
 * Once you call RestartBulkWrite(), you must not use
 * values previously obtained from this handle and must
 * reobtain the new corresponding values.
 */
template <typename T>
class BulkWriteHandle final {
  friend class nsTSubstring<T>;

 public:
  typedef typename mozilla::detail::nsTStringRepr<T> base_string_type;
  typedef typename base_string_type::size_type size_type;

  /**
   * Pointer to the start of the writable buffer. Never nullptr.
   *
   * This pointer is valid until whichever of these happens first:
   *  1) Finish() is called
   *  2) RestartBulkWrite() is called
   *  3) BulkWriteHandle goes out of scope
   */
  T* Elements() const {
    MOZ_ASSERT(mString);
    return mString->mData;
  }

  /**
   * How many code units can be written to the buffer.
   * (Note: This is not the same as the string's Length().)
   *
   * This value is valid until whichever of these happens first:
   *  1) Finish() is called
   *  2) RestartBulkWrite() is called
   *  3) BulkWriteHandle goes out of scope
   */
  size_type Length() const {
    MOZ_ASSERT(mString);
    return mCapacity;
  }

  /**
   * Pointer past the end of the buffer.
   *
   * This pointer is valid until whichever of these happens first:
   *  1) Finish() is called
   *  2) RestartBulkWrite() is called
   *  3) BulkWriteHandle goes out of scope
   */
  T* End() const { return Elements() + Length(); }

  /**
   * The writable buffer as Span.
   *
   * This Span is valid until whichever of these happens first:
   *  1) Finish() is called
   *  2) RestartBulkWrite() is called
   *  3) BulkWriteHandle goes out of scope
   */
  auto AsSpan() const { return mozilla::Span<T>{Elements(), Length()}; }

  /**
   * Autoconvert to the buffer as writable Span.
   *
   * This Span is valid until whichever of these happens first:
   *  1) Finish() is called
   *  2) RestartBulkWrite() is called
   *  3) BulkWriteHandle goes out of scope
   */
  operator mozilla::Span<T>() const { return AsSpan(); }

  /**
   * Restart the bulk write with a different capacity.
   *
   * This method invalidates previous return values
   * of the other methods above.
   *
   * Can fail if out of memory leaving the buffer
   * in the state before this call.
   *
   * @param aCapacity the new requested capacity
   * @param aPrefixToPreserve the number of code units at
   *                          the start of the string to
   *                          copy over to the new buffer
   * @param aAllowShrinking whether the string is
   *                        allowed to attempt to
   *                        allocate a smaller buffer
   *                        for its content and copy
   *                        the data over.
   */
  mozilla::Result<mozilla::Ok, nsresult> RestartBulkWrite(
      size_type aCapacity, size_type aPrefixToPreserve, bool aAllowShrinking) {
    MOZ_ASSERT(mString);
    MOZ_TRY_VAR(mCapacity, mString->StartBulkWriteImpl(
                               aCapacity, aPrefixToPreserve, aAllowShrinking));
    return mozilla::Ok();
  }

  /**
   * Indicate that the bulk write finished successfully.
   *
   * @param aLength the number of code units written;
   *                must not exceed Length()
   * @param aAllowShrinking whether the string is
   *                        allowed to attempt to
   *                        allocate a smaller buffer
   *                        for its content and copy
   *                        the data over.
   */
  void Finish(size_type aLength, bool aAllowShrinking) {
    MOZ_ASSERT(mString);
    MOZ_ASSERT(aLength <= mCapacity);
    if (!aLength) {
      // Truncate is safe even when the string is in an invalid state
      mString->Truncate();
      mString = nullptr;
      return;
    }
    if (aAllowShrinking) {
      mozilla::Unused << mString->StartBulkWriteImpl(aLength, aLength, true);
    }
    mString->FinishBulkWriteImpl(aLength);
    mString = nullptr;
  }

  BulkWriteHandle(BulkWriteHandle&& aOther)
      : mString(aOther.Forget()), mCapacity(aOther.mCapacity) {}

  ~BulkWriteHandle() {
    if (!mString || !mCapacity) {
      return;
    }
    // The old zero terminator may be gone by now, so we need
    // to write a new one somewhere and make length match.
    // We can use a length between 1 and self.capacity.
    // The contents of the string can be partially uninitialized
    // or partially initialized in a way that would be dangerous
    // if parsed by some recipient. It's prudent to write something
    // same as the contents of the string. U+FFFD is the safest
    // placeholder, but when it doesn't fit, let's use ASCII
    // substitute. Merely truncating the string to a zero-length
    // string might be dangerous in some scenarios. See
    // https://www.unicode.org/reports/tr36/#Substituting_for_Ill_Formed_Subsequences
    // for closely related scenario.
    auto ptr = Elements();
    // Cast the pointer below to silence warnings
    if (sizeof(T) == 1) {
      unsigned char* charPtr = reinterpret_cast<unsigned char*>(ptr);
      if (mCapacity >= 3) {
        *charPtr++ = 0xEF;
        *charPtr++ = 0xBF;
        *charPtr++ = 0xBD;
        mString->mLength = 3;
      } else {
        *charPtr++ = 0x1A;
        mString->mLength = 1;
      }
      *charPtr = 0;
    } else if (sizeof(T) == 2) {
      char16_t* charPtr = reinterpret_cast<char16_t*>(ptr);
      *charPtr++ = 0xFFFD;
      *charPtr = 0;
      mString->mLength = 1;
    } else {
      MOZ_ASSERT_UNREACHABLE("Only 8-bit and 16-bit code units supported.");
    }
  }

  BulkWriteHandle() = delete;
  BulkWriteHandle(const BulkWriteHandle&) = delete;
  BulkWriteHandle& operator=(const BulkWriteHandle&) = delete;

 private:
  BulkWriteHandle(nsTSubstring<T>* aString, size_type aCapacity)
      : mString(aString), mCapacity(aCapacity) {}

  nsTSubstring<T>* Forget() {
    auto string = mString;
    mString = nullptr;
    return string;
  }

  nsTSubstring<T>* mString;  // nullptr upon finish
  size_type mCapacity;
};

}  // namespace mozilla

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
class nsTSubstring : public mozilla::detail::nsTStringRepr<T> {
  friend class mozilla::BulkWriteHandle<T>;
  friend class nsStringBuffer;

 public:
  typedef nsTSubstring<T> self_type;

  typedef nsTString<T> string_type;

  typedef typename mozilla::detail::nsTStringRepr<T> base_string_type;
  typedef typename base_string_type::substring_type substring_type;

  typedef typename base_string_type::fallible_t fallible_t;

  typedef typename base_string_type::char_type char_type;
  typedef typename base_string_type::char_traits char_traits;
  typedef
      typename base_string_type::incompatible_char_type incompatible_char_type;

  typedef typename base_string_type::substring_tuple_type substring_tuple_type;

  typedef typename base_string_type::const_iterator const_iterator;
  typedef typename base_string_type::iterator iterator;

  typedef typename base_string_type::comparator_type comparator_type;

  typedef typename base_string_type::const_char_iterator const_char_iterator;

  typedef typename base_string_type::string_view string_view;

  typedef typename base_string_type::index_type index_type;
  typedef typename base_string_type::size_type size_type;

  // These are only for internal use within the string classes:
  typedef typename base_string_type::DataFlags DataFlags;
  typedef typename base_string_type::ClassFlags ClassFlags;
  typedef typename base_string_type::LengthStorage LengthStorage;

  // this acts like a virtual destructor
  ~nsTSubstring() { Finalize(); }

  /**
   * writing iterators
   *
   * BeginWriting() makes the string mutable (if it isn't
   * already) and returns (or writes into an outparam) a
   * pointer that provides write access to the string's buffer.
   *
   * Note: Consider if BulkWrite() suits your use case better
   * than BeginWriting() combined with SetLength().
   *
   * Note: Strings autoconvert into writable mozilla::Span,
   * which may suit your use case better than calling
   * BeginWriting() directly.
   *
   * When writing via the pointer obtained from BeginWriting(),
   * you are allowed to write at most the number of code units
   * indicated by Length() or, alternatively, write up to, but
   * not including, the position indicated by EndWriting().
   *
   * In particular, calling SetCapacity() does not affect what
   * the above paragraph says.
   */

  iterator BeginWriting() {
    if (!EnsureMutable()) {
      AllocFailed(base_string_type::mLength);
    }

    return base_string_type::mData;
  }

  iterator BeginWriting(const fallible_t&) {
    return EnsureMutable() ? base_string_type::mData : iterator(0);
  }

  iterator EndWriting() {
    if (!EnsureMutable()) {
      AllocFailed(base_string_type::mLength);
    }

    return base_string_type::mData + base_string_type::mLength;
  }

  iterator EndWriting(const fallible_t&) {
    return EnsureMutable()
               ? (base_string_type::mData + base_string_type::mLength)
               : iterator(0);
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
   * assignment
   */

  void NS_FASTCALL Assign(char_type aChar);
  [[nodiscard]] bool NS_FASTCALL Assign(char_type aChar, const fallible_t&);

  void NS_FASTCALL Assign(const char_type* aData,
                          size_type aLength = size_type(-1));
  [[nodiscard]] bool NS_FASTCALL Assign(const char_type* aData,
                                        const fallible_t&);
  [[nodiscard]] bool NS_FASTCALL Assign(const char_type* aData,
                                        size_type aLength, const fallible_t&);

  void NS_FASTCALL Assign(const self_type&);
  [[nodiscard]] bool NS_FASTCALL Assign(const self_type&, const fallible_t&);

  void NS_FASTCALL Assign(self_type&&);
  [[nodiscard]] bool NS_FASTCALL Assign(self_type&&, const fallible_t&);

  void NS_FASTCALL Assign(const substring_tuple_type&);
  [[nodiscard]] bool NS_FASTCALL Assign(const substring_tuple_type&,
                                        const fallible_t&);

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void Assign(char16ptr_t aData) {
    Assign(static_cast<const char16_t*>(aData));
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void Assign(char16ptr_t aData, size_type aLength) {
    Assign(static_cast<const char16_t*>(aData), aLength);
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  [[nodiscard]] bool Assign(char16ptr_t aData, size_type aLength,
                            const fallible_t& aFallible) {
    return Assign(static_cast<const char16_t*>(aData), aLength, aFallible);
  }
#endif

  void NS_FASTCALL AssignASCII(const char* aData, size_type aLength);
  [[nodiscard]] bool NS_FASTCALL AssignASCII(const char* aData,
                                             size_type aLength,
                                             const fallible_t&);

  void NS_FASTCALL AssignASCII(const char* aData) {
    AssignASCII(aData, strlen(aData));
  }
  [[nodiscard]] bool NS_FASTCALL AssignASCII(const char* aData,
                                             const fallible_t& aFallible) {
    return AssignASCII(aData, strlen(aData), aFallible);
  }

  // AssignLiteral must ONLY be called with an actual literal string, or
  // a character array *constant* of static storage duration declared
  // without an explicit size and with an initializer that is a string
  // literal or is otherwise null-terminated.
  // Use Assign or AssignASCII for other character array variables.
  //
  // This method does not need a fallible version, because it uses the
  // POD buffer of the literal as the string's buffer without allocating.
  // The literal does not need to be ASCII. If this a 16-bit string, this
  // method takes a u"" literal. (The overload on 16-bit strings that takes
  // a "" literal takes only ASCII.)
  template <int N>
  void AssignLiteral(const char_type (&aStr)[N]) {
    AssignLiteral(aStr, N - 1);
  }

  // AssignLiteral must ONLY be called with an actual literal string, or
  // a char array *constant* declared without an explicit size and with an
  // initializer that is a string literal or is otherwise null-terminated.
  // Use AssignASCII for other char array variables.
  //
  // This method takes an 8-bit (ASCII-only!) string that is expanded
  // into a 16-bit string at run time causing a run-time allocation.
  // To avoid the run-time allocation (at the cost of the literal
  // taking twice the size in the binary), use the above overload that
  // takes a u"" string instead. Using the overload that takes a u""
  // literal is generally preferred when working with 16-bit strings.
  //
  // There is not a fallible version of this method because it only really
  // applies to small allocations that we wouldn't want to check anyway.
  template <int N, typename Q = T,
            typename EnableIfChar16 = typename mozilla::Char16OnlyT<Q>>
  void AssignLiteral(const incompatible_char_type (&aStr)[N]) {
    AssignASCII(aStr, N - 1);
  }

  self_type& operator=(char_type aChar) {
    Assign(aChar);
    return *this;
  }
  self_type& operator=(const char_type* aData) {
    Assign(aData);
    return *this;
  }
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  self_type& operator=(char16ptr_t aData) {
    Assign(aData);
    return *this;
  }
#endif
  self_type& operator=(const self_type& aStr) {
    Assign(aStr);
    return *this;
  }
  self_type& operator=(self_type&& aStr) {
    Assign(std::move(aStr));
    return *this;
  }
  self_type& operator=(const substring_tuple_type& aTuple) {
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
  [[nodiscard]] bool NS_FASTCALL Replace(index_type aCutStart,
                                         size_type aCutLength, char_type aChar,
                                         const fallible_t&);
  void NS_FASTCALL Replace(index_type aCutStart, size_type aCutLength,
                           const char_type* aData,
                           size_type aLength = size_type(-1));
  [[nodiscard]] bool NS_FASTCALL Replace(index_type aCutStart,
                                         size_type aCutLength,
                                         const char_type* aData,
                                         size_type aLength, const fallible_t&);
  void Replace(index_type aCutStart, size_type aCutLength,
               const self_type& aStr) {
    Replace(aCutStart, aCutLength, aStr.Data(), aStr.Length());
  }
  [[nodiscard]] bool Replace(index_type aCutStart, size_type aCutLength,
                             const self_type& aStr,
                             const fallible_t& aFallible) {
    return Replace(aCutStart, aCutLength, aStr.Data(), aStr.Length(),
                   aFallible);
  }
  void NS_FASTCALL Replace(index_type aCutStart, size_type aCutLength,
                           const substring_tuple_type& aTuple);

  // ReplaceLiteral must ONLY be called with an actual literal string, or
  // a character array *constant* of static storage duration declared
  // without an explicit size and with an initializer that is a string
  // literal or is otherwise null-terminated.
  // Use Replace for other character array variables.
  template <int N>
  void ReplaceLiteral(index_type aCutStart, size_type aCutLength,
                      const char_type (&aStr)[N]) {
    ReplaceLiteral(aCutStart, aCutLength, aStr, N - 1);
  }

  /**
   * |Left|, |Mid|, and |Right| are annoying signatures that seem better almost
   * any _other_ way than they are now.  Consider these alternatives
   *
   * // ...a member function that returns a |Substring|
   * aWritable = aReadable.Left(17);
   * // ...a global function that returns a |Substring|
   * aWritable = Left(aReadable, 17);
   * // ...a global function that does the assignment
   * Left(aReadable, 17, aWritable);
   *
   * as opposed to the current signature
   *
   * // ...a member function that does the assignment
   * aReadable.Left(aWritable, 17);
   *
   * or maybe just stamping them out in favor of |Substring|, they are just
   * duplicate functionality
   *
   * aWritable = Substring(aReadable, 0, 17);
   */
  size_type Mid(self_type& aResult, index_type aStartPos,
                size_type aCount) const;

  size_type Left(self_type& aResult, size_type aCount) const {
    return Mid(aResult, 0, aCount);
  }

  size_type Right(self_type& aResult, size_type aCount) const {
    aCount = XPCOM_MIN(this->Length(), aCount);
    return Mid(aResult, this->mLength - aCount, aCount);
  }

  /**
   *  This method strips whitespace throughout the string.
   */
  void StripWhitespace();
  bool StripWhitespace(const fallible_t&);

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
   * swaps occurence of 1 string for another
   */
  void ReplaceChar(char_type aOldChar, char_type aNewChar);
  void ReplaceChar(const string_view& aSet, char_type aNewChar);

  /**
   * Replace all occurrences of aTarget with aNewValue.
   * The complexity of this function is O(n+m), n being the length of the string
   * and m being the length of aNewValue.
   */
  void ReplaceSubstring(const self_type& aTarget, const self_type& aNewValue);
  void ReplaceSubstring(const char_type* aTarget, const char_type* aNewValue);
  [[nodiscard]] bool ReplaceSubstring(const self_type& aTarget,
                                      const self_type& aNewValue,
                                      const fallible_t&);
  [[nodiscard]] bool ReplaceSubstring(const char_type* aTarget,
                                      const char_type* aNewValue,
                                      const fallible_t&);

  /**
   *  This method trims characters found in aSet from either end of the
   *  underlying string.
   *
   *  @param   aSet -- contains chars to be trimmed from both ends
   *  @param   aTrimLeading
   *  @param   aTrimTrailing
   *  @param   aIgnoreQuotes -- if true, causes surrounding quotes to be ignored
   *  @return  this
   */
  void Trim(const std::string_view& aSet, bool aTrimLeading = true,
            bool aTrimTrailing = true, bool aIgnoreQuotes = false);

  /**
   *  This method strips whitespace from string.
   *  You can control whether whitespace is yanked from start and end of
   *  string as well.
   *
   *  @param   aTrimLeading controls stripping of leading ws
   *  @param   aTrimTrailing controls stripping of trailing ws
   */
  void CompressWhitespace(bool aTrimLeading = true, bool aTrimTrailing = true);

  void Append(char_type aChar);

  [[nodiscard]] bool Append(char_type aChar, const fallible_t& aFallible);

  void Append(const char_type* aData, size_type aLength = size_type(-1));

  [[nodiscard]] bool Append(const char_type* aData, size_type aLength,
                            const fallible_t& aFallible);

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void Append(char16ptr_t aData, size_type aLength = size_type(-1)) {
    Append(static_cast<const char16_t*>(aData), aLength);
  }
#endif

  void Append(const self_type& aStr);

  [[nodiscard]] bool Append(const self_type& aStr, const fallible_t& aFallible);

  void Append(const substring_tuple_type& aTuple);

  [[nodiscard]] bool Append(const substring_tuple_type& aTuple,
                            const fallible_t& aFallible);

  void AppendASCII(const char* aData, size_type aLength = size_type(-1));

  [[nodiscard]] bool AppendASCII(const char* aData,
                                 const fallible_t& aFallible);

  [[nodiscard]] bool AppendASCII(const char* aData, size_type aLength,
                                 const fallible_t& aFallible);

  // Appends a literal string ("" literal in the 8-bit case and u"" literal
  // in the 16-bit case) to the string.
  //
  // AppendLiteral must ONLY be called with an actual literal string, or
  // a character array *constant* of static storage duration declared
  // without an explicit size and with an initializer that is a string
  // literal or is otherwise null-terminated.
  // Use Append or AppendASCII for other character array variables.
  template <int N>
  void AppendLiteral(const char_type (&aStr)[N]) {
    // The case where base_string_type::mLength is zero is intentionally
    // left unoptimized (could be optimized as call to AssignLiteral),
    // because it's rare/nonexistent. If you add that optimization,
    // please be sure to also check that
    // !(base_string_type::mDataFlags & DataFlags::REFCOUNTED)
    // to avoid undoing the effects of SetCapacity().
    Append(aStr, N - 1);
  }

  template <int N>
  void AppendLiteral(const char_type (&aStr)[N], const fallible_t& aFallible) {
    // The case where base_string_type::mLength is zero is intentionally
    // left unoptimized (could be optimized as call to AssignLiteral),
    // because it's rare/nonexistent. If you add that optimization,
    // please be sure to also check that
    // !(base_string_type::mDataFlags & DataFlags::REFCOUNTED)
    // to avoid undoing the effects of SetCapacity().
    return Append(aStr, N - 1, aFallible);
  }

  // Only enable for T = char16_t
  //
  // Appends an 8-bit literal string ("" literal) to a 16-bit string by
  // expanding it. The literal must only contain ASCII.
  //
  // Using u"" literals with 16-bit strings is generally preferred.
  template <int N, typename Q = T,
            typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void AppendLiteral(const incompatible_char_type (&aStr)[N]) {
    AppendASCII(aStr, N - 1);
  }

  // Only enable for T = char16_t
  template <int N, typename Q = T,
            typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  [[nodiscard]] bool AppendLiteral(const incompatible_char_type (&aStr)[N],
                                   const fallible_t& aFallible) {
    return AppendASCII(aStr, N - 1, aFallible);
  }

  /**
   * Append a formatted string to the current string. Uses the
   * standard printf format codes.  This uses NSPR formatting, which will be
   * locale-aware for floating-point values.  You probably don't want to use
   * this with floating-point values as a result.
   */
  void AppendPrintf(const char* aFormat, ...) MOZ_FORMAT_PRINTF(2, 3);
  void AppendVprintf(const char* aFormat, va_list aAp) MOZ_FORMAT_PRINTF(2, 0);
  void AppendInt(int32_t aInteger) { AppendIntDec(aInteger); }
  void AppendInt(int32_t aInteger, int aRadix) {
    if (aRadix == 10) {
      AppendIntDec(aInteger);
    } else if (aRadix == 8) {
      AppendIntOct(static_cast<uint32_t>(aInteger));
    } else {
      AppendIntHex(static_cast<uint32_t>(aInteger));
    }
  }
  void AppendInt(uint32_t aInteger) { AppendIntDec(aInteger); }
  void AppendInt(uint32_t aInteger, int aRadix) {
    if (aRadix == 10) {
      AppendIntDec(aInteger);
    } else if (aRadix == 8) {
      AppendIntOct(aInteger);
    } else {
      AppendIntHex(aInteger);
    }
  }
  void AppendInt(int64_t aInteger) { AppendIntDec(aInteger); }
  void AppendInt(int64_t aInteger, int aRadix) {
    if (aRadix == 10) {
      AppendIntDec(aInteger);
    } else if (aRadix == 8) {
      AppendIntOct(static_cast<uint64_t>(aInteger));
    } else {
      AppendIntHex(static_cast<uint64_t>(aInteger));
    }
  }
  void AppendInt(uint64_t aInteger) { AppendIntDec(aInteger); }
  void AppendInt(uint64_t aInteger, int aRadix) {
    if (aRadix == 10) {
      AppendIntDec(aInteger);
    } else if (aRadix == 8) {
      AppendIntOct(aInteger);
    } else {
      AppendIntHex(aInteger);
    }
  }

 private:
  void AppendIntDec(int32_t);
  void AppendIntDec(uint32_t);
  void AppendIntOct(uint32_t);
  void AppendIntHex(uint32_t);
  void AppendIntDec(int64_t);
  void AppendIntDec(uint64_t);
  void AppendIntOct(uint64_t);
  void AppendIntHex(uint64_t);

 public:
  /**
   * Append the given float to this string
   */
  void NS_FASTCALL AppendFloat(float aFloat);
  void NS_FASTCALL AppendFloat(double aFloat);

  self_type& operator+=(char_type aChar) {
    Append(aChar);
    return *this;
  }
  self_type& operator+=(const char_type* aData) {
    Append(aData);
    return *this;
  }
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  self_type& operator+=(char16ptr_t aData) {
    Append(aData);
    return *this;
  }
#endif
  self_type& operator+=(const self_type& aStr) {
    Append(aStr);
    return *this;
  }
  self_type& operator+=(const substring_tuple_type& aTuple) {
    Append(aTuple);
    return *this;
  }

  void Insert(char_type aChar, index_type aPos) { Replace(aPos, 0, aChar); }
  void Insert(const char_type* aData, index_type aPos,
              size_type aLength = size_type(-1)) {
    Replace(aPos, 0, aData, aLength);
  }
#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  void Insert(char16ptr_t aData, index_type aPos,
              size_type aLength = size_type(-1)) {
    Insert(static_cast<const char16_t*>(aData), aPos, aLength);
  }
#endif
  void Insert(const self_type& aStr, index_type aPos) {
    Replace(aPos, 0, aStr);
  }
  void Insert(const substring_tuple_type& aTuple, index_type aPos) {
    Replace(aPos, 0, aTuple);
  }

  // InsertLiteral must ONLY be called with an actual literal string, or
  // a character array *constant* of static storage duration declared
  // without an explicit size and with an initializer that is a string
  // literal or is otherwise null-terminated.
  // Use Insert for other character array variables.
  template <int N>
  void InsertLiteral(const char_type (&aStr)[N], index_type aPos) {
    ReplaceLiteral(aPos, 0, aStr, N - 1);
  }

  void Cut(index_type aCutStart, size_type aCutLength) {
    Replace(aCutStart, aCutLength, char_traits::sEmptyBuffer, 0);
  }

  nsTSubstringSplitter<T> Split(const char_type aChar) const;

  /**
   * buffer sizing
   */

  /**
   * Attempts to set the capacity to the given size in number of
   * code units without affecting the length of the string in
   * order to avoid reallocation during a subsequent sequence of
   * appends.
   *
   * This method is appropriate to use before a sequence of multiple
   * operations from the following list (without operations that are
   * not on the list between the SetCapacity() call and operations
   * from the list):
   *
   * Append()
   * AppendASCII()
   * AppendLiteral() (except if the string is empty: bug 1487606)
   * AppendPrintf()
   * AppendInt()
   * AppendFloat()
   * LossyAppendUTF16toASCII()
   * AppendASCIItoUTF16()
   *
   * DO NOT call SetCapacity() if the subsequent operations on the
   * string do not meet the criteria above. Operations that undo
   * the benefits of SetCapacity() include but are not limited to:
   *
   * SetLength()
   * Truncate()
   * Assign()
   * AssignLiteral()
   * Adopt()
   * CopyASCIItoUTF16()
   * LossyCopyUTF16toASCII()
   * AppendUTF16toUTF8()
   * AppendUTF8toUTF16()
   * CopyUTF16toUTF8()
   * CopyUTF8toUTF16()
   *
   * If your string is an nsAuto[C]String and you are calling
   * SetCapacity() with a constant N, please instead declare the
   * string as nsAuto[C]StringN<N+1> without calling SetCapacity().
   *
   * There is no need to include room for the null terminator: it is
   * the job of the string class.
   *
   * Note: Calling SetCapacity() does not give you permission to
   * use the pointer obtained from BeginWriting() to write
   * past the current length (as returned by Length()) of the
   * string. Please use either BulkWrite() or SetLength()
   * instead.
   *
   * Note: SetCapacity() won't make the string shorter if
   * called with an argument smaller than the length of the
   * string.
   *
   * Note: You must not use previously obtained iterators
   * or spans after calling SetCapacity().
   */
  void NS_FASTCALL SetCapacity(size_type aNewCapacity);
  [[nodiscard]] bool NS_FASTCALL SetCapacity(size_type aNewCapacity,
                                             const fallible_t&);

  /**
   * Changes the logical length of the string, potentially
   * allocating a differently-sized buffer for the string.
   *
   * When making the string shorter, this method never
   * reports allocation failure.
   *
   * Exposes uninitialized memory if the string got longer.
   *
   * If called with the argument 0, releases the
   * heap-allocated buffer, if any. (But the no-argument
   * overload of Truncate() is a more idiomatic and efficient
   * option than SetLength(0).)
   *
   * Note: You must not use previously obtained iterators
   * or spans after calling SetLength().
   */
  void NS_FASTCALL SetLength(size_type aNewLength);
  [[nodiscard]] bool NS_FASTCALL SetLength(size_type aNewLength,
                                           const fallible_t&);

  /**
   * Like SetLength() but asserts in that the string
   * doesn't become longer. Never fails, so doesn't need a
   * fallible variant.
   *
   * Note: You must not use previously obtained iterators
   * or spans after calling Truncate().
   */
  void Truncate(size_type aNewLength) {
    MOZ_RELEASE_ASSERT(aNewLength <= base_string_type::mLength,
                       "Truncate cannot make string longer");
    mozilla::DebugOnly<bool> success = SetLength(aNewLength, mozilla::fallible);
    MOZ_ASSERT(success);
  }

  /**
   * A more efficient overload for Truncate(0). Releases the
   * heap-allocated buffer if any.
   */
  void Truncate();

  /**
   * buffer access
   */

  /**
   * Get a const pointer to the string's internal buffer.  The caller
   * MUST NOT modify the characters at the returned address.
   *
   * @returns The length of the buffer in characters.
   */
  inline size_type GetData(const char_type** aData) const {
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
  size_type GetMutableData(char_type** aData,
                           size_type aNewLen = size_type(-1)) {
    if (!EnsureMutable(aNewLen)) {
      AllocFailed(aNewLen == size_type(-1) ? base_string_type::Length()
                                           : aNewLen);
    }

    *aData = base_string_type::mData;
    return base_string_type::Length();
  }

  size_type GetMutableData(char_type** aData, size_type aNewLen,
                           const fallible_t&) {
    if (!EnsureMutable(aNewLen)) {
      *aData = nullptr;
      return 0;
    }

    *aData = base_string_type::mData;
    return base_string_type::mLength;
  }

#if defined(MOZ_USE_CHAR16_WRAPPER)
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  size_type GetMutableData(wchar_t** aData, size_type aNewLen = size_type(-1)) {
    return GetMutableData(reinterpret_cast<char16_t**>(aData), aNewLen);
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  size_type GetMutableData(wchar_t** aData, size_type aNewLen,
                           const fallible_t& aFallible) {
    return GetMutableData(reinterpret_cast<char16_t**>(aData), aNewLen,
                          aFallible);
  }
#endif

  mozilla::Span<char_type> GetMutableData(size_type aNewLen = size_type(-1)) {
    if (!EnsureMutable(aNewLen)) {
      AllocFailed(aNewLen == size_type(-1) ? base_string_type::Length()
                                           : aNewLen);
    }

    return mozilla::Span{base_string_type::mData, base_string_type::Length()};
  }

  mozilla::Maybe<mozilla::Span<char_type>> GetMutableData(size_type aNewLen,
                                                          const fallible_t&) {
    if (!EnsureMutable(aNewLen)) {
      return mozilla::Nothing();
    }
    return Some(
        mozilla::Span{base_string_type::mData, base_string_type::Length()});
  }

  /**
   * Span integration
   */

  operator mozilla::Span<const char_type>() const {
    return mozilla::Span{base_string_type::BeginReading(),
                         base_string_type::Length()};
  }

  void Append(mozilla::Span<const char_type> aSpan) {
    Append(aSpan.Elements(), aSpan.Length());
  }

  [[nodiscard]] bool Append(mozilla::Span<const char_type> aSpan,
                            const fallible_t& aFallible) {
    return Append(aSpan.Elements(), aSpan.Length(), aFallible);
  }

  void NS_FASTCALL AssignASCII(mozilla::Span<const char> aData) {
    AssignASCII(aData.Elements(), aData.Length());
  }
  [[nodiscard]] bool NS_FASTCALL AssignASCII(mozilla::Span<const char> aData,
                                             const fallible_t& aFallible) {
    return AssignASCII(aData.Elements(), aData.Length(), aFallible);
  }

  void AppendASCII(mozilla::Span<const char> aData) {
    AppendASCII(aData.Elements(), aData.Length());
  }

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  operator mozilla::Span<const uint8_t>() const {
    return mozilla::Span{
        reinterpret_cast<const uint8_t*>(base_string_type::BeginReading()),
        base_string_type::Length()};
  }

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  void Append(mozilla::Span<const uint8_t> aSpan) {
    Append(reinterpret_cast<const char*>(aSpan.Elements()), aSpan.Length());
  }

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  [[nodiscard]] bool Append(mozilla::Span<const uint8_t> aSpan,
                            const fallible_t& aFallible) {
    return Append(reinterpret_cast<const char*>(aSpan.Elements()),
                  aSpan.Length(), aFallible);
  }

  void Insert(mozilla::Span<const char_type> aSpan, index_type aPos) {
    Insert(aSpan.Elements(), aPos, aSpan.Length());
  }

  /**
   * string data is never null, but can be marked void.  if true, the
   * string will be truncated.  @see nsTSubstring::IsVoid
   */

  void NS_FASTCALL SetIsVoid(bool);

  /**
   * If the string uses a shared buffer, this method
   * clears the pointer without releasing the buffer.
   */
  void ForgetSharedBuffer() {
    if (base_string_type::mDataFlags & DataFlags::REFCOUNTED) {
      SetToEmptyBuffer();
    }
  }

 protected:
  void AssertValid() {
    MOZ_DIAGNOSTIC_ASSERT(!(this->mClassFlags & ClassFlags::INVALID_MASK));
    MOZ_DIAGNOSTIC_ASSERT(!(this->mDataFlags & DataFlags::INVALID_MASK));
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
      : base_string_type(nullptr, 0, DataFlags(0), ClassFlags(0)) {
    AssertValid();
    Assign(aTuple);
  }

  size_t SizeOfExcludingThisIfUnshared(
      mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThisIfUnshared(
      mozilla::MallocSizeOf aMallocSizeOf) const;

  /**
   * WARNING: Only use these functions if you really know what you are
   * doing, because they can easily lead to double-counting strings.  If
   * you do use them, please explain clearly in a comment why it's safe
   * and won't lead to double-counting.
   */
  size_t SizeOfExcludingThisEvenIfShared(
      mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThisEvenIfShared(
      mozilla::MallocSizeOf aMallocSizeOf) const;

  template <class N>
  void NS_ABORT_OOM(T) {
    struct never {};  // a compiler-friendly way to do static_assert(false)
    static_assert(
        std::is_same_v<N, never>,
        "In string classes, use AllocFailed to account for sizeof(char_type). "
        "Use the global ::NS_ABORT_OOM if you really have a count of bytes.");
  }

  MOZ_ALWAYS_INLINE void AllocFailed(size_t aLength) {
    ::NS_ABORT_OOM(aLength * sizeof(char_type));
  }

 protected:
  // default initialization
  nsTSubstring()
      : base_string_type(char_traits::sEmptyBuffer, 0, DataFlags::TERMINATED,
                         ClassFlags(0)) {
    AssertValid();
  }

  // copy-constructor, constructs as dependent on given object
  // (NOTE: this is for internal use only)
  nsTSubstring(const self_type& aStr)
      : base_string_type(aStr.base_string_type::mData,
                         aStr.base_string_type::mLength,
                         aStr.base_string_type::mDataFlags &
                             (DataFlags::TERMINATED | DataFlags::VOIDED),
                         ClassFlags(0)) {
    AssertValid();
  }

  // initialization with ClassFlags
  explicit nsTSubstring(ClassFlags aClassFlags)
      : base_string_type(char_traits::sEmptyBuffer, 0, DataFlags::TERMINATED,
                         aClassFlags) {
    AssertValid();
  }

  /**
   * allows for direct initialization of a nsTSubstring object.
   */
  nsTSubstring(char_type* aData, size_type aLength, DataFlags aDataFlags,
               ClassFlags aClassFlags)
#if defined(NS_BUILD_REFCNT_LOGGING)
#  define XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
      ;
#else
#  undef XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
      : base_string_type(aData, aLength, aDataFlags, aClassFlags) {
    AssertValid();
  }
#endif /* NS_BUILD_REFCNT_LOGGING */

  void SetToEmptyBuffer() {
    base_string_type::mData = char_traits::sEmptyBuffer;
    base_string_type::mLength = 0;
    base_string_type::mDataFlags = DataFlags::TERMINATED;
    AssertValid();
  }

  void SetData(char_type* aData, LengthStorage aLength, DataFlags aDataFlags) {
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

 public:
  /**
   * Starts a low-level write transaction to the string.
   *
   * Prepares the string for mutation such that the capacity
   * of the string is at least aCapacity. The returned handle
   * exposes the actual, potentially larger, capacity.
   *
   * If meeting the capacity or mutability requirement requires
   * reallocation, aPrefixToPreserve code units are copied from the
   * start of the old buffer to the start of the new buffer.
   * aPrefixToPreserve must not be greater than the string's current
   * length or greater than aCapacity.
   *
   * aAllowShrinking indicates whether an allocation may be
   * performed when the string is already mutable and the requested
   * capacity is smaller than the current capacity.
   *
   * If this method returns successfully, you must not access
   * the string except through the returned BulkWriteHandle
   * until either the BulkWriteHandle goes out of scope or
   * you call Finish() on the BulkWriteHandle.
   *
   * Compared to SetLength() and BeginWriting(), this more
   * complex API accomplishes two things:
   *  1) It exposes the actual capacity which may be larger
   *     than the requested capacity, which is useful in some
   *     multi-step write operations that don't allocate for
   *     the worst case up front.
   *  2) It writes the zero terminator after the string
   *     content has been written, which results in a
   *     cache-friendly linear write pattern.
   */
  mozilla::Result<mozilla::BulkWriteHandle<T>, nsresult> NS_FASTCALL BulkWrite(
      size_type aCapacity, size_type aPrefixToPreserve, bool aAllowShrinking);

  /**
   * THIS IS NOT REALLY A PUBLIC METHOD! DO NOT CALL FROM OUTSIDE
   * THE STRING IMPLEMENTATION. (It's public only because friend
   * declarations don't allow extern or static and this needs to
   * be called from Rust FFI glue.)
   *
   * Prepares mData to be mutated such that the capacity of the string
   * (not counting the zero-terminator) is at least aCapacity.
   * Returns the actual capacity, which may be larger than what was
   * requested or Err(NS_ERROR_OUT_OF_MEMORY) on allocation failure.
   *
   * mLength is ignored by this method. If the buffer is reallocated,
   * aUnitsToPreserve specifies how many code units to copy over to
   * the new buffer. The old buffer is freed if applicable.
   *
   * Unless the return value is Err(NS_ERROR_OUT_OF_MEMORY) to signal
   * failure or 0 to signal that the string has been set to
   * the special empty state, this method leaves the string in an
   * invalid state! The caller is responsible for calling
   * FinishBulkWrite() (or in Rust calling
   * nsA[C]StringBulkWriteHandle::finish()), which put the string
   * into a valid state by setting mLength and zero-terminating.
   * This method sets the flag to claim that the string is
   * zero-terminated before it actually is.
   *
   * Once this method has been called and before FinishBulkWrite()
   * has been called, only accessing mData or calling this method
   * again are valid operations. Do not call any other methods or
   * access other fields between calling this method and
   * FinishBulkWrite().
   *
   * @param aCapacity The requested capacity. The return value
   *                  will be greater than or equal to this value.
   * @param aPrefixToPreserve The number of code units at the start
   *                          of the old buffer to copy into the
   *                          new buffer.
   * @parem aAllowShrinking If true, an allocation may be performed
   *                        if the requested capacity is smaller
   *                        than the current capacity.
   * @param aSuffixLength The length, in code units, of a suffix
   *                      to move.
   * @param aOldSuffixStart The old start index of the suffix to
   *                        move.
   * @param aNewSuffixStart The new start index of the suffix to
   *                        move.
   *
   */
  mozilla::Result<size_type, nsresult> NS_FASTCALL StartBulkWriteImpl(
      size_type aCapacity, size_type aPrefixToPreserve = 0,
      bool aAllowShrinking = true, size_type aSuffixLength = 0,
      size_type aOldSuffixStart = 0, size_type aNewSuffixStart = 0);

 private:
  void AssignOwned(self_type&& aStr);
  bool AssignNonDependent(const substring_tuple_type& aTuple,
                          size_type aTupleLength,
                          const mozilla::fallible_t& aFallible);

  /**
   * Do not call this except from within FinishBulkWriteImpl() and
   * SetCapacity().
   */
  MOZ_ALWAYS_INLINE void NS_FASTCALL
  FinishBulkWriteImplImpl(LengthStorage aLength) {
    base_string_type::mData[aLength] = char_type(0);
    base_string_type::mLength = aLength;
#ifdef DEBUG
    // ifdefed in order to avoid the call to Capacity() in non-debug
    // builds.
    //
    // Our string is mutable, so Capacity() doesn't return zero.
    // Capacity() doesn't include the space for the zero terminator,
    // but we want to unitialize that slot, too. Since we start
    // counting after the zero terminator the we just wrote above,
    // we end up overwriting the space for terminator not reflected
    // in the capacity number.
    char_traits::uninitialize(
        base_string_type::mData + aLength + 1,
        XPCOM_MIN(size_t(Capacity() - aLength), kNsStringBufferMaxPoison));
#endif
  }

 protected:
  /**
   * Restores the string to a valid state after a call to StartBulkWrite()
   * that returned a non-error result. The argument to this method
   * must be less than or equal to the value returned by the most recent
   * StartBulkWrite() call.
   */
  void NS_FASTCALL FinishBulkWriteImpl(size_type aLength);

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
  [[nodiscard]] bool ReplacePrep(index_type aCutStart, size_type aCutLength,
                                 size_type aNewLength);

  [[nodiscard]] bool NS_FASTCALL ReplacePrepInternal(index_type aCutStart,
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
  [[nodiscard]] bool NS_FASTCALL
  EnsureMutable(size_type aNewLen = size_type(-1));

  void NS_FASTCALL ReplaceLiteral(index_type aCutStart, size_type aCutLength,
                                  const char_type* aData, size_type aLength);

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

#include "nsCharSeparatedTokenizer.h"
#include "nsTDependentSubstring.h"

/**
 * Span integration
 */
namespace mozilla {
Span(const nsTSubstring<char>&) -> Span<const char>;
Span(const nsTSubstring<char16_t>&) -> Span<const char16_t>;

}  // namespace mozilla

#endif
