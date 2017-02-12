// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! Legacy Japanese encodings based on JIS X 0208 and JIS X 0212.

use std::convert::Into;
use std::default::Default;
use util::StrCharIndex;
use index_japanese as index;
use types::*;
use self::ISO2022JPState::{ASCII,Katakana,Lead};

/**
 * EUC-JP. (XXX with asymmetric JIS X 0212 support)
 *
 * This is a Japanese encoding created from three JIS character sets:
 *
 * - JIS X 0201, which lower half is ISO/IEC 646:JP (US-ASCII with yen sign and overline)
 *   and upper half contains legacy half-width Katakanas.
 * - JIS X 0208, a primary graphic character set (94x94).
 * - JIS X 0212, a supplementary graphic character set (94x94).
 *
 * EUC-JP contains the lower half of JIS X 0201 in G0 (`[21-7E]`),
 * JIS X 0208 in G1 (`[A1-FE] [A1-FE]`),
 * the upper half of JIS X 0212 in G2 (`8E [A1-DF]`), and
 * JIS X 0212 in G3 (`8F [A1-FE] [A1-FE]`).
 */
#[derive(Clone, Copy)]
pub struct EUCJPEncoding;

impl Encoding for EUCJPEncoding {
    fn name(&self) -> &'static str { "euc-jp" }
    fn whatwg_name(&self) -> Option<&'static str> { Some("euc-jp") }
    fn raw_encoder(&self) -> Box<RawEncoder> { EUCJPEncoder::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { EUCJP0212Decoder::new() }
}

/// An encoder for EUC-JP with unused G3 character set.
#[derive(Clone, Copy)]
pub struct EUCJPEncoder;

impl EUCJPEncoder {
    pub fn new() -> Box<RawEncoder> { Box::new(EUCJPEncoder) }
}

impl RawEncoder for EUCJPEncoder {
    fn from_self(&self) -> Box<RawEncoder> { EUCJPEncoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len());

        for ((i,j), ch) in input.index_iter() {
            match ch {
                '\u{0}'...'\u{7f}' => { output.write_byte(ch as u8); }
                '\u{a5}' => { output.write_byte(0x5c); }
                '\u{203e}' => { output.write_byte(0x7e); }
                '\u{ff61}'...'\u{ff9f}' => {
                    output.write_byte(0x8e);
                    output.write_byte((ch as usize - 0xff61 + 0xa1) as u8);
                }
                _ => {
                    let ptr = index::jis0208::backward(ch as u32);
                    if ptr == 0xffff {
                        return (i, Some(CodecError {
                            upto: j as isize, cause: "unrepresentable character".into()
                        }));
                    } else {
                        let lead = ptr / 94 + 0xa1;
                        let trail = ptr % 94 + 0xa1;
                        output.write_byte(lead as u8);
                        output.write_byte(trail as u8);
                    }
                }
            }
        }
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder for EUC-JP with JIS X 0212 in G3.
#[derive(Clone, Copy)]
struct EUCJP0212Decoder {
    st: eucjp::State,
}

impl EUCJP0212Decoder {
    pub fn new() -> Box<RawDecoder> {
        Box::new(EUCJP0212Decoder { st: Default::default() })
    }
}

impl RawDecoder for EUCJP0212Decoder {
    fn from_self(&self) -> Box<RawDecoder> { EUCJP0212Decoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        let (st, processed, err) = eucjp::raw_feed(self.st, input, output, &());
        self.st = st;
        (processed, err)
    }

    fn raw_finish(&mut self, output: &mut StringWriter) -> Option<CodecError> {
        let (st, err) = eucjp::raw_finish(self.st, output, &());
        self.st = st;
        err
    }
}

stateful_decoder! {
    module eucjp;

    internal pub fn map_two_0208_bytes(lead: u8, trail: u8) -> u32 {
        use index_japanese as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let index = match (lead, trail) {
            (0xa1...0xfe, 0xa1...0xfe) => (lead - 0xa1) * 94 + trail - 0xa1,
            _ => 0xffff,
        };
        index::jis0208::forward(index)
    }

    internal pub fn map_two_0212_bytes(lead: u8, trail: u8) -> u32 {
        use index_japanese as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let index = match (lead, trail) {
            (0xa1...0xfe, 0xa1...0xfe) => (lead - 0xa1) * 94 + trail - 0xa1,
            _ => 0xffff,
        };
        index::jis0212::forward(index)
    }

initial:
    // euc-jp lead = 0x00
    state S0(ctx: Context) {
        case b @ 0x00...0x7f => ctx.emit(b as u32);
        case 0x8e => S1(ctx);
        case 0x8f => S2(ctx);
        case b @ 0xa1...0xfe => S3(ctx, b);
        case _ => ctx.err("invalid sequence");
    }

transient:
    // euc-jp lead = 0x8e
    state S1(ctx: Context) {
        case b @ 0xa1...0xdf => ctx.emit(0xff61 + b as u32 - 0xa1);
        case 0xa1...0xfe => ctx.err("invalid sequence");
        case _ => ctx.backup_and_err(1, "invalid sequence");
    }

    // euc-jp lead = 0x8f
    // JIS X 0201 half-width katakana
    state S2(ctx: Context) {
        case b @ 0xa1...0xfe => S4(ctx, b);
        case _ => ctx.backup_and_err(1, "invalid sequence");
    }

    // euc-jp lead != 0x00, euc-jp jis0212 flag = unset
    // JIS X 0208 two-byte sequence
    state S3(ctx: Context, lead: u8) {
        case b @ 0xa1...0xfe => match map_two_0208_bytes(lead, b) {
            // do NOT backup, we only backup for out-of-range trails.
            0xffff => ctx.err("invalid sequence"),
            ch => ctx.emit(ch as u32)
        };
        case _ => ctx.backup_and_err(1, "invalid sequence");
    }

    // euc-jp lead != 0x00, euc-jp jis0212 flag = set
    // JIS X 0212 three-byte sequence
    state S4(ctx: Context, lead: u8) {
        case b @ 0xa1...0xfe => match map_two_0212_bytes(lead, b) {
            // do NOT backup, we only backup for out-of-range trails.
            0xffff => ctx.err("invalid sequence"),
            ch => ctx.emit(ch as u32)
        };
        case _ => ctx.backup_and_err(1, "invalid sequence");
    }
}

#[cfg(test)]
mod eucjp_tests {
    extern crate test;
    use super::EUCJPEncoding;
    use testutils;
    use types::*;

