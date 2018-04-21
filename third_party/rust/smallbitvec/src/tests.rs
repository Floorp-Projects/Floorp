// Copyright 2017 Matt Brubeck. See the COPYRIGHT file at the top-level
// directory of this distribution and at http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::*;

#[cfg(target_pointer_width = "32")]
#[test]
fn test_inline_capacity() {
    assert_eq!(inline_capacity(), 30);
}

#[cfg(target_pointer_width = "64")]
#[test]
fn test_inline_capacity() {
    assert_eq!(inline_capacity(), 62);
}

#[test]
fn new() {
    let v = SmallBitVec::new();
    assert_eq!(v.len(), 0);
    assert_eq!(v.capacity(), inline_capacity());
    assert!(v.is_empty());
    assert!(v.is_inline());
}

#[test]
fn with_capacity_inline() {
    for cap in 0..(inline_capacity() + 1) {
        let v = SmallBitVec::with_capacity(cap);
        assert_eq!(v.len(), 0);
        assert_eq!(v.capacity(), inline_capacity());
        assert!(v.is_inline());
    }
}

#[test]
fn with_capacity_heap() {
    let cap = inline_capacity() + 1;
    let v = SmallBitVec::with_capacity(cap);
    assert_eq!(v.len(), 0);
    assert!(v.capacity() > inline_capacity());
    assert!(v.is_heap());
}

#[test]
fn set_len_inline() {
    let mut v = SmallBitVec::new();
    for i in 0..(inline_capacity() + 1) {
        unsafe {
            v.set_len(i);
        }
        assert_eq!(v.len(), i);
    }
    for i in (0..(inline_capacity() + 1)).rev() {
        unsafe {
            v.set_len(i);
        }
        assert_eq!(v.len(), i);
    }
}

#[test]
fn set_len_heap() {
    let mut v = SmallBitVec::with_capacity(500);
    unsafe {
        v.set_len(30);
    }
    assert_eq!(v.len(), 30);
}

#[test]
fn push_many() {
    let mut v = SmallBitVec::new();
    for i in 0..500 {
        v.push(i % 3 == 0);
    }
    assert_eq!(v.len(), 500);

    for i in 0..500 {
        assert_eq!(v.get(i).unwrap(), (i % 3 == 0), "{}", i);
        assert_eq!(v[i], v.get(i).unwrap());
    }
}

#[test]
#[should_panic(expected = "index out of range")]
fn index_out_of_bounds() {
    let v = SmallBitVec::new();
    v[0];
}

#[test]
#[should_panic(expected = "index out of range")]
fn index_out_of_bounds_nonempty() {
    let mut v = SmallBitVec::new();
    v.push(true);
    v[1 << 32];
}

#[test]
fn get_out_of_bounds() {
    let v = SmallBitVec::new();
    assert!(v.get(0).is_none());
}

#[test]
#[should_panic]
fn set_out_of_bounds() {
    let mut v = SmallBitVec::new();
    v.set(2, false);
}

#[test]
fn all_false() {
    let mut v = SmallBitVec::new();
    assert!(v.all_false());
    for _ in 0..100 {
        v.push(false);
        assert!(v.all_false());

        let mut v2 = v.clone();
        v2.push(true);
        assert!(!v2.all_false());
    }
}

#[test]
fn all_true() {
    let mut v = SmallBitVec::new();
    assert!(v.all_true());
    for _ in 0..100 {
        v.push(true);
        assert!(v.all_true());

        let mut v2 = v.clone();
        v2.push(false);
        assert!(!v2.all_true());
    }
}

#[test]
fn iter() {
    let mut v = SmallBitVec::new();
    v.push(true);
    v.push(false);
    v.push(false);

    let mut i = v.iter();
    assert_eq!(i.next(), Some(true));
    assert_eq!(i.next(), Some(false));
    assert_eq!(i.next(), Some(false));
    assert_eq!(i.next(), None);
}

#[test]
fn into_iter() {
    let mut v = SmallBitVec::new();
    v.push(true);
    v.push(false);
    v.push(false);

    let mut i = v.into_iter();
    assert_eq!(i.next(), Some(true));
    assert_eq!(i.next(), Some(false));
    assert_eq!(i.next(), Some(false));
    assert_eq!(i.next(), None);
}

