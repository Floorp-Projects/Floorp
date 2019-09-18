// Copyright 2015-2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#pragma once

#ifndef encoding_rs_cpp_h_
#define encoding_rs_cpp_h_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include "gsl/gsl"

namespace encoding_rs {
class Encoding;
class Decoder;
class Encoder;
};  // namespace encoding_rs

#define ENCODING_RS_ENCODING encoding_rs::Encoding
#define ENCODING_RS_NOT_NULL_CONST_ENCODING_PTR \
  gsl::not_null<const encoding_rs::Encoding*>
#define ENCODING_RS_ENCODER encoding_rs::Encoder
#define ENCODING_RS_DECODER encoding_rs::Decoder

#include "encoding_rs.h"

namespace encoding_rs {

/**
 * A converter that decodes a byte stream into Unicode according to a
 * character encoding in a streaming (incremental) manner.
 *
 * The various `decode_*` methods take an input buffer (`src`) and an output
 * buffer `dst` both of which are caller-allocated. There are variants for
 * both UTF-8 and UTF-16 output buffers.
 *
 * A `decode_*` method decodes bytes from `src` into Unicode characters stored
 * into `dst` until one of the following three things happens:
 *
 * 1. A malformed byte sequence is encountered (`*_without_replacement`
 *    variants only).
 *
 * 2. The output buffer has been filled so near capacity that the decoder
 *    cannot be sure that processing an additional byte of input wouldn't
 *    cause so much output that the output buffer would overflow.
 *
 * 3. All the input bytes have been processed.
 *
 * The `decode_*` method then returns tuple of a status indicating which one
 * of the three reasons to return happened, how many input bytes were read,
 * how many output code units (`uint8_t` when decoding into UTF-8 and `char16_t`
 * when decoding to UTF-16) were written, and in the case of the
 * variants performing replacement, a boolean indicating whether an error was
 * replaced with the REPLACEMENT CHARACTER during the call.
 *
 * The number of bytes "written" is what's logically written. Garbage may be
 * written in the output buffer beyond the point logically written to.
 *
 * In the case of the `*_without_replacement` variants, the status is a
 * `uint32_t` whose possible values are packed info about a malformed byte
 * sequence, `OUTPUT_FULL` and `INPUT_EMPTY` corresponding to the three cases
 * listed above).
 *
 * Packed info about malformed sequences has the following format:
 * The lowest 8 bits, which can have the decimal value 0, 1, 2 or 3,
 * indicate the number of bytes that were consumed after the malformed
 * sequence and whose next-lowest 8 bits, when shifted right by 8 indicate
 * the length of the malformed byte sequence (possible decimal values 1, 2,
 * 3 or 4). The maximum possible sum of the two is 6.
 *
 * In the case of methods whose name does not end with
 * `*_without_replacement`, malformed sequences are automatically replaced
 * with the REPLACEMENT CHARACTER and errors do not cause the methods to
 * return early.
 *
 * When decoding to UTF-8, the output buffer must have at least 4 bytes of
 * space. When decoding to UTF-16, the output buffer must have at least two
 * UTF-16 code units (`char16_t`) of space.
 *
 * When decoding to UTF-8 without replacement, the methods are guaranteed
 * not to return indicating that more output space is needed if the length
 * of the output buffer is at least the length returned by
 * `max_utf8_buffer_length_without_replacement()`. When decoding to UTF-8
 * with replacement, the length of the output buffer that guarantees the
 * methods not to return indicating that more output space is needed is given
 * by `max_utf8_buffer_length()`. When decoding to UTF-16 with
 * or without replacement, the length of the output buffer that guarantees
 * the methods not to return indicating that more output space is needed is
 * given by `max_utf16_buffer_length()`.
 *
 * The output written into `dst` is guaranteed to be valid UTF-8 or UTF-16,
 * and the output after each `decode_*` call is guaranteed to consist of
 * complete characters. (I.e. the code unit sequence for the last character is
 * guaranteed not to be split across output buffers.)
 *
 * The boolean argument `last` indicates that the end of the stream is reached
 * when all the bytes in `src` have been consumed.
 *
 * A `Decoder` object can be used to incrementally decode a byte stream.
 *
 * During the processing of a single stream, the caller must call `decode_*`
 * zero or more times with `last` set to `false` and then call `decode_*` at
 * least once with `last` set to `true`. If `decode_*` returns `INPUT_EMPTY`,
 * the processing of the stream has ended. Otherwise, the caller must call
 * `decode_*` again with `last` set to `true` (or treat a malformed result,
 * i.e. neither `INPUT_EMPTY` nor `OUTPUT_FULL`, as a fatal error).
 *
 * Once the stream has ended, the `Decoder` object must not be used anymore.
 * That is, you need to create another one to process another stream.
 *
 * When the decoder returns `OUTPUT_FULL` or the decoder returns a malformed
 * result and the caller does not wish to treat it as a fatal error, the input
 * buffer `src` may not have been completely consumed. In that case, the caller
 * must pass the unconsumed contents of `src` to `decode_*` again upon the next
 * call.
 *
 * # Infinite loops
 *
 * When converting with a fixed-size output buffer whose size is too small to
 * accommodate one character of output, an infinite loop ensues. When
 * converting with a fixed-size output buffer, it generally makes sense to
 * make the buffer fairly large (e.g. couple of kilobytes).
 */
class Decoder final {
 public:
  ~Decoder() {}
  static inline void operator delete(void* decoder) {
    decoder_free(reinterpret_cast<Decoder*>(decoder));
  }

  /**
   * The `Encoding` this `Decoder` is for.
   *
   * BOM sniffing can change the return value of this method during the life
   * of the decoder.
   */
  inline gsl::not_null<const Encoding*> encoding() const {
    return gsl::not_null<const Encoding*>(decoder_encoding(this));
  }

  /**
   * Query the worst-case UTF-8 output size _with replacement_.
   *
   * Returns the size of the output buffer in UTF-8 code units (`uint8_t`)
   * that will not overflow given the current state of the decoder and
   * `byte_length` number of additional input bytes when decoding with
   * errors handled by outputting a REPLACEMENT CHARACTER for each malformed
   * sequence or `std::optional` without value if `size_t` would overflow.
   */
  inline std::optional<size_t> max_utf8_buffer_length(
      size_t byte_length) const {
    size_t val = decoder_max_utf8_buffer_length(this, byte_length);
    if (val == SIZE_MAX) {
      return std::nullopt;
    }
    return val;
  }

