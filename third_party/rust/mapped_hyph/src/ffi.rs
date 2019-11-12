// Copyright 2019 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::slice;
use std::str;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::str::Utf8Error;

use memmap::Mmap;

use super::Hyphenator;

/// Opaque type representing a hyphenation dictionary loaded from a file,
/// for use in FFI function signatures.
pub struct HyphDic;

// Helper to convert word and hyphen buffer parameters from raw C pointer/length
// pairs to the Rust types expected by mapped_hyph.
unsafe fn params_from_c<'a>(word: *const c_char, word_len: u32,
                            hyphens: *mut u8, hyphens_len: u32) ->
        (Result<&'a str, Utf8Error>, &'a mut [u8]) {
    (str::from_utf8(slice::from_raw_parts(word as *const u8, word_len as usize)),
     slice::from_raw_parts_mut(hyphens, hyphens_len as usize))
}

/// C-callable function to load a hyphenation dictionary from a file at `path`.
///
/// Returns null on failure.
///
/// This does not fully validate that the file contains usable hyphenation
/// data, it only opens the file (read-only) and mmap's it into memory, and
/// does some minimal sanity-checking that it *might* be valid.
///
/// The returned `HyphDic` must be released with `mapped_hyph_free_dictionary`.
///
/// # Safety
/// The given `path` must be a valid pointer to a NUL-terminated (C-style)
/// string.
#[no_mangle]
pub unsafe extern "C" fn mapped_hyph_load_dictionary(path: *const c_char) -> *const HyphDic {
    let path_str = match CStr::from_ptr(path).to_str() {
        Ok(str) => str,
        Err(_) => return std::ptr::null(),
    };
    let hyph = Box::new(match super::load_file(path_str) {
        Some(dic) => dic,
        _ => return std::ptr::null(),
    });
    Box::into_raw(hyph) as *const HyphDic
}

/// C-callable function to free a hyphenation dictionary
/// that was loaded by `mapped_hyph_load_dictionary`.
///
/// # Safety
/// The `dic` parameter must be a `HyphDic` pointer obtained from
/// `mapped_hyph_load_dictionary`, and not previously freed.
#[no_mangle]
pub unsafe extern "C" fn mapped_hyph_free_dictionary(dic: *mut HyphDic) {
    Box::from_raw(dic);
}

/// C-callable function to find hyphenation values for a given `word`,
/// using a dictionary loaded via `mapped_hyph_load_dictionary`.
///
/// The `word` must be UTF-8-encoded, and is `word_len` bytes (not characters)
/// long.
///
/// Caller must supply the `hyphens` output buffer for results; its size is
/// given in `hyphens_len`.
/// It should be at least `word_len` elements long.
///
/// Returns -1 if `word` is not valid UTF-8, or the output `hyphens` buffer is
/// too small.
/// Otherwise returns the number of potential hyphenation positions found.
///
/// # Panics
/// This function may panic if the given dictionary is not valid.
///
/// # Safety
/// The `dic` parameter must be a `HyphDic` pointer obtained from
/// `mapped_hyph_load_dictionary`.
///
/// The `word` and `hyphens` parameter must be valid pointers to memory buffers
/// of at least the respective sizes `word_len` and `hyphens_len`.
#[no_mangle]
pub unsafe extern "C" fn mapped_hyph_find_hyphen_values_dic(dic: *const HyphDic,
                                                            word: *const c_char, word_len: u32,
                                                            hyphens: *mut u8, hyphens_len: u32) -> i32 {
    if word_len > hyphens_len {
        return -1;
    }
    let (word_str, hyphen_buf) = params_from_c(word, word_len, hyphens, hyphens_len);
    if word_str.is_err() {
        return -1;
    }
    Hyphenator::new(&*(dic as *const Mmap))
        .find_hyphen_values(word_str.unwrap(), hyphen_buf) as i32
}

/// C-callable function to find hyphenation values for a given `word`,
/// using a dictionary loaded and owned by the caller.
///
/// The dictionary is supplied as a raw memory buffer `dic_buf` of size
/// `dic_len`.
///
/// The `word` must be UTF-8-encoded, and is `word_len` bytes (not characters)
/// long.
///
/// Caller must supply the `hyphens` output buffer for results; its size is
/// given in `hyphens_len`.
/// It should be at least `word_len` elements long.
///
/// Returns -1 if `word` is not valid UTF-8, or the output `hyphens` buffer is
/// too small.
/// Otherwise returns the number of potential hyphenation positions found.
///
/// # Panics
/// This function may panic if the given dictionary is not valid.
///
/// # Safety
/// The `dic_buf` parameter must be a valid pointer to a memory block of size
/// at least `dic_len`.
///
/// The `word` and `hyphens` parameter must be valid pointers to memory buffers
/// of at least the respective sizes `word_len` and `hyphens_len`.
#[no_mangle]
pub unsafe extern "C" fn mapped_hyph_find_hyphen_values_raw(dic_buf: *const u8, dic_len: u32,
                                                            word: *const c_char, word_len: u32,
                                                            hyphens: *mut u8, hyphens_len: u32) -> i32 {
    if word_len > hyphens_len {
        return -1;
    }
    let (word_str, hyphen_buf) = params_from_c(word, word_len, hyphens, hyphens_len);
    if word_str.is_err() {
        return -1;
    }
    Hyphenator::new(slice::from_raw_parts(dic_buf, dic_len as usize))
        .find_hyphen_values(word_str.unwrap(), hyphen_buf) as i32
}

/// C-callable function to check if a given memory buffer `dic_buf` of size
/// `dic_len` is potentially usable as a hyphenation dictionary.
///
/// Returns `true` if the given memory buffer looks like it may be a valid
/// hyphenation dictionary, `false` if it is clearly not usable.
///
/// # Safety
/// The `dic_buf` parameter must be a valid pointer to a memory block of size
/// at least `dic_len`.
#[no_mangle]
pub unsafe extern "C" fn mapped_hyph_is_valid_hyphenator(dic_buf: *const u8, dic_len: u32) -> bool {
    if dic_buf.is_null() {
        return false;
    }
    let dic = Hyphenator::new(slice::from_raw_parts(dic_buf, dic_len as usize));
    dic.is_valid_hyphenator()
}
