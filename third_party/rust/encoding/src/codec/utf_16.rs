// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! UTF-16.

use std::convert::Into;
use std::marker::PhantomData;
use util::as_char;
use types::*;

/// An implementation type for little endian.
///
/// Can be used as a type parameter to `UTF16Encoding`, `UTF16Encoder` and `UTF16Decoder`.
#[derive(Clone, Copy)]
pub struct Little;

/// An implementation type for big endian.
///
/// Can be used as a type parameter to `UTF16Encoding`, `UTF16Encoder` and `UTF16Decoder`.
#[derive(Clone, Copy)]
pub struct Big;

/// An internal trait used to customize UTF-16 implementations.
#[doc(hidden)] // XXX never intended to be used publicly, should be gone later
pub trait Endian: Clone + 'static {
    fn name() -> &'static str;
    fn whatwg_name() -> Option<&'static str>;
    fn write_two_bytes(output: &mut ByteWriter, msb: u8, lsb: u8);
    fn concat_two_bytes(lead: u16, trail: u8) -> u16;
}

impl Endian for Little {
    fn name() -> &'static str { "utf-16le" }
    fn whatwg_name() -> Option<&'static str> { Some("utf-16le") }
    fn write_two_bytes(output: &mut ByteWriter, msb: u8, lsb: u8) {
        output.write_byte(lsb);
        output.write_byte(msb);
    }
    fn concat_two_bytes(lead: u16, trail: u8) -> u16 {
        lead | ((trail as u16) << 8)
    }
}

impl Endian for Big {
    fn name() -> &'static str { "utf-16be" }
    fn whatwg_name() -> Option<&'static str> { Some("utf-16be") }
    fn write_two_bytes(output: &mut ByteWriter, msb: u8, lsb: u8) {
        output.write_byte(msb);
        output.write_byte(lsb);
    }
    fn concat_two_bytes(lead: u16, trail: u8) -> u16 {
        (lead << 8) | trail as u16
    }
}

/**
 * UTF-16 (UCS Transformation Format, 16-bit).
 *
 * This is a Unicode encoding where one codepoint may use
 * 2 (up to U+FFFF) or 4 bytes (up to U+10FFFF) depending on its value.
 * It uses a "surrogate" mechanism to encode non-BMP codepoints,
 * which are represented as a pair of lower surrogate and upper surrogate characters.
 * In this effect, surrogate characters (U+D800..DFFF) cannot appear alone
 * and cannot be included in a valid Unicode string.
 *
 * ## Specialization
 *
 * This type is specialized with endianness type `E`,
 * which should be either `Little` (little endian) or `Big` (big endian).
 */
#[derive(Clone, Copy)]
pub struct UTF16Encoding<E> {
    _marker: PhantomData<E>
}

/// A type for UTF-16 in little endian.
pub type UTF16LEEncoding = UTF16Encoding<Little>;
/// A type for UTF-16 in big endian.
pub type UTF16BEEncoding = UTF16Encoding<Big>;

/// An instance for UTF-16 in little endian.
pub const UTF_16LE_ENCODING: UTF16LEEncoding = UTF16Encoding { _marker: PhantomData };
/// An instance for UTF-16 in big endian.
pub const UTF_16BE_ENCODING: UTF16BEEncoding = UTF16Encoding { _marker: PhantomData };

impl<E: Endian> Encoding for UTF16Encoding<E> {
    fn name(&self) -> &'static str { <E as Endian>::name() }
    fn whatwg_name(&self) -> Option<&'static str> { <E as Endian>::whatwg_name() }
    fn raw_encoder(&self) -> Box<RawEncoder> { UTF16Encoder::<E>::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { UTF16Decoder::<E>::new() }
}

/**
 * An encoder for UTF-16.
 *
 * ## Specialization
 *
 * This type is specialized with endianness type `E`,
 * which should be either `Little` (little endian) or `Big` (big endian).
 */
#[derive(Clone, Copy)]
pub struct UTF16Encoder<E> {
    _marker: PhantomData<E>
}

impl<E: Endian> UTF16Encoder<E> {
    fn new() -> Box<RawEncoder> {
        Box::new(UTF16Encoder::<E> { _marker: PhantomData })
    }
}

impl<E: Endian> RawEncoder for UTF16Encoder<E> {
    fn from_self(&self) -> Box<RawEncoder> { UTF16Encoder::<E>::new() }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len() * 2);

