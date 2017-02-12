// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.
//
// Portions Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! UTF-8, the universal encoding.

use std::{str, mem};
use std::convert::Into;
use types::*;

/**
 * UTF-8 (UCS Transformation Format, 8-bit).
 *
 * This is a Unicode encoding compatible to ASCII (ISO/IEC 646:US)
 * and able to represent all Unicode codepoints uniquely and unambiguously.
 * It has a variable-length design,
 * where one codepoint may use 1 (up to U+007F), 2 (up to U+07FF), 3 (up to U+FFFF)
 * and 4 bytes (up to U+10FFFF) depending on its value.
 * The first byte of the sequence is distinct from other "continuation" bytes of the sequence
 * making UTF-8 self-synchronizable and easy to handle.
 * It has a fixed endianness, and can be lexicographically sorted by codepoints.
 *
 * The UTF-8 scanner used by this module is heavily based on Bjoern Hoehrmann's
 * [Flexible and Economical UTF-8 Decoder](http://bjoern.hoehrmann.de/utf-8/decoder/dfa/).
 */
#[derive(Clone, Copy)]
pub struct UTF8Encoding;

impl Encoding for UTF8Encoding {
    fn name(&self) -> &'static str { "utf-8" }
    fn whatwg_name(&self) -> Option<&'static str> { Some("utf-8") }
    fn raw_encoder(&self) -> Box<RawEncoder> { UTF8Encoder::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { UTF8Decoder::new() }
}

/// An encoder for UTF-8.
#[derive(Clone, Copy)]
pub struct UTF8Encoder;

impl UTF8Encoder {
    pub fn new() -> Box<RawEncoder> { Box::new(UTF8Encoder) }
}

impl RawEncoder for UTF8Encoder {
    fn from_self(&self) -> Box<RawEncoder> { UTF8Encoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &str, output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        let input: &[u8] = input.as_bytes();
        assert!(str::from_utf8(input).is_ok());
        output.write_bytes(input);
        (input.len(), None)
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder for UTF-8.
#[derive(Clone, Copy)]
pub struct UTF8Decoder {
    queuelen: usize,
    queue: [u8; 4],
    state: u8,
}

impl UTF8Decoder {
    pub fn new() -> Box<RawDecoder> {
        Box::new(UTF8Decoder { queuelen: 0, queue: [0; 4], state: INITIAL_STATE })
    }
}

static CHAR_CATEGORY: [u8; 256] = [
    //  0 (00-7F): one byte sequence
    //  1 (80-8F): continuation byte
    //  2 (C2-DF): start of two byte sequence
    //  3 (E1-EC,EE-EF): start of three byte sequence, next byte unrestricted
    //  4 (ED): start of three byte sequence, next byte restricted to non-surrogates (80-9F)
    //  5 (F4): start of four byte sequence, next byte restricted to 0+10FFFF (80-8F)
    //  6 (F1-F3): start of four byte sequence, next byte unrestricted
    //  7 (A0-BF): continuation byte
    //  8 (C0-C1,F5-FF): invalid (overlong or out-of-range) start of multi byte sequences
    //  9 (90-9F): continuation byte
    // 10 (E0): start of three byte sequence, next byte restricted to non-overlong (A0-BF)
    // 11 (F0): start of four byte sequence, next byte restricted to non-overlong (90-BF)

     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
];

static STATE_TRANSITIONS: [u8; 110] = [
     0,98,12,24,48,84,72,98,98,98,36,60,       //  0: '??
    86, 0,86,86,86,86,86, 0,86, 0,86,86,       // 12: .. 'cc
    86,12,86,86,86,86,86,12,86,12,86,86,       // 24: .. 'cc cc
    86,86,86,86,86,86,86,12,86,86,86,86,       // 36: .. 'cc(A0-BF) cc
    86,12,86,86,86,86,86,86,86,12,86,86,       // 48: .. 'cc(80-9F) cc
    86,86,86,86,86,86,86,24,86,24,86,86,       // 60: .. 'cc(90-BF) cc cc
    86,24,86,86,86,86,86,24,86,24,86,86,       // 72: .. 'cc cc cc
    86,24,86,86,86,86,86,86,86,86,86,86,86,86, // 84: .. 'cc(80-8F) cc cc
       // 86,86,86,86,86,86,86,86,86,86,86,86, // 86: .. xx '..
          98,98,98,98,98,98,98,98,98,98,98,98, // 98: xx '..
];

static INITIAL_STATE: u8 = 0;
static ACCEPT_STATE: u8 = 0;
static REJECT_STATE: u8 = 98;
static REJECT_STATE_WITH_BACKUP: u8 = 86;

macro_rules! is_reject_state(($state:expr) => ($state >= REJECT_STATE_WITH_BACKUP));
macro_rules! next_state(($state:expr, $ch:expr) => (
    STATE_TRANSITIONS[($state + CHAR_CATEGORY[$ch as usize]) as usize]
));

impl RawDecoder for UTF8Decoder {
    fn from_self(&self) -> Box<RawDecoder> { UTF8Decoder::new() }
    fn is_ascii_compatible(&self) -> bool { true }

    fn raw_feed(&mut self, input: &[u8], output: &mut StringWriter) -> (usize, Option<CodecError>) {
        output.writer_hint(input.len());

        fn write_bytes(output: &mut StringWriter, bytes: &[u8]) {
            output.write_str(unsafe {mem::transmute(bytes)});
        }

        let mut state = self.state;
        let mut processed = 0;
        let mut offset = 0;

        // optimization: if we are in the initial state, quickly skip to the first non-MSB-set byte.
        if state == INITIAL_STATE {
            let first_msb = input.iter().position(|&ch| ch >= 0x80).unwrap_or(input.len());
            offset += first_msb;
            processed += first_msb;
        }

        for (i, &ch) in input[offset..].iter().enumerate() {
            state = next_state!(state, ch);
            if state == ACCEPT_STATE {
                processed = i + offset + 1;
            } else if is_reject_state!(state) {
                let upto = if state == REJECT_STATE {i + offset + 1} else {i + offset};
                self.state = INITIAL_STATE;
                if processed > 0 && self.queuelen > 0 { // flush `queue` outside the problem
                    write_bytes(output, &self.queue[0..self.queuelen]);
                }
                self.queuelen = 0;
                write_bytes(output, &input[0..processed]);
                return (processed, Some(CodecError {
                    upto: upto as isize, cause: "invalid sequence".into()
                }));
            }
        }

        self.state = state;
        if processed > 0 && self.queuelen > 0 { // flush `queue`
            write_bytes(output, &self.queue[0..self.queuelen]);
            self.queuelen = 0;
        }
        write_bytes(output, &input[0..processed]);
        if processed < input.len() {
            let morequeuelen = input.len() - processed;
            for i in 0..morequeuelen {
                self.queue[self.queuelen + i] = input[processed + i];
            }
            self.queuelen += morequeuelen;
        }
        (processed, None)
    }

    fn raw_finish(&mut self, _output: &mut StringWriter) -> Option<CodecError> {
        let state = self.state;
        let queuelen = self.queuelen;
        self.state = INITIAL_STATE;
        self.queuelen = 0;
        if state != ACCEPT_STATE {
            Some(CodecError { upto: 0, cause: "incomplete sequence".into() })
        } else {
            assert!(queuelen == 0);
            None
        }
    }
}

/// Almost equivalent to `std::str::from_utf8`.
/// This function is provided for the fair benchmark against the stdlib's UTF-8 conversion
/// functions, as rust-encoding always allocates a new string.
pub fn from_utf8<'a>(input: &'a [u8]) -> Option<&'a str> {
    let mut iter = input.iter();
    let mut state;