  /**
   * Query the worst-case UTF-8 output size _without replacement_.
   *
   * Returns the size of the output buffer in UTF-8 code units (`uint8_t`)
   * that will not overflow given the current state of the decoder and
   * `byte_length` number of additional input bytes when decoding without
   * replacement error handling or `std::optional` without value if `size_t`
   * would overflow.
   *
   * Note that this value may be too small for the `_with_replacement` case.
   * Use `max_utf8_buffer_length()` for that case.
   */
  inline std::optional<size_t> max_utf8_buffer_length_without_replacement(
      size_t byte_length) const {
    size_t val =
        decoder_max_utf8_buffer_length_without_replacement(this, byte_length);
    if (val == SIZE_MAX) {
      return std::nullopt;
    }
    return val;
  }

  /**
   * Incrementally decode a byte stream into UTF-8 with malformed sequences
   * replaced with the REPLACEMENT CHARACTER.
   *
   * See the documentation of the class for documentation for `decode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t, bool> decode_to_utf8(
      gsl::span<const uint8_t> src, gsl::span<uint8_t> dst, bool last) {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    bool had_replacements;
    uint32_t result =
        decoder_decode_to_utf8(this, null_to_bogus<const uint8_t>(src.data()),
                               &src_read, null_to_bogus<uint8_t>(dst.data()),
                               &dst_written, last, &had_replacements);
    return {result, src_read, dst_written, had_replacements};
  }

  /**
   * Incrementally decode a byte stream into UTF-8 _without replacement_.
   *
   * See the documentation of the class for documentation for `decode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t>
  decode_to_utf8_without_replacement(gsl::span<const uint8_t> src,
                                     gsl::span<uint8_t> dst, bool last) {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    uint32_t result = decoder_decode_to_utf8_without_replacement(
        this, null_to_bogus<const uint8_t>(src.data()), &src_read,
        null_to_bogus<uint8_t>(dst.data()), &dst_written, last);
    return {result, src_read, dst_written};
  }

  /**
   * Query the worst-case UTF-16 output size (with or without replacement).
   *
   * Returns the size of the output buffer in UTF-16 code units (`char16_t`)
   * that will not overflow given the current state of the decoder and
   * `byte_length` number of additional input bytes  or `std::optional`
   * without value if `size_t` would overflow.
   *
   * Since the REPLACEMENT CHARACTER fits into one UTF-16 code unit, the
   * return value of this method applies also in the
   * `_without_replacement` case.
   */
  inline std::optional<size_t> max_utf16_buffer_length(
      size_t byte_length) const {
    size_t val = decoder_max_utf16_buffer_length(this, byte_length);
    if (val == SIZE_MAX) {
      return std::nullopt;
    }
    return val;
  }

  /**
   * Incrementally decode a byte stream into UTF-16 with malformed sequences
   * replaced with the REPLACEMENT CHARACTER.
   *
   * See the documentation of the class for documentation for `decode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t, bool> decode_to_utf16(
      gsl::span<const uint8_t> src, gsl::span<char16_t> dst, bool last) {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    bool had_replacements;
    uint32_t result =
        decoder_decode_to_utf16(this, null_to_bogus<const uint8_t>(src.data()),
                                &src_read, null_to_bogus<char16_t>(dst.data()),
                                &dst_written, last, &had_replacements);
    return {result, src_read, dst_written, had_replacements};
  }

  /**
   * Incrementally decode a byte stream into UTF-16 _without replacement_.
   *
   * See the documentation of the class for documentation for `decode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t>
  decode_to_utf16_without_replacement(gsl::span<const uint8_t> src,
                                      gsl::span<char16_t> dst, bool last) {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    uint32_t result = decoder_decode_to_utf16_without_replacement(
        this, null_to_bogus<const uint8_t>(src.data()), &src_read,
        null_to_bogus<char16_t>(dst.data()), &dst_written, last);
    return {result, src_read, dst_written};
  }

  /**
   * Checks for compatibility with storing Unicode scalar values as unsigned
   * bytes taking into account the state of the decoder.
   *
   * Returns `std::nullopt` if the decoder is still waiting for (parts of) a
   * potential BOM.
   *
   * Otherwise returns the index of the first byte whose unsigned value doesn't
   * directly correspond to the decoded Unicode scalar value, or the length
   * of the input if all bytes in the input decode directly to scalar values
   * corresponding to the unsigned byte values. (This is always returns zero
   * for UTF-16LE, UTF-16BE, and replacement. It's also zero when a multibyte
   * decoder is in the middle of a multibyte sequence.)
   *
   * Does not change the state of the decoder.
   *
   * Do not use this unless you are supporting SpiderMonkey/V8-style string
   * storage optimizations.
   */
  inline std::optional<size_t> latin1_byte_compatible_up_to(
      gsl::span<const uint8_t> buffer) const {
    size_t val = decoder_latin1_byte_compatible_up_to(
        this, null_to_bogus<const uint8_t>(buffer.data()),
        static_cast<size_t>(buffer.size()));
    if (val == SIZE_MAX) {
      return std::nullopt;
    }
    return val;
  }

 private:
  /**
   * Replaces `nullptr` with a bogus pointer suitable for use as part of a
   * zero-length Rust slice.
   */
  template <class T>
  static inline T* null_to_bogus(T* ptr) {
    return ptr ? ptr : reinterpret_cast<T*>(alignof(T));
  }

