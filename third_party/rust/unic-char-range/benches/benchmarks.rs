// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![feature(test)]

extern crate test;

use std::char;
use unic_char_range::CharRange;

#[bench]
fn forward_iteration(b: &mut test::Bencher) {
    b.iter(|| CharRange::all().iter().count())
}

#[bench]
fn forward_iteration_baseline(b: &mut test::Bencher) {
    b.iter(|| (0..0x11_0000).filter_map(char::from_u32).count())
}

#[bench]
fn reverse_iteration(b: &mut test::Bencher) {
    b.iter(|| CharRange::all().iter().rev().count())
}

#[bench]
fn reverse_iteration_baseline(b: &mut test::Bencher) {
    b.iter(|| (0..0x11_0000).rev().filter_map(char::from_u32).count())
}

#[bench]
fn range_length(b: &mut test::Bencher) {
    b.iter(|| CharRange::all().len())
}
