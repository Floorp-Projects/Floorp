// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! Legacy simplified Chinese encodings based on GB 2312 and GB 18030.

use std::convert::Into;
use std::marker::PhantomData;
use std::default::Default;
use util::StrCharIndex;
use index_simpchinese as index;
use types::*;

/// An implementation type for GBK.
///
/// Can be used as a type parameter to `GBEncoding` and `GBEncoder`.
/// (GB18030Decoder is shared by both.)
#[derive(Clone, Copy)]
pub struct GBK;

/// An implementation type for GB18030.
///
/// Can be used as a type parameter to `GBEncoding` and `GBEncoder.'
/// (GB18030Decoder is shared by both.)
#[derive(Clone, Copy)]
pub struct GB18030;

/// An internal trait used to customize GBK and GB18030 implementations.
#[doc(hidden)] // XXX never intended to be used publicly, should be gone later
pub trait GBType: Clone + 'static {
    fn name() -> &'static str;
    fn whatwg_name() -> Option<&'static str>;
    fn initial_gbk_flag() -> bool;
}

impl GBType for GBK {
    fn name() -> &'static str { "gbk" }
    fn whatwg_name() -> Option<&'static str> { Some("gbk") }
    fn initial_gbk_flag() -> bool { true }
}

impl GBType for GB18030 {
    fn name() -> &'static str { "gb18030" }
    fn whatwg_name() -> Option<&'static str> { Some("gb18030") }
    fn initial_gbk_flag() -> bool { false }
}

/**
 * GBK and GB 18030-2005.
 *
 * The original GBK 1.0 region spans `[81-FE] [40-7E 80-FE]`, and is derived from
 * several different revisions of a family of encodings named "GBK":
 *
 * - GBK as specified in the normative annex of GB 13000.1-93,
 *   the domestic standard equivalent to Unicode 1.1,
 *   consisted of characters included in Unicode 1.1 and not in GB 2312-80.
 * - Windows code page 936 is the widespread extension to GBK.
 * - Due to the popularity of Windows code page 936,
 *   a formal encoding based on Windows code page 936 (while adding new characters)
 *   was standardized into GBK 1.0.
 * - Finally, GB 18030 added four-byte sequences to GBK for becoming a pan-Unicode encoding,
 *   while adding new characters to the (former) GBK region again.
 *
 * GB 18030-2005 is a simplified Chinese encoding which extends GBK 1.0 to a pan-Unicode encoding.
 * It assigns four-byte sequences to every Unicode codepoint missing from the GBK area,
 * lexicographically ordered with occasional "gaps" for codepoints in the GBK area.
 * Due to this compatibility decision,
 * there is no simple relationship between these four-byte sequences and Unicode codepoints,
 * though there *exists* a relatively simple mapping algorithm with a small lookup table.
 *
 * ## Specialization
 *
 * This type is specialized with GBType `T`,
 * which should be either `GBK` or `GB18030`.
 */
#[derive(Clone, Copy)]
pub struct GBEncoding<T> {
    _marker: PhantomData<T>
}

/// A type for GBK.
pub type GBKEncoding = GBEncoding<GBK>;
/// A type for GB18030.
pub type GB18030Encoding = GBEncoding<GB18030>;

/// An instance for GBK.
pub const GBK_ENCODING: GBKEncoding = GBEncoding { _marker: PhantomData };
/// An instance for GB18030.
pub const GB18030_ENCODING: GB18030Encoding = GBEncoding { _marker: PhantomData };

impl<T: GBType> Encoding for GBEncoding<T> {
    fn name(&self) -> &'static str { <T as GBType>::name() }
    fn whatwg_name(&self) -> Option<&'static str> { <T as GBType>::whatwg_name() }
    fn raw_encoder(&self) -> Box<RawEncoder> { GBEncoder::<T>::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { GB18030Decoder::new() }
}

/**
 * An encoder for GBK and GB18030.
 *
 * ## Specialization
 *
 * This type is specialized with GBType `T`,
 * which should be either `GBK` or `GB18030`.
 */
#[derive(Clone, Copy)]
pub struct GBEncoder<T> {
    _marker: PhantomData<T>
}

impl<T: GBType> GBEncoder<T> {
    pub fn new() -> Box<RawEncoder> {
        Box::new(GBEncoder::<T> { _marker: PhantomData })
    }
}

impl<T: GBType> RawEncoder for GBEncoder<T> {
    fn from_self(&self) -> Box<RawEncoder> { GBEncoder::<T>::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len());

