// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! Legacy Korean encodings based on KS X 1001.

use std::convert::Into;
use std::default::Default;
use util::StrCharIndex;
use index_korean as index;
use types::*;

/**
 * Windows code page 949.
 *
 * This is a Korean encoding derived from EUC-KR,
 * which is so widespread that most occurrences of EUC-KR actually mean this encoding.
 * Unlike KS X 1001 (and EUC-KR) which only contains a set of 2,350 common Hangul syllables,
 * it assigns remaining 8,822 Hangul syllables to the two-byte sequence
 * which second byte have its MSB unset (i.e. `[81-C6] [41-5A 61-7A 81-FE]`).
 * Its design strongly resembles that of Shift_JIS but less prone to errors
 * since the set of MSB-unset second bytes is much limited compared to Shift_JIS.
 */
#[derive(Clone, Copy)]
pub struct Windows949Encoding;

impl Encoding for Windows949Encoding {
    fn name(&self) -> &'static str { "windows-949" }
    fn whatwg_name(&self) -> Option<&'static str> { Some("euc-kr") } // WHATWG compatibility
    fn raw_encoder(&self) -> Box<RawEncoder> { Windows949Encoder::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { Windows949Decoder::new() }
}

/// An encoder for Windows code page 949.
#[derive(Clone, Copy)]
pub struct Windows949Encoder;

impl Windows949Encoder {
    pub fn new() -> Box<RawEncoder> { Box::new(Windows949Encoder) }
}

impl RawEncoder for Windows949Encoder {
    fn from_self(&self) -> Box<RawEncoder> { Windows949Encoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len());

        for ((i,j), ch) in input.index_iter() {
            if ch <= '\u{7f}' {
                output.write_byte(ch as u8);
            } else {
                let ptr = index::euc_kr::backward(ch as u32);
                if ptr == 0xffff {
                    return (i, Some(CodecError {
                        upto: j as isize, cause: "unrepresentable character".into()
                    }));
                } else {
                    output.write_byte((ptr / 190 + 0x81) as u8);
                    output.write_byte((ptr % 190 + 0x41) as u8);
                }
            }
        }
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder for Windows code page 949.
#[derive(Clone, Copy)]
struct Windows949Decoder {
    st: windows949::State,
}

impl Windows949Decoder {
    pub fn new() -> Box<RawDecoder> {
        Box::new(Windows949Decoder { st: Default::default() })
    }
}

impl RawDecoder for Windows949Decoder {
    fn from_self(&self) -> Box<RawDecoder> { Windows949Decoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        let (st, processed, err) = windows949::raw_feed(self.st, input, output, &());
        self.st = st;
        (processed, err)
    }

    fn raw_finish(&mut self, output: &mut StringWriter) -> Option<CodecError> {
        let (st, err) = windows949::raw_finish(self.st, output, &());
        self.st = st;
        err
    }
}

stateful_decoder! {
    module windows949;

    internal pub fn map_two_bytes(lead: u8, trail: u8) -> u32 {
        use index_korean as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let index = match (lead, trail) {
            (0x81...0xfe, 0x41...0xfe) => (lead - 0x81) * 190 + (trail - 0x41),
            (_, _) => 0xffff,
        };
        index::euc_kr::forward(index)
    }

initial:
    // euc-kr lead = 0x00
    state S0(ctx: Context) {
        case b @ 0x00...0x7f => ctx.emit(b as u32);
        case b @ 0x81...0xfe => S1(ctx, b);
        case _ => ctx.err("invalid sequence");
    }

transient:
    // euc-kr lead != 0x00
    state S1(ctx: Context, lead: u8) {
        case b => match map_two_bytes(lead, b) {
            0xffff => {
                let backup = if b < 0x80 {1} else {0};
                ctx.backup_and_err(backup, "invalid sequence")
            },
            ch => ctx.emit(ch as u32)
        };
    }
}

#[cfg(test)]
mod windows949_tests {
    extern crate test;
    use super::Windows949Encoding;
    use testutils;
    use types::*;

