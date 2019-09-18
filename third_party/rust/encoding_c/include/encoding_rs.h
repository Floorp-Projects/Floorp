
// Copyright 2015-2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
// Instead, please regenerate using encoding_c/build.rs.

#ifndef cheddar_generated_encoding_rs_h
#define cheddar_generated_encoding_rs_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "encoding_rs_statics.h"

/// Implements the
/// [_get an encoding_](https://encoding.spec.whatwg.org/#concept-encoding-get)
/// algorithm.
///
/// If, after ASCII-lowercasing and removing leading and trailing
/// whitespace, the argument matches a label defined in the ENCODING_RS_ENCODING
/// Standard, `const ENCODING_RS_ENCODING*` representing the corresponding
/// encoding is returned. If there is no match, `NULL` is returned.
///
/// This is the right function to use if the action upon the method returning
/// `NULL` is to use a fallback encoding (e.g. `WINDOWS_1252_ENCODING`) instead.
/// When the action upon the method returning `NULL` is not to proceed with
/// a fallback but to refuse processing, `encoding_for_label_no_replacement()`
/// is more appropriate.
///
/// The argument buffer can be in any ASCII-compatible encoding. It is not
/// required to be UTF-8.
///
/// `label` must be non-`NULL` even if `label_len` is zero. When `label_len`
/// is zero, it is OK for `label` to be something non-dereferencable,
/// such as `0x1`. This is required due to Rust's optimization for slices
/// within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if `label` and `label_len` don't designate a valid memory block
/// of if `label` is `NULL`.
ENCODING_RS_ENCODING const* encoding_for_label(uint8_t const* label,
                                               size_t label_len);

/// This function behaves the same as `encoding_for_label()`, except when
/// `encoding_for_label()` would return `REPLACEMENT_ENCODING`, this method
/// returns `NULL` instead.
///
/// This method is useful in scenarios where a fatal error is required
/// upon invalid label, because in those cases the caller typically wishes
/// to treat the labels that map to the replacement encoding as fatal
/// errors, too.
///
/// It is not OK to use this funciton when the action upon the method returning
/// `NULL` is to use a fallback encoding (e.g. `WINDOWS_1252_ENCODING`). In
/// such a case, the `encoding_for_label()` function should be used instead
/// in order to avoid unsafe fallback for labels that `encoding_for_label()`
/// maps to `REPLACEMENT_ENCODING`.
///
/// The argument buffer can be in any ASCII-compatible encoding. It is not
/// required to be UTF-8.
///
/// `label` must be non-`NULL` even if `label_len` is zero. When `label_len`
/// is zero, it is OK for `label` to be something non-dereferencable,
/// such as `0x1`. This is required due to Rust's optimization for slices
/// within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if `label` and `label_len` don't designate a valid memory block
/// of if `label` is `NULL`.
ENCODING_RS_ENCODING const* encoding_for_label_no_replacement(
    uint8_t const* label, size_t label_len);

/// Performs non-incremental BOM sniffing.
///
/// The argument must either be a buffer representing the entire input
/// stream (non-streaming case) or a buffer representing at least the first
/// three bytes of the input stream (streaming case).
///
/// Returns `UTF_8_ENCODING`, `UTF_16LE_ENCODING` or `UTF_16BE_ENCODING` if the
/// argument starts with the UTF-8, UTF-16LE or UTF-16BE BOM or `NULL`
/// otherwise. Upon return, `*buffer_len` is the length of the BOM (zero if
/// there is no BOM).
///
/// `buffer` must be non-`NULL` even if `*buffer_len` is zero. When
/// `*buffer_len` is zero, it is OK for `buffer` to be something
/// non-dereferencable, such as `0x1`. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `*buffer_len` don't designate a valid memory
/// block of if `buffer` is `NULL`.
ENCODING_RS_ENCODING const* encoding_for_bom(uint8_t const* buffer,
                                             size_t* buffer_len);

