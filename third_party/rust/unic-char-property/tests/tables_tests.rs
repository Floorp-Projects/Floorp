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

use unic_char_property::tables::CharDataTable;

#[test]
fn test_range_value_table() {
    const TABLE: CharDataTable<u32> = CharDataTable::Range(&[
        (chars!('a'..='g'), 1),
        (chars!('j'..='q'), 2),
        (chars!('w'..='z'), 3),
    ]);
    for ch in chars!('a'..='g') {
        assert_eq!(TABLE.find(ch), Some(1));
        assert_eq!(TABLE.find_or_default(ch), 1);
    }
    for ch in chars!('h'..='i') {
        assert_eq!(TABLE.find(ch), None);
        assert_eq!(TABLE.find_or_default(ch), 0);
    }
    for ch in chars!('j'..='q') {
        assert_eq!(TABLE.find(ch), Some(2));
        assert_eq!(TABLE.find_or_default(ch), 2);
    }
    for ch in chars!('r'..='v') {
        assert_eq!(TABLE.find(ch), None);
        assert_eq!(TABLE.find_or_default(ch), 0);
    }
    for ch in chars!('x'..='z') {
        assert_eq!(TABLE.find(ch), Some(3));
        assert_eq!(TABLE.find_or_default(ch), 3);
    }
}