    macro_rules! return_as_whole(() => (return Some(unsafe {mem::transmute(input)})));

    // optimization: if we are in the initial state, quickly skip to the first non-MSB-set byte.
    loop {
        match iter.next() {
            Some(&ch) if ch < 0x80 => {}
            Some(&ch) => {
                state = next_state!(INITIAL_STATE, ch);
                break;
            }
            None => { return_as_whole!(); }
        }
    }

    for &ch in iter {
        state = next_state!(state, ch);
        if is_reject_state!(state) { return None; }
    }
    if state != ACCEPT_STATE { return None; }
    return_as_whole!();
}

#[cfg(test)]
mod tests {
    // portions of these tests are adopted from Markus Kuhn's UTF-8 decoder capability and
    // stress test: <http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt>.

    use super::{UTF8Encoding, from_utf8};
    use std::str;
    use testutils;
    use types::*;

    #[test]
    fn test_valid() {
        // one byte
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0x41], [], "A");
        assert_feed_ok!(d, [0x42, 0x43], [], "BC");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0x44, 0x45, 0x46], [], "DEF");
        assert_finish_ok!(d, "");

        // two bytes
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xc2, 0xa2], [], "\u{a2}");
        assert_feed_ok!(d, [0xc2, 0xac, 0xc2, 0xa9], [], "\u{ac}\u{0a9}");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0xd5, 0xa1, 0xd5, 0xb5, 0xd5, 0xa2, 0xd5, 0xb8, 0xd6, 0x82,
                            0xd5, 0xa2, 0xd5, 0xa5, 0xd5, 0xb6], [],
                        "\u{561}\u{0575}\u{562}\u{578}\u{582}\u{562}\u{565}\u{576}");
        assert_finish_ok!(d, "");

        // three bytes
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xed, 0x92, 0x89], [], "\u{d489}");
        assert_feed_ok!(d, [0xe6, 0xbc, 0xa2, 0xe5, 0xad, 0x97], [], "\u{6f22}\u{5b57}");
        assert_feed_ok!(d, [], [], "");
        assert_feed_ok!(d, [0xc9, 0x99, 0xc9, 0x94, 0xc9, 0x90], [], "\u{259}\u{0254}\u{250}");
        assert_finish_ok!(d, "");

        // four bytes
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xf0, 0x90, 0x82, 0x82], [], "\u{10082}");
        assert_feed_ok!(d, [], [], "");
        assert_finish_ok!(d, "");

        // we don't test encoders as it is largely a no-op.
    }

    #[test]
    fn test_valid_boundary() {
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0x00], [], "\x00");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0x7f], [], "\x7f");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xc2, 0x80], [], "\u{80}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xdf, 0xbf], [], "\u{7ff}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xe0, 0xa0, 0x80], [], "\u{800}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xed, 0x9f, 0xbf], [], "\u{d7ff}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xee, 0x80, 0x80], [], "\u{e000}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xef, 0xbf, 0xbf], [], "\u{ffff}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xf0, 0x90, 0x80, 0x80], [], "\u{10000}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xf4, 0x8f, 0xbf, 0xbf], [], "\u{10ffff}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_valid_partial() {
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [], [0xf0], "");
        assert_feed_ok!(d, [], [0x90], "");
        assert_feed_ok!(d, [], [0x82], "");
        assert_feed_ok!(d, [0x82], [0xed], "\u{10082}");
        assert_feed_ok!(d, [0x92, 0x89], [], "\u{d489}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [], [0xc2], "");
        assert_feed_ok!(d, [0xa9, 0x20], [], "\u{a9}\u{020}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_invalid_continuation() {
        for c in 0x80..0xc0 {
            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [c], [], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [c], [c], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [c], [c, c], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_invalid_surrogate() {
        // surrogates should fail at the second byte.

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xed], [0xa0, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xed], [0xad, 0xbf], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xed], [0xae, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xed], [0xaf, 0xbf], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xed], [0xb0, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xed], [0xbe, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xed], [0xbf, 0xbf], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_invalid_boundary() {
        // as with surrogates, should fail at the second byte.
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xf4], [0x90, 0x90, 0x90], ""); // U+110000
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_invalid_start_immediate_test_finish() {
        for c in 0xf5..0x100 {
            let c = c as u8;
            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [c], [], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_invalid_start_followed_by_space() {
        for c in 0xf5..0x100 {
            let c = c as u8;

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [c], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [c], [], "");
            assert_feed_ok!(d, [0x20], [], "\x20");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_invalid_lone_start_immediate_test_finish() {
        for c in 0xc2..0xf5 {
            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_ok!(d, [], [c], ""); // wait for cont. bytes
            assert_finish_err!(d, "");
        }
    }

    #[test]
    fn test_invalid_lone_start_followed_by_space() {
        for c in 0xc2..0xf5 {
            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [c], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_ok!(d, [], [c], ""); // wait for cont. bytes
            assert_feed_err!(d, [], [], [0x20], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_invalid_incomplete_three_byte_seq_followed_by_space() {
        for b in 0xe0..0xf5 {
            let c = if b == 0xe0 || b == 0xf0 {0xa0} else {0x80};

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [b, c], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_ok!(d, [], [b, c], ""); // wait for cont. bytes
            assert_feed_err!(d, [], [], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_ok!(d, [], [b], ""); // wait for cont. bytes
            assert_feed_err!(d, [], [c], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_ok!(d, [], [b], ""); // wait for cont. bytes
            assert_feed_ok!(d, [], [c], ""); // wait for cont. bytes
            assert_feed_err!(d, [], [], [0x20], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_invalid_incomplete_four_byte_seq_followed_by_space() {
        for a in 0xf0..0xf5 {
            let b = if a == 0xf0 {0xa0} else {0x80};
            let c = 0x80;

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_err!(d, [], [a, b, c], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_ok!(d, [], [a], ""); // wait for cont. bytes
            assert_feed_ok!(d, [], [b], ""); // wait for cont. bytes
            assert_feed_ok!(d, [], [c], ""); // wait for cont. bytes
            assert_feed_err!(d, [], [], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_ok!(d, [], [a, b], ""); // wait for cont. bytes
            assert_feed_err!(d, [], [c], [0x20], "");
            assert_finish_ok!(d, "");

            let mut d = UTF8Encoding.raw_decoder();
            assert_feed_ok!(d, [], [a, b, c], ""); // wait for cont. bytes
            assert_feed_err!(d, [], [], [0x20], "");
            assert_finish_ok!(d, "");
        }
    }

    #[test]
    fn test_invalid_too_many_cont_bytes() {
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [0xc2, 0x80], [0x80], [], "\u{80}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [0xe0, 0xa0, 0x80], [0x80], [], "\u{800}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [0xf0, 0x90, 0x80, 0x80], [0x80], [], "\u{10000}");
        assert_finish_ok!(d, "");

        // no continuation byte is consumed after 5/6-byte sequence starters and FE/FF
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xf8], [0x88, 0x80, 0x80, 0x80, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xfc], [0x84, 0x80, 0x80, 0x80, 0x80, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xfe], [0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xff], [0x80], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_invalid_too_many_cont_bytes_partial() {
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [], [0xc2], "");
        assert_feed_err!(d, [0x80], [0x80], [], "\u{80}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [], [0xe0, 0xa0], "");
        assert_feed_err!(d, [0x80], [0x80], [], "\u{800}");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [], [0xf0, 0x90, 0x80], "");
        assert_feed_err!(d, [0x80], [0x80], [], "\u{10000}");
        assert_finish_ok!(d, "");

        // no continuation byte is consumed after 5/6-byte sequence starters and FE/FF
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xf8], [], "");
        assert_feed_err!(d, [], [0x88], [0x80, 0x80, 0x80, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xfc], [], "");
        assert_feed_err!(d, [], [0x84], [0x80, 0x80, 0x80, 0x80, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xfe], [], "");
        assert_feed_err!(d, [], [0x80], [], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xff], [], "");
        assert_feed_err!(d, [], [0x80], [], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_invalid_overlong_minimal() {
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xc0], [0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xe0], [0x80, 0x80], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xf0], [0x80, 0x80, 0x80], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_invalid_overlong_maximal() {
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xc1], [0xbf], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xe0], [0x9f, 0xbf], "");
        assert_finish_ok!(d, "");

        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_err!(d, [], [0xf0], [0x8f, 0xbf, 0xbf], "");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_feed_after_finish() {
        let mut d = UTF8Encoding.raw_decoder();
        assert_feed_ok!(d, [0xc2, 0x80], [0xc2], "\u{80}");
        assert_finish_err!(d, "");
        assert_feed_ok!(d, [0xc2, 0x80], [], "\u{80}");
        assert_finish_ok!(d, "");
    }

    #[test]
    fn test_correct_from_utf8() {
        let s = testutils::ASCII_TEXT.as_bytes();
        assert_eq!(from_utf8(s), str::from_utf8(s).ok());

        let s = testutils::KOREAN_TEXT.as_bytes();
        assert_eq!(from_utf8(s), str::from_utf8(s).ok());

        let s = testutils::INVALID_UTF8_TEXT;
        assert_eq!(from_utf8(s), str::from_utf8(s).ok());
    }

    mod bench_ascii {
        extern crate test;
        use super::super::{UTF8Encoding, from_utf8};
        use std::str;
        use testutils;
        use types::*;

        #[bench]
        fn bench_encode(bencher: &mut test::Bencher) {
            let s = testutils::ASCII_TEXT;
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                UTF8Encoding.encode(s, EncoderTrap::Strict)
            }))
        }

        #[bench]
        fn bench_decode(bencher: &mut test::Bencher) {
            let s = testutils::ASCII_TEXT.as_bytes();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                UTF8Encoding.decode(s, DecoderTrap::Strict)
            }))
        }

        #[bench]
        fn bench_from_utf8(bencher: &mut test::Bencher) {
            let s = testutils::ASCII_TEXT.as_bytes();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                from_utf8(s)
            }))
        }

        #[bench] // for the comparison
        fn bench_stdlib_from_utf8(bencher: &mut test::Bencher) {
            let s = testutils::ASCII_TEXT.as_bytes();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                str::from_utf8(s)
            }))
        }

        #[bench] // for the comparison
        fn bench_stdlib_from_utf8_lossy(bencher: &mut test::Bencher) {
            let s = testutils::ASCII_TEXT.as_bytes();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                String::from_utf8_lossy(s)
            }))
        }
    }

    // why Korean? it has an excellent mix of multibyte sequences and ASCII sequences
    // unlike other CJK scripts, so it reflects a practical use case a bit better.
    mod bench_korean {
        extern crate test;
        use super::super::{UTF8Encoding, from_utf8};
        use std::str;
        use testutils;
        use types::*;

        #[bench]
        fn bench_encode(bencher: &mut test::Bencher) {
            let s = testutils::KOREAN_TEXT;
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                UTF8Encoding.encode(s, EncoderTrap::Strict)
            }))
        }

        #[bench]
        fn bench_decode(bencher: &mut test::Bencher) {
            let s = testutils::KOREAN_TEXT.as_bytes();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                UTF8Encoding.decode(s, DecoderTrap::Strict)
            }))
        }

        #[bench]
        fn bench_from_utf8(bencher: &mut test::Bencher) {
            let s = testutils::KOREAN_TEXT.as_bytes();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                from_utf8(s)
            }))
        }

        #[bench] // for the comparison
        fn bench_stdlib_from_utf8(bencher: &mut test::Bencher) {
            let s = testutils::KOREAN_TEXT.as_bytes();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                str::from_utf8(s)
            }))
        }

        #[bench] // for the comparison
        fn bench_stdlib_from_utf8_lossy(bencher: &mut test::Bencher) {
            let s = testutils::KOREAN_TEXT.as_bytes();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                String::from_utf8_lossy(s)
            }))
        }
    }

    mod bench_lossy_invalid {
        extern crate test;
        use super::super::{UTF8Encoding, from_utf8};
        use std::str;
        use testutils;
        use types::*;
        use types::DecoderTrap::Replace as DecodeReplace;

        #[bench]
        fn bench_decode_replace(bencher: &mut test::Bencher) {
            let s = testutils::INVALID_UTF8_TEXT;
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                UTF8Encoding.decode(s, DecodeReplace)
            }))
        }

        #[bench] // for the comparison
        fn bench_from_utf8_failing(bencher: &mut test::Bencher) {
            let s = testutils::INVALID_UTF8_TEXT;
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                from_utf8(s)
            }))
        }

        #[bench] // for the comparison
        fn bench_stdlib_from_utf8_failing(bencher: &mut test::Bencher) {
            let s = testutils::INVALID_UTF8_TEXT;
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                str::from_utf8(s)
            }))
        }

        #[bench] // for the comparison
        fn bench_stdlib_from_utf8_lossy(bencher: &mut test::Bencher) {
            let s = testutils::INVALID_UTF8_TEXT;
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                String::from_utf8_lossy(s)
            }))
        }
    }

    mod bench_lossy_external {
        extern crate test;
        use super::super::{UTF8Encoding, from_utf8};
        use std::str;
        use testutils;
        use types::*;
        use types::DecoderTrap::Replace as DecodeReplace;

        #[bench]
        fn bench_decode_replace(bencher: &mut test::Bencher) {
            let s = testutils::get_external_bench_data();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                UTF8Encoding.decode(&s, DecodeReplace)
            }))
        }

        #[bench] // for the comparison
        fn bench_from_utf8_failing(bencher: &mut test::Bencher) {
            let s = testutils::get_external_bench_data();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                from_utf8(&s)
            }))
        }

        #[bench] // for the comparison
        fn bench_stdlib_from_utf8_failing(bencher: &mut test::Bencher) {
            let s = testutils::get_external_bench_data();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                str::from_utf8(&s)
            }))
        }

        #[bench] // for the comparison
        fn bench_stdlib_from_utf8_lossy(bencher: &mut test::Bencher) {
            let s = testutils::get_external_bench_data();
            bencher.bytes = s.len() as u64;
            bencher.iter(|| test::black_box({
                String::from_utf8_lossy(&s)
            }))
        }
    }
}