/// Writes the name of the given `ENCODING_RS_ENCODING` to a caller-supplied
/// buffer as ASCII and returns the number of bytes / ASCII characters written.
///
/// The output is not null-terminated.
///
/// The caller _MUST_ ensure that `name_out` points to a buffer whose length
/// is at least `ENCODING_NAME_MAX_LENGTH` bytes.
///
/// # Undefined behavior
///
/// UB ensues if either argument is `NULL` or if `name_out` doesn't point to
/// a valid block of memory whose length is at least
/// `ENCODING_NAME_MAX_LENGTH` bytes.
size_t encoding_name(ENCODING_RS_ENCODING const* encoding, uint8_t* name_out);

/// Checks whether the _output encoding_ of this encoding can encode every
/// Unicode scalar. (Only true if the output encoding is UTF-8.)
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
bool encoding_can_encode_everything(ENCODING_RS_ENCODING const* encoding);

/// Checks whether the bytes 0x00...0x7F map exclusively to the characters
/// U+0000...U+007F and vice versa.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
bool encoding_is_ascii_compatible(ENCODING_RS_ENCODING const* encoding);

/// Checks whether this encoding maps one byte to one Basic Multilingual
/// Plane code point (i.e. byte length equals decoded UTF-16 length) and
/// vice versa (for mappable characters).
///
/// `true` iff this encoding is on the list of [Legacy single-byte
/// encodings](https://encoding.spec.whatwg.org/#legacy-single-byte-encodings)
/// in the spec or x-user-defined.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
bool encoding_is_single_byte(ENCODING_RS_ENCODING const* encoding);

/// Returns the _output encoding_ of this encoding. This is UTF-8 for
/// UTF-16BE, UTF-16LE and replacement and the encoding itself otherwise.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
ENCODING_RS_ENCODING const* encoding_output_encoding(
    ENCODING_RS_ENCODING const* encoding);

/// Allocates a new `ENCODING_RS_DECODER` for the given `ENCODING_RS_ENCODING`
/// on the heap with BOM sniffing enabled and returns a pointer to the
/// newly-allocated `ENCODING_RS_DECODER`.
///
/// BOM sniffing may cause the returned decoder to morph into a decoder
/// for UTF-8, UTF-16LE or UTF-16BE instead of this encoding.
///
/// Once the allocated `ENCODING_RS_DECODER` is no longer needed, the caller
/// _MUST_ deallocate it by passing the pointer returned by this function to
/// `decoder_free()`.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
ENCODING_RS_DECODER* encoding_new_decoder(ENCODING_RS_ENCODING const* encoding);

/// Allocates a new `ENCODING_RS_DECODER` for the given `ENCODING_RS_ENCODING`
/// on the heap with BOM removal and returns a pointer to the newly-allocated
/// `ENCODING_RS_DECODER`.
///
/// If the input starts with bytes that are the BOM for this encoding,
/// those bytes are removed. However, the decoder never morphs into a
/// decoder for another encoding: A BOM for another encoding is treated as
/// (potentially malformed) input to the decoding algorithm for this
/// encoding.
///
/// Once the allocated `ENCODING_RS_DECODER` is no longer needed, the caller
/// _MUST_ deallocate it by passing the pointer returned by this function to
/// `decoder_free()`.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
ENCODING_RS_DECODER* encoding_new_decoder_with_bom_removal(
    ENCODING_RS_ENCODING const* encoding);

/// Allocates a new `ENCODING_RS_DECODER` for the given `ENCODING_RS_ENCODING`
/// on the heap with BOM handling disabled and returns a pointer to the
/// newly-allocated `ENCODING_RS_DECODER`.
///
/// If the input starts with bytes that look like a BOM, those bytes are
/// not treated as a BOM. (Hence, the decoder never morphs into a decoder
/// for another encoding.)
///
/// _Note:_ If the caller has performed BOM sniffing on its own but has not
/// removed the BOM, the caller should use
/// `encoding_new_decoder_with_bom_removal()` instead of this function to cause
/// the BOM to be removed.
///
/// Once the allocated `ENCODING_RS_DECODER` is no longer needed, the caller
/// _MUST_ deallocate it by passing the pointer returned by this function to
/// `decoder_free()`.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
ENCODING_RS_DECODER* encoding_new_decoder_without_bom_handling(
    ENCODING_RS_ENCODING const* encoding);