        let gbk_flag = <T as GBType>::initial_gbk_flag();
        for ((i, j), ch) in input.index_iter() {
            if ch < '\u{80}' {
                output.write_byte(ch as u8);
            } else if gbk_flag && ch == '\u{20AC}' {
                output.write_byte('\u{80}' as u8)
            } else {
                let ptr = index::gb18030::backward(ch as u32);
                if ptr == 0xffff {
                    if gbk_flag {
                        return (i, Some(CodecError {
                            upto: j as isize,
                            cause: "gbk doesn't support gb18030 extensions".into()
                        }));
                    }
                    let ptr = index::gb18030_ranges::backward(ch as u32);
                    assert!(ptr != 0xffffffff);
                    let (ptr, byte4) = (ptr / 10, ptr % 10);
                    let (ptr, byte3) = (ptr / 126, ptr % 126);
                    let (byte1, byte2) = (ptr / 10, ptr % 10);
                    output.write_byte((byte1 + 0x81) as u8);
                    output.write_byte((byte2 + 0x30) as u8);
                    output.write_byte((byte3 + 0x81) as u8);
                    output.write_byte((byte4 + 0x30) as u8);
                } else {
                    let lead = ptr / 190 + 0x81;
                    let trail = ptr % 190;
                    let trailoffset = if trail < 0x3f {0x40} else {0x41};
                    output.write_byte(lead as u8);
                    output.write_byte((trail + trailoffset) as u8);
                }
            }
        }
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder for GB 18030 (also used by GBK).
#[derive(Clone, Copy)]
struct GB18030Decoder {
    st: gb18030::State,
}

impl GB18030Decoder {
    pub fn new() -> Box<RawDecoder> {
        Box::new(GB18030Decoder { st: Default::default() })
    }
}

impl RawDecoder for GB18030Decoder {
    fn from_self(&self) -> Box<RawDecoder> { GB18030Decoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        let (st, processed, err) = gb18030::raw_feed(self.st, input, output, &());
        self.st = st;
        (processed, err)
    }

    fn raw_finish(&mut self, output: &mut StringWriter) -> Option<CodecError> {
        let (st, err) = gb18030::raw_finish(self.st, output, &());
        self.st = st;
        err
    }
}

stateful_decoder! {
    module gb18030;

    internal pub fn map_two_bytes(lead: u8, trail: u8) -> u32 {
        use index_simpchinese as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let index = match (lead, trail) {
            (0x81...0xfe, 0x40...0x7e) | (0x81...0xfe, 0x80...0xfe) => {
                let trailoffset = if trail < 0x7f {0x40} else {0x41};
                (lead - 0x81) * 190 + trail - trailoffset
            }
            _ => 0xffff,
        };
        index::gb18030::forward(index)
    }

    internal pub fn map_four_bytes(b1: u8, b2: u8, b3: u8, b4: u8) -> u32 {
        use index_simpchinese as index;

        // no range check here, caller should have done all checks
        let index = (b1 as u32 - 0x81) * 12600 + (b2 as u32 - 0x30) * 1260 +
                    (b3 as u32 - 0x81) * 10 + (b4 as u32 - 0x30);
        index::gb18030_ranges::forward(index)
    }

initial:
    // gb18030 first = 0x00, gb18030 second = 0x00, gb18030 third = 0x00
    state S0(ctx: Context) {
        case b @ 0x00...0x7f => ctx.emit(b as u32);
        case 0x80 => ctx.emit(0x20ac);
        case b @ 0x81...0xfe => S1(ctx, b);
        case _ => ctx.err("invalid sequence");
    }

transient:
    // gb18030 first != 0x00, gb18030 second = 0x00, gb18030 third = 0x00
    state S1(ctx: Context, first: u8) {
        case b @ 0x30...0x39 => S2(ctx, first, b);
        case b => match map_two_bytes(first, b) {
            0xffff => ctx.backup_and_err(1, "invalid sequence"), // unconditional
            ch => ctx.emit(ch)
        };
    }

    // gb18030 first != 0x00, gb18030 second != 0x00, gb18030 third = 0x00
    state S2(ctx: Context, first: u8, second: u8) {
        case b @ 0x81...0xfe => S3(ctx, first, second, b);
        case _ => ctx.backup_and_err(2, "invalid sequence");
    }

    // gb18030 first != 0x00, gb18030 second != 0x00, gb18030 third != 0x00
    state S3(ctx: Context, first: u8, second: u8, third: u8) {
        case b @ 0x30...0x39 => match map_four_bytes(first, second, third, b) {
            0xffffffff => ctx.backup_and_err(3, "invalid sequence"), // unconditional
            ch => ctx.emit(ch)
        };
        case _ => ctx.backup_and_err(3, "invalid sequence");
    }
}

#[cfg(test)]
mod gb18030_tests {
    extern crate test;
    use super::GB18030_ENCODING;
    use testutils;
    use types::*;

