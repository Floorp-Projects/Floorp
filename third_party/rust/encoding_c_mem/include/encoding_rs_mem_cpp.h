// Copyright 2015-2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#pragma once

#ifndef encoding_rs_mem_cpp_h_
#define encoding_rs_mem_cpp_h_

#include <optional>
#include <string_view>
#include <tuple>
#include "gsl/gsl"

#include "encoding_rs_mem.h"

namespace encoding_rs {
namespace mem {

namespace detail {
/**
 * Replaces `nullptr` with a bogus pointer suitable for use as part of a
 * zero-length Rust slice.
 */
template <class T>
static inline T* null_to_bogus(T* ptr) {
  return ptr ? ptr : reinterpret_cast<T*>(alignof(T));
}
};  // namespace detail

/**
 * Checks whether a potentially invalid UTF-16 buffer contains code points
 * that trigger right-to-left processing or is all-Latin1.
 *
 * Possibly more efficient than performing the checks separately.
 *
 * Returns `Latin1Bidi::Latin1` if `is_utf16_latin1()` would return `true`.
 * Otherwise, returns `Latin1Bidi::Bidi` if `is_utf16_bidi()` would return
 * `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
 */
inline Latin1Bidi check_for_latin1_and_bidi(std::u16string_view buffer) {
  return encoding_mem_check_utf16_for_latin1_and_bidi(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(buffer.data()),
      buffer.size());
}

/**
 * Checks whether a potentially invalid UTF-8 buffer contains code points
 * that trigger right-to-left processing or is all-Latin1.
 *
 * Possibly more efficient than performing the checks separately.
 *
 * Returns `Latin1Bidi::Latin1` if `is_utf8_latin1()` would return `true`.
 *
 * Otherwise, returns `Latin1Bidi::Bidi` if `is_utf8_bidi()` would return
 * `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
 */
inline Latin1Bidi check_for_latin1_and_bidi(std::string_view buffer) {
  return encoding_mem_check_utf8_for_latin1_and_bidi(
      encoding_rs::mem::detail::null_to_bogus<const char>(buffer.data()),
      buffer.size());
}

/**
 * Converts bytes whose unsigned value is interpreted as Unicode code point
 * (i.e. U+0000 to U+00FF, inclusive) to UTF-16.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * The number of `char16_t`s written equals the length of the source buffer.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 */
inline void convert_latin1_to_utf16(gsl::span<const char> src,
                                    gsl::span<char16_t> dst) {
  encoding_mem_convert_latin1_to_utf16(
      encoding_rs::mem::detail::null_to_bogus<const char>(src.data()),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char16_t>(dst.data()),
      dst.size());
}

/**
 * Converts bytes whose unsigned value is interpreted as Unicode code point
 * (i.e. U+0000 to U+00FF, inclusive) to UTF-8.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer times two.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Safety
 *
 * Note that this function may write garbage beyond the number of bytes
 * indicated by the return value.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `dst` overlap.
 */
inline size_t convert_latin1_to_utf8(gsl::span<const char> src,
                                     gsl::span<char> dst) {
  return encoding_mem_convert_latin1_to_utf8(
      encoding_rs::mem::detail::null_to_bogus<const char>(src.data()),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char>(dst.data()),
      dst.size());
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
 * # Undefined behavior
 *
 * UB ensues if `src` and `dst` overlap.
 */
inline std::tuple<size_t, size_t> convert_latin1_to_utf8_partial(
    gsl::span<const char> src, gsl::span<char> dst) {
  size_t src_read = src.size();
  size_t dst_written = dst.size();
  encoding_mem_convert_latin1_to_utf8_partial(
      encoding_rs::mem::detail::null_to_bogus<const char>(src.data()),
      &src_read, encoding_rs::mem::detail::null_to_bogus<char>(dst.data()),
      &dst_written);
  return {src_read, dst_written};
}

/**
 * Converts valid UTF-8 to valid UTF-16.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of `char16_t`s written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 */
inline size_t convert_str_to_utf16(std::string_view src,
                                   gsl::span<char16_t> dst) {
  return encoding_mem_convert_str_to_utf16(
      encoding_rs::mem::detail::null_to_bogus<const char>(
          reinterpret_cast<const char*>(src.data())),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char16_t>(dst.data()),
      dst.size());
}

/**
 * If the input is valid UTF-16 representing only Unicode code points from
 * U+0000 to U+00FF, inclusive, converts the input into output that
 * represents the value of each code point as the unsigned byte value of
 * each output byte.
 *
 * If the input does not fulfill the condition stated above, does something
 * that is memory-safe without any promises about any properties of the
 * output and will probably assert in debug builds in future versions.
 * In particular, callers shouldn't assume the output to be the same across
 * crate versions or CPU architectures and should not assume that non-ASCII
 * input can't map to ASCII output.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * The number of bytes written equals the length of the source buffer.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 * (Probably in future versions if debug assertions are enabled (and not
 * fuzzing) and the input is not in the range U+0000 to U+00FF, inclusive.)
 */
inline void convert_utf16_to_latin1_lossy(std::u16string_view src,
                                          gsl::span<char> dst) {
  encoding_mem_convert_utf16_to_latin1_lossy(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(src.data()),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char>(dst.data()),
      dst.size());
}

/**
 * Converts potentially-invalid UTF-16 to valid UTF-8 with errors replaced
 * with the REPLACEMENT CHARACTER.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer times three.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 */
inline size_t convert_utf16_to_utf8(std::u16string_view src,
                                    gsl::span<char> dst) {
  return encoding_mem_convert_utf16_to_utf8(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(src.data()),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char>(dst.data()),
      dst.size());
}

/**
 * Converts potentially-invalid UTF-16 to valid UTF-8 with errors replaced
 * with the REPLACEMENT CHARACTER with potentially insufficient output
 * space.
 *
 * Returns the number of code units read and the number of bytes written.
 *
 * Guarantees that the bytes in the destination beyond the number of
 * bytes claimed as written by the second item of the return tuple
 * are left unmodified.
 *
 * Not all code units are read if there isn't enough output space.
 * Note  that this method isn't designed for general streamability but for
 * not allocating memory for the worst case up front. Specifically,
 * if the input starts with or ends with an unpaired surrogate, those are
 * replaced with the REPLACEMENT CHARACTER.
 *
 * Matches the semantics of `TextEncoder.encodeInto()` from the
 * Encoding Standard.
 */
inline std::tuple<size_t, size_t> convert_utf16_to_utf8_partial(
    std::u16string_view src, gsl::span<char> dst) {
  size_t src_read = src.size();
  size_t dst_written = dst.size();
  encoding_mem_convert_utf16_to_utf8_partial(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(src.data()),
      &src_read, encoding_rs::mem::detail::null_to_bogus<char>(dst.data()),
      &dst_written);
  return {src_read, dst_written};
}

/**
 * If the input is valid UTF-8 representing only Unicode code points from
 * U+0000 to U+00FF, inclusive, converts the input into output that
 * represents the value of each code point as the unsigned byte value of
 * each output byte.
 *
 * If the input does not fulfill the condition stated above, this function
 * panics if debug assertions are enabled (and fuzzing isn't) and otherwise
 * does something that is memory-safe without any promises about any
 * properties of the output. In particular, callers shouldn't assume the
 * output to be the same across crate versions or CPU architectures and
 * should not assume that non-ASCII input can't map to ASCII output.
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 * If debug assertions are enabled (and not fuzzing) and the input is
 * not in the range U+0000 to U+00FF, inclusive.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `dst` overlap.
 */
inline size_t convert_utf8_to_latin1_lossy(std::string_view src,
                                           gsl::span<char> dst) {
  return encoding_mem_convert_utf8_to_latin1_lossy(
      encoding_rs::mem::detail::null_to_bogus<const char>(
          reinterpret_cast<const char*>(src.data())),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char>(dst.data()),
      dst.size());
}

/**
 * Converts potentially-invalid UTF-8 to valid UTF-16 with errors replaced
 * with the REPLACEMENT CHARACTER.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer _plus one_.
 *
 * Returns the number of `char16_t`s written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 */
inline size_t convert_utf8_to_utf16(std::string_view src,
                                    gsl::span<char16_t> dst) {
  return encoding_mem_convert_utf8_to_utf16(
      encoding_rs::mem::detail::null_to_bogus<const char>(
          reinterpret_cast<const char*>(src.data())),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char16_t>(dst.data()),
      dst.size());
}

/**
 * Converts potentially-invalid UTF-8 to valid UTF-16 signaling on error.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of `char16_t`s written or `std::nullopt` if the input was
 * invalid.
 *
 * When the input was invalid, some output may have been written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 */
inline std::optional<size_t> convert_utf8_to_utf16_without_replacement(
    std::string_view src, gsl::span<char16_t> dst) {
  size_t val = encoding_mem_convert_utf8_to_utf16_without_replacement(
      encoding_rs::mem::detail::null_to_bogus<const char>(
          reinterpret_cast<const char*>(src.data())),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char16_t>(dst.data()),
      dst.size());
  if (val == SIZE_MAX) {
    return std::nullopt;
  }
  return val;
}

/**
 * Copies ASCII from source to destination up to the first non-ASCII byte
 * (or the end of the input if it is ASCII in its entirety).
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `dst` overlap.
 */
inline size_t copy_ascii_to_ascii(gsl::span<const char> src,
                                  gsl::span<char> dst) {
  return encoding_mem_copy_ascii_to_ascii(
      encoding_rs::mem::detail::null_to_bogus<const char>(src.data()),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char>(dst.data()),
      dst.size());
}

/**
 * Copies ASCII from source to destination zero-extending it to UTF-16 up to
 * the first non-ASCII byte (or the end of the input if it is ASCII in its
 * entirety).
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of `char16_t`s written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 */
inline size_t copy_ascii_to_basic_latin(gsl::span<const char> src,
                                        gsl::span<char16_t> dst) {
  return encoding_mem_copy_ascii_to_basic_latin(
      encoding_rs::mem::detail::null_to_bogus<const char>(src.data()),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char16_t>(dst.data()),
      dst.size());
}

/**
 * Copies Basic Latin from source to destination narrowing it to ASCII up to
 * the first non-Basic Latin code unit (or the end of the input if it is
 * Basic Latin in its entirety).
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 */
inline size_t copy_basic_latin_to_ascii(gsl::span<const char16_t> src,
                                        gsl::span<char> dst) {
  return encoding_mem_copy_basic_latin_to_ascii(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(src.data()),
      src.size(), encoding_rs::mem::detail::null_to_bogus<char>(dst.data()),
      dst.size());
}

/**
 * Replaces unpaired surrogates in the input with the REPLACEMENT CHARACTER.
 */
inline void ensure_utf16_validity(gsl::span<char16_t> buffer) {
  encoding_mem_ensure_utf16_validity(
      encoding_rs::mem::detail::null_to_bogus<char16_t>(buffer.data()),
      buffer.size());
}

/**
 * Checks whether the buffer is all-ASCII.
 *
 * May read the entire buffer even if it isn't all-ASCII. (I.e. the function
 * is not guaranteed to fail fast.)
 */
inline bool is_ascii(std::string_view buffer) {
  return encoding_mem_is_ascii(
      encoding_rs::mem::detail::null_to_bogus<const char>(buffer.data()),
      buffer.size());
}

/**
 * Checks whether the buffer is all-Basic Latin (i.e. UTF-16 representing
 * only ASCII characters).
 *
 * May read the entire buffer even if it isn't all-ASCII. (I.e. the function
 * is not guaranteed to fail fast.)
 */
inline bool is_ascii(std::u16string_view buffer) {
  return encoding_mem_is_basic_latin(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(buffer.data()),
      buffer.size());
}

/**
 * Checks whether a scalar value triggers right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 *
 * # Undefined behavior
 *
 * Undefined behavior ensues if `c` is not a valid Unicode Scalar Value.
 */
inline bool is_scalar_value_bidi(char32_t c) {
  return encoding_mem_is_char_bidi(c);
}

/**
 * Checks whether a UTF-16 buffer contains code points that trigger
 * right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 * Returns `true` if the input contains an RTL character or an unpaired
 * high surrogate that could be the high half of an RTL character.
 * Returns `false` if the input contains neither RTL characters nor
 * unpaired high surrogates that could be higher halves of RTL characters.
 */
inline bool is_bidi(std::u16string_view buffer) {
  return encoding_mem_is_utf16_bidi(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(buffer.data()),
      buffer.size());
}

/**
 * Checks whether a UTF-16 code unit triggers right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 * Since supplementary-plane right-to-left blocks are identifiable from the
 * high surrogate without examining the low surrogate, this function returns
 * `true` for such high surrogates making the function suitable for handling
 * supplementary-plane text without decoding surrogate pairs to scalar
 * values. Obviously, such high surrogates are then reported as right-to-left
 * even if actually unpaired.
 */
inline bool is_utf16_code_unit_bidi(char16_t u) {
  return encoding_mem_is_utf16_code_unit_bidi(u);
}

/**
 * Checks whether the buffer represents only code point less than or equal
 * to U+00FF.
 *
 * May read the entire buffer even if it isn't all-Latin1. (I.e. the function
 * is not guaranteed to fail fast.)
 */
inline bool is_utf16_latin1(std::u16string_view buffer) {
  return encoding_mem_is_utf16_latin1(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(buffer.data()),
      buffer.size());
}

/**
 * Checks whether a potentially-invalid UTF-8 buffer contains code points
 * that trigger right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 * Returns `true` if the input is invalid UTF-8 or the input contains an
 * RTL character. Returns `false` if the input is valid UTF-8 and contains
 * no RTL characters.
 */
inline bool is_bidi(std::string_view buffer) {
  return encoding_mem_is_utf8_bidi(
      encoding_rs::mem::detail::null_to_bogus<const char>(buffer.data()),
      buffer.size());
}

/**
 * Checks whether the buffer is valid UTF-8 representing only code points
 * less than or equal to U+00FF.
 *
 * Fails fast. (I.e. returns before having read the whole buffer if UTF-8
 * invalidity or code points above U+00FF are discovered.
 */
inline bool is_utf8_latin1(std::string_view buffer) {
  return encoding_mem_is_utf8_latin1(
      encoding_rs::mem::detail::null_to_bogus<const char>(buffer.data()),
      buffer.size());
}

/**
 * Returns the index of the first unpaired surrogate or, if the input is
 * valid UTF-16 in its entirety, the length of the input.
 */
inline size_t utf16_valid_up_to(std::u16string_view buffer) {
  return encoding_mem_utf16_valid_up_to(
      encoding_rs::mem::detail::null_to_bogus<const char16_t>(buffer.data()),
      buffer.size());
}

/**
 * Returns the index of first byte that starts a non-Latin1 byte
 * sequence, or the length of the string if there are none.
 */
inline size_t utf8_latin1_up_to(std::string_view buffer) {
  return encoding_mem_utf8_latin1_up_to(
      encoding_rs::mem::detail::null_to_bogus<const char>(buffer.data()),
      buffer.size());
}

};  // namespace mem
};  // namespace encoding_rs

#endif  // encoding_rs_mem_cpp_h_