/// Allocates a new `ENCODING_RS_DECODER` for the given `ENCODING_RS_ENCODING`
/// into memory provided by the caller with BOM sniffing enabled. (In practice,
/// the target should likely be a pointer previously returned by
/// `encoding_new_decoder()`.)
///
/// Note: If the caller has already performed BOM sniffing but has
/// not removed the BOM, the caller should still use this function in
/// order to cause the BOM to be ignored.
///
/// # Undefined behavior
///
/// UB ensues if either argument is `NULL`.
void encoding_new_decoder_into(ENCODING_RS_ENCODING const* encoding,
                               ENCODING_RS_DECODER* decoder);

/// Allocates a new `ENCODING_RS_DECODER` for the given `ENCODING_RS_ENCODING`
/// into memory provided by the caller with BOM removal.
///
/// If the input starts with bytes that are the BOM for this encoding,
/// those bytes are removed. However, the decoder never morphs into a
/// decoder for another encoding: A BOM for another encoding is treated as
/// (potentially malformed) input to the decoding algorithm for this
/// encoding.
///
/// Once the allocated `ENCODING_RS_DECODER` is no longer needed, the caller
/// _MUST_ deallocate it by passing the pointer returned by this function to
/// `decoder_free()`.
///
/// # Undefined behavior
///
/// UB ensues if either argument is `NULL`.
void encoding_new_decoder_with_bom_removal_into(
    ENCODING_RS_ENCODING const* encoding, ENCODING_RS_DECODER* decoder);

/// Allocates a new `ENCODING_RS_DECODER` for the given `ENCODING_RS_ENCODING`
/// into memory provided by the caller with BOM handling disabled.
///
/// If the input starts with bytes that look like a BOM, those bytes are
/// not treated as a BOM. (Hence, the decoder never morphs into a decoder
/// for another encoding.)
///
/// _Note:_ If the caller has performed BOM sniffing on its own but has not
/// removed the BOM, the caller should use
/// `encoding_new_decoder_with_bom_removal_into()` instead of this function to
/// cause the BOM to be removed.
///
/// # Undefined behavior
///
/// UB ensues if either argument is `NULL`.
void encoding_new_decoder_without_bom_handling_into(
    ENCODING_RS_ENCODING const* encoding, ENCODING_RS_DECODER* decoder);

/// Allocates a new `ENCODING_RS_ENCODER` for the given `ENCODING_RS_ENCODING`
/// on the heap and returns a pointer to the newly-allocated
/// `ENCODING_RS_ENCODER`. (Exception, if the `ENCODING_RS_ENCODING` is
/// `replacement`, a new `ENCODING_RS_DECODER` for UTF-8 is instantiated (and
/// that `ENCODING_RS_DECODER` reports `UTF_8` as its `ENCODING_RS_ENCODING`).
///
/// Once the allocated `ENCODING_RS_ENCODER` is no longer needed, the caller
/// _MUST_ deallocate it by passing the pointer returned by this function to
/// `encoder_free()`.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
ENCODING_RS_ENCODER* encoding_new_encoder(ENCODING_RS_ENCODING const* encoding);

/// Allocates a new `ENCODING_RS_ENCODER` for the given `ENCODING_RS_ENCODING`
/// into memory provided by the caller. (In practice, the target should likely
/// be a pointer previously returned by `encoding_new_encoder()`.)
///
/// # Undefined behavior
///
/// UB ensues if either argument is `NULL`.
void encoding_new_encoder_into(ENCODING_RS_ENCODING const* encoding,
                               ENCODING_RS_ENCODER* encoder);

/// Validates UTF-8.
///
/// Returns the index of the first byte that makes the input malformed as
/// UTF-8 or `buffer_len` if `buffer` is entirely valid.
///
/// `buffer` must be non-`NULL` even if `buffer_len` is zero. When
/// `buffer_len` is zero, it is OK for `buffer` to be something
/// non-dereferencable, such as `0x1`. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory
/// block of if `buffer` is `NULL`.
size_t encoding_utf8_valid_up_to(uint8_t const* buffer, size_t buffer_len);