    #[test]
    fn test_encoder() {
        let mut e = GB18030_ENCODING.raw_encoder();
        assert_feed_ok!(e, "A", "", [0x41]);
        assert_feed_ok!(e, "BC", "", [0x42, 0x43]);
        assert_feed_ok!(e, "", "", []);
        assert_feed_ok!(e, "\u{4e2d}\u{534e}\u{4eba}\u{6c11}\u{5171}\u{548c}\u{56fd}", "",
                        [0xd6, 0xd0, 0xbb, 0xaa, 0xc8, 0xcb, 0xc3, 0xf1,
                         0xb9, 0xb2, 0xba, 0xcd, 0xb9, 0xfa]);
        assert_feed_ok!(e, "1\u{20ac}/m", "", [0x31, 0xa2, 0xe3, 0x2f, 0x6d]);
        assert_feed_ok!(e, "\u{ff21}\u{ff22}\u{ff23}", "", [0xa3, 0xc1, 0xa3, 0xc2, 0xa3, 0xc3]);
        assert_feed_ok!(e, "\u{80}", "", [0x81, 0x30, 0x81, 0x30]);
        assert_feed_ok!(e, "\u{81}", "", [0x81, 0x30, 0x81, 0x31]);
        assert_feed_ok!(e, "\u{a3}", "", [0x81, 0x30, 0x84, 0x35]);
        assert_feed_ok!(e, "\u{a4}", "", [0xa1, 0xe8]);
        assert_feed_ok!(e, "\u{a5}", "", [0x81, 0x30, 0x84, 0x36]);
        assert_feed_ok!(e, "\u{10ffff}", "", [0xe3, 0x32, 0x9a, 0x35]);
        assert_feed_ok!(e, "\u{2a6a5}\u{3007}", "", [0x98, 0x35, 0xee, 0x37, 0xa9, 0x96]);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder_valid() {
        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [0x41], [], "A");
        assert_feed_ok!(d, [0x42, 0x43], [], "BC");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0xd6, 0xd0, 0xbb, 0xaa, 0xc8, 0xcb, 0xc3, 0xf1,
                            0xb9, 0xb2, 0xba, 0xcd, 0xb9, 0xfa], [],
                        "\u{4e2d}\u{534e}\u{4eba}\u{6c11}\u{5171}\u{548c}\u{56fd}");
        assert_feed_ok!(d, [0x31, 0x80, 0x2f, 0x6d], [], "1\u{20ac}/m");
        assert_feed_ok!(d, [0xa3, 0xc1, 0xa3, 0xc2, 0xa3, 0xc3], [], "\u{ff21}\u{ff22}\u{ff23}");
        assert_feed_ok!(d, [0x81, 0x30, 0x81, 0x30], [], "\u{80}");
        assert_feed_ok!(d, [0x81, 0x30, 0x81, 0x31], [], "\u{81}");
        assert_feed_ok!(d, [0x81, 0x30, 0x84, 0x35], [], "\u{a3}");
        assert_feed_ok!(d, [0xa1, 0xe8], [], "\u{a4}" );
        assert_feed_ok!(d, [0x81, 0x30, 0x84, 0x36], [], "\u{a5}");
        assert_feed_ok!(d, [0xe3, 0x32, 0x9a, 0x35], [], "\u{10ffff}");
        assert_feed_ok!(d, [0x98, 0x35, 0xee, 0x37, 0xa9, 0x96], [], "\u{2a6a5}\u{3007}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_valid_partial() {
        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [], [0xa1], "");
        assert_feed_ok!(d, [0xa1], [], "\u{3000}");
        assert_feed_ok!(d, [], [0x81], "");
        assert_feed_ok!(d, [], [0x30], "");
        assert_feed_ok!(d, [], [0x81], "");
        assert_feed_ok!(d, [0x30], [], "\u{80}");
        assert_feed_ok!(d, [], [0x81], "");
        assert_feed_ok!(d, [], [0x30], "");
        assert_feed_ok!(d, [0x81, 0x31], [], "\u{81}");
        assert_feed_ok!(d, [], [0x81], "");
        assert_feed_ok!(d, [0x30, 0x81, 0x32], [], "\u{82}");
        assert_feed_ok!(d, [], [0x81], "");
        assert_feed_ok!(d, [], [0x30, 0x81], "");
        assert_feed_ok!(d, [0x33], [], "\u{83}");
        assert_feed_ok!(d, [], [0x81, 0x30], "");
        assert_feed_ok!(d, [], [0x81], "");
        assert_feed_ok!(d, [0x34], [], "\u{84}");
        assert_feed_ok!(d, [], [0x81, 0x30], "");
        assert_feed_ok!(d, [0x81, 0x35], [], "\u{85}");
        assert_feed_ok!(d, [], [0x81, 0x30, 0x81], "");
        assert_feed_ok!(d, [0x36], [], "\u{86}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_partial() {
        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [], [0xa1], "");
        assert_finish_err!(d, "");

        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [], [0x81], "");
        assert_finish_err!(d, "");

        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [], [0x81, 0x30], "");
        assert_finish_err!(d, "");

        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [], [0x81, 0x30, 0x81], "");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_invalid_out_of_range() {
        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_err!(d, [], [0xff], [], "");
        assert_feed_err!(d, [], [0x81], [0x00], "");
        assert_feed_err!(d, [], [0x81], [0x7f], "");
        assert_feed_err!(d, [], [0x81], [0xff], "");
        assert_feed_err!(d, [], [0x81], [0x31, 0x00], "");
        assert_feed_err!(d, [], [0x81], [0x31, 0x80], "");
        assert_feed_err!(d, [], [0x81], [0x31, 0xff], "");
        assert_feed_err!(d, [], [0x81], [0x31, 0x81, 0x00], "");
        assert_feed_err!(d, [], [0x81], [0x31, 0x81, 0x2f], "");
        assert_feed_err!(d, [], [0x81], [0x31, 0x81, 0x3a], "");
        assert_feed_err!(d, [], [0x81], [0x31, 0x81, 0xff], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_boundary() {
        // U+10FFFF (E3 32 9A 35) is the last Unicode codepoint, E3 32 9A 36 is invalid.
        // note that since the 2nd to 4th bytes may coincide with ASCII, bytes 32 9A 36 is
        // not considered to be in the problem. this is compatible to WHATWG Encoding standard.
        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [], [0xe3], "");
        assert_feed_err!(d, [], [], [0x32, 0x9a, 0x36], "");
        assert_finish_ok!(d, "");

        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [], [0xe3], "");
        assert_feed_ok!(d, [], [0x32, 0x9a], "");
        assert_feed_err!(d, -2, [], [], [0x32, 0x9a, 0x36], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_feed_after_finish() {
        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [0xd2, 0xbb], [0xd2], "\u{4e00}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0xd2, 0xbb], [], "\u{4e00}");
        assert_finish_ok!(d, "");

        let mut d = GB18030_ENCODING.raw_decoder();
        assert_feed_ok!(d, [0x98, 0x35, 0xee, 0x37], [0x98, 0x35, 0xee], "\u{2a6a5}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0x98, 0x35, 0xee, 0x37], [0x98, 0x35], "\u{2a6a5}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0x98, 0x35, 0xee, 0x37], [0x98], "\u{2a6a5}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0x98, 0x35, 0xee, 0x37], [], "\u{2a6a5}");
        assert_finish_ok!(d, "");
    }

    #[bench]
    fn bench_encode_short_text(bencher: &mut test::Bencher) {
        let s = testutils::SIMPLIFIED_CHINESE_TEXT;
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            GB18030_ENCODING.encode(&s, EncoderTrap::Strict)
        }))
    }

    #[bench]
    fn bench_decode_short_text(bencher: &mut test::Bencher) {
        let s = GB18030_ENCODING.encode(testutils::SIMPLIFIED_CHINESE_TEXT,
                                       EncoderTrap::Strict).ok().unwrap();
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            GB18030_ENCODING.decode(&s, DecoderTrap::Strict)
        }))
    }
}