  Decoder() = delete;
  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;
};

/**
 * A converter that encodes a Unicode stream into bytes according to a
 * character encoding in a streaming (incremental) manner.
 *
 * The various `encode_*` methods take an input buffer (`src`) and an output
 * buffer `dst` both of which are caller-allocated. There are variants for
 * both UTF-8 and UTF-16 input buffers.
 *
 * An `encode_*` method encode characters from `src` into bytes characters
 * stored into `dst` until one of the following three things happens:
 *
 * 1. An unmappable character is encountered (`*_without_replacement` variants
 *    only).
 *
 * 2. The output buffer has been filled so near capacity that the decoder
 *    cannot be sure that processing an additional character of input wouldn't
 *    cause so much output that the output buffer would overflow.
 *
 * 3. All the input characters have been processed.
 *
 * The `encode_*` method then returns tuple of a status indicating which one
 * of the three reasons to return happened, how many input code units (`uint8_t`
 * when encoding from UTF-8 and `char16_t` when encoding from UTF-16) were read,
 * how many output bytes were written, and in the case of the variants that
 * perform replacement, a boolean indicating whether an unmappable
 * character was replaced with a numeric character reference during the call.
 *
 * The number of bytes "written" is what's logically written. Garbage may be
 * written in the output buffer beyond the point logically written to.
 *
 * In the case of the methods whose name ends with
 * `*_without_replacement`, the status is a `uint32_t` whose possible values
 * are an unmappable code point, `OUTPUT_FULL` and `INPUT_EMPTY` corresponding
 * to the three cases listed above).
 *
 * In the case of methods whose name does not end with
 * `*_without_replacement`, unmappable characters are automatically replaced
 * with the corresponding numeric character references and unmappable
 * characters do not cause the methods to return early.
 *
 * When encoding from UTF-8 without replacement, the methods are guaranteed
 * not to return indicating that more output space is needed if the length
 * of the output buffer is at least the length returned by
 * `max_buffer_length_from_utf8_without_replacement()`. When encoding from
 * UTF-8 with replacement, the length of the output buffer that guarantees the
 * methods not to return indicating that more output space is needed in the
 * absence of unmappable characters is given by
 * `max_buffer_length_from_utf8_if_no_unmappables()`. When encoding from
 * UTF-16 without replacement, the methods are guaranteed not to return
 * indicating that more output space is needed if the length of the output
 * buffer is at least the length returned by
 * `max_buffer_length_from_utf16_without_replacement()`. When encoding
 * from UTF-16 with replacement, the the length of the output buffer that
 * guarantees the methods not to return indicating that more output space is
 * needed in the absence of unmappable characters is given by
 * `max_buffer_length_from_utf16_if_no_unmappables()`.
 * When encoding with replacement, applications are not expected to size the
 * buffer for the worst case ahead of time but to resize the buffer if there
 * are unmappable characters. This is why max length queries are only available
 * for the case where there are no unmappable characters.
 *
 * When encoding from UTF-8, each `src` buffer _must_ be valid UTF-8. When
 * encoding from UTF-16, unpaired surrogates in the input are treated as U+FFFD
 * REPLACEMENT CHARACTERS. Therefore, in order for astral characters not to
 * turn into a pair of REPLACEMENT CHARACTERS, the caller must ensure that
 * surrogate pairs are not split across input buffer boundaries.
 *
 * After an `encode_*` call returns, the output produced so far, taken as a
 * whole from the start of the stream, is guaranteed to consist of a valid
 * byte sequence in the target encoding. (I.e. the code unit sequence for a
 * character is guaranteed not to be split across output buffers. However, due
 * to the stateful nature of ISO-2022-JP, the stream needs to be considered
 * from the start for it to be valid. For other encodings, the validity holds
 * on a per-output buffer basis.)
 *
 * The boolean argument `last` indicates that the end of the stream is reached
 * when all the characters in `src` have been consumed. This argument is needed
 * for ISO-2022-JP and is ignored for other encodings.
 *
 * An `Encoder` object can be used to incrementally encode a byte stream.
 *
 * During the processing of a single stream, the caller must call `encode_*`
 * zero or more times with `last` set to `false` and then call `encode_*` at
 * least once with `last` set to `true`. If `encode_*` returns `INPUT_EMPTY`,
 * the processing of the stream has ended. Otherwise, the caller must call
 * `encode_*` again with `last` set to `true` (or treat an unmappable result,
 * i.e. neither `INPUT_EMPTY` nor `OUTPUT_FULL`, as a fatal error).
 *
 * Once the stream has ended, the `Encoder` object must not be used anymore.
 * That is, you need to create another one to process another stream.
 *
 * When the encoder returns `OUTPUT_FULL` or the encoder returns an unmappable
 * result and the caller does not wish to treat it as a fatal error, the input
 * buffer `src` may not have been completely consumed. In that case, the caller
 * must pass the unconsumed contents of `src` to `encode_*` again upon the next
 * call.
 *
 * # Infinite loops
 *
 * When converting with a fixed-size output buffer whose size is too small to
 * accommodate one character of output, an infinite loop ensues. When
 * converting with a fixed-size output buffer, it generally makes sense to
 * make the buffer fairly large (e.g. couple of kilobytes).
 */
class Encoder final {
 public:
  ~Encoder() {}

  static inline void operator delete(void* encoder) {
    encoder_free(reinterpret_cast<Encoder*>(encoder));
  }

  /**
   * The `Encoding` this `Encoder` is for.
   */
  inline gsl::not_null<const Encoding*> encoding() const {
    return gsl::not_null<const Encoding*>(encoder_encoding(this));
  }

  /**
   * Returns `true` if this is an ISO-2022-JP encoder that's not in the
   * ASCII state and `false` otherwise.
   */
  inline bool has_pending_state() const {
    return encoder_has_pending_state(this);
  }

  /**
   * Query the worst-case output size when encoding from UTF-8 with
   * replacement.
   *
   * Returns the size of the output buffer in bytes that will not overflow
   * given the current state of the encoder and `byte_length` number of
   * additional input code units if there are no unmappable characters in
   * the input or `SIZE_MAX` if `size_t` would overflow.
   */
  inline std::optional<size_t> max_buffer_length_from_utf8_if_no_unmappables(
      size_t byte_length) const {
    size_t val = encoder_max_buffer_length_from_utf8_if_no_unmappables(
        this, byte_length);
    if (val == SIZE_MAX) {
      return std::nullopt;
    }
    return val;
  }

  /**
   * Query the worst-case output size when encoding from UTF-8 without
   * replacement.
   *
   * Returns the size of the output buffer in bytes that will not overflow
   * given the current state of the encoder and `byte_length` number of
   * additional input code units or `SIZE_MAX` if `size_t` would overflow.
   */
  inline std::optional<size_t> max_buffer_length_from_utf8_without_replacement(
      size_t byte_length) const {
    size_t val = encoder_max_buffer_length_from_utf8_without_replacement(
        this, byte_length);
    if (val == SIZE_MAX) {
      return std::nullopt;
    }
    return val;
  }