    #[test]
    fn test_encoder_valid() {
        let mut e = EUCJPEncoding.raw_encoder();
        assert_feed_ok!(e, "A", "", [0x41]);
        assert_feed_ok!(e, "BC", "", [0x42, 0x43]);
        assert_feed_ok!(e, "", "", []);
        assert_feed_ok!(e, "\u{a5}", "", [0x5c]);
        assert_feed_ok!(e, "\u{203e}", "", [0x7e]);
        assert_feed_ok!(e, "\u{306b}\u{307b}\u{3093}", "", [0xa4, 0xcb, 0xa4, 0xdb, 0xa4, 0xf3]);
        assert_feed_ok!(e, "\u{ff86}\u{ff8e}\u{ff9d}", "", [0x8e, 0xc6, 0x8e, 0xce, 0x8e, 0xdd]);
        assert_feed_ok!(e, "\u{65e5}\u{672c}", "", [0xc6, 0xfc, 0xcb, 0xdc]);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_double_mapped() {
        // these characters are double-mapped to both EUDC area and Shift_JIS extension area
        // but only the former should be used. (note that U+FFE2 is triple-mapped!)
        let mut e = EUCJPEncoding.raw_encoder();
        assert_feed_ok!(e, "\u{9ed1}\u{2170}\u{ffe2}", "", [0xfc, 0xee, 0xfc, 0xf1, 0xa2, 0xcc]);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_invalid() {
        let mut e = EUCJPEncoding.raw_encoder();
        assert_feed_err!(e, "", "\u{ffff}", "", []);
        assert_feed_err!(e, "?", "\u{ffff}", "!", [0x3f]);
        // JIS X 0212 is not supported in the encoder
        assert_feed_err!(e, "", "\u{736c}", "\u{8c78}", []);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder_valid() {
        let mut d = EUCJPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x41], [], "A");
        assert_feed_ok!(d, [0x42, 0x43], [], "BC");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0x5c], [], "\\");
        assert_feed_ok!(d, [0x7e], [], "~");
        assert_feed_ok!(d, [0xa4, 0xcb, 0xa4, 0xdb, 0xa4, 0xf3], [], "\u{306b}\u{307b}\u{3093}");
        assert_feed_ok!(d, [0x8e, 0xc6, 0x8e, 0xce, 0x8e, 0xdd], [], "\u{ff86}\u{ff8e}\u{ff9d}");
        assert_feed_ok!(d, [0xc6, 0xfc, 0xcb, 0xdc], [], "\u{65e5}\u{672c}");
        assert_feed_ok!(d, [0x8f, 0xcb, 0xc6, 0xec, 0xb8], [], "\u{736c}\u{8c78}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_valid_partial() {
        let mut d = EUCJPEncoding.raw_decoder();
        assert_feed_ok!(d, [], [0xa4], "");
        assert_feed_ok!(d, [0xcb], [0xa4], "\u{306b}");
        assert_feed_ok!(d, [0xdb], [0xa4], "\u{307b}");
        assert_feed_ok!(d, [0xf3], [], "\u{3093}");
        assert_feed_ok!(d, [], [0x8e], "");
        assert_feed_ok!(d, [0xc6], [0x8e], "\u{ff86}");
        assert_feed_ok!(d, [0xce], [0x8e], "\u{ff8e}");
        assert_feed_ok!(d, [0xdd], [], "\u{ff9d}");
        assert_feed_ok!(d, [], [0xc6], "");
        assert_feed_ok!(d, [0xfc], [0xcb], "\u{65e5}");
        assert_feed_ok!(d, [0xdc], [], "\u{672c}");
        assert_feed_ok!(d, [], [0x8f], "");
        assert_feed_ok!(d, [], [0xcb], "");
        assert_feed_ok!(d, [0xc6], [0xec], "\u{736c}");
        assert_feed_ok!(d, [0xb8], [], "\u{8c78}");
        assert_feed_ok!(d, [], [0x8f, 0xcb], "");
        assert_feed_ok!(d, [0xc6, 0xec, 0xb8], [], "\u{736c}\u{8c78}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lead_immediate_test_finish() {
        for i in 0x8e..0x90 {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_ok!(d, [], [i], ""); // wait for a trail
            assert_finish_err!(d, "");
        }

        for i in 0xa1..0xff {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_ok!(d, [], [i], ""); // wait for a trail
            assert_finish_err!(d, "");
        }

        // immediate failures
        let mut d = EUCJPEncoding.raw_decoder();
        for i in 0x80..0x8e {
            assert_feed_err!(d, [], [i], [], "");
        }
        for i in 0x90..0xa1 {
            assert_feed_err!(d, [], [i], [], "");
        }
        assert_feed_err!(d, [], [0xff], [], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lead_followed_by_space() {
        for i in 0x80..0x100 {
            let i = i as u8;
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_err!(d, [], [i], [0x20], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_lead_followed_by_invalid_trail() {
        for i in 0x80..0x100 {
            let i = i as u8;
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_err!(d, [], [i], [0x80], "");
            assert_feed_err!(d, [], [i], [0xff], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_lone_lead_for_0212_immediate_test_finish() {
        for i in 0xa1..0xff {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_ok!(d, [], [0x8f, i], ""); // wait for a trail
            assert_finish_err!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_lone_lead_for_0212_immediate_test_finish_partial() {
        for i in 0xa1..0xff {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_ok!(d, [], [0x8f], "");
            assert_feed_ok!(d, [], [i], ""); // wait for a trail
            assert_finish_err!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_trail_for_0201() {
        for i in 0..0xa1 {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_err!(d, [], [0x8e], [i], "");
            assert_finish_ok!(d, "");
        }

        for i in 0xe0..0xff {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_err!(d, [], [0x8e, i], [], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_trail_for_0201_partial() {
        for i in 0..0xa1 {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_ok!(d, [], [0x8e], "");
            assert_feed_err!(d, [], [], [i], "");
            assert_finish_ok!(d, "");
        }

        for i in 0xe0..0xff {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_ok!(d, [], [0x8e], "");
            assert_feed_err!(d, [], [i], [], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_middle_for_0212() {
        for i in 0..0xa1 {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_err!(d, [], [0x8f], [i], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_middle_for_0212_partial() {
        for i in 0..0xa1 {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_ok!(d, [], [0x8f], "");
            assert_feed_err!(d, [], [], [i], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_trail_for_0212() {
        for i in 0..0xa1 {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_err!(d, [], [0x8f, 0xa1], [i], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_trail_for_0212_partial() {
        for i in 0..0xa1 {
            let mut d = EUCJPEncoding.raw_decoder();
            assert_feed_ok!(d, [], [0x8f], "");
            assert_feed_ok!(d, [], [0xa1], "");
            assert_feed_err!(d, [], [], [i], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_feed_after_finish() {
        let mut d = EUCJPEncoding.raw_decoder();
        assert_feed_ok!(d, [0xa4, 0xa2], [0xa4], "\u{3042}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0xa4, 0xa2], [], "\u{3042}");
        assert_finish_ok!(d, "");
    }

    #[bench]
    fn bench_encode_short_text(bencher: &mut test::Bencher) {
        let s = testutils::JAPANESE_TEXT;
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            EUCJPEncoding.encode(&s, EncoderTrap::Strict)
        }))
    }

    #[bench]
    fn bench_decode_short_text(bencher: &mut test::Bencher) {
        let s = EUCJPEncoding.encode(testutils::JAPANESE_TEXT,
                                     EncoderTrap::Strict).ok().unwrap();
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            EUCJPEncoding.decode(&s, DecoderTrap::Strict)
        }))
    }
}

/**
 * Windows code page 932, i.e. Shift_JIS with IBM/NEC extensions.
 *
 * This is a Japanese encoding for JIS X 0208
 * compatible to the original assignments of JIS X 0201 (`[21-7E A1-DF]`).
 * The 94 by 94 region of JIS X 0208 is sliced, or rather "shifted" into
 * the odd half (odd row number) and even half (even row number),
 * and merged into the 188 by 47 region mapped to `[81-9F E0-EF] [40-7E 80-FC]`.
 * The remaining area, `[80 A0 F0-FF] [40-7E 80-FC]`, has been subjected to
 * numerous extensions incompatible to each other.
 * This particular implementation uses IBM/NEC extensions
 * which assigns more characters to `[F0-FC 80-FC]` and also to the Private Use Area (PUA).
 * It requires some cares to handle
 * since the second byte of JIS X 0208 can have its MSB unset.
 */
#[derive(Clone, Copy)]
pub struct Windows31JEncoding;

impl Encoding for Windows31JEncoding {
    fn name(&self) -> &'static str { "windows-31j" }
    fn whatwg_name(&self) -> Option<&'static str> { Some("shift_jis") } // WHATWG compatibility
    fn raw_encoder(&self) -> Box<RawEncoder> { Windows31JEncoder::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { Windows31JDecoder::new() }
}

/// An encoder for Shift_JIS with IBM/NEC extensions.
#[derive(Clone, Copy)]
pub struct Windows31JEncoder;

impl Windows31JEncoder {
    pub fn new() -> Box<RawEncoder> { Box::new(Windows31JEncoder) }
}

impl RawEncoder for Windows31JEncoder {
    fn from_self(&self) -> Box<RawEncoder> { Windows31JEncoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len());

        for ((i,j), ch) in input.index_iter() {
            match ch {
                '\u{0}'...'\u{80}' => { output.write_byte(ch as u8); }
                '\u{a5}' => { output.write_byte(0x5c); }
                '\u{203e}' => { output.write_byte(0x7e); }
                '\u{ff61}'...'\u{ff9f}' => {
                    output.write_byte((ch as usize - 0xff61 + 0xa1) as u8);
                }
                _ => {
                    // corresponds to the "index shift_jis pointer" in the WHATWG spec
                    let ptr = index::jis0208::backward_remapped(ch as u32);
                    if ptr == 0xffff {
                        return (i, Some(CodecError {
                            upto: j as isize, cause: "unrepresentable character".into(),
                        }));
                    } else {
                        let lead = ptr / 188;
                        let leadoffset = if lead < 0x1f {0x81} else {0xc1};
                        let trail = ptr % 188;
                        let trailoffset = if trail < 0x3f {0x40} else {0x41};
                        output.write_byte((lead + leadoffset) as u8);
                        output.write_byte((trail + trailoffset) as u8);
                    }
                }
            }
        }
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder for Shift_JIS with IBM/NEC extensions.
#[derive(Clone, Copy)]
struct Windows31JDecoder {
    st: windows31j::State,
}

impl Windows31JDecoder {
    pub fn new() -> Box<RawDecoder> {
        Box::new(Windows31JDecoder { st: Default::default() })
    }
}

impl RawDecoder for Windows31JDecoder {
    fn from_self(&self) -> Box<RawDecoder> { Windows31JDecoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        let (st, processed, err) = windows31j::raw_feed(self.st, input, output, &());
        self.st = st;
        (processed, err)
    }

    fn raw_finish(&mut self, output: &mut StringWriter) -> Option<CodecError> {
        let (st, err) = windows31j::raw_finish(self.st, output, &());
        self.st = st;
        err
    }
}

stateful_decoder! {
    module windows31j;

    internal pub fn map_two_0208_bytes(lead: u8, trail: u8) -> u32 {
        use index_japanese as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let leadoffset = if lead < 0xa0 {0x81} else {0xc1};
        let trailoffset = if trail < 0x7f {0x40} else {0x41};
        let index = match (lead, trail) {
            (0xf0...0xf9, 0x40...0x7e) | (0xf0...0xf9, 0x80...0xfc) =>
                return (0xe000 + (lead - 0xf0) * 188 + trail - trailoffset) as u32,
            (0x81...0x9f, 0x40...0x7e) | (0x81...0x9f, 0x80...0xfc) |
            (0xe0...0xfc, 0x40...0x7e) | (0xe0...0xfc, 0x80...0xfc) =>
                (lead - leadoffset) * 188 + trail - trailoffset,
            _ => 0xffff,
        };
        index::jis0208::forward(index)
    }

initial:
    // shift_jis lead = 0x00
    state S0(ctx: Context) {
        case b @ 0x00...0x80 => ctx.emit(b as u32);
        case b @ 0xa1...0xdf => ctx.emit(0xff61 + b as u32 - 0xa1);
        case b @ 0x81...0x9f, b @ 0xe0...0xfc => S1(ctx, b);
        case _ => ctx.err("invalid sequence");
    }

transient:
    // shift_jis lead != 0x00
    state S1(ctx: Context, lead: u8) {
        case b => match map_two_0208_bytes(lead, b) {
            0xffff => ctx.backup_and_err(1, "invalid sequence"), // unconditional
            ch => ctx.emit(ch)
        };
    }
}

#[cfg(test)]
mod windows31j_tests {
    extern crate test;
    use super::Windows31JEncoding;
    use testutils;
    use types::*;

    #[test]
    fn test_encoder_valid() {
        let mut e = Windows31JEncoding.raw_encoder();
        assert_feed_ok!(e, "A", "", [0x41]);
        assert_feed_ok!(e, "BC", "", [0x42, 0x43]);
        assert_feed_ok!(e, "", "", []);
        assert_feed_ok!(e, "\u{a5}", "", [0x5c]);
        assert_feed_ok!(e, "\u{203e}", "", [0x7e]);
        assert_feed_ok!(e, "\u{306b}\u{307b}\u{3093}", "", [0x82, 0xc9, 0x82, 0xd9, 0x82, 0xf1]);
        assert_feed_ok!(e, "\u{ff86}\u{ff8e}\u{ff9d}", "", [0xc6, 0xce, 0xdd]);
        assert_feed_ok!(e, "\u{65e5}\u{672c}", "", [0x93, 0xfa, 0x96, 0x7b]);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_no_eudc() {
        let mut e = Windows31JEncoding.raw_encoder();
        assert_feed_err!(e, "", "\u{e000}", "", []);
        assert_feed_err!(e, "", "\u{e757}", "", []);
        assert_feed_err!(e, "", "\u{e758}", "", []);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_double_mapped() {
        // these characters are double-mapped to both EUDC area and Shift_JIS extension area
        // but only the latter should be used. (note that U+FFE2 is triple-mapped!)
        let mut e = Windows31JEncoding.raw_encoder();
        assert_feed_ok!(e, "\u{9ed1}\u{2170}\u{ffe2}", "", [0xfc, 0x4b, 0xfa, 0x40, 0x81, 0xca]);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_invalid() {
        let mut e = Windows31JEncoding.raw_encoder();
        assert_feed_err!(e, "", "\u{ffff}", "", []);
        assert_feed_err!(e, "?", "\u{ffff}", "!", [0x3f]);
        assert_feed_err!(e, "", "\u{736c}", "\u{8c78}", []);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder_valid() {
        let mut d = Windows31JEncoding.raw_decoder();
        assert_feed_ok!(d, [0x41], [], "A");
        assert_feed_ok!(d, [0x42, 0x43], [], "BC");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0x5c], [], "\\");
        assert_feed_ok!(d, [0x7e], [], "~");
        assert_feed_ok!(d, [0x80], [], "\u{80}"); // compatibility
        assert_feed_ok!(d, [0x82, 0xc9, 0x82, 0xd9, 0x82, 0xf1], [], "\u{306b}\u{307b}\u{3093}");
        assert_feed_ok!(d, [0xc6, 0xce, 0xdd], [], "\u{ff86}\u{ff8e}\u{ff9d}");
        assert_feed_ok!(d, [0x93, 0xfa, 0x96, 0x7b], [], "\u{65e5}\u{672c}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_eudc() {
        let mut d = Windows31JEncoding.raw_decoder();
        assert_feed_ok!(d, [], [0xf0], "");
        assert_feed_ok!(d, [0x40], [], "\u{e000}");
        assert_feed_ok!(d, [0xf9, 0xfc], [], "\u{e757}");
        assert_feed_err!(d, [], [0xf0], [0x00], "");
        assert_feed_err!(d, [], [0xf0], [0xff], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lead_immediate_test_finish() {
        for i in 0x81..0xa0 {
            let mut d = Windows31JEncoding.raw_decoder();
            assert_feed_ok!(d, [], [i], ""); // wait for a trail
            assert_finish_err!(d, "");
        }

        for i in 0xe0..0xfd {
            let mut d = Windows31JEncoding.raw_decoder();
            assert_feed_ok!(d, [], [i], ""); // wait for a trail
            assert_finish_err!(d, "");
        }

        // A0/FD/FE/FF: immediate failure
        let mut d = Windows31JEncoding.raw_decoder();
        assert_feed_err!(d, [], [0xa0], [], "");
        assert_feed_err!(d, [], [0xfd], [], "");
        assert_feed_err!(d, [], [0xfe], [], "");
        assert_feed_err!(d, [], [0xff], [], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lead_followed_by_space() {
        for i in 0x81..0xa0 {
            let mut d = Windows31JEncoding.raw_decoder();
            assert_feed_err!(d, [], [i], [0x20], "");
            assert_finish_ok!(d, "");
        }

        for i in 0xe0..0xfd {
            let mut d = Windows31JEncoding.raw_decoder();
            assert_feed_err!(d, [], [i], [0x20], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_lead_followed_by_invalid_trail() {
        for i in 0x81..0xa0 {
            let mut d = Windows31JEncoding.raw_decoder();
            assert_feed_err!(d, [], [i], [0x3f], "");
            assert_feed_err!(d, [], [i], [0x7f], "");
            assert_feed_err!(d, [], [i], [0xfd], "");
            assert_feed_err!(d, [], [i], [0xfe], "");
            assert_feed_err!(d, [], [i], [0xff], "");
            assert_finish_ok!(d, "");
        }

        for i in 0xe0..0xfd {
            let mut d = Windows31JEncoding.raw_decoder();
            assert_feed_err!(d, [], [i], [0x3f], "");
            assert_feed_err!(d, [], [i], [0x7f], "");
            assert_feed_err!(d, [], [i], [0xfd], "");
            assert_feed_err!(d, [], [i], [0xfe], "");
            assert_feed_err!(d, [], [i], [0xff], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_lead_followed_by_invalid_trail_partial() {
        for i in 0x81..0xa0 {
            let mut d = Windows31JEncoding.raw_decoder();
            assert_feed_ok!(d, [], [i], "");
            assert_feed_err!(d, [], [], [0xff], "");
            assert_finish_ok!(d, "");
        }

        for i in 0xe0..0xfd {
            let mut d = Windows31JEncoding.raw_decoder();
            assert_feed_ok!(d, [], [i], "");
            assert_feed_err!(d, [], [], [0xff], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_feed_after_finish() {
        let mut d = Windows31JEncoding.raw_decoder();
        assert_feed_ok!(d, [0x82, 0xa0], [0x82], "\u{3042}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0x82, 0xa0], [], "\u{3042}");
        assert_finish_ok!(d, "");
    }

    #[bench]
    fn bench_encode_short_text(bencher: &mut test::Bencher) {
        let s = testutils::JAPANESE_TEXT;
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            Windows31JEncoding.encode(&s, EncoderTrap::Strict)
        }))
    }

    #[bench]
    fn bench_decode_short_text(bencher: &mut test::Bencher) {
        let s = Windows31JEncoding.encode(testutils::JAPANESE_TEXT,
                                          EncoderTrap::Strict).ok().unwrap();
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            Windows31JEncoding.decode(&s, DecoderTrap::Strict)
        }))
    }
}

/**
 * ISO-2022-JP.
 *
 * This version of ISO-2022-JP does not correspond to any standardized repertoire of character sets
 * due to the widespread implementation differences. The following character sets are supported:
 *
 * - JIS X 0201-1976 roman (`ESC ( J` or `ESC ( B`; the latter is originally allocated to ASCII
 *   but willfully violated)
 * - JIS X 0201-1976 kana (`ESC ( I`)
 * - JIS X 0208-1983 (`ESC $ B` or `ESC $ @`; the latter is originally allocated to JIS X 0208-1978
 *   but willfully violated)
 * - JIS X 0212-1990 (`ESC $ ( D`, XXX asymmetric support)
 */
#[derive(Clone, Copy)]
pub struct ISO2022JPEncoding;

impl Encoding for ISO2022JPEncoding {
    fn name(&self) -> &'static str { "iso-2022-jp" }
    fn whatwg_name(&self) -> Option<&'static str> { Some("iso-2022-jp") }
    fn raw_encoder(&self) -> Box<RawEncoder> { ISO2022JPEncoder::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { ISO2022JPDecoder::new() }
}

#[derive(PartialEq,Clone,Copy)]
enum ISO2022JPState {
    ASCII, // U+0000..007F, U+00A5, U+203E
    Katakana, // JIS X 0201: U+FF61..FF9F
    Lead, // JIS X 0208
}

/// An encoder for ISO-2022-JP without JIS X 0212/0213 support.
#[derive(Clone, Copy)]
pub struct ISO2022JPEncoder {
    st: ISO2022JPState
}

impl ISO2022JPEncoder {
    pub fn new() -> Box<RawEncoder> { Box::new(ISO2022JPEncoder { st: ASCII }) }
}

impl RawEncoder for ISO2022JPEncoder {
    fn from_self(&self) -> Box<RawEncoder> { ISO2022JPEncoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len());

        let mut st = self.st;
        macro_rules! ensure_ASCII(
            () => (if st != ASCII { output.write_bytes(b"\x1b(B"); st = ASCII; })
        );
        macro_rules! ensure_Katakana(
            () => (if st != Katakana { output.write_bytes(b"\x1b(I"); st = Katakana; })
        );
        macro_rules! ensure_Lead(
            () => (if st != Lead { output.write_bytes(b"\x1b$B"); st = Lead; })
        );

        for ((i,j), ch) in input.index_iter() {
            match ch {
                '\u{0}'...'\u{7f}' => { ensure_ASCII!(); output.write_byte(ch as u8); }
                '\u{a5}' => { ensure_ASCII!(); output.write_byte(0x5c); }
                '\u{203e}' => { ensure_ASCII!(); output.write_byte(0x7e); }
                '\u{ff61}'...'\u{ff9f}' => {
                    ensure_Katakana!();
                    output.write_byte((ch as usize - 0xff61 + 0x21) as u8);
                }
                _ => {
                    let ptr = index::jis0208::backward(ch as u32);
                    if ptr == 0xffff {
                        self.st = st; // do NOT reset the state!
                        return (i, Some(CodecError {
                            upto: j as isize, cause: "unrepresentable character".into()
                        }));
                    } else {
                        ensure_Lead!();
                        let lead = ptr / 94 + 0x21;
                        let trail = ptr % 94 + 0x21;
                        output.write_byte(lead as u8);
                        output.write_byte(trail as u8);
                    }
                }
            }
        }

        self.st = st;
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder for ISO-2022-JP with JIS X 0212 support.
#[derive(Clone, Copy)]
struct ISO2022JPDecoder {
    st: iso2022jp::State,
}

impl ISO2022JPDecoder {
    pub fn new() -> Box<RawDecoder> {
        Box::new(ISO2022JPDecoder { st: Default::default() })
    }
}

impl RawDecoder for ISO2022JPDecoder {
    fn from_self(&self) -> Box<RawDecoder> { ISO2022JPDecoder::new() }
    fn is_ascii_compatible(&self) -> bool { false }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        let (st, processed, err) = iso2022jp::raw_feed(self.st, input, output, &());
        self.st = st;
        (processed, err)
    }

    fn raw_finish(&mut self, output: &mut StringWriter) -> Option<CodecError> {
        let (st, err) = iso2022jp::raw_finish(self.st, output, &());
        self.st = st;
        err
    }
}

stateful_decoder! {
    module iso2022jp;

    internal pub fn map_two_0208_bytes(lead: u8, trail: u8) -> u32 {
        use index_japanese as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let index = match (lead, trail) {
            (0x21...0x7e, 0x21...0x7e) => (lead - 0x21) * 94 + trail - 0x21,
            _ => 0xffff,
        };
        index::jis0208::forward(index)
    }

    internal pub fn map_two_0212_bytes(lead: u8, trail: u8) -> u32 {
        use index_japanese as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let index = match (lead, trail) {
            (0x21...0x7e, 0x21...0x7e) => (lead - 0x21) * 94 + trail - 0x21,
            _ => 0xffff,
        };
        index::jis0212::forward(index)
    }

initial:
    // iso-2022-jp state = ASCII, iso-2022-jp jis0212 flag = unset, iso-2022-jp lead = 0x00
    state ASCII(ctx: Context) {
        case 0x1b => EscapeStart(ctx);
        case b @ 0x00...0x7f => ctx.emit(b as u32), ASCII(ctx);
        case _ => ctx.err("invalid sequence"), ASCII(ctx);
        final => ctx.reset();
    }

checkpoint:
    // iso-2022-jp state = Lead, iso-2022-jp jis0212 flag = unset
    state Lead0208(ctx: Context) {
        case 0x0a => ctx.emit(0x000a); // return to ASCII
        case 0x1b => EscapeStart(ctx);
        case b => Trail0208(ctx, b);
        final => ctx.reset();
    }

    // iso-2022-jp state = Lead, iso-2022-jp jis0212 flag = set
    state Lead0212(ctx: Context) {
        case 0x0a => ctx.emit(0x000a); // return to ASCII
        case 0x1b => EscapeStart(ctx);
        case b => Trail0212(ctx, b);
        final => ctx.reset();
    }

    // iso-2022-jp state = Katakana
    state Katakana(ctx: Context) {
        case 0x1b => EscapeStart(ctx);
        case b @ 0x21...0x5f => ctx.emit(0xff61 + b as u32 - 0x21), Katakana(ctx);
        case _ => ctx.err("invalid sequence"), Katakana(ctx);
        final => ctx.reset();
    }

transient:
    // iso-2022-jp state = EscapeStart
    // ESC
    state EscapeStart(ctx: Context) {
        case 0x24 => EscapeMiddle24(ctx); // ESC $
        case 0x28 => EscapeMiddle28(ctx); // ESC (
        case _ => ctx.backup_and_err(1, "invalid sequence");
        final => ctx.err("incomplete sequence");
    }

    // iso-2022-jp state = EscapeMiddle, iso-2022-jp lead = 0x24
    // ESC $
    state EscapeMiddle24(ctx: Context) {
        case 0x40, 0x42 => Lead0208(ctx); // ESC $ @ (JIS X 0208-1978) or ESC $ B (-1983)
        case 0x28 => EscapeFinal(ctx); // ESC $ (
        case _ => ctx.backup_and_err(2, "invalid sequence");
        final => ctx.err("incomplete sequence");
    }

    // iso-2022-jp state = EscapeMiddle, iso-2022-jp lead = 0x28
    // ESC (
    state EscapeMiddle28(ctx: Context) {
        case 0x42, 0x4a => ctx.reset(); // ESC ( B (ASCII) or ESC ( J (JIS X 0201-1976 roman)
        case 0x49 => Katakana(ctx); // ESC ( I (JIS X 0201-1976 kana)
        case _ => ctx.backup_and_err(2, "invalid sequence");
        final => ctx.err("incomplete sequence");
    }

    // iso-2022-jp state = EscapeFinal
    // ESC $ (
    state EscapeFinal(ctx: Context) {
        case 0x44 => Lead0212(ctx); // ESC $ ( D (JIS X 0212-1990)
        case _ => ctx.backup_and_err(3, "invalid sequence");
        final => ctx.backup_and_err(1, "incomplete sequence");
    }

    // iso-2022-jp state = Trail, iso-2022-jp jis0212 flag = unset
    state Trail0208(ctx: Context, lead: u8) {
        case b =>
            match map_two_0208_bytes(lead, b) {
                0xffff => ctx.err("invalid sequence"),
                ch => ctx.emit(ch as u32)
            },
            Lead0208(ctx);
        final => ctx.err("incomplete sequence");
    }

    // iso-2022-jp state = Trail, iso-2022-jp jis0212 flag = set
    state Trail0212(ctx: Context, lead: u8) {
        case b =>
            match map_two_0212_bytes(lead, b) {
                0xffff => ctx.err("invalid sequence"),
                ch => ctx.emit(ch as u32)
            },
            Lead0212(ctx);
        final => ctx.err("incomplete sequence");
    }
}

#[cfg(test)]
mod iso2022jp_tests {
    extern crate test;
    use super::ISO2022JPEncoding;
    use testutils;
    use types::*;

    #[test]
    fn test_encoder_valid() {
        let mut e = ISO2022JPEncoding.raw_encoder();
        assert_feed_ok!(e, "A", "", [0x41]);
        assert_feed_ok!(e, "BC", "", [0x42, 0x43]);
        assert_feed_ok!(e, "\x1b\x24\x42", "", [0x1b, 0x24, 0x42]); // no round-trip guarantee
        assert_feed_ok!(e, "", "", []);
        assert_feed_ok!(e, "\u{a5}", "", [0x5c]);
        assert_feed_ok!(e, "\u{203e}", "", [0x7e]);
        assert_feed_ok!(e, "\u{306b}\u{307b}\u{3093}", "", [0x1b, 0x24, 0x42,
                                                            0x24, 0x4b, 0x24, 0x5b, 0x24, 0x73]);
        assert_feed_ok!(e, "\u{65e5}\u{672c}", "", [0x46, 0x7c, 0x4b, 0x5c]);
        assert_feed_ok!(e, "\u{ff86}\u{ff8e}\u{ff9d}", "", [0x1b, 0x28, 0x49,
                                                            0x46, 0x4e, 0x5d]);
        assert_feed_ok!(e, "XYZ", "", [0x1b, 0x28, 0x42,
                                       0x58, 0x59, 0x5a]);
        assert_finish_ok!(e, []);

        // one ASCII character and two similarly looking characters:
        // - A: U+0020 SPACE (requires ASCII state)
        // - B: U+30CD KATAKANA LETTER NE (requires JIS X 0208 Lead state)
        // - C: U+FF88 HALFWIDTH KATAKANA LETTER NE (requires Katakana state)
        // - D is omitted as the encoder does not support JIS X 0212.
        // a (3,2) De Bruijn near-sequence "ABCACBA" is used to test all possible cases.
        const AD: &'static str = "\x20";
        const BD: &'static str = "\u{30cd}";
        const CD: &'static str = "\u{ff88}";
        const AE: &'static [u8] = &[0x1b, 0x28, 0x42, 0x20];
        const BE: &'static [u8] = &[0x1b, 0x24, 0x42, 0x25, 0x4d];
        const CE: &'static [u8] = &[0x1b, 0x28, 0x49, 0x48];
        let mut e = ISO2022JPEncoding.raw_encoder();
        let decoded: String = ["\x20",      BD, CD, AD, CD, BD, AD].concat();
        let encoded: Vec<_> = [&[0x20][..], BE, CE, AE, CE, BE, AE].concat();
        assert_feed_ok!(e, decoded, "", encoded);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_invalid() {
        let mut e = ISO2022JPEncoding.raw_encoder();
        assert_feed_err!(e, "", "\u{ffff}", "", []);
        assert_feed_err!(e, "?", "\u{ffff}", "!", [0x3f]);
        // JIS X 0212 is not supported in the encoder
        assert_feed_err!(e, "", "\u{736c}", "\u{8c78}", []);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder_valid() {
        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x41], [], "A");
        assert_feed_ok!(d, [0x42, 0x43], [], "BC");
        assert_feed_ok!(d, [0x1b, 0x28, 0x4a,
                            0x44, 0x45, 0x46], [], "DEF");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0x5c], [], "\\");
        assert_feed_ok!(d, [0x7e], [], "~");
        assert_feed_ok!(d, [0x1b, 0x24, 0x42,
                            0x24, 0x4b,
                            0x1b, 0x24, 0x42,
                            0x24, 0x5b, 0x24, 0x73], [], "\u{306b}\u{307b}\u{3093}");
        assert_feed_ok!(d, [0x46, 0x7c, 0x4b, 0x5c], [], "\u{65e5}\u{672c}");
        assert_feed_ok!(d, [0x1b, 0x28, 0x49,
                            0x46, 0x4e, 0x5d], [], "\u{ff86}\u{ff8e}\u{ff9d}");
        assert_feed_ok!(d, [0x1b, 0x24, 0x28, 0x44,
                            0x4b, 0x46,
                            0x1b, 0x24, 0x40,
                            0x6c, 0x38], [], "\u{736c}\u{8c78}");
        assert_feed_ok!(d, [0x1b, 0x28, 0x42,
                            0x58, 0x59, 0x5a], [], "XYZ");
        assert_finish_ok!(d, "");

        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x1b, 0x24, 0x42,
                            0x24, 0x4b, 0x24, 0x5b, 0x24, 0x73], [], "\u{306b}\u{307b}\u{3093}");
        assert_finish_ok!(d, "");

        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x1b, 0x28, 0x49,
                            0x46, 0x4e, 0x5d], [], "\u{ff86}\u{ff8e}\u{ff9d}");
        assert_finish_ok!(d, "");

        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x1b, 0x24, 0x28, 0x44,
                            0x4b, 0x46], [], "\u{736c}");
        assert_finish_ok!(d, "");

        // one ASCII character and three similarly looking characters:
        // - A: U+0020 SPACE (requires ASCII state)
        // - B: U+30CD KATAKANA LETTER NE (requires JIS X 0208 Lead state)
        // - C: U+FF88 HALFWIDTH KATAKANA LETTER NE (requires Katakana state)
        // - D: U+793B CJK UNIFIED IDEOGRAPH-793B (requires JIS X 0212 Lead state)
        // a (4,2) De Bruijn sequence "AABBCCACBADDBDCDA" is used to test all possible cases.
        const AD: &'static str = "\x20";
        const BD: &'static str = "\u{30cd}";
        const CD: &'static str = "\u{ff88}";
        const DD: &'static str = "\u{793b}";
        const AE: &'static [u8] = &[0x1b, 0x28, 0x42,       0x20];
        const BE: &'static [u8] = &[0x1b, 0x24, 0x42,       0x25, 0x4d];
        const CE: &'static [u8] = &[0x1b, 0x28, 0x49,       0x48];
        const DE: &'static [u8] = &[0x1b, 0x24, 0x28, 0x44, 0x50, 0x4b];
        let mut d = ISO2022JPEncoding.raw_decoder();
        let dec: String = ["\x20",     AD,BD,BD,CD,CD,AD,CD,BD,AD,DD,DD,BD,DD,CD,DD,AD].concat();
        let enc: Vec<_> = [&[0x20][..],AE,BE,BE,CE,CE,AE,CE,BE,AE,DE,DE,BE,DE,CE,DE,AE].concat();
        assert_feed_ok!(d, enc, [], dec);
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_valid_partial() {
        let mut d = ISO2022JPEncoding.raw_decoder();

        assert_feed_ok!(d, [], [0x1b], "");
        assert_feed_ok!(d, [], [0x28], "");
        assert_feed_ok!(d, [0x4a, 0x41], [], "A");
        assert_feed_ok!(d, [], [0x1b, 0x28], "");
        assert_feed_ok!(d, [0x4a, 0x42], [0x1b], "B");
        assert_feed_ok!(d, [0x28, 0x4a, 0x43], [], "C");

        assert_feed_ok!(d, [], [0x1b], "");
        assert_feed_ok!(d, [], [0x24], "");
        assert_feed_ok!(d, [0x42], [0x24], "");
        assert_feed_ok!(d, [0x4b], [0x1b, 0x24], "\u{306b}");
        assert_feed_ok!(d, [0x42, 0x24, 0x5b], [], "\u{307b}");
        assert_feed_ok!(d, [], [0x1b], "");
        assert_feed_ok!(d, [0x24, 0x42, 0x24, 0x73], [], "\u{3093}");

        assert_feed_ok!(d, [], [0x1b], "");
        assert_feed_ok!(d, [], [0x28], "");
        assert_feed_ok!(d, [0x49, 0x46], [], "\u{ff86}");
        assert_feed_ok!(d, [], [0x1b, 0x28], "");
        assert_feed_ok!(d, [0x49, 0x4e], [0x1b], "\u{ff8e}");
        assert_feed_ok!(d, [0x28, 0x49, 0x5d], [], "\u{ff9d}");

        assert_feed_ok!(d, [], [0x1b, 0x24], "");
        assert_feed_ok!(d, [], [0x28], "");
        assert_feed_ok!(d, [0x44], [0x4b], "");
        assert_feed_ok!(d, [0x46], [0x1b, 0x24, 0x28], "\u{736c}");
        assert_feed_ok!(d, [0x44, 0x4b, 0x46], [], "\u{736c}");

        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_carriage_return() {
        // CR in Lead state "resets to ASCII"
        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x1b, 0x24, 0x42,
                            0x25, 0x4d,
                            0x0a,
                            0x25, 0x4d], [], "\u{30cd}\n\x25\x4d");
        assert_feed_ok!(d, [0x1b, 0x24, 0x28, 0x44,
                            0x50, 0x4b,
                            0x0a,
                            0x50, 0x4b], [], "\u{793b}\n\x50\x4b");
        assert_finish_ok!(d, "");

        // other states don't allow CR
        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_err!(d, [0x1b, 0x28, 0x49, 0x48], [0x0a], [], "\u{ff88}"); // Katakana
        assert_feed_err!(d, [0x1b, 0x24, 0x42], [0x25, 0x0a], [], ""); // Trail
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_partial() {
        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x1b, 0x24, 0x42, 0x24, 0x4b], [0x24], "\u{306b}");
        assert_finish_err!(d, "");

        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x1b, 0x24, 0x28, 0x44, 0x4b, 0x46], [0x50], "\u{736c}");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_invalid_partial_escape() {
        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [], [0x1b], "");
        assert_finish_err!(d, "");

        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [], [0x1b, 0x24], "");
        assert_finish_err!(d, ""); // no backup

        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [], [0x1b, 0x24, 0x28], "");
        assert_finish_err!(d, -1, ""); // backup of -1, not -2

        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [], [0x1b, 0x28], "");
        assert_finish_err!(d, ""); // no backup

        assert_eq!(ISO2022JPEncoding.decode(&[0x1b], DecoderTrap::Replace),
                   Ok("\u{fffd}".to_string()));
        assert_eq!(ISO2022JPEncoding.decode(&[0x1b, 0x24], DecoderTrap::Replace),
                   Ok("\u{fffd}".to_string()));
        assert_eq!(ISO2022JPEncoding.decode(&[0x1b, 0x24, 0x28], DecoderTrap::Replace),
                   Ok("\u{fffd}\x28".to_string()));
        assert_eq!(ISO2022JPEncoding.decode(&[0x1b, 0x28], DecoderTrap::Replace),
                   Ok("\u{fffd}".to_string()));
    }

    #[test]
    fn test_decoder_invalid_escape() {
        // also tests allowed but never used escape codes in ISO 2022
        let mut d = ISO2022JPEncoding.raw_decoder();
        macro_rules! reset(() => (
            assert_feed_ok!(d, [0x41, 0x42, 0x43, 0x1b, 0x24, 0x42, 0x21, 0x21], [],
                            "ABC\u{3000}")
        ));

        reset!();
        assert_feed_ok!(d, [], [0x1b], "");
        assert_feed_err!(d, [], [], [0x00], "");
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x0a], "");
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x20], "");
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x21, 0x5a], ""); // ESC ! Z (CZD)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x22, 0x5a], ""); // ESC " Z (C1D)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x24, 0x5a], ""); // ESC $ Z (GZDM4)
        reset!();
        assert_feed_ok!(d, [], [0x1b, 0x24], "");
        assert_feed_err!(d, -1, [], [], [0x24, 0x5a], "");
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x24, 0x28, 0x5a], ""); // ESC $ ( Z (GZDM4)
        reset!();
        assert_feed_ok!(d, [], [0x1b, 0x24, 0x28], "");
        assert_feed_err!(d, -2, [], [], [0x24, 0x28, 0x5a], "");
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x24, 0x29, 0x5a], ""); // ESC $ ) Z (G1DM4)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x24, 0x2a, 0x5a], ""); // ESC $ * Z (G2DM4)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x24, 0x2b, 0x5a], ""); // ESC $ + Z (G3DM4)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x24, 0x2d, 0x5a], ""); // ESC $ - Z (G1DM6)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x24, 0x2e, 0x5a], ""); // ESC $ . Z (G2DM6)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x24, 0x2f, 0x5a], ""); // ESC $ / Z (G3DM6)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x25, 0x5a], ""); // ESC % Z (DOCS)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x25, 0x2f, 0x5a], ""); // ESC % / Z (DOCS)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x28, 0x5a], ""); // ESC ( Z (GZD4)
        reset!();
        assert_feed_ok!(d, [], [0x1b, 0x28], "");
        assert_feed_err!(d, -1, [], [], [0x28, 0x5a], "");
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x29, 0x5a], ""); // ESC ) Z (G1D4)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x2a, 0x5a], ""); // ESC * Z (G2D4)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x2b, 0x5a], ""); // ESC + Z (G3D4)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x2d, 0x5a], ""); // ESC - Z (G1D6)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x2e, 0x5a], ""); // ESC . Z (G2D6)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x2f, 0x5a], ""); // ESC / Z (G3D6)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x4e], ""); // ESC N (SS2)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x4f], ""); // ESC O (SS3)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x6e], ""); // ESC n (LS2)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x6f], ""); // ESC o (LS3)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x7c], ""); // ESC | (LS3R)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x7d], ""); // ESC } (LS2R)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0x7e], ""); // ESC ~ (LS1R)
        reset!();
        assert_feed_err!(d, [], [0x1b], [0xff], "");
        reset!();
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_out_or_range() {
        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_err!(d, [], [0x80], [], "");
        assert_feed_err!(d, [], [0xff], [], "");
        assert_feed_err!(d, [0x1b, 0x24, 0x42], [0x80, 0x21], [], "");
        assert_feed_err!(d, [0x1b, 0x24, 0x42], [0x21, 0x80], [], "");
        assert_feed_err!(d, [0x1b, 0x24, 0x42], [0x20, 0x21], [], "");
        assert_feed_err!(d, [0x1b, 0x24, 0x42], [0x21, 0x20], [], "");
        assert_feed_err!(d, [0x1b, 0x28, 0x49], [0x20], [], "");
        assert_feed_err!(d, [0x1b, 0x28, 0x49], [0x60], [], "");
        assert_feed_err!(d, [0x1b, 0x24, 0x28, 0x44], [0x80, 0x21], [], "");
        assert_feed_err!(d, [0x1b, 0x24, 0x28, 0x44], [0x21, 0x80], [], "");
        assert_feed_err!(d, [0x1b, 0x24, 0x28, 0x44], [0x20, 0x21], [], "");
        assert_feed_err!(d, [0x1b, 0x24, 0x28, 0x44], [0x21, 0x20], [], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_feed_after_finish() {
        let mut d = ISO2022JPEncoding.raw_decoder();
        assert_feed_ok!(d, [0x24, 0x22,
                            0x1b, 0x24, 0x42,
                            0x24, 0x22], [0x24], "\x24\x22\u{3042}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0x24, 0x22,
                            0x1b, 0x24, 0x42,
                            0x24, 0x22], [], "\x24\x22\u{3042}");
        assert_finish_ok!(d, "");
    }

    #[bench]
    fn bench_encode_short_text(bencher: &mut test::Bencher) {
        let s = testutils::JAPANESE_TEXT;
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            ISO2022JPEncoding.encode(&s, EncoderTrap::Strict)
        }))
    }

    #[bench]
    fn bench_decode_short_text(bencher: &mut test::Bencher) {
        let s = ISO2022JPEncoding.encode(testutils::JAPANESE_TEXT,
                                         EncoderTrap::Strict).ok().unwrap();
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            ISO2022JPEncoding.decode(&s, DecoderTrap::Strict)
        }))
    }
}
