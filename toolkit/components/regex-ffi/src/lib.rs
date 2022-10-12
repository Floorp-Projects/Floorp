/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

pub extern crate regex;
use nsstring::nsACString;
pub use regex::{Regex, RegexBuilder};
use std::ptr;

pub struct RegexWrapper {
    regex: Regex,
}

#[no_mangle]
pub extern "C" fn regex_new(pattern: &nsACString) -> *mut RegexWrapper {
    let pattern = pattern.to_utf8();
    let re = match RegexBuilder::new(&pattern).case_insensitive(true).build() {
        Ok(re) => re,
        Err(_err) => {
            return ptr::null_mut();
        }
    };
    let re = RegexWrapper { regex: re };
    Box::into_raw(Box::new(re))
}

#[no_mangle]
pub unsafe extern "C" fn regex_delete(re: *mut RegexWrapper) {
    drop(Box::from_raw(re));
}

#[no_mangle]
pub extern "C" fn regex_is_match(re: &RegexWrapper, text: &nsACString) -> bool {
    let re = &re.regex;
    re.is_match(&text.to_utf8())
}

#[no_mangle]
pub extern "C" fn regex_count_matches(re: &RegexWrapper, text: &nsACString) -> usize {
    let re = &re.regex;
    re.find_iter(&text.to_utf8()).count()
}
