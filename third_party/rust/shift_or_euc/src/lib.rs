// Copyright 2018 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![doc(html_root_url = "https://docs.rs/shift_or_euc/0.1.0")]

//! A Japanese legacy encoding detector for detecting between Shift_JIS,
//! EUC-JP, and, optionally, ISO-2022-JP _given_ the assumption that the
//! encoding is one of those.
//!
//! This detector is generally more accurate (but see below about the failure
//! mode on half-width katakana) and decides much sooner than machine
//! learning-based detectors. To decide EUC-JP, machine learning-based
//! detectors try to gain confidence that the input looks like EUC-JP. To
//! decide EUC-JP, this detector instead looks for two simple rule-based
//! signs of the input not being Shift_JIS.
//!
//! As a consequence of not containing machine learning tables, the binary
//! size footprint that this crate adds on top of
//! [`encoding_rs`](https://docs.rs/crate/encoding_rs) is tiny.
//!
//! # Licensing
//!
//! See the file named [COPYRIGHT](https://github.com/hsivonen/shift_or_euc/blob/master/COPYRIGHT).
//!
//! # Principle of Operation
//!
//! The detector is based on two observations:
//!
//! 1. The ISO-2022-JP escape sequences don't normally occur in Shift_JIS or
//! EUC-JP, so encountering such an escape sequence (before non-ASCII has been
//! encountered) can be taken as indication of ISO-2022-JP.
//! 2. When normal (full-with) kana or common kanji encoded as Shift_JIS is
//! decoded as EUC-JP, or vice versa, the result is either an error or
//! half-width katakana, and it's very uncommon for Japanese HTML to have
//! half-width katakana character before a normal kana or common kanji
//! character. Therefore, if decoding as Shift_JIS results in error or
//! have-width katakana, the detector decides that the content is EUC-JP, and
//! vice versa.
//!
//! # Failure Modes
//!
//! The detector gives the wrong answer if the text has a half-width katakana
//! character before normal kana or common kanji. Some uncommon kanji are
//! undecidable. (All JIS X 0208 Level 1 kanji are decidable.)
//!
//! The half-width katakana issue is mainly relevant for old 8-bit JIS X
//! 0201-only text files that would decode correctly as Shift_JIS but that the
//!  detector detects as EUC-JP.
//!
//! The undecidable kanji issue does not realistically show up when a full
//! document is fed to the detector, because, realistically, in a full
//! document, there is at least one kana or common kanji. It can occur,
//! though, if the detector is only run on a prefix of a document and the
//! prefix only contains the title of the document. It is possible for
//! document title to consist entirely of undecidable kanji. (Indeed,
//! Japanese Wikipedia has articles with such titles.) If the detector is
//! undecided, falling back to Shift_JIS is typically the Web oriented better
//! guess.

use encoding_rs::Decoder;
use encoding_rs::DecoderResult;
use encoding_rs::Encoding;
use encoding_rs::EUC_JP;
use encoding_rs::ISO_2022_JP;
use encoding_rs::SHIFT_JIS;

/// Returns the index of the first non-ASCII byte or the first
/// 0x1B, whichever comes first, or the length of the buffer
/// if neither is found.
fn find_non_ascii_or_escape(buffer: &[u8]) -> usize {
    let ascii_up_to = Encoding::ascii_valid_up_to(buffer);
    if let Some(escape) = memchr::memchr(0x1B, &buffer[..ascii_up_to]) {
        escape
    } else {
        ascii_up_to
    }
}

/// Feed decoder with one byte (if `last` is `false`) or EOF (if `last` is
/// `true`). `byte` is ignored if `last` is `true`.
/// Returns `true` if there was no rejection or `false` upon rejecting the
/// encoding hypothesis represented by this decoder.
#[inline(always)]
fn feed_decoder(decoder: &mut Decoder, byte: u8, last: bool) -> bool {
    let mut output = [0u16; 1];
    let input = [byte];
    let (result, _read, written) = decoder.decode_to_utf16_without_replacement(
        if last { b"" } else { &input },
        &mut output,
        last,
    );
    match result {
        DecoderResult::InputEmpty => {
            if written == 1 {
                match output[0] {
                    0xFF61...0xFF9F => {
                        return false;
                    }
                    _ => {}
                }
            }
        }
        DecoderResult::Malformed(_, _) => {
            return false;
        }
        DecoderResult::OutputFull => {
            unreachable!();
        }
    }
    true
}

/// A detector for detecting the character encoding of input on the
/// precondition that the encoding is a Japanese legacy encoding.
pub struct Detector {
    shift_jis_decoder: Decoder,
    euc_jp_decoder: Decoder,
    second_byte_in_escape: u8,
    iso_2022_jp_disqualified: bool,
    escape_seen: bool,
    finished: bool,
}