        let write_two_bytes = |output: &mut ByteWriter, msb: u8, lsb: u8|
            <E as Endian>::write_two_bytes(output, msb, lsb);

        for ch in input.chars() {
            match ch {
                '\u{0}'...'\u{d7ff}' | '\u{e000}'...'\u{ffff}' => {
                    let ch = ch as u32;
                    write_two_bytes(output, (ch >> 8) as u8, (ch & 0xff) as u8);
                }
                '\u{10000}'...'\u{10ffff}' => {
                    let ch = ch as u32 - 0x10000;
                    write_two_bytes(output, (0xd8 | (ch >> 18)) as u8,
                                            ((ch >> 10) & 0xff) as u8);
                    write_two_bytes(output, (0xdc | ((ch >> 8) & 0x3)) as u8,
                                            (ch & 0xff) as u8);
                }
                _ => unreachable!() // XXX Rust issue #12483, this is redundant
            }
        }
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/**
 * A decoder for UTF-16.
 *
 * ## Specialization
 *
 * This type is specialized with endianness type `E`,
 * which should be either `Little` (little endian) or `Big` (big endian).
 */
pub struct UTF16Decoder<E> {
    leadbyte: u16,
    leadsurrogate: u16,
    _marker: PhantomData<E>
}

impl<E: Endian> UTF16Decoder<E> {
    pub fn new() -> Box<RawDecoder> {
        Box::new(UTF16Decoder::<E> { leadbyte: 0xffff, leadsurrogate: 0xffff,
                                     _marker: PhantomData })
    }
}

impl<E: Endian> RawDecoder for UTF16Decoder<E> {
    fn from_self(&self) -> Box<RawDecoder> { UTF16Decoder::<E>::new() }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len() / 2); // when every codepoint is U+0000..007F

        let concat_two_bytes = |lead: u16, trail: u8|
            <E as Endian>::concat_two_bytes(lead, trail);

        let mut i = 0;
        let mut processed = 0;
        let len = input.len();

        if i >= len { return (processed, None); }

        if self.leadbyte != 0xffff {
            let ch = concat_two_bytes(self.leadbyte, input[i]);
            i += 1;
            self.leadbyte = 0xffff;
            if self.leadsurrogate != 0xffff { // `ch` is lower surrogate
                let upper = self.leadsurrogate;
                self.leadsurrogate = 0xffff;
                match ch {
                    0xdc00...0xdfff => {
                        let ch = ((upper as u32 - 0xd800) << 10) + (ch as u32 - 0xdc00);
                        output.write_char(as_char(ch + 0x10000));
                        processed = i;
                    }
                    _ => {
                        return (processed, Some(CodecError {
                            upto: i as isize - 2, cause: "invalid sequence".into()
                        }));
                    }
                }
            } else {
                match ch {
                    0xd800...0xdbff => {
                        self.leadsurrogate = ch;
                        // pass through
                    }
                    0xdc00...0xdfff => {
                        return (processed, Some(CodecError {
                            upto: i as isize, cause: "invalid sequence".into()
                        }));
                    }
                    _ => {
                        output.write_char(as_char(ch as u32));
                        processed = i;
                    }
                }
            }
            if i >= len { return (processed, None); }
        }

        if self.leadsurrogate != 0xffff {
            i += 1;
            if i >= len {
                self.leadbyte = input[i-1] as u16;
                return (processed, None);
            }
            let upper = self.leadsurrogate;
            let ch = concat_two_bytes(input[i-1] as u16, input[i]);
            i += 1;
            match ch {
                0xdc00...0xdfff => {
                    let ch = ((upper as u32 - 0xd800) << 10) + (ch as u32 - 0xdc00);
                    output.write_char(as_char(ch + 0x10000));
                }
                _ => {
                    self.leadbyte = 0xffff;
                    self.leadsurrogate = 0xffff;
                    return (processed, Some(CodecError {
                        upto: i as isize - 2, cause: "invalid sequence".into()
                    }));
                }
            }
        }