#[cfg(test)]
mod gbk_tests {
    extern crate test;
    use super::GBK_ENCODING;
    use testutils;
    use types::*;

    // GBK and GB 18030 share the same decoder logic.

    #[test]
    fn test_encoder() {
        let mut e = GBK_ENCODING.raw_encoder();
        assert_feed_ok!(e, "A", "", [0x41]);
        assert_feed_ok!(e, "BC", "", [0x42, 0x43]);
        assert_feed_ok!(e, "", "", []);
        assert_feed_ok!(e, "\u{4e2d}\u{534e}\u{4eba}\u{6c11}\u{5171}\u{548c}\u{56fd}", "",
                        [0xd6, 0xd0, 0xbb, 0xaa, 0xc8, 0xcb, 0xc3, 0xf1,
                         0xb9, 0xb2, 0xba, 0xcd, 0xb9, 0xfa]);
        assert_feed_ok!(e, "1\u{20ac}/m", "", [0x31, 0x80, 0x2f, 0x6d]);
        assert_feed_ok!(e, "\u{ff21}\u{ff22}\u{ff23}", "", [0xa3, 0xc1, 0xa3, 0xc2, 0xa3, 0xc3]);
        assert_feed_err!(e, "", "\u{80}", "", []);
        assert_feed_err!(e, "", "\u{81}", "", []);
        assert_feed_err!(e, "", "\u{a3}", "", []);
        assert_feed_ok!(e, "\u{a4}", "", [0xa1, 0xe8]);
        assert_feed_err!(e, "", "\u{a5}", "", []);
        assert_feed_err!(e, "", "\u{10ffff}", "", []);
        assert_feed_err!(e, "", "\u{2a6a5}", "\u{3007}", []);
        assert_feed_err!(e, "\u{3007}", "\u{2a6a5}", "", [0xa9, 0x96]);
        assert_finish_ok!(e, []);
    }