/// Validates ASCII.
///
/// Returns the index of the first byte that makes the input malformed as
/// ASCII or `buffer_len` if `buffer` is entirely valid.
///
/// `buffer` must be non-`NULL` even if `buffer_len` is zero. When
/// `buffer_len` is zero, it is OK for `buffer` to be something
/// non-dereferencable, such as `0x1`. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory
/// block of if `buffer` is `NULL`.
size_t encoding_ascii_valid_up_to(uint8_t const* buffer, size_t buffer_len);

/// Validates ISO-2022-JP ASCII-state data.
///
/// Returns the index of the first byte that makes the input not representable
/// in the ASCII state of ISO-2022-JP or `buffer_len` if `buffer` is entirely
/// representable in the ASCII state of ISO-2022-JP.
///
/// `buffer` must be non-`NULL` even if `buffer_len` is zero. When
/// `buffer_len` is zero, it is OK for `buffer` to be something
/// non-dereferencable, such as `0x1`. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory
/// block of if `buffer` is `NULL`.
size_t encoding_iso_2022_jp_ascii_valid_up_to(uint8_t const* buffer,
                                              size_t buffer_len);

/// Deallocates a `ENCODING_RS_DECODER` previously allocated by
/// `encoding_new_decoder()`.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
void decoder_free(ENCODING_RS_DECODER* decoder);

/// The `ENCODING_RS_ENCODING` this `ENCODING_RS_DECODER` is for.
///
/// BOM sniffing can change the return value of this method during the life
/// of the decoder.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
ENCODING_RS_ENCODING const* decoder_encoding(
    ENCODING_RS_DECODER const* decoder);

/// Query the worst-case UTF-8 output size _with replacement_.
///
/// Returns the size of the output buffer in UTF-8 code units (`uint8_t`)
/// that will not overflow given the current state of the decoder and
/// `byte_length` number of additional input bytes when decoding with
/// errors handled by outputting a REPLACEMENT CHARACTER for each malformed
/// sequence or `SIZE_MAX` if `size_t` would overflow.
///
/// # Undefined behavior
///
/// UB ensues if `decoder` is `NULL`.
size_t decoder_max_utf8_buffer_length(ENCODING_RS_DECODER const* decoder,
                                      size_t byte_length);

/// Query the worst-case UTF-8 output size _without replacement_.
///
/// Returns the size of the output buffer in UTF-8 code units (`uint8_t`)
/// that will not overflow given the current state of the decoder and
/// `byte_length` number of additional input bytes when decoding without
/// replacement error handling or `SIZE_MAX` if `size_t` would overflow.
///
/// Note that this value may be too small for the `_with_replacement` case.
/// Use `decoder_max_utf8_buffer_length()` for that case.
///
/// # Undefined behavior
///
/// UB ensues if `decoder` is `NULL`.
size_t decoder_max_utf8_buffer_length_without_replacement(
    ENCODING_RS_DECODER const* decoder, size_t byte_length);

/// Incrementally decode a byte stream into UTF-8 with malformed sequences
/// replaced with the REPLACEMENT CHARACTER.
///
/// See the top-level FFI documentation for documentation for how the
/// `decoder_decode_*` functions are mapped from Rust and the documentation
/// for the [`ENCODING_RS_DECODER`][1] struct for the semantics.
///
/// `src` must be non-`NULL` even if `src_len` is zero. When`src_len` is zero,
/// it is OK for `src` to be something non-dereferencable, such as `0x1`.
/// Likewise for `dst` when `dst_len` is zero. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if any of the pointer arguments is `NULL`, `src` and `src_len`
/// don't designate a valid block of memory or `dst` and `dst_len` don't
/// designate a valid block of memory.
///
/// [1]: https://docs.rs/encoding_rs/0.6.10/encoding_rs/struct.Decoder.html
uint32_t decoder_decode_to_utf8(ENCODING_RS_DECODER* decoder,
                                uint8_t const* src, size_t* src_len,
                                uint8_t* dst, size_t* dst_len, bool last,
                                bool* had_replacements);

