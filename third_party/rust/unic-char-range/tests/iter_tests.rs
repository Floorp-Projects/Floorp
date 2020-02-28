// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{char, u32, vec};

use unic_char_range::CharRange;

fn all_chars() -> vec::IntoIter<char> {
    (u32::MIN..u32::MAX)
        .take_while(|&u| u <= char::MAX as u32)
        .filter_map(char::from_u32)
        .collect::<Vec<_>>()
        .into_iter()
}

#[test]
fn test_iter_all_chars() {
    assert!(CharRange::all().iter().eq(all_chars()))
}

#[test]
fn test_iter_all_chars_rev() {
    assert!(CharRange::all().iter().rev().eq(all_chars().rev()))
}

#[test]
fn test_iter_all_chars_mixed_next_back() {
    let mut custom = CharRange::all().iter();
    let mut simple = all_chars();
    while let Some(custom_char) = custom.next() {
        assert_eq!(Some(custom_char), simple.next());
        assert_eq!(custom.next_back(), simple.next_back());
    }
    assert_eq!(None, simple.next());
}

#[test]
fn test_iter_all_chars_into_iter() {
    for _ch in CharRange::all() {
        // nothing
    }
}

#[test]
fn test_iter_fused() {
    let mut iter = CharRange::all().iter();
    let mut fused = all_chars().fuse();
    assert!(iter.by_ref().eq(fused.by_ref()));
    for _ in 0..100 {
        assert_eq!(iter.next(), fused.next());
    }
}

#[test]
fn test_iter_exact_trusted_len() {
    fn assert_presents_right_len<I: ExactSizeIterator>(iter: &I, len: usize) {
        assert_eq!(iter.len(), len);
        assert_eq!(iter.size_hint(), (len, Some(len)));
    }

    let mut iter = CharRange::all().iter();
    let mut predicted_length = iter.len();
    assert_eq!(predicted_length, all_chars().len());

    while let Some(_) = iter.next() {
        predicted_length -= 1;
        assert_presents_right_len(&iter, predicted_length);
    }

    assert_presents_right_len(&iter, 0);
}