    #[bench]
    fn bench_encode_short_text(bencher: &mut test::Bencher) {
        let s = testutils::SIMPLIFIED_CHINESE_TEXT;
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            GBK_ENCODING.encode(&s, EncoderTrap::Strict)
        }))
    }
}

/**
 * HZ. (RFC 1843)
 *
 * This is a simplified Chinese encoding based on GB 2312.
 * It bears a resemblance to ISO 2022 encodings in such that the printable escape sequences `~{`
 * and `~}` are used to delimit a sequence of 7-bit-safe GB 2312 sequences. For the comparison,
 * they are equivalent to ISO-2022-CN escape sequences `ESC $ ) A` and `ESC ( B`.
 * Additional escape sequences `~~` (for a literal `~`) and `~\n` (ignored) are also supported.
 */
#[derive(Clone, Copy)]
pub struct HZEncoding;

impl Encoding for HZEncoding {
    fn name(&self) -> &'static str { "hz" }
    fn whatwg_name(&self) -> Option<&'static str> { None }
    fn raw_encoder(&self) -> Box<RawEncoder> { HZEncoder::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { HZDecoder::new() }
}

/// An encoder for HZ.
#[derive(Clone, Copy)]
pub struct HZEncoder {
    escaped: bool,
}

impl HZEncoder {
    pub fn new() -> Box<RawEncoder> { Box::new(HZEncoder { escaped: false }) }
}

impl RawEncoder for HZEncoder {
    fn from_self(&self) -> Box<RawEncoder> { HZEncoder::new() }
    fn is_ascii_compatible(&self) -> bool { false }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len());

