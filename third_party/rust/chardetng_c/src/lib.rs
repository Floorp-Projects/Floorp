// Copyright Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![doc(html_root_url = "https://docs.rs/chardetng_c/0.1.0")]

//! C API for [`chardetng`](https://docs.rs/chardetng/)
//!
//! # Panics
//!
//! This crate is designed to be used only in a `panic=abort` scenario.
//! Panic propagation across FFI is not handled!
//!
//! # Licensing
//!
//! See the file named [COPYRIGHT](https://github.com/hsivonen/chardetng_c/blob/master/COPYRIGHT).

use encoding_rs::Encoding;
use chardetng::EncodingDetector;

/// Instantiates a Web browser-oriented detector for guessing what
/// character encoding a stream of bytes is encoded in.
///
/// The bytes are fed to the detector incrementally using the
/// `chardetng_encoding_detector_free` function. The current guess of the
/// detector can be queried using the `chardetng_encoding_detector_guess`
/// function. The guessing parameters are arguments to the
/// `chardetng_encoding_detector_guess` function rather than arguments to the
/// constructor in order to enable the application to check if the arguments
/// affect the guessing outcome. (The specific use case is to disable UI for
/// re-running the detector with UTF-8 allowed and the top-level domain name
/// ignored if those arguments don't change the guess.)
///
/// The instantiated detector must be freed after use using
/// `chardetng_detectordetector_free`.
#[no_mangle]
pub unsafe extern "C" fn chardetng_encoding_detector_new() -> *mut EncodingDetector {
    Box::into_raw(Box::new(EncodingDetector::new()))
}

/// Deallocates a detector obtained from `chardetng_encodingdetector_new`.
#[no_mangle]
pub unsafe extern "C" fn chardetng_encoding_detector_free(detector: *mut EncodingDetector) {
    let _ = Box::from_raw(detector);
}

/// Queries whether the TLD is considered non-generic and could affect the guess.
///
/// # Undefined Behavior
///
/// UB ensues if
///
/// * `tld` is non-NULL and `tld_len` is non-zero but `tld` and `tld_len`
///   don't designate a range of memory valid for reading.
#[no_mangle]
pub unsafe extern "C" fn chardetng_encoding_detector_tld_may_affect_guess(
    tld: *const u8,
    tld_len: usize,
) -> bool {
    let tld_opt = if tld.is_null() {
        assert_eq!(tld_len, 0);
        None
    } else {
        Some(::std::slice::from_raw_parts(tld, tld_len))
    };
    EncodingDetector::tld_may_affect_guess(tld_opt)
}

/// Inform the detector of a chunk of input.
///
/// The byte stream is represented as a sequence of calls to this
/// function such that the concatenation of the arguments to this
/// function form the byte stream. It does not matter how the application
/// chooses to chunk the stream. It is OK to call this function with
/// a zero-length byte slice.
///
/// The end of the stream is indicated by calling this function with
/// `last` set to `true`. In that case, the end of the stream is
/// considered to occur after the last byte of the `buffer` (which
/// may be zero-length) passed in the same call. Once this function
/// has been called with `last` set to `true` this function must not
/// be called again.
///
/// If you want to perform detection on just the prefix of a longer
/// stream, do not pass `last=true` after the prefix if the stream
/// actually still continues.
///
/// Returns `true` if after processing `buffer` the stream has
/// contained at least one non-ASCII byte and `false` if only
/// ASCII has been seen so far.
///
/// # Panics
///
/// If this function has previously been called with `last` set to `true`.
///
/// # Undefined Behavior
///
/// UB ensues if
///
/// * `detector` does not point to a detector obtained from
///   `chardetng_detector_new` but not yet freed with
///   `chardetng_detector_free`.
/// * `buffer` is `NULL`. (It can be a bogus pointer when `buffer_len` is 0.)
/// * ,buffer_len` is non-zero and `buffer` and `buffer_len` don't designate
///    a range of memory valid for reading.
#[no_mangle]
pub unsafe extern "C" fn chardetng_encoding_detector_feed(
    detector: *mut EncodingDetector,
    buffer: *const u8,
    buffer_len: usize,
    last: bool,
) -> bool {
    (*detector).feed(::std::slice::from_raw_parts(buffer, buffer_len), last)
}

/// Guess the encoding given the bytes pushed to the detector so far
/// (via `chardetng_encoding_detector_feed()`), the top-level domain name 
/// from which the bytes were loaded, and an indication of whether to
/// consider UTF-8 as a permissible guess.
///
/// The `tld` argument takes the rightmost DNS label of the hostname of the
/// host the stream was loaded from in lower-case ASCII form. That is, if
/// the label is an internationalized top-level domain name, it must be
/// provided in its Punycode form. If the TLD that the stream was loaded
/// from is unavalable, `NULL` may be passed instead (and 0 as `tld_len`),
/// which is equivalent to passing pointer to "com" as `tld` and 3 as 
/// `tld_len`.
///
/// If the `allow_utf8` argument is set to `false`, the return value of
/// this function won't be `UTF_8_ENCODING`. When performing detection
/// on `text/html` on non-`file:` URLs, Web browsers must pass `false`,
/// unless the user has taken a specific contextual action to request an
/// override. This way, Web developers cannot start depending on UTF-8
/// detection. Such reliance would make the Web Platform more brittle.
///
/// Returns the guessed encoding (never `NULL`).
///
/// # Panics
///
/// If `tld` is `NULL` but `tld_len` is not zero.
///
/// If `tld` contains non-ASCII, period, or upper-case letters. (The panic
/// condition is intentionally limited to signs of failing to extract the
/// label correctly, failing to provide it in its Punycode form, and failure
/// to lower-case it. Full DNS label validation is intentionally not performed
/// to avoid panics when the reality doesn't match the specs.)
///
/// # Undefined Behavior
///
/// UB ensues if
///
/// * `detector` does not point to a detector obtained from
///   `chardetng_detector_new` but not yet freed with
///   `chardetng_detector_free`.
/// * `tld` is non-NULL and `tld_len` is non-zero but `tld` and `tld_len`
///   don't designate a range of memory valid for reading.
#[no_mangle]
pub unsafe extern "C" fn chardetng_encoding_detector_guess(
    detector: *const EncodingDetector,
    tld: *const u8,
    tld_len: usize,
    allow_utf8: bool,
) -> *const Encoding {
	let tld_opt = if tld.is_null() {
		assert_eq!(tld_len, 0);
		None
	} else {
		Some(::std::slice::from_raw_parts(tld, tld_len))
	};
	(*detector).guess(tld_opt, allow_utf8)
}
