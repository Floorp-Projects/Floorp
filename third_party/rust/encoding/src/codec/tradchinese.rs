// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! Legacy traditional Chinese encodings.

use std::convert::Into;
use std::default::Default;
use util::StrCharIndex;
use index_tradchinese as index;
use types::*;

/**
 * Big5-2003 with common extensions. (XXX with asymmetric HKSCS-2008 support)
 *
 * This is a traditional Chinese encoding spanning the region `[81-FE] [40-7E A1-FE]`.
 * Originally a proprietary encoding by the consortium of five companies (hence the name),
 * the Republic of China government standardized Big5-2003 in an appendix of CNS 11643
 * so that CNS 11643 plane 1 and plane 2 have
 * an almost identical set of characters as Big5 (but with a different mapping).
 * The Hong Kong government has an official extension to Big5
 * named Hong Kong Supplementary Character Set (HKSCS).
 *
 * This particular implementation of Big5 includes the widespread ETEN and HKSCS extensions,
 * but excludes less common extensions such as Big5+, Big-5E and Unicode-at-on.
 */
#[derive(Clone, Copy)]
pub struct BigFive2003Encoding;

impl Encoding for BigFive2003Encoding {
    fn name(&self) -> &'static str { "big5-2003" }
    fn whatwg_name(&self) -> Option<&'static str> { Some("big5") } // WHATWG compatibility
    fn raw_encoder(&self) -> Box<RawEncoder> { BigFive2003Encoder::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { BigFive2003HKSCS2008Decoder::new() }
}

/// An encoder for Big5-2003.
#[derive(Clone, Copy)]
pub struct BigFive2003Encoder;

impl BigFive2003Encoder {
    pub fn new() -> Box<RawEncoder> { Box::new(BigFive2003Encoder) }
}

impl RawEncoder for BigFive2003Encoder {
    fn from_self(&self) -> Box<RawEncoder> { BigFive2003Encoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len());

        for ((i,j), ch) in input.index_iter() {
            if ch < '\u{80}' {
                output.write_byte(ch as u8);
            } else {
                let ptr = index::big5::backward(ch as u32);
                if ptr == 0xffff || ptr < (0xa1 - 0x81) * 157 {
                    // no HKSCS extension (XXX doesn't HKSCS include 0xFA40..0xFEFE?)
                    return (i, Some(CodecError {
                        upto: j as isize, cause: "unrepresentable character".into()
                    }));
                }
                let lead = ptr / 157 + 0x81;
                let trail = ptr % 157;
                let trailoffset = if trail < 0x3f {0x40} else {0x62};
                output.write_byte(lead as u8);
                output.write_byte((trail + trailoffset) as u8);
            }
        }
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder for Big5-2003 with HKSCS-2008 extension.
#[derive(Clone, Copy)]
struct BigFive2003HKSCS2008Decoder {
    st: bigfive2003::State,
}

impl BigFive2003HKSCS2008Decoder {
    pub fn new() -> Box<RawDecoder> {
        Box::new(BigFive2003HKSCS2008Decoder { st: Default::default() })
    }
}

impl RawDecoder for BigFive2003HKSCS2008Decoder {
    fn from_self(&self) -> Box<RawDecoder> { BigFive2003HKSCS2008Decoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        let (st, processed, err) = bigfive2003::raw_feed(self.st, input, output, &());
        self.st = st;
        (processed, err)
    }

    fn raw_finish(&mut self, output: &mut StringWriter) -> Option<CodecError> {
        let (st, err) = bigfive2003::raw_finish(self.st, output, &());
        self.st = st;
        err
    }
}

stateful_decoder! {
    module bigfive2003;

    internal pub fn map_two_bytes(lead: u8, trail: u8) -> u32 {
        use index_tradchinese as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let index = match (lead, trail) {
            (0x81...0xfe, 0x40...0x7e) | (0x81...0xfe, 0xa1...0xfe) => {
                let trailoffset = if trail < 0x7f {0x40} else {0x62};
                (lead - 0x81) * 157 + trail - trailoffset
            }
            _ => 0xffff,
        };
        index::big5::forward(index) // may return two-letter replacements 0..3
    }

initial:
    // big5 lead = 0x00
    state S0(ctx: Context) {
        case b @ 0x00...0x7f => ctx.emit(b as u32);
        case b @ 0x81...0xfe => S1(ctx, b);
        case _ => ctx.err("invalid sequence");
    }

transient:
    // big5 lead != 0x00
    state S1(ctx: Context, lead: u8) {
        case b => match map_two_bytes(lead, b) {
            0xffff => {
                let backup = if b < 0x80 {1} else {0};
                ctx.backup_and_err(backup, "invalid sequence")
            },
            0 /*index=1133*/ => ctx.emit_str("\u{ca}\u{304}"),
            1 /*index=1135*/ => ctx.emit_str("\u{ca}\u{30c}"),
            2 /*index=1164*/ => ctx.emit_str("\u{ea}\u{304}"),
            3 /*index=1166*/ => ctx.emit_str("\u{ea}\u{30c}"),
            ch => ctx.emit(ch),
        };
    }
}

#[cfg(test)]
mod bigfive2003_tests {
    extern crate test;
    use super::BigFive2003Encoding;
    use testutils;
    use types::*;

