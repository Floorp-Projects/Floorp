/// Regression tests from American Fuzzy Lop test cases.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/// These all caused panics at some point during development.

extern crate mp4parse;

use std::io::Cursor;

/// https://github.com/mozilla/mp4parse-rust/issues/2
///
/// Test a box with 4-byte size, smaller than the smallest header.
#[test]
fn fuzz_2() {
    let mut c = Cursor::new(b"\x00\x00\x00\x04\xa6\x00\x04\xa6".to_vec());
    let mut context = mp4parse::MediaContext::new();
    let _ = mp4parse::read_mp4(&mut c, &mut context);
}

/// https://github.com/mozilla/mp4parse-rust/issues/4
///
/// Test a large (64 bit) box header with zero declared size.
#[test]
fn fuzz_4() {
    let mut c = Cursor::new(b"\x00\x00\x00\x01\x30\x30\x30\x30\x00\x00\x00\x00\x00\x00\x00\x00".to_vec());
    let mut context = mp4parse::MediaContext::new();
    let _ = mp4parse::read_mp4(&mut c, &mut context);
}

/// https://github.com/mozilla/mp4parse-rust/issues/5
///
/// Declares 202116104 compatible brands but does not supply them,
/// verifying read is properly bounded at the end of the stream.
#[test]
fn fuzz_5() {
    let mut c = Cursor::new(b"\x30\x30\x30\x30\x66\x74\x79\x70\x30\x30\x30\x30\x30\x30\x30\x30".to_vec());
    let mut context = mp4parse::MediaContext::new();
    let _ = mp4parse::read_mp4(&mut c, &mut context);
}

/// https://github.com/mozilla/mp4parse-rust/issues/6
///
/// Declares an ftyp box with a single invalid (short - 3 byte) compatible
/// brand and excludes the extra 3 bytes from the stream.
#[test]
fn fuzz_6() {
    let mut c = Cursor::new(b"\x00\x00\x00\x13\x66\x74\x79\x70\x30\x30\x30\x30\x30\x30\x30\x30".to_vec());
    let mut context = mp4parse::MediaContext::new();
    let _ = mp4parse::read_mp4(&mut c, &mut context);
}