        let mut escaped = self.escaped;
        macro_rules! ensure_escaped(
            () => (if !escaped { output.write_bytes(b"~{"); escaped = true; })
        );
        macro_rules! ensure_unescaped(
            () => (if escaped { output.write_bytes(b"~}"); escaped = false; })
        );

        for ((i,j), ch) in input.index_iter() {
            if ch < '\u{80}' {
                ensure_unescaped!();
                output.write_byte(ch as u8);
                if ch == '~' { output.write_byte('~' as u8); }
            } else {
                let ptr = index::gb18030::backward(ch as u32);
                if ptr == 0xffff {
                    self.escaped = escaped; // do NOT reset the state!
                    return (i, Some(CodecError {
                        upto: j as isize, cause: "unrepresentable character".into()
                    }));
                } else {
                    let lead = ptr / 190;
                    let trail = ptr % 190;
                    if lead < 0x21 - 1 || trail < 0x21 + 0x3f { // GBK extension, ignored
                        self.escaped = escaped; // do NOT reset the state!
                        return (i, Some(CodecError {
                            upto: j as isize, cause: "unrepresentable character".into()
                        }));
                    } else {
                        ensure_escaped!();
                        output.write_byte((lead + 1) as u8);
                        output.write_byte((trail - 0x3f) as u8);
                    }
                }
            }
        }

        self.escaped = escaped;
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder for HZ.
#[derive(Clone, Copy)]
struct HZDecoder {
    st: hz::State,
}

impl HZDecoder {
    pub fn new() -> Box<RawDecoder> {
        Box::new(HZDecoder { st: Default::default() })
    }
}

impl RawDecoder for HZDecoder {
    fn from_self(&self) -> Box<RawDecoder> { HZDecoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        let (st, processed, err) = hz::raw_feed(self.st, input, output, &());
        self.st = st;
        (processed, err)
    }