/// Incrementally decode a byte stream into UTF-8 _without replacement_.
///
/// See the top-level FFI documentation for documentation for how the
/// `decoder_decode_*` functions are mapped from Rust and the documentation
/// for the [`ENCODING_RS_DECODER`][1] struct for the semantics.
///
/// `src` must be non-`NULL` even if `src_len` is zero. When`src_len` is zero,
/// it is OK for `src` to be something non-dereferencable, such as `0x1`.
/// Likewise for `dst` when `dst_len` is zero. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if any of the pointer arguments is `NULL`, `src` and `src_len`
/// don't designate a valid block of memory or `dst` and `dst_len` don't
/// designate a valid block of memory.
///
/// [1]: https://docs.rs/encoding_rs/0.6.10/encoding_rs/struct.Decoder.html
uint32_t decoder_decode_to_utf8_without_replacement(
    ENCODING_RS_DECODER* decoder, uint8_t const* src, size_t* src_len,
    uint8_t* dst, size_t* dst_len, bool last);

/// Query the worst-case UTF-16 output size (with or without replacement).
///
/// Returns the size of the output buffer in UTF-16 code units (`char16_t`)
/// that will not overflow given the current state of the decoder and
/// `byte_length` number of additional input bytes or `SIZE_MAX` if `size_t`
/// would overflow.
///
/// Since the REPLACEMENT CHARACTER fits into one UTF-16 code unit, the
/// return value of this method applies also in the
/// `_without_replacement` case.
///
/// # Undefined behavior
///
/// UB ensues if `decoder` is `NULL`.
size_t decoder_max_utf16_buffer_length(ENCODING_RS_DECODER const* decoder,
                                       size_t u16_length);

/// Incrementally decode a byte stream into UTF-16 with malformed sequences
/// replaced with the REPLACEMENT CHARACTER.
///
/// See the top-level FFI documentation for documentation for how the
/// `decoder_decode_*` functions are mapped from Rust and the documentation
/// for the [`ENCODING_RS_DECODER`][1] struct for the semantics.
///
/// `src` must be non-`NULL` even if `src_len` is zero. When`src_len` is zero,
/// it is OK for `src` to be something non-dereferencable, such as `0x1`.
/// Likewise for `dst` when `dst_len` is zero. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if any of the pointer arguments is `NULL`, `src` and `src_len`
/// don't designate a valid block of memory or `dst` and `dst_len` don't
/// designate a valid block of memory.
///
/// [1]: https://docs.rs/encoding_rs/0.6.10/encoding_rs/struct.Decoder.html
uint32_t decoder_decode_to_utf16(ENCODING_RS_DECODER* decoder,
                                 uint8_t const* src, size_t* src_len,
                                 char16_t* dst, size_t* dst_len, bool last,
                                 bool* had_replacements);

/// Incrementally decode a byte stream into UTF-16 _without replacement_.
///
/// See the top-level FFI documentation for documentation for how the
/// `decoder_decode_*` functions are mapped from Rust and the documentation
/// for the [`ENCODING_RS_DECODER`][1] struct for the semantics.
///
/// `src` must be non-`NULL` even if `src_len` is zero. When`src_len` is zero,
/// it is OK for `src` to be something non-dereferencable, such as `0x1`.
/// Likewise for `dst` when `dst_len` is zero. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if any of the pointer arguments is `NULL`, `src` and `src_len`
/// don't designate a valid block of memory or `dst` and `dst_len` don't
/// designate a valid block of memory.
///
/// [1]: https://docs.rs/encoding_rs/0.6.10/encoding_rs/struct.Decoder.html
uint32_t decoder_decode_to_utf16_without_replacement(
    ENCODING_RS_DECODER* decoder, uint8_t const* src, size_t* src_len,
    char16_t* dst, size_t* dst_len, bool last);

