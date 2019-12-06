// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "pattern", feature(pattern))]

extern crate rand;
extern crate regex;

// Due to macro scoping rules, this definition only applies for the modules
// defined below. Effectively, it allows us to use the same tests for both
// native and dynamic regexes.
//
// This is also used to test the various matching engines. This one exercises
// the normal code path which automatically chooses the engine based on the
// regex and the input. Other dynamic tests explicitly set the engine to use.
macro_rules! regex_new {
    ($re:expr) => {{
        use regex::Regex;
        Regex::new($re)
    }}
}

macro_rules! regex {
    ($re:expr) => {
        regex_new!($re).unwrap()
    }
}

macro_rules! regex_set_new {
    ($re:expr) => {{
        use regex::RegexSet;
        RegexSet::new($re)
    }}
}

macro_rules! regex_set {
    ($res:expr) => {
        regex_set_new!($res).unwrap()
    }
}

// Must come before other module definitions.
include!("macros_str.rs");
include!("macros.rs");

mod api;
mod api_str;
mod crazy;
mod flags;
mod fowler;
mod misc;
mod multiline;
mod noparse;
mod regression;
mod replace;
mod searcher;
mod set;
mod shortest_match;
mod suffix_reverse;
mod unicode;
mod word_boundary;
mod word_boundary_unicode;

#[test]
fn disallow_non_utf8() {
    assert!(regex::Regex::new(r"(?-u)\xFF").is_err());
    assert!(regex::Regex::new(r"(?-u).").is_err());
    assert!(regex::Regex::new(r"(?-u)[\xFF]").is_err());
    assert!(regex::Regex::new(r"(?-u)☃").is_err());
}

#[test]
fn disallow_octal() {
    assert!(regex::Regex::new(r"\0").is_err());
}

#[test]
fn allow_octal() {
    assert!(regex::RegexBuilder::new(r"\0").octal(true).build().is_ok());
}

#[test]
fn oibits() {
    use std::panic::UnwindSafe;
    use regex::{Regex, RegexBuilder};
    use regex::bytes;

    fn assert_send<T: Send>() {}
    fn assert_sync<T: Sync>() {}
    fn assert_unwind_safe<T: UnwindSafe>() {}

    assert_send::<Regex>();
    assert_sync::<Regex>();
    assert_unwind_safe::<Regex>();
    assert_send::<RegexBuilder>();
    assert_sync::<RegexBuilder>();
    assert_unwind_safe::<RegexBuilder>();

    assert_send::<bytes::Regex>();
    assert_sync::<bytes::Regex>();
    assert_unwind_safe::<bytes::Regex>();
    assert_send::<bytes::RegexBuilder>();
    assert_sync::<bytes::RegexBuilder>();
    assert_unwind_safe::<bytes::RegexBuilder>();
}

// See: https://github.com/rust-lang/regex/issues/568
#[test]
fn oibits_regression() {
    use std::panic;
    use regex::Regex;

    let _ = panic::catch_unwind(|| Regex::new("a").unwrap());
}