  /**
   * Incrementally encode into byte stream from UTF-8 with unmappable
   * characters replaced with HTML (decimal) numeric character references.
   *
   * See the documentation of the class for documentation for `encode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t, bool> encode_from_utf8(
      std::string_view src, gsl::span<uint8_t> dst, bool last) {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    bool had_replacements;
    uint32_t result = encoder_encode_from_utf8(
        this,
        null_to_bogus<const uint8_t>(
            reinterpret_cast<const uint8_t*>(src.data())),
        &src_read, null_to_bogus<uint8_t>(dst.data()), &dst_written, last,
        &had_replacements);
    return {result, src_read, dst_written, had_replacements};
  }

  /**
   * Incrementally encode into byte stream from UTF-8 _without replacement_.
   *
   * See the documentation of the class for documentation for `encode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t>
  encode_from_utf8_without_replacement(std::string_view src,
                                       gsl::span<uint8_t> dst, bool last) {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    uint32_t result = encoder_encode_from_utf8_without_replacement(
        this,
        null_to_bogus<const uint8_t>(
            reinterpret_cast<const uint8_t*>(src.data())),
        &src_read, null_to_bogus<uint8_t>(dst.data()), &dst_written, last);
    return {result, src_read, dst_written};
  }

  /**
   * Query the worst-case output size when encoding from UTF-16 with
   * replacement.
   *
   * Returns the size of the output buffer in bytes that will not overflow
   * given the current state of the encoder and `u16_length` number of
   * additional input code units if there are no unmappable characters in
   * the input or `SIZE_MAX` if `size_t` would overflow.
   */
  inline std::optional<size_t> max_buffer_length_from_utf16_if_no_unmappables(
      size_t u16_length) const {
    size_t val = encoder_max_buffer_length_from_utf16_if_no_unmappables(
        this, u16_length);
    if (val == SIZE_MAX) {
      return std::nullopt;
    }
    return val;
  }

  /**
   * Query the worst-case output size when encoding from UTF-16 without
   * replacement.
   *
   * Returns the size of the output buffer in bytes that will not overflow
   * given the current state of the encoder and `u16_length` number of
   * additional input code units or `SIZE_MAX` if `size_t` would overflow.
   */
  inline std::optional<size_t> max_buffer_length_from_utf16_without_replacement(
      size_t u16_length) const {
    size_t val = encoder_max_buffer_length_from_utf16_without_replacement(
        this, u16_length);
    if (val == SIZE_MAX) {
      return std::nullopt;
    }
    return val;
  }

  /**
   * Incrementally encode into byte stream from UTF-16 with unmappable
   * characters replaced with HTML (decimal) numeric character references.
   *
   * See the documentation of the class for documentation for `encode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t, bool> encode_from_utf16(
      std::u16string_view src, gsl::span<uint8_t> dst, bool last) {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    bool had_replacements;
    uint32_t result = encoder_encode_from_utf16(
        this, null_to_bogus<const char16_t>(src.data()), &src_read,
        null_to_bogus<uint8_t>(dst.data()), &dst_written, last,
        &had_replacements);
    return {result, src_read, dst_written, had_replacements};
  }

  /**
   * Incrementally encode into byte stream from UTF-16 _without replacement_.
   *
   * See the documentation of the class for documentation for `encode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t>
  encode_from_utf16_without_replacement(std::u16string_view src,
                                        gsl::span<uint8_t> dst, bool last) {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    uint32_t result = encoder_encode_from_utf16_without_replacement(
        this, null_to_bogus<const char16_t>(src.data()), &src_read,
        null_to_bogus<uint8_t>(dst.data()), &dst_written, last);
    return {result, src_read, dst_written};
  }

 private:
  /**
   * Replaces `nullptr` with a bogus pointer suitable for use as part of a
   * zero-length Rust slice.
   */
  template <class T>
  static inline T* null_to_bogus(T* ptr) {
    return ptr ? ptr : reinterpret_cast<T*>(alignof(T));
  }

  Encoder() = delete;
  Encoder(const Encoder&) = delete;
  Encoder& operator=(const Encoder&) = delete;
};

/**
 * An encoding as defined in the Encoding Standard
 * (https://encoding.spec.whatwg.org/).
 *
 * An _encoding_ defines a mapping from a byte sequence to a Unicode code point
 * sequence and, in most cases, vice versa. Each encoding has a name, an output
 * encoding, and one or more labels.
 *
 * _Labels_ are ASCII-case-insensitive strings that are used to identify an
 * encoding in formats and protocols. The _name_ of the encoding is the
 * preferred label in the case appropriate for returning from the
 * `characterSet` property of the `Document` DOM interface, except for
 * the replacement encoding whose name is not one of its labels.
 *
 * The _output encoding_ is the encoding used for form submission and URL
 * parsing on Web pages in the encoding. This is UTF-8 for the replacement,
 * UTF-16LE and UTF-16BE encodings and the encoding itself for other
 * encodings.
 *
 * # Streaming vs. Non-Streaming
 *
 * When you have the entire input in a single buffer, you can use the
 * methods `decode()`, `decode_with_bom_removal()`,
 * `decode_without_bom_handling()`,
 * `decode_without_bom_handling_and_without_replacement()` and
 * `encode()`. Unlike the rest of the API, these methods perform heap
 * allocations. You should the `Decoder` and `Encoder` objects when your input
 * is split into multiple buffers or when you want to control the allocation of
 * the output buffers.
 *
 * # Instances
 *
 * All instances of `Encoding` are statically allocated and have the process's
 * lifetime. There is precisely one unique `Encoding` instance for each
 * encoding defined in the Encoding Standard.
 *
 * To obtain a reference to a particular encoding whose identity you know at
 * compile time, use a `static` that refers to encoding. There is a `static`
 * for each encoding. The `static`s are named in all caps with hyphens
 * replaced with underscores and with `_ENCODING` appended to the
 * name. For example, if you know at compile time that you will want to
 * decode using the UTF-8 encoding, use the `UTF_8_ENCODING` `static`.
 *
 * If you don't know what encoding you need at compile time and need to
 * dynamically get an encoding by label, use `Encoding::for_label()`.
 *
 * Instances of `Encoding` can be compared with `==`.
 */
class Encoding final {
 public:
  /**
   * Implements the _get an encoding_ algorithm
   * (https://encoding.spec.whatwg.org/#concept-encoding-get).
   *
   * If, after ASCII-lowercasing and removing leading and trailing
   * whitespace, the argument matches a label defined in the Encoding
   * Standard, `const Encoding*` representing the corresponding
   * encoding is returned. If there is no match, `nullptr` is returned.
   *
   * This is the right method to use if the action upon the method returning
   * `nullptr` is to use a fallback encoding (e.g. `WINDOWS_1252_ENCODING`)
   * instead. When the action upon the method returning `nullptr` is not to
   * proceed with a fallback but to refuse processing,
   * `for_label_no_replacement()` is more appropriate.
   */
  static inline const Encoding* for_label(gsl::cstring_span<> label) {
    return encoding_for_label(
        null_to_bogus<const uint8_t>(
            reinterpret_cast<const uint8_t*>(label.data())),
        label.length());
  }

