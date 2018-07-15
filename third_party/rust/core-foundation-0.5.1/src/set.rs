// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! An immutable bag of elements.

pub use core_foundation_sys::set::*;
use core_foundation_sys::base::{CFTypeRef, kCFAllocatorDefault};

use base::{CFIndexConvertible, TCFType};

use std::mem;


declare_TCFType!{
    /// An immutable bag of elements.
    CFSet, CFSetRef
}
impl_TCFType!(CFSet, CFSetRef, CFSetGetTypeID);
impl_CFTypeDescription!(CFSet);

impl CFSet {
    /// Creates a new set from a list of `CFType` instances.
    pub fn from_slice<T>(elems: &[T]) -> CFSet where T: TCFType {
        unsafe {
            let elems: Vec<CFTypeRef> = elems.iter().map(|elem| elem.as_CFTypeRef()).collect();
            let set_ref = CFSetCreate(kCFAllocatorDefault,
                                      mem::transmute(elems.as_ptr()),
                                      elems.len().to_CFIndex(),
                                      &kCFTypeSetCallBacks);
            TCFType::wrap_under_create_rule(set_ref)
        }
    }
}