        self.leadbyte = 0xffff;
        self.leadsurrogate = 0xffff;
        processed = i;
        while i < len {
            i += 1;
            if i >= len {
                self.leadbyte = input[i-1] as u16;
                break;
            }
            let ch = concat_two_bytes(input[i-1] as u16, input[i]);
            match ch {
                0xd800...0xdbff => {
                    i += 2;
                    if i >= len {
                        self.leadsurrogate = ch;
                        if i-1 < len { self.leadbyte = input[i-1] as u16; }
                        break;
                    }
                    let ch2 = concat_two_bytes(input[i-1] as u16, input[i]);
                    match ch2 {
                        0xdc00...0xdfff => {
                            let ch = ((ch as u32 - 0xd800) << 10) + (ch2 as u32 - 0xdc00);
                            output.write_char(as_char(ch + 0x10000));
                        }
                        _ => {
                            return (processed, Some(CodecError {
                                upto: i as isize - 1, cause: "invalid sequence".into()
                            }));
                        }
                    }
                }
                0xdc00...0xdfff => {
                    return (processed, Some(CodecError {
                        upto: i as isize + 1, cause: "invalid sequence".into()
                    }));
                }
                _ => {
                    output.write_char(as_char(ch as u32));
                }
            }
            i += 1;
            processed = i;
        }
        (processed, None)
    }

    fn raw_finish(&mut self, _output: &mut StringWriter) -> Option<CodecError> {
        let leadbyte = self.leadbyte;
        let leadsurrogate = self.leadsurrogate;
        self.leadbyte = 0xffff;
        self.leadsurrogate = 0xffff;
        if leadbyte != 0xffff || leadsurrogate != 0xffff {
            Some(CodecError { upto: 0, cause: "incomplete sequence".into() })
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    // little endian and big endian is symmetric to each other, there's no need to test both.
    // since big endian is easier to inspect we test UTF_16BE only.

    use super::UTF_16BE_ENCODING as UTF_16BE;
    use types::*;

    #[test]
    fn test_encoder_valid() {
        let mut e = UTF_16BE.raw_encoder();
        assert_feed_ok!(e, "\u{0}\
                            \u{1}\u{02}\u{004}\u{0008}\
                            \u{10}\u{020}\u{0040}\u{80}\
                            \u{100}\u{0200}\u{400}\u{800}\
                            \u{1000}\u{2000}\u{4000}\u{8000}\
                            \u{ffff}", "",
                        [0x00, 0x00,
                         0x00, 0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x08,
                         0x00, 0x10, 0x00, 0x20, 0x00, 0x40, 0x00, 0x80,
                         0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x08, 0x00,
                         0x10, 0x00, 0x20, 0x00, 0x40, 0x00, 0x80, 0x00,
                         0xff, 0xff]);
        assert_feed_ok!(e, "\u{10000}\
                            \u{10001}\u{010002}\
                            \u{10004}\u{010008}\
                            \u{10010}\u{010020}\
                            \u{10040}\u{010080}\
                            \u{10100}\u{010200}\
                            \u{10400}\u{010800}\
                            \u{11000}\u{012000}\
                            \u{14000}\u{018000}\
                            \u{20000}\u{030000}\
                            \u{50000}\u{090000}\
                            \u{10FFFF}", "",
                        [0xd8, 0x00, 0xdc, 0x00,
                         0xd8, 0x00, 0xdc, 0x01, 0xd8, 0x00, 0xdc, 0x02,
                         0xd8, 0x00, 0xdc, 0x04, 0xd8, 0x00, 0xdc, 0x08,
                         0xd8, 0x00, 0xdc, 0x10, 0xd8, 0x00, 0xdc, 0x20,
                         0xd8, 0x00, 0xdc, 0x40, 0xd8, 0x00, 0xdc, 0x80,
                         0xd8, 0x00, 0xdd, 0x00, 0xd8, 0x00, 0xde, 0x00,
                         0xd8, 0x01, 0xdc, 0x00, 0xd8, 0x02, 0xdc, 0x00,
                         0xd8, 0x04, 0xdc, 0x00, 0xd8, 0x08, 0xdc, 0x00,
                         0xd8, 0x10, 0xdc, 0x00, 0xd8, 0x20, 0xdc, 0x00,
                         0xd8, 0x40, 0xdc, 0x00, 0xd8, 0x80, 0xdc, 0x00,
                         0xd9, 0x00, 0xdc, 0x00, 0xda, 0x00, 0xdc, 0x00,
                         0xdb, 0xff, 0xdf, 0xff]);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder_valid() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [0x00, 0x00,
                            0x00, 0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x08,
                            0x00, 0x10, 0x00, 0x20, 0x00, 0x40, 0x00, 0x80,
                            0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x08, 0x00,
                            0x10, 0x00, 0x20, 0x00, 0x40, 0x00, 0x80, 0x00,
                            0xff, 0xff], [],
                        "\u{0}\
                         \u{1}\u{02}\u{004}\u{0008}\
                         \u{10}\u{020}\u{0040}\u{80}\
                         \u{100}\u{0200}\u{400}\u{800}\
                         \u{1000}\u{2000}\u{4000}\u{8000}\
                         \u{ffff}");
        assert_feed_ok!(d, [0xd8, 0x00, 0xdc, 0x00,
                            0xd8, 0x00, 0xdc, 0x01, 0xd8, 0x00, 0xdc, 0x02,
                            0xd8, 0x00, 0xdc, 0x04, 0xd8, 0x00, 0xdc, 0x08,
                            0xd8, 0x00, 0xdc, 0x10, 0xd8, 0x00, 0xdc, 0x20,
                            0xd8, 0x00, 0xdc, 0x40, 0xd8, 0x00, 0xdc, 0x80,
                            0xd8, 0x00, 0xdd, 0x00, 0xd8, 0x00, 0xde, 0x00,
                            0xd8, 0x01, 0xdc, 0x00, 0xd8, 0x02, 0xdc, 0x00,
                            0xd8, 0x04, 0xdc, 0x00, 0xd8, 0x08, 0xdc, 0x00,
                            0xd8, 0x10, 0xdc, 0x00, 0xd8, 0x20, 0xdc, 0x00,
                            0xd8, 0x40, 0xdc, 0x00, 0xd8, 0x80, 0xdc, 0x00,
                            0xd9, 0x00, 0xdc, 0x00, 0xda, 0x00, 0xdc, 0x00,
                            0xdb, 0xff, 0xdf, 0xff], [],
                        "\u{10000}\
                         \u{10001}\u{010002}\
                         \u{10004}\u{010008}\
                         \u{10010}\u{010020}\
                         \u{10040}\u{010080}\
                         \u{10100}\u{010200}\
                         \u{10400}\u{010800}\
                         \u{11000}\u{012000}\
                         \u{14000}\u{018000}\
                         \u{20000}\u{030000}\
                         \u{50000}\u{090000}\
                         \u{10FFFF}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_valid_partial_bmp() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0x12], "");
        assert_feed_ok!(d, [0x34], [], "\u{1234}");
        assert_feed_ok!(d, [], [0x56], "");
        assert_feed_ok!(d, [0x78], [], "\u{5678}");
        assert_finish_ok!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0x12], "");
        assert_feed_ok!(d, [0x34], [0x56], "\u{1234}");
        assert_feed_ok!(d, [0x78, 0xab, 0xcd], [], "\u{5678}\u{abcd}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_valid_partial_non_bmp() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8], "");
        assert_feed_ok!(d, [], [0x08], "");
        assert_feed_ok!(d, [], [0xdf], "");
        assert_feed_ok!(d, [0x45], [0xd9], "\u{12345}");
        assert_feed_ok!(d, [], [0x5e], "");
        assert_feed_ok!(d, [], [0xdc], "");
        assert_feed_ok!(d, [0x90], [], "\u{67890}");
        assert_finish_ok!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8], "");
        assert_feed_ok!(d, [], [0x08, 0xdf], "");
        assert_feed_ok!(d, [0x45], [0xd9, 0x5e], "\u{12345}");
        assert_feed_ok!(d, [0xdc, 0x90], [], "\u{67890}");
        assert_finish_ok!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8, 0x08, 0xdf], "");
        assert_feed_ok!(d, [0x45], [0xd9, 0x5e, 0xdc], "\u{12345}");
        assert_feed_ok!(d, [0x90], [], "\u{67890}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_partial() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0x12], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8, 0x08], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8, 0x08, 0xdf], "");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_upper_surrogate() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8, 0x00], "");
        assert_feed_err!(d, [], [], [0x12, 0x34], "");
        assert_feed_err!(d, [], [0xd8, 0x00], [0x56, 0x78], "");
        assert_feed_ok!(d, [], [0xd8, 0x00], "");
        assert_feed_err!(d, [], [], [0xd8, 0x00], "");
        assert_feed_ok!(d, [], [0xd8, 0x00], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xdb, 0xff], "");
        assert_feed_err!(d, [], [], [0x12, 0x34], "");
        assert_feed_err!(d, [], [0xdb, 0xff], [0x56, 0x78], "");
        assert_feed_ok!(d, [], [0xdb, 0xff], "");
        assert_feed_err!(d, [], [], [0xdb, 0xff], "");
        assert_feed_ok!(d, [], [0xdb, 0xff], "");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_upper_surrogate_partial() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8], "");
        assert_feed_err!(d, [], [0x00], [0x12, 0x34], "");
        assert_feed_ok!(d, [], [0xd8, 0x00, 0x56], "");
        assert_feed_err!(d, -1, [], [], [0x56, 0x78], "");
        assert_feed_ok!(d, [], [0xd8], "");
        assert_feed_err!(d, [], [0x00], [0xd8, 0x00], "");
        assert_feed_ok!(d, [], [0xd8, 0x00, 0xdb], "");
        assert_feed_err!(d, -1, [], [], [0xdb, 0xff], "");
        assert_feed_ok!(d, [], [0xd8], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xdb], "");
        assert_feed_err!(d, [], [0xff], [0x12, 0x34], "");
        assert_feed_ok!(d, [], [0xdb, 0xff, 0x56], "");
        assert_feed_err!(d, -1, [], [], [0x56, 0x78], "");
        assert_feed_ok!(d, [], [0xdb], "");
        assert_feed_err!(d, [], [0xff], [0xdb, 0xff], "");
        assert_feed_ok!(d, [], [0xdb, 0xff, 0xd8], "");
        assert_feed_err!(d, -1, [], [], [0xd8, 0x00], "");
        assert_feed_ok!(d, [], [0xdb], "");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lower_surrogate() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_err!(d, [], [0xdc, 0x00], [], "");
        assert_feed_err!(d, [0x12, 0x34], [0xdc, 0x00], [0x56, 0x78], "\u{1234}");
        assert_finish_ok!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_err!(d, [], [0xdf, 0xff], [], "");
        assert_feed_err!(d, [0x12, 0x34], [0xdf, 0xff], [0x56, 0x78], "\u{1234}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lower_surrogate_partial() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xdc], "");
        assert_feed_err!(d, [], [0x00], [], "");
        assert_feed_ok!(d, [0x12, 0x34], [0xdc], "\u{1234}");
        assert_feed_err!(d, [], [0x00], [0x56, 0x78], "");
        assert_finish_ok!(d, "");

        assert_feed_ok!(d, [], [0xdf], "");
        assert_feed_err!(d, [], [0xff], [], "");
        assert_feed_ok!(d, [0x12, 0x34], [0xdf], "\u{1234}");
        assert_feed_err!(d, [], [0xff], [0x56, 0x78], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_one_byte_before_finish() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0x12], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [0x12, 0x34], [0x56], "\u{1234}");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_invalid_three_bytes_before_finish() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8, 0x00, 0xdc], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [0x12, 0x34], [0xd8, 0x00, 0xdc], "\u{1234}");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_invalid_three_bytes_before_finish_partial() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [], [0xd8], "");
        assert_feed_ok!(d, [], [0x00], "");
        assert_feed_ok!(d, [], [0xdc], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [0x12, 0x34], [0xd8], "\u{1234}");
        assert_feed_ok!(d, [], [0x00, 0xdc], "");
        assert_finish_err!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [0x12, 0x34], [0xd8, 0x00], "\u{1234}");
        assert_feed_ok!(d, [], [0xdc], "");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_feed_after_finish() {
        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [0x12, 0x34], [0x12], "\u{1234}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0x12, 0x34], [], "\u{1234}");
        assert_finish_ok!(d, "");

        let mut d = UTF_16BE.raw_decoder();
        assert_feed_ok!(d, [0xd8, 0x08, 0xdf, 0x45], [0xd8, 0x08, 0xdf], "\u{12345}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0xd8, 0x08, 0xdf, 0x45], [0xd8, 0x08], "\u{12345}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0xd8, 0x08, 0xdf, 0x45], [0xd8], "\u{12345}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0xd8, 0x08, 0xdf, 0x45], [], "\u{12345}");
        assert_finish_ok!(d, "");
    }
}