  /**
   * This method behaves the same as `for_label()`, except when `for_label()`
   * would return `REPLACEMENT_ENCODING`, this method returns `nullptr` instead.
   *
   * This method is useful in scenarios where a fatal error is required
   * upon invalid label, because in those cases the caller typically wishes
   * to treat the labels that map to the replacement encoding as fatal
   * errors, too.
   *
   * It is not OK to use this method when the action upon the method returning
   * `nullptr` is to use a fallback encoding (e.g. `WINDOWS_1252_ENCODING`). In
   * such a case, the `for_label()` method should be used instead in order to
   * avoid
   * unsafe fallback for labels that `for_label()` maps to
   * `REPLACEMENT_ENCODING`.
   */
  static inline const Encoding* for_label_no_replacement(
      gsl::cstring_span<> label) {
    return encoding_for_label_no_replacement(
        null_to_bogus<const uint8_t>(
            reinterpret_cast<const uint8_t*>(label.data())),
        label.length());
  }

  /**
   * Performs non-incremental BOM sniffing.
   *
   * The argument must either be a buffer representing the entire input
   * stream (non-streaming case) or a buffer representing at least the first
   * three bytes of the input stream (streaming case).
   *
   * Returns a std::optinal wrapping `make_tuple(UTF_8_ENCODING, 3)`,
   * `make_tuple(UTF_16LE_ENCODING, 2)` or `make_tuple(UTF_16BE_ENCODING, 3)`
   * if the argument starts with the UTF-8, UTF-16LE or UTF-16BE BOM or
   * `std::nullopt` otherwise.
   */
  static inline std::optional<
      std::tuple<gsl::not_null<const Encoding*>, size_t>>
  for_bom(gsl::span<const uint8_t> buffer) {
    size_t len = buffer.size();
    const Encoding* encoding =
        encoding_for_bom(null_to_bogus(buffer.data()), &len);
    if (encoding) {
      return std::make_tuple(gsl::not_null<const Encoding*>(encoding), len);
    }
    return std::nullopt;
  }

  /**
   * Returns the name of this encoding.
   *
   * This name is appropriate to return as-is from the DOM
   * `document.characterSet` property.
   */
  inline std::string name() const {
    std::string name(ENCODING_NAME_MAX_LENGTH, '\0');
    // http://herbsutter.com/2008/04/07/cringe-not-vectors-are-guaranteed-to-be-contiguous/#comment-483
    size_t length = encoding_name(this, reinterpret_cast<uint8_t*>(&name[0]));
    name.resize(length);
    return name;
  }

  /**
   * Checks whether the _output encoding_ of this encoding can encode every
   * Unicode code point. (Only true if the output encoding is UTF-8.)
   */
  inline bool can_encode_everything() const {
    return encoding_can_encode_everything(this);
  }

  /**
   * Checks whether the bytes 0x00...0x7F map exclusively to the characters
   * U+0000...U+007F and vice versa.
   */
  inline bool is_ascii_compatible() const {
    return encoding_is_ascii_compatible(this);
  }

  /**
   * Checks whether this encoding maps one byte to one Basic Multilingual
   * Plane code point (i.e. byte length equals decoded UTF-16 length) and
   * vice versa (for mappable characters).
   *
   * `true` iff this encoding is on the list of Legacy single-byte
   * encodings (https://encoding.spec.whatwg.org/#legacy-single-byte-encodings)
   * in the spec or x-user-defined.
   */
  inline bool is_single_byte() const { return encoding_is_single_byte(this); }

  /**
   * Returns the _output encoding_ of this encoding. This is UTF-8 for
   * UTF-16BE, UTF-16LE and replacement and the encoding itself otherwise.
   */
  inline gsl::not_null<const Encoding*> output_encoding() const {
    return gsl::not_null<const Encoding*>(encoding_output_encoding(this));
  }

  /**
   * Decode complete input to `std::string` _with BOM sniffing_ and with
   * malformed sequences replaced with the REPLACEMENT CHARACTER when the
   * entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * This method implements the (non-streaming version of) the
   * _decode_ (https://encoding.spec.whatwg.org/#decode) spec concept.
   *
   * The second item in the returned tuple is the encoding that was actually
   * used (which may differ from this encoding thanks to BOM sniffing).
   *
   * The third item in the returned tuple indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use `new_decoder()`
   * when decoding segmented input.
   */
  inline std::tuple<std::string, gsl::not_null<const Encoding*>, bool> decode(
      gsl::span<const uint8_t> bytes) const {
    auto opt = Encoding::for_bom(bytes);
    const Encoding* encoding;
    if (opt) {
      size_t bom_length;
      std::tie(encoding, bom_length) = *opt;
      bytes = bytes.subspan(bom_length);
    } else {
      encoding = this;
    }
    auto [str, had_errors] = encoding->decode_without_bom_handling(bytes);
    return {str, gsl::not_null<const Encoding*>(encoding), had_errors};
  }