    fn raw_finish(&mut self, output: &mut StringWriter) -> Option<CodecError> {
        let (st, err) = hz::raw_finish(self.st, output, &());
        self.st = st;
        err
    }
}

stateful_decoder! {
    module hz;

    internal pub fn map_two_bytes(lead: u8, trail: u8) -> u32 {
        use index_simpchinese as index;

        let lead = lead as u16;
        let trail = trail as u16;
        let index = match (lead, trail) {
            (0x20...0x7f, 0x21...0x7e) => (lead - 1) * 190 + (trail + 0x3f),
            _ => 0xffff,
        };
        index::gb18030::forward(index)
    }

initial:
    // hz-gb-2312 flag = unset, hz-gb-2312 lead = 0x00
    state A0(ctx: Context) {
        case 0x7e => A1(ctx);
        case b @ 0x00...0x7f => ctx.emit(b as u32);
        case _ => ctx.err("invalid sequence");
        final => ctx.reset();
    }

checkpoint:
    // hz-gb-2312 flag = set, hz-gb-2312 lead = 0x00
    state B0(ctx: Context) {
        case 0x7e => B1(ctx);
        case b @ 0x20...0x7f => B2(ctx, b);
        case 0x0a => ctx.err("invalid sequence"); // error *and* reset
        case _ => ctx.err("invalid sequence"), B0(ctx);
        final => ctx.reset();
    }

transient:
    // hz-gb-2312 flag = unset, hz-gb-2312 lead = 0x7e
    state A1(ctx: Context) {
        case 0x7b => B0(ctx);
        case 0x7d => A0(ctx);
        case 0x7e => ctx.emit(0x7e), A0(ctx);
        case 0x0a => A0(ctx);
        case _ => ctx.backup_and_err(1, "invalid sequence");
        final => ctx.err("incomplete sequence");
    }

    // hz-gb-2312 flag = set, hz-gb-2312 lead = 0x7e
    state B1(ctx: Context) {
        case 0x7b => B0(ctx);
        case 0x7d => A0(ctx);
        case 0x7e => ctx.emit(0x7e), B0(ctx);
        case 0x0a => A0(ctx);
        case _ => ctx.backup_and_err(1, "invalid sequence"), B0(ctx);
        final => ctx.err("incomplete sequence");
    }

    // hz-gb-2312 flag = set, hz-gb-2312 lead != 0 & != 0x7e
    state B2(ctx: Context, lead: u8) {
        case 0x0a => ctx.err("invalid sequence"); // should reset the state!
        case b =>
            match map_two_bytes(lead, b) {
                0xffff => ctx.err("invalid sequence"),
                ch => ctx.emit(ch)
            },
            B0(ctx);
        final => ctx.err("incomplete sequence");
    }
}

#[cfg(test)]
mod hz_tests {
    extern crate test;
    use super::HZEncoding;
    use testutils;
    use types::*;

