// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! A placeholder encoding that returns encoder/decoder error for every case.

use std::convert::Into;
use types::*;

/// An encoding that returns encoder/decoder error for every case.
#[derive(Clone, Copy)]
pub struct ErrorEncoding;

impl Encoding for ErrorEncoding {
    fn name(&self) -> &'static str { "error" }
    fn raw_encoder(&self) -> Box<RawEncoder> { ErrorEncoder::new() }
    fn raw_decoder(&self) -> Box<RawDecoder> { ErrorDecoder::new() }
}

/// An encoder that always returns error.
#[derive(Clone, Copy)]
pub struct ErrorEncoder;

impl ErrorEncoder {
    pub fn new() -> Box<RawEncoder> { Box::new(ErrorEncoder) }
}

impl RawEncoder for ErrorEncoder {
    fn from_self(&self) -> Box<RawEncoder> { ErrorEncoder::new() }

    fn raw_feed(&mut self, input: &str, _output: &mut ByteWriter) -> (usize, Option<CodecError>) {
        if let Some(ch) = input.chars().next() {
            (0, Some(CodecError { upto: ch.len_utf8() as isize,
                                  cause: "unrepresentable character".into() }))
        } else {
            (0, None)
        }
    }

    fn raw_finish(&mut self, _output: &mut ByteWriter) -> Option<CodecError> {
        None
    }
}

/// A decoder that always returns error.
#[derive(Clone, Copy)]
pub struct ErrorDecoder;

impl ErrorDecoder {
    pub fn new() -> Box<RawDecoder> { Box::new(ErrorDecoder) }
}

impl RawDecoder for ErrorDecoder {
    fn from_self(&self) -> Box<RawDecoder> { ErrorDecoder::new() }

    fn raw_feed(&mut self,
                input: &[u8], _output: &mut StringWriter) -> (usize, Option<CodecError>) {
        if input.len() > 0 {
            (0, Some(CodecError { upto: 1, cause: "invalid sequence".into() }))
        } else {
            (0, None)
        }
    }

    fn raw_finish(&mut self, _output: &mut StringWriter) -> Option<CodecError> {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::ErrorEncoding;
    use types::*;

    #[test]
    fn test_encoder() {
        let mut e = ErrorEncoding.raw_encoder();
        assert_feed_err!(e, "", "A", "", []);
        assert_feed_err!(e, "", "B", "C", []);
        assert_feed_ok!(e, "", "", []);
        assert_feed_err!(e, "", "\u{a0}", "", []);
        assert_finish_ok!(e, []);
    }

    #[test]
    fn test_decoder() {
        let mut d = ErrorEncoding.raw_decoder();
        assert_feed_err!(d, [], [0x41], [], "");
        assert_feed_err!(d, [], [0x42], [0x43], "");
        assert_feed_ok!(d, [], [], "");
        assert_feed_err!(d, [], [0xa0], [], "");
        assert_finish_ok!(d, "");
    }
}
