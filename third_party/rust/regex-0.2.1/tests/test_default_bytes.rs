// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

extern crate rand;
extern crate regex;

macro_rules! regex_new {
    ($re:expr) => {{
        use regex::bytes::Regex;
        Regex::new($re)
    }}
}

macro_rules! regex_set_new {
    ($res:expr) => {{
        use regex::bytes::RegexSet;
        RegexSet::new($res)
    }}
}

macro_rules! regex {
    ($re:expr) => {
        regex_new!($re).unwrap()
    }
}

macro_rules! regex_set {
    ($res:expr) => {
        regex_set_new!($res).unwrap()
    }
}

// Must come before other module definitions.
include!("macros_bytes.rs");
include!("macros.rs");

mod api;
mod bytes;
mod crazy;
mod flags;
mod fowler;
mod multiline;
mod noparse;
mod regression;
mod replace;
mod set;
mod shortest_match;
mod suffix_reverse;
mod unicode;
mod word_boundary;
mod word_boundary_ascii;