    #[test]
    fn test_encoder_valid() {
        let mut e = HZEncoding.raw_encoder();
        assert_feed_ok!(e, "A", "", *b"A");
        assert_feed_ok!(e, "BC", "", *b"BC");
        assert_feed_ok!(e, "", "", *b"");
        assert_feed_ok!(e, "\u{4e2d}\u{534e}\u{4eba}\u{6c11}\u{5171}\u{548c}\u{56fd}", "",
                        *b"~{VP;*HKCq92:M9z");
        assert_feed_ok!(e, "\u{ff21}\u{ff22}\u{ff23}", "", *b"#A#B#C");
        assert_feed_ok!(e, "1\u{20ac}/m", "", *b"~}1~{\"c~}/m");
        assert_feed_ok!(e, "~<\u{a4}~\u{0a4}>~", "", *b"~~<~{!h~}~~~{!h~}>~~");
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_encoder_invalid() {
        let mut e = HZEncoding.raw_encoder();
        assert_feed_err!(e, "", "\u{ffff}", "", []);
        assert_feed_err!(e, "?", "\u{ffff}", "!", [0x3f]);
        // no support for GBK extension
        assert_feed_err!(e, "", "\u{3007}", "", []);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder_valid() {
        let mut d = HZEncoding.raw_decoder();
        assert_feed_ok!(d, *b"A", *b"", "A");
        assert_feed_ok!(d, *b"BC", *b"", "BC");
        assert_feed_ok!(d, *b"D~~E", *b"~", "D~E");
        assert_feed_ok!(d, *b"~F~\nG", *b"~", "~FG");
        assert_feed_ok!(d, *b"", *b"", "");
        assert_feed_ok!(d, *b"\nH", *b"~", "H");
        assert_feed_ok!(d, *b"{VP~}~{;*~{HKCq92:M9z", *b"",
                        "\u{4e2d}\u{534e}\u{4eba}\u{6c11}\u{5171}\u{548c}\u{56fd}");
        assert_feed_ok!(d, *b"", *b"#", "");
        assert_feed_ok!(d, *b"A", *b"~", "\u{ff21}");
        assert_feed_ok!(d, *b"~#B~~#C", *b"~", "~\u{ff22}~\u{ff23}");
        assert_feed_ok!(d, *b"", *b"", "");
        assert_feed_ok!(d, *b"\n#D~{#E~\n#F~{#G", *b"~", "#D\u{ff25}#F\u{ff27}");
        assert_feed_ok!(d, *b"}X~}YZ", *b"", "XYZ");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_out_or_range() {
        let mut d = HZEncoding.raw_decoder();
        assert_feed_ok!(d, *b"~{", *b"", "");
        assert_feed_err!(d, *b"", *b"\x20\x20", *b"", "");
        assert_feed_err!(d, *b"", *b"\x20\x7f", *b"", ""); // do not reset the state (except for CR)
        assert_feed_err!(d, *b"", *b"\x21\x7f", *b"", "");
        assert_feed_err!(d, *b"", *b"\x7f\x20", *b"", "");
        assert_feed_err!(d, *b"", *b"\x7f\x21", *b"", "");
        assert_feed_err!(d, *b"", *b"\x7f\x7f", *b"", "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_carriage_return() {
        // CR in the multibyte mode is invalid but *also* resets the state
        let mut d = HZEncoding.raw_decoder();
        assert_feed_ok!(d, *b"~{#A", *b"", "\u{ff21}");
        assert_feed_err!(d, *b"", *b"\n", *b"", "");
        assert_feed_ok!(d, *b"#B~{#C", *b"", "#B\u{ff23}");
        assert_feed_err!(d, *b"", *b"#\n", *b"", "");
        assert_feed_ok!(d, *b"#D", *b"", "#D");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_invalid_partial() {
        let mut d = HZEncoding.raw_decoder();
        assert_feed_ok!(d, *b"", *b"~", "");
        assert_finish_err!(d, "");

        let mut d = HZEncoding.raw_decoder();
        assert_feed_ok!(d, *b"~{", *b"#", "");
        assert_finish_err!(d, "");

        let mut d = HZEncoding.raw_decoder();
        assert_feed_ok!(d, *b"~{#A", *b"~", "\u{ff21}");
        assert_finish_err!(d, "");
    }

    #[test]
    fn test_decoder_invalid_escape() {
        let mut d = HZEncoding.raw_decoder();
        assert_feed_ok!(d, *b"#A", *b"", "#A");
        assert_feed_err!(d, *b"", *b"~", *b"xy", "");
        assert_feed_ok!(d, *b"#B", *b"", "#B");
        assert_feed_ok!(d, *b"", *b"~", "");
        assert_feed_err!(d, *b"", *b"", *b"xy", "");
        assert_feed_ok!(d, *b"#C~{#D", *b"", "#C\u{ff24}");
        assert_feed_err!(d, *b"", *b"~", *b"xy", "");
        assert_feed_ok!(d, *b"#E", *b"", "\u{ff25}"); // does not reset to ASCII
        assert_feed_ok!(d, *b"", *b"~", "");
        assert_feed_err!(d, *b"", *b"", *b"xy", "");
        assert_feed_ok!(d, *b"#F~}#G", *b"", "\u{ff26}#G");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_decoder_feed_after_finish() {
        let mut d = HZEncoding.raw_decoder();
        assert_feed_ok!(d, *b"R;~{R;", *b"R", "R;\u{4e00}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, *b"R;~{R;", *b"", "R;\u{4e00}");
        assert_finish_ok!(d, "");
    }

    #[bench]
    fn bench_encode_short_text(bencher: &mut test::Bencher) {
        let s = testutils::SIMPLIFIED_CHINESE_TEXT;
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            HZEncoding.encode(&s, EncoderTrap::Strict)
        }))
    }

    #[bench]
    fn bench_decode_short_text(bencher: &mut test::Bencher) {
        let s = HZEncoding.encode(testutils::SIMPLIFIED_CHINESE_TEXT,
                                  EncoderTrap::Strict).ok().unwrap();
        bencher.bytes = s.len() as u64;
        bencher.iter(|| test::black_box({
            HZEncoding.decode(&s, DecoderTrap::Strict)
        }))
    }
}