/// Checks for compatibility with storing Unicode scalar values as unsigned
/// bytes taking into account the state of the decoder.
///
/// Returns `SIZE_MAX` if the decoder is not in a neutral state, including waiting
/// for the BOM or if the encoding is never Latin-byte-compatible.
///
/// Otherwise returns the index of the first byte whose unsigned value doesn't
/// directly correspond to the decoded Unicode scalar value, or the length
/// of the input if all bytes in the input decode directly to scalar values
/// corresponding to the unsigned byte values.
///
/// Does not change the state of the decoder.
///
/// Do not use this unless you are supporting SpiderMonkey/V8-style string
/// storage optimizations.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `*buffer_len` don't designate a valid memory
/// block of if `buffer` is `NULL`.
size_t decoder_latin1_byte_compatible_up_to(ENCODING_RS_DECODER const* decoder,
                                            uint8_t const* buffer,
                                            size_t buffer_len);

/// Deallocates an `ENCODING_RS_ENCODER` previously allocated by
/// `encoding_new_encoder()`.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
void encoder_free(ENCODING_RS_ENCODER* encoder);

/// The `ENCODING_RS_ENCODING` this `ENCODING_RS_ENCODER` is for.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
ENCODING_RS_ENCODING const* encoder_encoding(
    ENCODING_RS_ENCODER const* encoder);

/// Returns `true` if this is an ISO-2022-JP encoder that's not in the
/// ASCII state and `false` otherwise.
///
/// # Undefined behavior
///
/// UB ensues if the argument is `NULL`.
bool encoder_has_pending_state(ENCODING_RS_ENCODER const* encoder);

/// Query the worst-case output size when encoding from UTF-8 with
/// replacement.
///
/// Returns the size of the output buffer in bytes that will not overflow
/// given the current state of the encoder and `byte_length` number of
/// additional input code units if there are no unmappable characters in
/// the input or `SIZE_MAX` if `size_t` would overflow.
size_t encoder_max_buffer_length_from_utf8_if_no_unmappables(
    ENCODING_RS_ENCODER const* encoder, size_t byte_length);

/// Query the worst-case output size when encoding from UTF-8 without
/// replacement.
///
/// Returns the size of the output buffer in bytes that will not overflow
/// given the current state of the encoder and `byte_length` number of
/// additional input code units or `SIZE_MAX` if `size_t` would overflow.
size_t encoder_max_buffer_length_from_utf8_without_replacement(
    ENCODING_RS_ENCODER const* encoder, size_t byte_length);

/// Incrementally encode into byte stream from UTF-8 with unmappable
/// characters replaced with HTML (decimal) numeric character references.
///
/// The input absolutely _MUST_ be valid UTF-8 or the behavior is memory-unsafe!
/// If in doubt, check the validity of input before using!
///
/// See the top-level FFI documentation for documentation for how the
/// `encoder_encode_*` functions are mapped from Rust and the documentation
/// for the [`ENCODING_RS_ENCODER`][1] struct for the semantics.
///
/// `src` must be non-`NULL` even if `src_len` is zero. When`src_len` is zero,
/// it is OK for `src` to be something non-dereferencable, such as `0x1`.
/// Likewise for `dst` when `dst_len` is zero. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if any of the pointer arguments is `NULL`, `src` and `src_len`
/// don't designate a valid block of memory or `dst` and `dst_len` don't
/// designate a valid block of memory.
///
/// [1]: https://docs.rs/encoding_rs/0.6.10/encoding_rs/struct.Encoder.html
uint32_t encoder_encode_from_utf8(ENCODING_RS_ENCODER* encoder,
                                  uint8_t const* src, size_t* src_len,
                                  uint8_t* dst, size_t* dst_len, bool last,
                                  bool* had_replacements);

