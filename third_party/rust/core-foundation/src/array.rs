// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Heterogeneous immutable arrays.

pub use core_foundation_sys::array::*;
pub use core_foundation_sys::base::{CFIndex, CFRelease};
use core_foundation_sys::base::{CFTypeRef, kCFAllocatorDefault};
use libc::c_void;
use std::mem;

use base::{CFIndexConvertible, TCFType};

/// A heterogeneous immutable array.
pub struct CFArray(CFArrayRef);

impl Drop for CFArray {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

pub struct CFArrayIterator<'a> {
    array: &'a CFArray,
    index: CFIndex,
}

impl<'a> Iterator for CFArrayIterator<'a> {
    type Item = *const c_void;

    fn next(&mut self) -> Option<*const c_void> {
        if self.index >= self.array.len() {
            None
        } else {
            let value = self.array.get(self.index);
            self.index += 1;
            Some(value)
        }
    }
}

impl_TCFType!(CFArray, CFArrayRef, CFArrayGetTypeID);

impl CFArray {
    /// Creates a new `CFArray` with the given elements, which must be `CFType` objects.
    pub fn from_CFTypes<R, T>(elems: &[T]) -> CFArray where T: TCFType<R> {
        unsafe {
            let elems: Vec<CFTypeRef> = elems.iter().map(|elem| elem.as_CFTypeRef()).collect();
            let array_ref = CFArrayCreate(kCFAllocatorDefault,
                                          mem::transmute(elems.as_ptr()),
                                          elems.len().to_CFIndex(),
                                          &kCFTypeArrayCallBacks);
            TCFType::wrap_under_create_rule(array_ref)
        }
    }

    /// Iterates over the elements of this `CFArray`.
    ///
    /// Careful; the loop body must wrap the reference properly. Generally, when array elements are
    /// Core Foundation objects (not always true), they need to be wrapped with
    /// `TCFType::wrap_under_get_rule()`.
    #[inline]
    pub fn iter<'a>(&'a self) -> CFArrayIterator<'a> {
        CFArrayIterator {
            array: self,
            index: 0
        }
    }

    #[inline]
    pub fn len(&self) -> CFIndex {
        unsafe {
            CFArrayGetCount(self.0)
        }
    }

    #[inline]
    pub fn get(&self, index: CFIndex) -> *const c_void {
        assert!(index < self.len());
        unsafe {
            CFArrayGetValueAtIndex(self.0, index)
        }
    }
}

impl<'a> IntoIterator for &'a CFArray {
    type Item = *const c_void;
    type IntoIter = CFArrayIterator<'a>;

    fn into_iter(self) -> CFArrayIterator<'a> {
        self.iter()
    }
}

#[test]
fn should_box_and_unbox() {
    use number::{CFNumber, number};

    let arr = CFArray::from_CFTypes(&[
        number(1).as_CFType(),
        number(2).as_CFType(),
        number(3).as_CFType(),
        number(4).as_CFType(),
        number(5).as_CFType(),
    ]);

    unsafe {
        let mut sum = 0;

        for elem in arr.iter() {
            let number: CFNumber = TCFType::wrap_under_get_rule(mem::transmute(elem));
            sum += number.to_i64().unwrap()
        }

        assert!(sum == 15);

        for elem in arr.iter() {
            let number: CFNumber = TCFType::wrap_under_get_rule(mem::transmute(elem));
            sum += number.to_i64().unwrap()
        }

        assert!(sum == 30);
    }
}