#[test]
fn iter_back() {
    let mut v = SmallBitVec::new();
    v.push(true);
    v.push(false);
    v.push(false);

    let mut i = v.iter();
    assert_eq!(i.next_back(), Some(false));
    assert_eq!(i.next_back(), Some(false));
    assert_eq!(i.next_back(), Some(true));
    assert_eq!(i.next(), None);
}

#[test]
fn range() {
    let mut v = SmallBitVec::new();
    v.push(true);
    v.push(false);
    v.push(false);
    v.push(true);

    let r = v.range(0..2);
    let mut ri = r.iter();
    assert_eq!(ri.next(), Some(true));
    assert_eq!(ri.next(), Some(false));
    assert_eq!(ri.next(), None);
    assert_eq!(r[0], true);
}

#[test]
#[should_panic(expected = "range out of bounds")]
fn range_oob() {
    let mut v = SmallBitVec::new();
    v.push(true);

    v.range(0..2);
}

#[test]
#[should_panic(expected = "index out of range")]
fn range_index_oob() {
    let mut v = SmallBitVec::new();
    v.push(true);
    v.push(false);
    v.push(true);

    let r = v.range(1..2);
    r[1];
}

#[test]
fn debug() {
    let mut v = SmallBitVec::new();
    v.push(true);
    v.push(false);
    v.push(false);

    assert_eq!(format!("{:?}", v), "[1, 0, 0]")
}

#[test]
fn from_elem() {
    for len in 0..100 {
        let ones = SmallBitVec::from_elem(len, true);
        assert_eq!(ones.len(), len);
        assert!(ones.all_true());

        let zeros = SmallBitVec::from_elem(len, false);
        assert_eq!(zeros.len(), len);
        assert!(zeros.all_false());
    }
}

#[test]
fn remove() {
    let mut v = SmallBitVec::new();
    v.push(false);
    v.push(true);
    v.push(false);
    v.push(false);
    v.push(true);

    assert_eq!(v.remove(1), true);
    assert_eq!(format!("{:?}", v), "[0, 0, 0, 1]");
    assert_eq!(v.remove(0), false);
    assert_eq!(format!("{:?}", v), "[0, 0, 1]");
    v.remove(2);
    assert_eq!(format!("{:?}", v), "[0, 0]");
    v.remove(1);
    assert_eq!(format!("{:?}", v), "[0]");
    v.remove(0);
    assert_eq!(format!("{:?}", v), "[]");
}

#[test]
fn remove_big() {
    let mut v = SmallBitVec::from_elem(256, false);
    v.set(100, true);
    v.set(255, true);
    v.remove(0);
    assert_eq!(v.len(), 255);
    assert_eq!(v.get(0).unwrap(), false);
    assert_eq!(v.get(99).unwrap(), true);
    assert_eq!(v.get(100).unwrap(), false);
    assert_eq!(v.get(253).unwrap(), false);
    assert_eq!(v.get(254).unwrap(), true);

    v.remove(254);
    assert_eq!(v.len(), 254);
    assert_eq!(v.get(0).unwrap(), false);
    assert_eq!(v.get(99).unwrap(), true);
    assert_eq!(v.get(100).unwrap(), false);
    assert_eq!(v.get(253).unwrap(), false);

    v.remove(99);
    assert_eq!(v, SmallBitVec::from_elem(253, false));
}

#[test]
fn eq() {
    assert_eq!(sbvec![], sbvec![]);
    assert_eq!(sbvec![true], sbvec![true]);
    assert_eq!(sbvec![false], sbvec![false]);

    assert_ne!(sbvec![], sbvec![false]);
    assert_ne!(sbvec![true], sbvec![]);
    assert_ne!(sbvec![true], sbvec![false]);
    assert_ne!(sbvec![false], sbvec![true]);

    assert_eq!(sbvec![true, false], sbvec![true, false]);
    assert_eq!(sbvec![true; 400], sbvec![true; 400]);
    assert_eq!(sbvec![false; 400], sbvec![false; 400]);

    assert_ne!(sbvec![true, false], sbvec![true, true]);
    assert_ne!(sbvec![true; 400], sbvec![true; 401]);
    assert_ne!(sbvec![false; 401], sbvec![false; 400]);
}