impl Detector {
    /// Instantiates the detector. If `allow_2022` is `true` the possible
    /// guesses are Shift_JIS, EUC-JP, ISO-2022-JP, and undecided. If
    /// `allow_2022` is `false`, the possible guesses are Shift_JIS, EUC-JP,
    /// and undecided.
    pub fn new(allow_2022: bool) -> Self {
        Detector {
            shift_jis_decoder: SHIFT_JIS.new_decoder_without_bom_handling(),
            euc_jp_decoder: EUC_JP.new_decoder_without_bom_handling(),
            second_byte_in_escape: 0,
            iso_2022_jp_disqualified: !allow_2022,
            escape_seen: false,
            finished: false,
        }
    }

    /// Feeds bytes to the detector. If `last` is `true` the end of the stream
    /// is considered to occur immediately after the end of `buffer`.
    /// Otherwise, the stream is expected to continue. `buffer` may be empty.
    ///
    /// If you're running the detector only on a prefix of a complete
    /// document, _do not_ pass `last` as `true` after the prefix if the
    /// stream as a whole still contains more content.
    ///
    /// Returns `Some(encoding_rs::SHIFT_JIS)` if the detector guessed
    /// Shift_JIS. Returns `Some(encoding_rs::EUC_JP)` if the detector
    /// guessed EUC-JP. Returns `Some(encoding_rs::ISO_2022_JP)` if the
    /// detector guessed ISO-2022-JP (only possible if `true` was passed as
    /// `allow_2022` when instantiating the detector). Returns `None` if the
    /// detector is undecided. If `None` is returned even when passing `true`
    /// as `last`, falling back to Shift_JIS is the best guess for Web
    /// purposes.
    ///
    /// Do not call again after the method has returned `Some(_)` or after
    /// the method has been called with `true` as `last`.
    ///
    /// # Panics
    ///
    /// If called after the method has returned `Some(_)` or after the method
    /// has been called with `true` as `last`.
    pub fn feed(&mut self, buffer: &[u8], last: bool) -> Option<&'static Encoding> {
        assert!(
            !self.finished,
            "Tried to used a detector that has finished."
        );
        self.finished = true; // Will change back to false unless we return early
        let mut i = 0;
        if !self.iso_2022_jp_disqualified {
            if !self.escape_seen {
                i = find_non_ascii_or_escape(buffer);
            }
            while i < buffer.len() {
                let byte = buffer[i];
                if byte > 0x7F {
                    self.iso_2022_jp_disqualified = true;
                    break;
                }
                if !self.escape_seen && byte == 0x1B {
                    self.escape_seen = true;
                    i += 1;
                    continue;
                }
                if self.escape_seen && self.second_byte_in_escape == 0 {
                    self.second_byte_in_escape = byte;
                    i += 1;
                    continue;
                }
                match (self.second_byte_in_escape, byte) {
                    (0x28, 0x42) | (0x28, 0x4A) | (0x28, 0x49) | (0x24, 0x40) | (0x24, 0x42) => {
                        return Some(ISO_2022_JP);
                    }
                    _ => {}
                }
                if self.escape_seen {
                    self.iso_2022_jp_disqualified = true;
                    break;
                }
                i += 1;
            }
        }
        for &byte in &buffer[i..] {
            if !feed_decoder(&mut self.euc_jp_decoder, byte, false) {
                return Some(SHIFT_JIS);
            }
            if !feed_decoder(&mut self.shift_jis_decoder, byte, false) {
                return Some(EUC_JP);
            }
        }
        if last {
            if !feed_decoder(&mut self.euc_jp_decoder, 0, true) {
                return Some(SHIFT_JIS);
            }
            if !feed_decoder(&mut self.shift_jis_decoder, 0, true) {
                return Some(EUC_JP);
            }
            return None;
        }
        self.finished = false;
        None
    }
}

// Any copyright to the test code below this comment is dedicated to the
// Public Domain. http://creativecommons.org/publicdomain/zero/1.0/

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_iso_2022_jp() {
        let mut detector = Detector::new(true);
        assert_eq!(
            detector.feed(b"abc\x1B\x28\x42\xFF", true),
            Some(ISO_2022_JP)
        );
    }

    #[test]
    fn test_error_precedence() {
        let mut detector = Detector::new(true);
        assert_eq!(detector.feed(b"abc\xFF", true), Some(SHIFT_JIS));
    }

    #[test]
    fn test_invalid_euc_jp() {
        let mut detector = Detector::new(true);
        assert_eq!(detector.feed(b"abc\x81\x40", true), Some(SHIFT_JIS));
    }

    #[test]
    fn test_invalid_shift_jis() {
        let mut detector = Detector::new(true);
        assert_eq!(detector.feed(b"abc\xEB\xA8", true), Some(EUC_JP));
    }

    #[test]
    fn test_invalid_shift_jis_before_invalid_euc_jp() {
        let mut detector = Detector::new(true);
        assert_eq!(detector.feed(b"abc\xEB\xA8\x81\x40", true), Some(EUC_JP));
    }

    #[test]
    fn test_undecided() {
        let mut detector = Detector::new(true);
        assert_eq!(detector.feed(b"abc", false), None);
        assert_eq!(detector.feed(b"abc", false), None);
    }

}