  /**
   * Decode complete input to `std::string` _with BOM removal_ and with
   * malformed sequences replaced with the REPLACEMENT CHARACTER when the
   * entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the _UTF-8 decode_
   * (https://encoding.spec.whatwg.org/#utf-8-decode) spec concept.
   *
   * The second item in the returned pair indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_with_bom_removal()` when decoding segmented input.
   */
  inline std::tuple<std::string, bool> decode_with_bom_removal(
      gsl::span<const uint8_t> bytes) const {
    if (this == UTF_8_ENCODING && bytes.size() >= 3 &&
        (gsl::as_bytes(bytes.first<3>()) ==
         gsl::as_bytes(gsl::make_span("\xEF\xBB\xBF")))) {
      bytes = bytes.subspan(3, bytes.size() - 3);
    } else if (this == UTF_16LE_ENCODING && bytes.size() >= 2 &&
               (gsl::as_bytes(bytes.first<2>()) ==
                gsl::as_bytes(gsl::make_span("\xFF\xFE")))) {
      bytes = bytes.subspan(2, bytes.size() - 2);
    } else if (this == UTF_16BE_ENCODING && bytes.size() >= 2 &&
               (gsl::as_bytes(bytes.first<2>()) ==
                gsl::as_bytes(gsl::make_span("\xFE\xFF")))) {
      bytes = bytes.subspan(2, bytes.size() - 2);
    }
    return decode_without_bom_handling(bytes);
  }

  /**
   * Decode complete input to `std::string` _without BOM handling_ and
   * with malformed sequences replaced with the REPLACEMENT CHARACTER when
   * the entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the _UTF-8 decode without BOM_
   * (https://encoding.spec.whatwg.org/#utf-8-decode-without-bom) spec concept.
   *
   * The second item in the returned pair indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_without_bom_handling()` when decoding segmented input.
   */
  inline std::tuple<std::string, bool> decode_without_bom_handling(
      gsl::span<const uint8_t> bytes) const {
    auto decoder = new_decoder_without_bom_handling();
    auto needed = decoder->max_utf8_buffer_length(bytes.size());
    if (!needed) {
      throw std::overflow_error("Overflow in buffer size computation.");
    }
    std::string string(needed.value(), '\0');
    const auto [result, read, written, had_errors] = decoder->decode_to_utf8(
        bytes,
        gsl::make_span(reinterpret_cast<uint8_t*>(&string[0]), string.size()),
        true);
    assert(read == static_cast<size_t>(bytes.size()));
    assert(written <= static_cast<size_t>(string.size()));
    assert(result == INPUT_EMPTY);
    string.resize(written);
    return {string, had_errors};
  }

  /**
   * Decode complete input to `std::string` _without BOM handling_ and
   * _with malformed sequences treated as fatal_ when the entire input is
   * available as a single buffer (i.e. the end of the buffer marks the end
   * of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the _UTF-8 decode without BOM or fail_
   * (https://encoding.spec.whatwg.org/#utf-8-decode-without-bom-or-fail)
   * spec concept.
   *
   * Returns `None` if a malformed sequence was encountered and the result
   * of the decode as `Some(String)` otherwise.
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_without_bom_handling()` when decoding segmented input.
   */
  inline std::optional<std::string>
  decode_without_bom_handling_and_without_replacement(
      gsl::span<const uint8_t> bytes) const {
    auto decoder = new_decoder_without_bom_handling();
    auto needed =
        decoder->max_utf8_buffer_length_without_replacement(bytes.size());
    if (!needed) {
      throw std::overflow_error("Overflow in buffer size computation.");
    }
    std::string string(needed.value(), '\0');
    const auto [result, read, written] =
        decoder->decode_to_utf8_without_replacement(
            bytes,
            gsl::make_span(reinterpret_cast<uint8_t*>(&string[0]),
                           string.size()),
            true);
    assert(result != OUTPUT_FULL);
    if (result == INPUT_EMPTY) {
      assert(read == static_cast<size_t>(bytes.size()));
      assert(written <= static_cast<size_t>(string.size()));
      string.resize(written);
      return string;
    }
  }

  /**
   * Decode complete input to `std::u16string` _with BOM sniffing_ and with
   * malformed sequences replaced with the REPLACEMENT CHARACTER when the
   * entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * This method implements the (non-streaming version of) the
   * _decode_ (https://encoding.spec.whatwg.org/#decode) spec concept.
   *
   * The second item in the returned tuple is the encoding that was actually
   * used (which may differ from this encoding thanks to BOM sniffing).
   *
   * The third item in the returned tuple indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use `new_decoder()`
   * when decoding segmented input.
   */
  inline std::tuple<std::u16string, gsl::not_null<const Encoding*>, bool>
  decode16(gsl::span<const uint8_t> bytes) const {
    auto opt = Encoding::for_bom(bytes);
    const Encoding* encoding;
    if (opt) {
      size_t bom_length;
      std::tie(encoding, bom_length) = *opt;
      bytes = bytes.subspan(bom_length);
    } else {
      encoding = this;
    }
    auto [str, had_errors] = encoding->decode16_without_bom_handling(bytes);
    return {str, gsl::not_null<const Encoding*>(encoding), had_errors};
  }

  /**
   * Decode complete input to `std::u16string` _with BOM removal_ and with
   * malformed sequences replaced with the REPLACEMENT CHARACTER when the
   * entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the _UTF-8 decode_
   * (https://encoding.spec.whatwg.org/#utf-8-decode) spec concept.
   *
   * The second item in the returned pair indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_with_bom_removal()` when decoding segmented input.
   */
  inline std::tuple<std::u16string, bool> decode16_with_bom_removal(
      gsl::span<const uint8_t> bytes) const {
    if (this == UTF_8_ENCODING && bytes.size() >= 3 &&
        (gsl::as_bytes(bytes.first<3>()) ==
         gsl::as_bytes(gsl::make_span("\xEF\xBB\xBF")))) {
      bytes = bytes.subspan(3, bytes.size() - 3);
    } else if (this == UTF_16LE_ENCODING && bytes.size() >= 2 &&
               (gsl::as_bytes(bytes.first<2>()) ==
                gsl::as_bytes(gsl::make_span("\xFF\xFE")))) {
      bytes = bytes.subspan(2, bytes.size() - 2);
    } else if (this == UTF_16BE_ENCODING && bytes.size() >= 2 &&
               (gsl::as_bytes(bytes.first<2>()) ==
                gsl::as_bytes(gsl::make_span("\xFE\xFF")))) {
      bytes = bytes.subspan(2, bytes.size() - 2);
    }
    return decode16_without_bom_handling(bytes);
  }