/// Incrementally encode into byte stream from UTF-8 _without replacement_.
///
/// See the top-level FFI documentation for documentation for how the
/// `encoder_encode_*` functions are mapped from Rust and the documentation
/// for the [`ENCODING_RS_ENCODER`][1] struct for the semantics.
///
/// The input absolutely _MUST_ be valid UTF-8 or the behavior is memory-unsafe!
/// If in doubt, check the validity of input before using!
///
/// `src` must be non-`NULL` even if `src_len` is zero. When`src_len` is zero,
/// it is OK for `src` to be something non-dereferencable, such as `0x1`.
/// Likewise for `dst` when `dst_len` is zero. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if any of the pointer arguments is `NULL`, `src` and `src_len`
/// don't designate a valid block of memory or `dst` and `dst_len` don't
/// designate a valid block of memory.
///
/// [1]: https://docs.rs/encoding_rs/0.6.10/encoding_rs/struct.Encoder.html
uint32_t encoder_encode_from_utf8_without_replacement(
    ENCODING_RS_ENCODER* encoder, uint8_t const* src, size_t* src_len,
    uint8_t* dst, size_t* dst_len, bool last);

/// Query the worst-case output size when encoding from UTF-16 with
/// replacement.
///
/// Returns the size of the output buffer in bytes that will not overflow
/// given the current state of the encoder and `u16_length` number of
/// additional input code units if there are no unmappable characters in
/// the input or `SIZE_MAX` if `size_t` would overflow.
size_t encoder_max_buffer_length_from_utf16_if_no_unmappables(
    ENCODING_RS_ENCODER const* encoder, size_t u16_length);

/// Query the worst-case output size when encoding from UTF-16 without
/// replacement.
///
/// Returns the size of the output buffer in bytes that will not overflow
/// given the current state of the encoder and `u16_length` number of
/// additional input code units or `SIZE_MAX` if `size_t` would overflow.
size_t encoder_max_buffer_length_from_utf16_without_replacement(
    ENCODING_RS_ENCODER const* encoder, size_t u16_length);

/// Incrementally encode into byte stream from UTF-16 with unmappable
/// characters replaced with HTML (decimal) numeric character references.
///
/// See the top-level FFI documentation for documentation for how the
/// `encoder_encode_*` functions are mapped from Rust and the documentation
/// for the [`ENCODING_RS_ENCODER`][1] struct for the semantics.
///
/// `src` must be non-`NULL` even if `src_len` is zero. When`src_len` is zero,
/// it is OK for `src` to be something non-dereferencable, such as `0x1`.
/// Likewise for `dst` when `dst_len` is zero. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if any of the pointer arguments is `NULL`, `src` and `src_len`
/// don't designate a valid block of memory or `dst` and `dst_len` don't
/// designate a valid block of memory.
///
/// [1]: https://docs.rs/encoding_rs/0.6.10/encoding_rs/struct.Encoder.html
uint32_t encoder_encode_from_utf16(ENCODING_RS_ENCODER* encoder,
                                   char16_t const* src, size_t* src_len,
                                   uint8_t* dst, size_t* dst_len, bool last,
                                   bool* had_replacements);

/// Incrementally encode into byte stream from UTF-16 _without replacement_.
///
/// See the top-level FFI documentation for documentation for how the
/// `encoder_encode_*` functions are mapped from Rust and the documentation
/// for the [`ENCODING_RS_ENCODER`][1] struct for the semantics.
///
/// `src` must be non-`NULL` even if `src_len` is zero. When`src_len` is zero,
/// it is OK for `src` to be something non-dereferencable, such as `0x1`.
/// Likewise for `dst` when `dst_len` is zero. This is required due to Rust's
/// optimization for slices within `Option`.
///
/// # Undefined behavior
///
/// UB ensues if any of the pointer arguments is `NULL`, `src` and `src_len`
/// don't designate a valid block of memory or `dst` and `dst_len` don't
/// designate a valid block of memory.
///
/// [1]: https://docs.rs/encoding_rs/0.6.10/encoding_rs/struct.Encoder.html
uint32_t encoder_encode_from_utf16_without_replacement(
    ENCODING_RS_ENCODER* encoder, char16_t const* src, size_t* src_len,
    uint8_t* dst, size_t* dst_len, bool last);

#ifdef __cplusplus
}
#endif

#endif
