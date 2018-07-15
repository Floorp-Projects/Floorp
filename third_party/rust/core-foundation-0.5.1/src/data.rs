// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Core Foundation byte buffers.

pub use core_foundation_sys::data::*;
use core_foundation_sys::base::CFIndex;
use core_foundation_sys::base::{kCFAllocatorDefault};
use std::ops::Deref;
use std::slice;

use base::{CFIndexConvertible, TCFType};


declare_TCFType!{
    /// A byte buffer.
    CFData, CFDataRef
}
impl_TCFType!(CFData, CFDataRef, CFDataGetTypeID);
impl_CFTypeDescription!(CFData);

impl CFData {
    pub fn from_buffer(buffer: &[u8]) -> CFData {
        unsafe {
            let data_ref = CFDataCreate(kCFAllocatorDefault,
                                        buffer.as_ptr(),
                                        buffer.len().to_CFIndex());
            TCFType::wrap_under_create_rule(data_ref)
        }
    }

    /// Returns a pointer to the underlying bytes in this data. Note that this byte buffer is
    /// read-only.
    #[inline]
    pub fn bytes<'a>(&'a self) -> &'a [u8] {
        unsafe {
            slice::from_raw_parts(CFDataGetBytePtr(self.0), self.len() as usize)
        }
    }

    /// Returns the length of this byte buffer.
    #[inline]
    pub fn len(&self) -> CFIndex {
        unsafe {
            CFDataGetLength(self.0)
        }
    }
}

impl Deref for CFData {
    type Target = [u8];

    #[inline]
    fn deref(&self) -> &[u8] {
        self.bytes()
    }
}
