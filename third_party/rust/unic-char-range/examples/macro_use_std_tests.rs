// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[macro_use]
extern crate unic_char_range;

fn main() {
    assert!(chars!('\u{0}'..='\u{2}')
        .iter()
        .eq(['\u{0}', '\u{1}', '\u{2}'].iter().cloned()));
}
