/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

#ifndef nsReadableUtils_h___
#define nsReadableUtils_h___

/**
 * I guess all the routines in this file are all mis-named.
 * According to our conventions, they should be |NS_xxx|.
 */

#include "mozilla/Assertions.h"
#include "nsAString.h"
#include "mozilla/Tuple.h"
#include "encoding_rs_mem.h"

#include "nsTArrayForwardDeclare.h"

// Can't include mozilla/Encoding.h here. The implementation is in
// the encoding_rs crate.
extern "C" {
// Declared as uint8_t instead of char to match declaration in another header.
size_t encoding_utf8_valid_up_to(uint8_t const* buffer, size_t buffer_len);
}

// From the nsstring crate
extern "C" {
bool nsstring_fallible_append_utf8_impl(nsAString* aThis, const char* aOther,
                                        size_t aOtherLen, size_t aOldLen);

bool nsstring_fallible_append_latin1_impl(nsAString* aThis, const char* aOther,
                                          size_t aOtherLen, size_t aOldLen,
                                          bool aAllowShrinking);

bool nscstring_fallible_append_utf16_to_utf8_impl(nsACString* aThis,
                                                  const char16_t*,
                                                  size_t aOtherLen,
                                                  size_t aOldLen);

bool nscstring_fallible_append_utf16_to_latin1_lossy_impl(nsACString* aThis,
                                                          const char16_t*,
                                                          size_t aOtherLen,
                                                          size_t aOldLen,
                                                          bool aAllowShrinking);

bool nscstring_fallible_append_utf8_to_latin1_lossy_check(
    nsACString* aThis, const nsACString* aOther, size_t aOldLen);

bool nscstring_fallible_append_latin1_to_utf8_check(nsACString* aThis,
                                                    const nsACString* aOther,
                                                    size_t aOldLen);
}

/**
 * If all the code points in the input are below U+0100, converts to Latin1,
 * i.e. unsigned byte value is Unicode scalar value; not windows-1252. If
 * there are code points above U+00FF, produces garbage in a memory-safe way
 * and will likely start asserting in future debug builds. The nature of the
 * garbage depends on the CPU architecture and must not be relied upon.
 *
 * The length of aDest must be not be less than the length of aSource.
 */
inline void LossyConvertUTF16toLatin1(mozilla::Span<const char16_t> aSource,
                                      mozilla::Span<char> aDest) {
  encoding_mem_convert_utf16_to_latin1_lossy(
      aSource.Elements(), aSource.Length(), aDest.Elements(), aDest.Length());
}

/**
 * If all the code points in the input are below U+0100, converts to Latin1,
 * i.e. unsigned byte value is Unicode scalar value; not windows-1252. If
 * there are code points above U+00FF, asserts in debug builds and produces
 * garbage in memory-safe way in release builds. The nature of the garbage
 * may depend on the CPU architecture and must not be relied upon.
 *
 * The length of aDest must be not be less than the length of aSource.
 */
inline size_t LossyConvertUTF8toLatin1(mozilla::Span<const char> aSource,
                                       mozilla::Span<char> aDest) {
  return encoding_mem_convert_utf8_to_latin1_lossy(
      aSource.Elements(), aSource.Length(), aDest.Elements(), aDest.Length());
}

/**
 * Interprets unsigned byte value as Unicode scalar value (i.e. not
 * windows-1252!).
 *
 * The length of aDest must be not be less than the length of aSource.
 */
inline void ConvertLatin1toUTF16(mozilla::Span<const char> aSource,
                                 mozilla::Span<char16_t> aDest) {
  encoding_mem_convert_latin1_to_utf16(aSource.Elements(), aSource.Length(),
                                       aDest.Elements(), aDest.Length());
}

/**
 * Lone surrogates are replaced with the REPLACEMENT CHARACTER.
 *
 * The length of aDest must be at least the length of aSource times three.
 *
 * Returns the number of code units written.
 */
inline size_t ConvertUTF16toUTF8(mozilla::Span<const char16_t> aSource,
                                 mozilla::Span<char> aDest) {
  return encoding_mem_convert_utf16_to_utf8(
      aSource.Elements(), aSource.Length(), aDest.Elements(), aDest.Length());
}