    #[test]
    fn test_encoder_valid() {
        let mut e = Windows949Encoding.raw_encoder();
        assert_feed_ok!(e, "A", "", [0x41]);
        assert_feed_ok!(e, "BC", "", [0x42, 0x43]);
        assert_feed_ok!(e, "", "", []);
        assert_feed_ok!(e, "\u{ac00}", "", [0xb0, 0xa1]);
        assert_feed_ok!(e, "\u{b098}\u{b2e4}", "", [0xb3, 0xaa, 0xb4, 0xd9]);
        assert_feed_ok!(e, "\u{bdc1}\u{314b}\u{d7a3}", "", [0x94, 0xee, 0xa4, 0xbb, 0xc6, 0x52]);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_invalid() {
        let mut e = Windows949Encoding.raw_encoder();
        assert_feed_err!(e, "", "\u{ffff}", "", []);
        assert_feed_err!(e, "?", "\u{ffff}", "!", [0x3f]);
        assert_feed_err!(e, "?", "\u{fffd}", "!", [0x3f]); // for invalid table entries
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder_valid() {
        let mut d = Windows949Encoding.raw_decoder();
        assert_feed_ok!(d, [0x41], [], "A");
        assert_feed_ok!(d, [0x42, 0x43], [], "BC");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0xb0, 0xa1], [], "\u{ac00}");
        assert_feed_ok!(d, [0xb3, 0xaa, 0xb4, 0xd9], [], "\u{b098}\u{b2e4}");
        assert_feed_ok!(d, [0x94, 0xee, 0xa4, 0xbb, 0xc6, 0x52, 0xc1, 0x64], [],
                        "\u{bdc1}\u{314b}\u{d7a3}\u{d58f}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_valid_partial() {
        let mut d = Windows949Encoding.raw_decoder();
        assert_feed_ok!(d, [], [0xb0], "");
        assert_feed_ok!(d, [0xa1], [], "\u{ac00}");
        assert_feed_ok!(d, [0xb3, 0xaa], [0xb4], "\u{b098}");
        assert_feed_ok!(d, [0xd9], [0x94], "\u{b2e4}");
        assert_feed_ok!(d, [0xee, 0xa4, 0xbb], [0xc6], "\u{bdc1}\u{314b}");
        assert_feed_ok!(d, [0x52, 0xc1, 0x64], [], "\u{d7a3}\u{d58f}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lead_immediate_test_finish() {
        for i in 0x81..0xff {
            let mut d = Windows949Encoding.raw_decoder();
            assert_feed_ok!(d, [], [i], ""); // wait for a trail
            assert_finish_err!(d, "");
        }

        // 80/FF: immediate failure
        let mut d = Windows949Encoding.raw_decoder();
        assert_feed_err!(d, [], [0x80], [], "");
        assert_feed_err!(d, [], [0xff], [], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_lone_lead_followed_by_space() {
        for i in 0x80..0x100 {
            let i = i as u8;
            let mut d = Windows949Encoding.raw_decoder();
            assert_feed_err!(d, [], [i], [0x20], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_decoder_invalid_lead_followed_by_invalid_trail() {
        // should behave similarly to Big5.
        // https://www.w3.org/Bugs/Public/show_bug.cgi?id=16691
        for i in 0x81..0xff {
            let mut d = Windows949Encoding.raw_decoder();
            assert_feed_err!(d, [], [i, 0x80], [0x20], "");
            assert_feed_err!(d, [], [i, 0xff], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = Windows949Encoding.raw_decoder();
            assert_feed_ok!(d, [], [i], "");
            assert_feed_err!(d, [], [0x80], [0x20], "");
            assert_feed_ok!(d, [], [i], "");
            assert_feed_err!(d, [], [0xff], [0x20], "");
            assert_finish_ok!(d, "");
        }

        let mut d = Windows949Encoding.raw_decoder();
        assert_feed_err!(d, [], [0x80], [0x80], "");
        assert_feed_err!(d, [], [0x80], [0xff], "");
        assert_feed_err!(d, [], [0xff], [0x80], "");
        assert_feed_err!(d, [], [0xff], [0xff], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_boundary() {
        // U+D7A3 (C6 52) is the last Hangul syllable not in KS X 1001, C6 53 is invalid.
        // note that since the trail byte may coincide with ASCII, the trail byte 53 is
        // not considered to be in the problem. this is compatible to WHATWG Encoding standard.
        let mut d = Windows949Encoding.raw_decoder();
        assert_feed_ok!(d, [], [0xc6], "");
        assert_feed_err!(d, [], [], [0x53], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_feed_after_finish() {
        let mut d = Windows949Encoding.raw_decoder();
        assert_feed_ok!(d, [0xb0, 0xa1], [0xb0], "\u{ac00}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0xb0, 0xa1], [], "\u{ac00}");
        assert_finish_ok!(d, "");
    }

    #[bench]
    fn bench_encode_short_text(bencher: &mut test::Bencher) {
        let s = testutils::KOREAN_TEXT;
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            Windows949Encoding.encode(&s, EncoderTrap::Strict)
        }))
    }

    #[bench]
    fn bench_decode_short_text(bencher: &mut test::Bencher) {
        let s = Windows949Encoding.encode(testutils::KOREAN_TEXT,
                                          EncoderTrap::Strict).ok().unwrap();
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            Windows949Encoding.decode(&s, DecoderTrap::Strict)
        }))
    }
}