    #[test]
    fn test_encoder_valid() {
        let mut e = BigFive2003Encoding.raw_encoder();
        assert_feed_ok!(e, "A", "", [0x41]);
        assert_feed_ok!(e, "BC", "", [0x42, 0x43]);
        assert_feed_ok!(e, "", "", []);
        assert_feed_ok!(e, "\u{4e2d}\u{83ef}\u{6c11}\u{570b}", "",
                        [0xa4, 0xa4, 0xb5, 0xd8, 0xa5, 0xc1, 0xb0, 0xea]);
        assert_feed_ok!(e, "1\u{20ac}/m", "", [0x31, 0xa3, 0xe1, 0x2f, 0x6d]);
        assert_feed_ok!(e, "\u{ffed}", "", [0xf9, 0xfe]);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_invalid() {
        let mut e = BigFive2003Encoding.raw_encoder();
        assert_feed_err!(e, "", "\u{ffff}", "", []);
        assert_feed_err!(e, "?", "\u{ffff}", "!", [0x3f]);
        assert_feed_err!(e, "", "\u{3eec}", "\u{4e00}", []); // HKSCS-2008 addition
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder_valid() {
        let mut d = BigFive2003Encoding.raw_decoder();
        assert_feed_ok!(d, [0x41], [], "A");
        assert_feed_ok!(d, [0x42, 0x43], [], "BC");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0xa4, 0xa4, 0xb5, 0xd8, 0xa5, 0xc1, 0xb0, 0xea], [],
                        "\u{4e2d}\u{83ef}\u{6c11}\u{570b}");
        assert_feed_ok!(d, [], [0xa4], "");
        assert_feed_ok!(d, [0xa4, 0xb5, 0xd8], [0xa5], "\u{4e2d}\u{83ef}");
        assert_feed_ok!(d, [0xc1, 0xb0, 0xea], [], "\u{6c11}\u{570b}");
        assert_feed_ok!(d, [0x31, 0xa3, 0xe1, 0x2f, 0x6d], [], "1\u{20ac}/m");
        assert_feed_ok!(d, [0xf9, 0xfe], [], "\u{ffed}");
        assert_feed_ok!(d, [0x87, 0x7e], [], "\u{3eec}"); // HKSCS-2008 addition
        assert_feed_ok!(d, [0x88, 0x62, 0x88, 0x64, 0x88, 0xa3, 0x88, 0xa5], [],
                        "\u{ca}\u{304}\u{00ca}\u{30c}\u{ea}\u{304}\u{ea}\u{30c}"); // 2-byte output
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lead_immediate_test_finish() {
        for i in 0x81..0xff {
            let mut d = BigFive2003Encoding.raw_decoder();
            assert_feed_ok!(d, [], [i], ""); // wait for a trail
            assert_finish_err!(d, "");
        }

        // 80/FF: immediate failure
        let mut d = BigFive2003Encoding.raw_decoder();
        assert_feed_err!(d, [], [0x80], [], "");
        assert_feed_err!(d, [], [0xff], [], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lead_followed_by_space() {
        for i in 0x80..0x100 {
            let i = i as u8;
            let mut d = BigFive2003Encoding.raw_decoder();
            assert_feed_err!(d, [], [i], [0x20], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_lead_followed_by_invalid_trail() {
        // unlike most other cases, valid lead + invalid MSB-set trail are entirely consumed.
        // https://www.w3.org/Bugs/Public/show_bug.cgi?id=16771
        for i in 0x81..0xff {
            let mut d = BigFive2003Encoding.raw_decoder();
            assert_feed_err!(d, [], [i, 0x80], [0x20], "");
            assert_feed_err!(d, [], [i, 0xff], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = BigFive2003Encoding.raw_decoder();
            assert_feed_ok!(d, [], [i], "");
            assert_feed_err!(d, [], [0x80], [0x20], "");
            assert_feed_ok!(d, [], [i], "");
            assert_feed_err!(d, [], [0xff], [0x20], "");
            assert_finish_ok!(d, "");
        }

        // 80/FF is not a valid lead and the trail is not consumed
        let mut d = BigFive2003Encoding.raw_decoder();
        assert_feed_err!(d, [], [0x80], [0x80], "");
        assert_feed_err!(d, [], [0x80], [0xff], "");
        assert_feed_err!(d, [], [0xff], [0x80], "");
        assert_feed_err!(d, [], [0xff], [0xff], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_feed_after_finish() {
        let mut d = BigFive2003Encoding.raw_decoder();
        assert_feed_ok!(d, [0xa4, 0x40], [0xa4], "\u{4e00}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0xa4, 0x40], [], "\u{4e00}");
        assert_finish_ok!(d, "");
    }

    #[bench]
    fn bench_encode_short_text(bencher: &mut test::Bencher) {
        let s = testutils::TRADITIONAL_CHINESE_TEXT;
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            BigFive2003Encoding.encode(&s, EncoderTrap::Strict)
        }))
    }

    #[bench]
    fn bench_decode_short_text(bencher: &mut test::Bencher) {
        let s = BigFive2003Encoding.encode(testutils::TRADITIONAL_CHINESE_TEXT,
                                           EncoderTrap::Strict).ok().unwrap();
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            BigFive2003Encoding.decode(&s, DecoderTrap::Strict)
        }))
    }
}