  /**
   * Decode complete input to `std::u16string` _without BOM handling_ and
   * with malformed sequences replaced with the REPLACEMENT CHARACTER when
   * the entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the _UTF-8 decode without BOM_
   * (https://encoding.spec.whatwg.org/#utf-8-decode-without-bom) spec concept.
   *
   * The second item in the returned pair indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_without_bom_handling()` when decoding segmented input.
   */
  inline std::tuple<std::u16string, bool> decode16_without_bom_handling(
      gsl::span<const uint8_t> bytes) const {
    auto decoder = new_decoder_without_bom_handling();
    auto needed = decoder->max_utf16_buffer_length(bytes.size());
    if (!needed) {
      throw std::overflow_error("Overflow in buffer size computation.");
    }
    std::u16string string(needed.value(), '\0');
    const auto [result, read, written, had_errors] = decoder->decode_to_utf16(
        bytes, gsl::make_span(&string[0], string.size()), true);
    assert(read == static_cast<size_t>(bytes.size()));
    assert(written <= static_cast<size_t>(string.size()));
    assert(result == INPUT_EMPTY);
    string.resize(written);
    return {string, had_errors};
  }

  /**
   * Decode complete input to `std::u16string` _without BOM handling_ and
   * _with malformed sequences treated as fatal_ when the entire input is
   * available as a single buffer (i.e. the end of the buffer marks the end
   * of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the _UTF-8 decode without BOM or fail_
   * (https://encoding.spec.whatwg.org/#utf-8-decode-without-bom-or-fail)
   * spec concept.
   *
   * Returns `None` if a malformed sequence was encountered and the result
   * of the decode as `Some(String)` otherwise.
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_without_bom_handling()` when decoding segmented input.
   */
  inline std::optional<std::u16string>
  decode16_without_bom_handling_and_without_replacement(
      gsl::span<const uint8_t> bytes) const {
    auto decoder = new_decoder_without_bom_handling();
    auto needed = decoder->max_utf16_buffer_length(bytes.size());
    if (!needed) {
      throw std::overflow_error("Overflow in buffer size computation.");
    }
    std::u16string string(needed.value(), '\0');
    const auto [result, read, written] =
        decoder->decode_to_utf16_without_replacement(
            bytes, gsl::make_span(&string[0], string.size()), true);
    assert(result != OUTPUT_FULL);
    if (result == INPUT_EMPTY) {
      assert(read == static_cast<size_t>(bytes.size()));
      assert(written <= static_cast<size_t>(string.size()));
      string.resize(written);
      return string;
    }
  }

  /**
   * Encode complete input to `std::vector<uint8_t>` with unmappable characters
   * replaced with decimal numeric character references when the entire input
   * is available as a single buffer (i.e. the end of the buffer marks the
   * end of the stream).
   *
   * This method implements the (non-streaming version of) the
   * _encode_ (https://encoding.spec.whatwg.org/#encode) spec concept.
   *
   * The second item in the returned tuple is the encoding that was actually
   * used (which may differ from this encoding thanks to some encodings
   * having UTF-8 as their output encoding).
   *
   * The third item in the returned tuple indicates whether there were
   * unmappable characters (that were replaced with HTML numeric character
   * references).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use `new_encoder()`
   * when encoding segmented output.
   */
  inline std::tuple<std::vector<uint8_t>, gsl::not_null<const Encoding*>, bool>
  encode(std::string_view string) const {
    auto output_enc = output_encoding();
    if (output_enc == UTF_8_ENCODING) {
      std::vector<uint8_t> vec(string.size());
      std::memcpy(&vec[0], string.data(), string.size());
    }
    auto encoder = output_enc->new_encoder();
    auto needed =
        encoder->max_buffer_length_from_utf8_if_no_unmappables(string.size());
    if (!needed) {
      throw std::overflow_error("Overflow in buffer size computation.");
    }
    std::vector<uint8_t> vec(needed.value());
    bool total_had_errors = false;
    size_t total_read = 0;
    size_t total_written = 0;
    for (;;) {
      const auto [result, read, written, had_errors] =
          encoder->encode_from_utf8(string.substr(total_read),
                                    gsl::make_span(vec).subspan(total_written),
                                    true);
      total_read += read;
      total_written += written;
      total_had_errors |= had_errors;
      if (result == INPUT_EMPTY) {
        assert(total_read == static_cast<size_t>(string.size()));
        assert(total_written <= static_cast<size_t>(vec.size()));
        vec.resize(total_written);
        return {vec, gsl::not_null<const Encoding*>(output_enc),
                total_had_errors};
      }
      auto needed = encoder->max_buffer_length_from_utf8_if_no_unmappables(
          string.size() - total_read);
      if (!needed) {
        throw std::overflow_error("Overflow in buffer size computation.");
      }
      vec.resize(total_written + needed.value());
    }
  }

  /**
   * Encode complete input to `std::vector<uint8_t>` with unmappable characters
   * replaced with decimal numeric character references when the entire input
   * is available as a single buffer (i.e. the end of the buffer marks the
   * end of the stream).
   *
   * This method implements the (non-streaming version of) the
   * _encode_ (https://encoding.spec.whatwg.org/#encode) spec concept.
   *
   * The second item in the returned tuple is the encoding that was actually
   * used (which may differ from this encoding thanks to some encodings
   * having UTF-8 as their output encoding).
   *
   * The third item in the returned tuple indicates whether there were
   * unmappable characters (that were replaced with HTML numeric character
   * references).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use `new_encoder()`
   * when encoding segmented output.
   */
  inline std::tuple<std::vector<uint8_t>, gsl::not_null<const Encoding*>, bool>
  encode(std::u16string_view string) const {
    auto output_enc = output_encoding();
    auto encoder = output_enc->new_encoder();
    auto needed =
        encoder->max_buffer_length_from_utf16_if_no_unmappables(string.size());
    if (!needed) {
      throw std::overflow_error("Overflow in buffer size computation.");
    }
    std::vector<uint8_t> vec(needed.value());
    bool total_had_errors = false;
    size_t total_read = 0;
    size_t total_written = 0;
    for (;;) {
      const auto [result, read, written, had_errors] =
          encoder->encode_from_utf16(string.substr(total_read),
                                     gsl::make_span(vec).subspan(total_written),
                                     true);
      total_read += read;
      total_written += written;
      total_had_errors |= had_errors;
      if (result == INPUT_EMPTY) {
        assert(total_read == static_cast<size_t>(string.size()));
        assert(total_written <= static_cast<size_t>(vec.size()));
        vec.resize(total_written);
        return {vec, gsl::not_null<const Encoding*>(output_enc),
                total_had_errors};
      }
      auto needed = encoder->max_buffer_length_from_utf16_if_no_unmappables(
          string.size() - total_read);
      if (!needed) {
        throw std::overflow_error("Overflow in buffer size computation.");
      }
      vec.resize(total_written + needed.value());
    }
  }