/**
 * Lone surrogates are replaced with the REPLACEMENT CHARACTER.
 *
 * The conversion is guaranteed to be complete if the length of aDest is
 * at least the length of aSource times three.
 *
 * The output is always valid UTF-8 ending on scalar value boundary
 * even in the case of partial conversion.
 *
 * Returns the number of code units read and the number of code
 * units written.
 */
inline mozilla::Tuple<size_t, size_t> ConvertUTF16toUTF8Partial(
    mozilla::Span<const char16_t> aSource, mozilla::Span<char> aDest) {
  size_t srcLen = aSource.Length();
  size_t dstLen = aDest.Length();
  encoding_mem_convert_utf16_to_utf8_partial(aSource.Elements(), &srcLen,
                                             aDest.Elements(), &dstLen);
  return mozilla::MakeTuple(srcLen, dstLen);
}

/**
 * Malformed byte sequences are replaced with the REPLACEMENT CHARACTER.
 *
 * The length of aDest must at least one greater than the length of aSource.
 *
 * Returns the number of code units written.
 */
inline size_t ConvertUTF8toUTF16(mozilla::Span<const char> aSource,
                                 mozilla::Span<char16_t> aDest) {
  return encoding_mem_convert_utf8_to_utf16(
      aSource.Elements(), aSource.Length(), aDest.Elements(), aDest.Length());
}

inline size_t Distance(const nsReadingIterator<char16_t>& aStart,
                       const nsReadingIterator<char16_t>& aEnd) {
  MOZ_ASSERT(aStart.get() <= aEnd.get());
  return static_cast<size_t>(aEnd.get() - aStart.get());
}

inline size_t Distance(const nsReadingIterator<char>& aStart,
                       const nsReadingIterator<char>& aEnd) {
  MOZ_ASSERT(aStart.get() <= aEnd.get());
  return static_cast<size_t>(aEnd.get() - aStart.get());
}

// UTF-8 to UTF-16
// Invalid UTF-8 byte sequences are replaced with the REPLACEMENT CHARACTER.

inline MOZ_MUST_USE bool CopyUTF8toUTF16(mozilla::Span<const char> aSource,
                                         nsAString& aDest,
                                         const mozilla::fallible_t&) {
  return nsstring_fallible_append_utf8_impl(&aDest, aSource.Elements(),
                                            aSource.Length(), 0);
}

