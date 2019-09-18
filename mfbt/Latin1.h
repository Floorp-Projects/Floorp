/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Latin-1 operations (i.e. a byte is the corresponding code point).
 * (Note: this is *not* the same as the encoding of windows-1252 or
 * latin1 content on the web. In Web terms, this encoding
 * corresponds to "isomorphic decode" / "isomorphic encoding" from
 * the Infra Standard.)
 */

#ifndef mozilla_Latin1_h
#define mozilla_Latin1_h

#include "mozilla/JsRust.h"
#include "mozilla/Span.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"

#if MOZ_HAS_JSRUST()
#  include "encoding_rs_mem.h"
#endif

namespace mozilla {

namespace detail {

template <typename Char>
class MakeUnsignedChar : public MakeUnsigned<Char> {};

template <>
class MakeUnsignedChar<char16_t> {
 public:
  using Type = char16_t;
};

template <>
class MakeUnsignedChar<char32_t> {
 public:
  using Type = char32_t;
};

}  // namespace detail

/**
 * Returns true iff |aChar| is Latin-1 but not ASCII, i.e. in the range
 * [0x80, 0xFF].
 */
template <typename Char>
constexpr bool IsNonAsciiLatin1(Char aChar) {
  using UnsignedChar = typename detail::MakeUnsignedChar<Char>::Type;
  auto uc = static_cast<UnsignedChar>(aChar);
  return uc >= 0x80 && uc <= 0xFF;
}

#if MOZ_HAS_JSRUST()

/**
 * Returns |true| iff |aString| contains only Latin1 characters, that is,
 * characters in the range [U+0000, U+00FF].
 *
 * @param aString a potentially-invalid UTF-16 string to scan
 */
inline bool IsUtf16Latin1(mozilla::Span<const char16_t> aString) {
  return encoding_mem_is_utf16_latin1(aString.Elements(), aString.Length());
}

/**
 * Returns |true| iff |aString| is valid UTF-8 containing only Latin-1
 * characters.
 *
 * If you know that the argument is always absolutely guaranteed to be valid
 * UTF-8, use the faster UnsafeIsValidUtf8Latin1() instead.
 *
 * @param aString potentially-invalid UTF-8 string to scan
 */
inline bool IsUtf8Latin1(mozilla::Span<const char> aString) {
  return encoding_mem_is_utf8_latin1(aString.Elements(), aString.Length());
}

/**
 * Returns |true| iff |aString|, which MUST be valid UTF-8, contains only
 * Latin1 characters, that is, characters in the range [U+0000, U+00FF].
 * (If |aString| might not be valid UTF-8, use |IsUtf8Latin1| instead.)
 *
 * @param aString known-valid UTF-8 string to scan
 */
inline bool UnsafeIsValidUtf8Latin1(mozilla::Span<const char> aString) {
  return encoding_mem_is_str_latin1(aString.Elements(), aString.Length());
}

/**
 * If all the code points in the input are below U+0100, converts to Latin1,
 * i.e. unsigned byte value is Unicode scalar value. If there are code points
 * above U+00FF, produces unspecified garbage in a memory-safe way. The
 * nature of the garbage must not be relied upon.
 *
 * The length of aDest must not be less than the length of aSource.
 */
inline void LossyConvertUtf16toLatin1(mozilla::Span<const char16_t> aSource,
                                      mozilla::Span<char> aDest) {
  encoding_mem_convert_utf16_to_latin1_lossy(
      aSource.Elements(), aSource.Length(), aDest.Elements(), aDest.Length());
}

/**
 * If all the code points in the input are below U+0100, converts to Latin1,
 * i.e. unsigned byte value is Unicode scalar value. If there are code points
 * above U+00FF, produces unspecified garbage in a memory-safe way. The
 * nature of the garbage must not be relied upon.
 *
 * Returns the number of code units written.
 *
 * The length of aDest must not be less than the length of aSource.
 */
inline size_t LossyConvertUtf8toLatin1(mozilla::Span<const char> aSource,
                                       mozilla::Span<char> aDest) {
  return encoding_mem_convert_utf8_to_latin1_lossy(
      aSource.Elements(), aSource.Length(), aDest.Elements(), aDest.Length());
}

/**
 * Converts each byte of |aSource|, interpreted as a Unicode scalar value
 * having that unsigned value, to its UTF-8 representation in |aDest|.
 *
 * Returns the number of code units written.
 *
 * The length of aDest must be at least twice the length of aSource.
 */
inline size_t ConvertLatin1toUtf8(mozilla::Span<const char> aSource,
                                  mozilla::Span<char> aDest) {
  return encoding_mem_convert_latin1_to_utf8(
      aSource.Elements(), aSource.Length(), aDest.Elements(), aDest.Length());
}

/**
 * Converts bytes whose unsigned value is interpreted as Unicode code point
 * (i.e. U+0000 to U+00FF, inclusive) to UTF-8 with potentially insufficient
 * output space.
 *
 * Returns the number of bytes read and the number of bytes written.
 *
 * If the output isn't large enough, not all input is consumed.
 *
 * The conversion is guaranteed to be complete if the length of aDest is
 * at least the length of aSource times two.
 *
 * The output is always valid UTF-8 ending on scalar value boundary
 * even in the case of partial conversion.
 *
 * The semantics of this function match the semantics of
 * TextEncoder.encodeInto.
 * https://encoding.spec.whatwg.org/#dom-textencoder-encodeinto
 */
inline mozilla::Tuple<size_t, size_t> ConvertLatin1toUtf8Partial(
    mozilla::Span<const char> aSource, mozilla::Span<char> aDest) {
  size_t srcLen = aSource.Length();
  size_t dstLen = aDest.Length();
  encoding_mem_convert_latin1_to_utf8_partial(aSource.Elements(), &srcLen,
                                              aDest.Elements(), &dstLen);
  return mozilla::MakeTuple(srcLen, dstLen);
}

/**
 * Converts Latin-1 code points (i.e. each byte is the identical code
 * point) from |aSource| to UTF-16 code points in |aDest|.
 *
 * The length of aDest must not be less than the length of aSource.
 */
inline void ConvertLatin1toUtf16(mozilla::Span<const char> aSource,
                                 mozilla::Span<char16_t> aDest) {
  encoding_mem_convert_latin1_to_utf16(aSource.Elements(), aSource.Length(),
                                       aDest.Elements(), aDest.Length());
}

#endif

};  // namespace mozilla

#endif  // mozilla_Latin1_h