  /**
   * Instantiates a new decoder for this encoding with BOM sniffing enabled.
   *
   * BOM sniffing may cause the returned decoder to morph into a decoder
   * for UTF-8, UTF-16LE or UTF-16BE instead of this encoding.
   */
  inline std::unique_ptr<Decoder> new_decoder() const {
    return std::unique_ptr<Decoder>(encoding_new_decoder(this));
  }

  /**
   * Instantiates a new decoder for this encoding with BOM sniffing enabled
   * into memory occupied by a previously-instantiated decoder.
   *
   * BOM sniffing may cause the returned decoder to morph into a decoder
   * for UTF-8, UTF-16LE or UTF-16BE instead of this encoding.
   */
  inline void new_decoder_into(Decoder& decoder) const {
    encoding_new_decoder_into(this, &decoder);
  }

  /**
   * Instantiates a new decoder for this encoding with BOM removal.
   *
   * If the input starts with bytes that are the BOM for this encoding,
   * those bytes are removed. However, the decoder never morphs into a
   * decoder for another encoding: A BOM for another encoding is treated as
   * (potentially malformed) input to the decoding algorithm for this
   * encoding.
   */
  inline std::unique_ptr<Decoder> new_decoder_with_bom_removal() const {
    return std::unique_ptr<Decoder>(
        encoding_new_decoder_with_bom_removal(this));
  }

  /**
   * Instantiates a new decoder for this encoding with BOM removal
   * into memory occupied by a previously-instantiated decoder.
   *
   * If the input starts with bytes that are the BOM for this encoding,
   * those bytes are removed. However, the decoder never morphs into a
   * decoder for another encoding: A BOM for another encoding is treated as
   * (potentially malformed) input to the decoding algorithm for this
   * encoding.
   */
  inline void new_decoder_with_bom_removal_into(Decoder& decoder) const {
    encoding_new_decoder_with_bom_removal_into(this, &decoder);
  }

  /**
   * Instantiates a new decoder for this encoding with BOM handling disabled.
   *
   * If the input starts with bytes that look like a BOM, those bytes are
   * not treated as a BOM. (Hence, the decoder never morphs into a decoder
   * for another encoding.)
   *
   * _Note:_ If the caller has performed BOM sniffing on its own but has not
   * removed the BOM, the caller should use `new_decoder_with_bom_removal()`
   * instead of this method to cause the BOM to be removed.
   */
  inline std::unique_ptr<Decoder> new_decoder_without_bom_handling() const {
    return std::unique_ptr<Decoder>(
        encoding_new_decoder_without_bom_handling(this));
  }

  /**
   * Instantiates a new decoder for this encoding with BOM handling disabled
   * into memory occupied by a previously-instantiated decoder.
   *
   * If the input starts with bytes that look like a BOM, those bytes are
   * not treated as a BOM. (Hence, the decoder never morphs into a decoder
   * for another encoding.)
   *
   * _Note:_ If the caller has performed BOM sniffing on its own but has not
   * removed the BOM, the caller should use
   * `new_decoder_with_bom_removal_into()`
   * instead of this method to cause the BOM to be removed.
   */
  inline void new_decoder_without_bom_handling_into(Decoder& decoder) const {
    encoding_new_decoder_without_bom_handling_into(this, &decoder);
  }

  /**
   * Instantiates a new encoder for the output encoding of this encoding.
   */
  inline std::unique_ptr<Encoder> new_encoder() const {
    return std::unique_ptr<Encoder>(encoding_new_encoder(this));
  }

  /**
   * Instantiates a new encoder for the output encoding of this encoding
   * into memory occupied by a previously-instantiated encoder.
   */
  inline void new_encoder_into(Encoder& encoder) const {
    encoding_new_encoder_into(this, &encoder);
  }

  /**
   * Validates UTF-8.
   *
   * Returns the index of the first byte that makes the input malformed as
   * UTF-8 or the length of the input if the input is entirely valid.
   */
  static inline size_t utf8_valid_up_to(gsl::span<const uint8_t> buffer) {
    return encoding_utf8_valid_up_to(
        null_to_bogus<const uint8_t>(buffer.data()), buffer.size());
  }

  /**
   * Validates ASCII.
   *
   * Returns the index of the first byte that makes the input malformed as
   * ASCII or the length of the input if the input is entirely valid.
   */
  static inline size_t ascii_valid_up_to(gsl::span<const uint8_t> buffer) {
    return encoding_ascii_valid_up_to(
        null_to_bogus<const uint8_t>(buffer.data()), buffer.size());
  }

  /**
   * Validates ISO-2022-JP ASCII-state data.
   *
   * Returns the index of the first byte that makes the input not
   * representable in the ASCII state of ISO-2022-JP or the length of the
   * input if the input is entirely representable in the ASCII state of
   * ISO-2022-JP.
   */
  static inline size_t iso_2022_jp_ascii_valid_up_to(
      gsl::span<const uint8_t> buffer) {
    return encoding_iso_2022_jp_ascii_valid_up_to(
        null_to_bogus<const uint8_t>(buffer.data()), buffer.size());
  }

 private:
  /**
   * Replaces `nullptr` with a bogus pointer suitable for use as part of a
   * zero-length Rust slice.
   */
  template <class T>
  static inline T* null_to_bogus(T* ptr) {
    return ptr ? ptr : reinterpret_cast<T*>(alignof(T));
  }

  Encoding() = delete;
  Encoding(const Encoding&) = delete;
  Encoding& operator=(const Encoding&) = delete;
  ~Encoding() = delete;
};

};  // namespace encoding_rs

#endif  // encoding_rs_cpp_h_