inline void CopyUTF8toUTF16(mozilla::Span<const char> aSource,
                            nsAString& aDest) {
  if (MOZ_UNLIKELY(!CopyUTF8toUTF16(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aSource.Length());
  }
}

inline MOZ_MUST_USE bool AppendUTF8toUTF16(mozilla::Span<const char> aSource,
                                           nsAString& aDest,
                                           const mozilla::fallible_t&) {
  return nsstring_fallible_append_utf8_impl(&aDest, aSource.Elements(),
                                            aSource.Length(), aDest.Length());
}

inline void AppendUTF8toUTF16(mozilla::Span<const char> aSource,
                              nsAString& aDest) {
  if (MOZ_UNLIKELY(!AppendUTF8toUTF16(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aDest.Length() + aSource.Length());
  }
}

// Latin1 to UTF-16
// Interpret each incoming unsigned byte value as a Unicode scalar value (not
// windows-1252!). The function names say "ASCII" instead of "Latin1" for
// legacy reasons.

inline MOZ_MUST_USE bool CopyASCIItoUTF16(mozilla::Span<const char> aSource,
                                          nsAString& aDest,
                                          const mozilla::fallible_t&) {
  return nsstring_fallible_append_latin1_impl(&aDest, aSource.Elements(),
                                              aSource.Length(), 0, true);
}

inline void CopyASCIItoUTF16(mozilla::Span<const char> aSource,
                             nsAString& aDest) {
  if (MOZ_UNLIKELY(!CopyASCIItoUTF16(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aSource.Length());
  }
}

inline MOZ_MUST_USE bool AppendASCIItoUTF16(mozilla::Span<const char> aSource,
                                            nsAString& aDest,
                                            const mozilla::fallible_t&) {
  return nsstring_fallible_append_latin1_impl(
      &aDest, aSource.Elements(), aSource.Length(), aDest.Length(), false);
}

inline void AppendASCIItoUTF16(mozilla::Span<const char> aSource,
                               nsAString& aDest) {
  if (MOZ_UNLIKELY(!AppendASCIItoUTF16(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aDest.Length() + aSource.Length());
  }
}

// UTF-16 to UTF-8
// Unpaired surrogates are replaced with the REPLACEMENT CHARACTER.

inline MOZ_MUST_USE bool CopyUTF16toUTF8(mozilla::Span<const char16_t> aSource,
                                         nsACString& aDest,
                                         const mozilla::fallible_t&) {
  return nscstring_fallible_append_utf16_to_utf8_impl(
      &aDest, aSource.Elements(), aSource.Length(), 0);
}

inline void CopyUTF16toUTF8(mozilla::Span<const char16_t> aSource,
                            nsACString& aDest) {
  if (MOZ_UNLIKELY(!CopyUTF16toUTF8(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aSource.Length());
  }
}

inline MOZ_MUST_USE bool AppendUTF16toUTF8(
    mozilla::Span<const char16_t> aSource, nsACString& aDest,
    const mozilla::fallible_t&) {
  return nscstring_fallible_append_utf16_to_utf8_impl(
      &aDest, aSource.Elements(), aSource.Length(), aDest.Length());
}

inline void AppendUTF16toUTF8(mozilla::Span<const char16_t> aSource,
                              nsACString& aDest) {
  if (MOZ_UNLIKELY(!AppendUTF16toUTF8(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aDest.Length() + aSource.Length());
  }
}

// UTF-16 to Latin1
// If all code points in the input are below U+0100, represents each scalar
// value as an unsigned byte. (This is not windows-1252!) If there are code
// points above U+00FF, memory-safely produces garbage and will likely start
// asserting in future debug builds. The nature of the garbage may differ
// based on CPU architecture and must not be relied upon. The names say
// "ASCII" instead of "Latin1" for legacy reasons.

inline MOZ_MUST_USE bool LossyCopyUTF16toASCII(
    mozilla::Span<const char16_t> aSource, nsACString& aDest,
    const mozilla::fallible_t&) {
  return nscstring_fallible_append_utf16_to_latin1_lossy_impl(
      &aDest, aSource.Elements(), aSource.Length(), 0, true);
}

inline void LossyCopyUTF16toASCII(mozilla::Span<const char16_t> aSource,
                                  nsACString& aDest) {
  if (MOZ_UNLIKELY(!LossyCopyUTF16toASCII(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aSource.Length());
  }
}

inline MOZ_MUST_USE bool LossyAppendUTF16toASCII(
    mozilla::Span<const char16_t> aSource, nsACString& aDest,
    const mozilla::fallible_t&) {
  return nscstring_fallible_append_utf16_to_latin1_lossy_impl(
      &aDest, aSource.Elements(), aSource.Length(), aDest.Length(), false);
}

inline void LossyAppendUTF16toASCII(mozilla::Span<const char16_t> aSource,
                                    nsACString& aDest) {
  if (MOZ_UNLIKELY(
          !LossyAppendUTF16toASCII(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aDest.Length() + aSource.Length());
  }
}

// Latin1 to UTF-8
// Interpret each incoming unsigned byte value as a Unicode scalar value (not
// windows-1252!).
// If the input is ASCII, the heap-allocated nsStringBuffer is shared if
// possible.

inline MOZ_MUST_USE bool CopyLatin1toUTF8(const nsACString& aSource,
                                          nsACString& aDest,
                                          const mozilla::fallible_t&) {
  return nscstring_fallible_append_latin1_to_utf8_check(&aDest, &aSource, 0);
}

inline void CopyLatin1toUTF8(const nsACString& aSource, nsACString& aDest) {
  if (MOZ_UNLIKELY(!CopyLatin1toUTF8(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aSource.Length());
  }
}

inline MOZ_MUST_USE bool AppendLatin1toUTF8(const nsACString& aSource,
                                            nsACString& aDest,
                                            const mozilla::fallible_t&) {
  return nscstring_fallible_append_latin1_to_utf8_check(&aDest, &aSource,
                                                        aDest.Length());
}

inline void AppendLatin1toUTF8(const nsACString& aSource, nsACString& aDest) {
  if (MOZ_UNLIKELY(!AppendLatin1toUTF8(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aDest.Length() + aSource.Length());
  }
}

// UTF-8 to Latin1
// If all code points in the input are below U+0100, represents each scalar
// value as an unsigned byte. (This is not windows-1252!) If there are code
// points above U+00FF, memory-safely produces garbage in release builds and
// asserts in debug builds. The nature of the garbage may differ
// based on CPU architecture and must not be relied upon.
// If the input is ASCII, the heap-allocated nsStringBuffer is shared if
// possible.

inline MOZ_MUST_USE bool LossyCopyUTF8toLatin1(const nsACString& aSource,
                                               nsACString& aDest,
                                               const mozilla::fallible_t&) {
  return nscstring_fallible_append_utf8_to_latin1_lossy_check(&aDest, &aSource,
                                                              0);
}

inline void LossyCopyUTF8toLatin1(const nsACString& aSource,
                                  nsACString& aDest) {
  if (MOZ_UNLIKELY(!LossyCopyUTF8toLatin1(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aSource.Length());
  }
}

inline MOZ_MUST_USE bool LossyAppendUTF8toLatin1(const nsACString& aSource,
                                                 nsACString& aDest,
                                                 const mozilla::fallible_t&) {
  return nscstring_fallible_append_utf8_to_latin1_lossy_check(&aDest, &aSource,
                                                              aDest.Length());
}

inline void LossyAppendUTF8toLatin1(const nsACString& aSource,
                                    nsACString& aDest) {
  if (MOZ_UNLIKELY(
          !LossyAppendUTF8toLatin1(aSource, aDest, mozilla::fallible))) {
    aDest.AllocFailed(aDest.Length() + aSource.Length());
  }
}

/**
 * Returns a new |char| buffer containing a zero-terminated copy of |aSource|.
 *
 * Allocates and returns a new |char| buffer which you must free with |free|.
 * Performs a conversion with LossyConvertUTF16toLatin1() writing into the
 * newly-allocated buffer.
 *
 * The new buffer is zero-terminated, but that may not help you if |aSource|
 * contains embedded nulls.
 *
 * @param aSource a 16-bit wide string
 * @return a new |char| buffer you must free with |free|.
 */
char* ToNewCString(const nsAString& aSource);

/**
 * Returns a new |char| buffer containing a zero-terminated copy of |aSource|.
 *
 * Allocates and returns a new |char| buffer which you must free with |free|.
 *
 * The new buffer is zero-terminated, but that may not help you if |aSource|
 * contains embedded nulls.
 *
 * @param aSource an 8-bit wide string
 * @return a new |char| buffer you must free with |free|.
 */
char* ToNewCString(const nsACString& aSource);

/**
 * Returns a new |char| buffer containing a zero-terminated copy of |aSource|.
 *
 * Allocates and returns a new |char| buffer which you must free with
 * |free|.
 * Performs an encoding conversion from a UTF-16 string to a UTF-8 string with
 * unpaired surrogates replaced with the REPLACEMENT CHARACTER copying
 * |aSource| to your new buffer.
 *
 * The new buffer is zero-terminated, but that may not help you if |aSource|
 * contains embedded nulls.
 *
 * @param aSource a UTF-16 string (made of char16_t's)
 * @param aUTF8Count the number of 8-bit units that was returned
 * @return a new |char| buffer you must free with |free|.
 */

char* ToNewUTF8String(const nsAString& aSource, uint32_t* aUTF8Count = nullptr);

/**
 * Returns a new |char16_t| buffer containing a zero-terminated copy of
 * |aSource|.
 *
 * Allocates and returns a new |char16_t| buffer which you must free with
 * |free|.
 *
 * The new buffer is zero-terminated, but that may not help you if |aSource|
 * contains embedded nulls.
 *
 * @param aSource a UTF-16 string
 * @return a new |char16_t| buffer you must free with |free|.
 */
char16_t* ToNewUnicode(const nsAString& aSource);

/**
 * Returns a new |char16_t| buffer containing a zero-terminated copy of
 * |aSource|.
 *
 * Allocates and returns a new |char16_t| buffer which you must free with
 * |free|.
 *
 * Performs an encoding conversion by 0-padding 8-bit wide characters up to
 * 16-bits wide (i.e. Latin1 to UTF-16 conversion) while copying |aSource|
 * to your new buffer.
 *
 * The new buffer is zero-terminated, but that may not help you if |aSource|
 * contains embedded nulls.
 *
 * @param aSource a Latin1 string
 * @return a new |char16_t| buffer you must free with |free|.
 */
char16_t* ToNewUnicode(const nsACString& aSource);

/**
 * Returns a new |char16_t| buffer containing a zero-terminated copy
 * of |aSource|.
 *
 * Allocates and returns a new |char| buffer which you must free with
 * |free|.  Performs an encoding conversion from UTF-8 to UTF-16
 * while copying |aSource| to your new buffer.  Malformed byte sequences
 * are replaced with the REPLACEMENT CHARACTER.
 *
 * The new buffer is zero-terminated, but that may not help you if |aSource|
 * contains embedded nulls.
 *
 * @param aSource an 8-bit wide string, UTF-8 encoded
 * @param aUTF16Count the number of 16-bit units that was returned
 * @return a new |char16_t| buffer you must free with |free|.
 *         (UTF-16 encoded)
 */
char16_t* UTF8ToNewUnicode(const nsACString& aSource,
                           uint32_t* aUTF16Count = nullptr);

/**
 * Copies |aLength| 16-bit code units from the start of |aSource| to the
 * |char16_t| buffer |aDest|.
 *
 * After this operation |aDest| is not null terminated.
 *
 * @param aSource a UTF-16 string
 * @param aSrcOffset start offset in the source string
 * @param aDest a |char16_t| buffer
 * @param aLength the number of 16-bit code units to copy
 * @return pointer to destination buffer - identical to |aDest|
 */
char16_t* CopyUnicodeTo(const nsAString& aSource, uint32_t aSrcOffset,
                        char16_t* aDest, uint32_t aLength);

/**
 * Returns |true| if |aString| contains only ASCII characters, that is,
 * characters in the range (0x00, 0x7F).
 *
 * @param aString a 16-bit wide string to scan
 */
inline bool IsASCII(mozilla::Span<const char16_t> aString) {
  return encoding_mem_is_basic_latin(aString.Elements(), aString.Length());
}

/**
 * Returns |true| if |aString| contains only ASCII characters, that is,
 * characters in the range (0x00, 0x7F).
 *
 * @param aString a 8-bit wide string to scan
 */
inline bool IsASCII(mozilla::Span<const char> aString) {
  size_t length = aString.Length();
  const char* ptr = aString.Elements();
  // For short strings, avoid the function call, since, the SIMD
  // code won't have a chance to kick in anyway.
  if (length < 16) {
    const uint8_t* uptr = reinterpret_cast<const uint8_t*>(ptr);
    uint8_t accu = 0;
    for (size_t i = 0; i < length; i++) {
      accu |= uptr[i];
    }
    return accu < 0x80U;
  }
  return encoding_mem_is_ascii(ptr, length);
}

/**
 * Returns |true| if |aString| contains only Latin1 characters, that is,
 * characters in the range (U+0000, U+00FF).
 *
 * @param aString a potentially-invalid UTF-16 string to scan
 */
inline bool IsUTF16Latin1(mozilla::Span<const char16_t> aString) {
  return encoding_mem_is_utf16_latin1(aString.Elements(), aString.Length());
}

/**
 * Returns |true| if |aString| contains only Latin1 characters, that is,
 * characters in the range (U+0000, U+00FF).
 *
 * If you know that the argument is always absolutely guaranteed to be valid
 * UTF-8, use the faster UnsafeIsValidUTF8Latin1() instead.
 *
 * @param aString potentially-invalid UTF-8 string to scan
 */
inline bool IsUTF8Latin1(mozilla::Span<const char> aString) {
  return encoding_mem_is_utf8_latin1(aString.Elements(), aString.Length());
}

/**
 * Returns |true| if |aString| contains only Latin1 characters, that is,
 * characters in the range (U+0000, U+00FF).
 *
 * The argument MUST be valid UTF-8. If you are at all unsure, use IsUTF8Latin1
 * instead!
 *
 * @param aString known-valid UTF-8 string to scan
 */
inline bool UnsafeIsValidUTF8Latin1(mozilla::Span<const char> aString) {
  return encoding_mem_is_str_latin1(aString.Elements(), aString.Length());
}

/**
 * Returns |true| if |aString| is a valid UTF-8 string.
 *
 * Note that this doesn't check whether the string might look like a valid
 * string in another encoding, too, e.g. ISO-2022-JP.
 *
 * @param aString an 8-bit wide string to scan
 */
inline bool IsUTF8(mozilla::Span<const char> aString) {
  size_t length = aString.Length();
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(aString.Elements());
  // For short strings, the function call is a pessimization, and the SIMD
  // code won't have a chance to kick in anyway.
  if (length < 16) {
    for (size_t i = 0; i < length; i++) {
      if (ptr[i] >= 0x80U) {
        ptr += i;
        length -= i;
        goto end;
      }
    }
    return true;
  }
end:
  return length == encoding_utf8_valid_up_to(ptr, length);
}

/**
 * Returns the index of the first unpaired surrogate or
 * the length of the string if there are none.
 */
inline uint32_t UTF16ValidUpTo(mozilla::Span<const char16_t> aString) {
  return encoding_mem_utf16_valid_up_to(aString.Elements(), aString.Length());
}

/**
 * Replaces unpaired surrogates with U+FFFD in the argument.
 */
inline void EnsureUTF16ValiditySpan(mozilla::Span<char16_t> aString) {
  encoding_mem_ensure_utf16_validity(aString.Elements(), aString.Length());
}

/**
 * Replaces unpaired surrogates with U+FFFD in the argument.
 *
 * Copies a shared string buffer or an otherwise read-only
 * buffer only if there are unpaired surrogates.
 */
inline void EnsureUTF16Validity(nsAString& aString) {
  uint32_t upTo = UTF16ValidUpTo(aString);
  uint32_t len = aString.Length();
  if (upTo == len) {
    return;
  }
  char16_t* ptr = aString.BeginWriting();
  auto span = mozilla::MakeSpan(ptr, len);
  span[upTo] = 0xFFFD;
  EnsureUTF16ValiditySpan(span.From(upTo + 1));
}

bool ParseString(const nsACString& aAstring, char aDelimiter,
                 nsTArray<nsCString>& aArray);

/**
 * Converts case in place in the argument string.
 */
void ToUpperCase(nsACString&);

void ToLowerCase(nsACString&);

void ToUpperCase(nsACString&);

void ToLowerCase(nsACString&);

/**
 * Converts case from string aSource to aDest.
 */
void ToUpperCase(const nsACString& aSource, nsACString& aDest);

void ToLowerCase(const nsACString& aSource, nsACString& aDest);

/**
 * Finds the leftmost occurrence of |aPattern|, if any in the range
 * |aSearchStart|..|aSearchEnd|.
 *
 * Returns |true| if a match was found, and adjusts |aSearchStart| and
 * |aSearchEnd| to point to the match.  If no match was found, returns |false|
 * and makes |aSearchStart == aSearchEnd|.
 *
 * Currently, this is equivalent to the O(m*n) implementation previously on
 * |ns[C]String|.
 *
 * If we need something faster, then we can implement that later.
 */

bool FindInReadable(const nsAString& aPattern, nsAString::const_iterator&,
                    nsAString::const_iterator&,
                    const nsStringComparator& = nsDefaultStringComparator());
bool FindInReadable(const nsACString& aPattern, nsACString::const_iterator&,
                    nsACString::const_iterator&,
                    const nsCStringComparator& = nsDefaultCStringComparator());

/* sometimes we don't care about where the string was, just that we
 * found it or not */
inline bool FindInReadable(
    const nsAString& aPattern, const nsAString& aSource,
    const nsStringComparator& aCompare = nsDefaultStringComparator()) {
  nsAString::const_iterator start, end;
  aSource.BeginReading(start);
  aSource.EndReading(end);
  return FindInReadable(aPattern, start, end, aCompare);
}

inline bool FindInReadable(
    const nsACString& aPattern, const nsACString& aSource,
    const nsCStringComparator& aCompare = nsDefaultCStringComparator()) {
  nsACString::const_iterator start, end;
  aSource.BeginReading(start);
  aSource.EndReading(end);
  return FindInReadable(aPattern, start, end, aCompare);
}

bool CaseInsensitiveFindInReadable(const nsACString& aPattern,
                                   nsACString::const_iterator&,
                                   nsACString::const_iterator&);

/**
 * Finds the rightmost occurrence of |aPattern|
 * Returns |true| if a match was found, and adjusts |aSearchStart| and
 * |aSearchEnd| to point to the match.  If no match was found, returns |false|
 * and makes |aSearchStart == aSearchEnd|.
 */
bool RFindInReadable(const nsAString& aPattern, nsAString::const_iterator&,
                     nsAString::const_iterator&,
                     const nsStringComparator& = nsDefaultStringComparator());
bool RFindInReadable(const nsACString& aPattern, nsACString::const_iterator&,
                     nsACString::const_iterator&,
                     const nsCStringComparator& = nsDefaultCStringComparator());

/**
 * Finds the leftmost occurrence of |aChar|, if any in the range
 * |aSearchStart|..|aSearchEnd|.
 *
 * Returns |true| if a match was found, and adjusts |aSearchStart| to
 * point to the match.  If no match was found, returns |false| and
 * makes |aSearchStart == aSearchEnd|.
 */
bool FindCharInReadable(char16_t aChar, nsAString::const_iterator& aSearchStart,
                        const nsAString::const_iterator& aSearchEnd);
bool FindCharInReadable(char aChar, nsACString::const_iterator& aSearchStart,
                        const nsACString::const_iterator& aSearchEnd);

bool StringBeginsWith(const nsAString& aSource, const nsAString& aSubstring);
bool StringBeginsWith(const nsAString& aSource, const nsAString& aSubstring,
                      const nsStringComparator& aComparator);
bool StringBeginsWith(const nsACString& aSource, const nsACString& aSubstring);
bool StringBeginsWith(const nsACString& aSource, const nsACString& aSubstring,
                      const nsCStringComparator& aComparator);
bool StringEndsWith(const nsAString& aSource, const nsAString& aSubstring);
bool StringEndsWith(const nsAString& aSource, const nsAString& aSubstring,
                    const nsStringComparator& aComparator);
bool StringEndsWith(const nsACString& aSource, const nsACString& aSubstring);
bool StringEndsWith(const nsACString& aSource, const nsACString& aSubstring,
                    const nsCStringComparator& aComparator);

const nsString& EmptyString();
const nsCString& EmptyCString();

const nsString& VoidString();
const nsCString& VoidCString();

/**
 * Compare a UTF-8 string to an UTF-16 string.
 *
 * Returns 0 if the strings are equal, -1 if aUTF8String is less
 * than aUTF16Count, and 1 in the reverse case. Errors are replaced
 * with U+FFFD and then the U+FFFD is compared as if it had occurred
 * in the input. If aErr is not nullptr, *aErr is set to true if
 * either string had malformed sequences.
 */
int32_t CompareUTF8toUTF16(const nsACString& aUTF8String,
                           const nsAString& aUTF16String, bool* aErr = nullptr);

void AppendUCS4ToUTF16(const uint32_t aSource, nsAString& aDest);

#endif  // !defined(nsReadableUtils_h___)
