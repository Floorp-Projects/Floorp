// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Immutable strings.

pub use core_foundation_sys::string::*;

use base::{CFIndexConvertible, TCFType};

use core_foundation_sys::base::{Boolean, CFIndex, CFRange, CFRelease};
use core_foundation_sys::base::{kCFAllocatorDefault, kCFAllocatorNull};
use std::fmt;
use std::str::{self, FromStr};
use std::ptr;
use std::ffi::CStr;

/// An immutable string in one of a variety of encodings.
pub struct CFString(CFStringRef);

impl Clone for CFString {
    #[inline]
    fn clone(&self) -> CFString {
        unsafe {
            TCFType::wrap_under_get_rule(self.0)
        }
    }
}

impl Drop for CFString {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFString, CFStringRef, CFStringGetTypeID);

impl FromStr for CFString {
    type Err = ();

    /// See also CFString::new for a variant of this which does not return a Result
    #[inline]
    fn from_str(string: &str) -> Result<CFString, ()> {
        Ok(CFString::new(string))
    }
}

impl fmt::Display for CFString {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        unsafe {
            // Do this without allocating if we can get away with it
            let c_string = CFStringGetCStringPtr(self.0, kCFStringEncodingUTF8);
            if c_string != ptr::null() {
                let c_str = CStr::from_ptr(c_string);
                fmt.write_str(str::from_utf8_unchecked(c_str.to_bytes()))
            } else {
                let char_len = self.char_len();

                // First, ask how big the buffer ought to be.
                let mut bytes_required: CFIndex = 0;
                CFStringGetBytes(self.0,
                                 CFRange { location: 0, length: char_len },
                                 kCFStringEncodingUTF8,
                                 0,
                                 false as Boolean,
                                 ptr::null_mut(),
                                 0,
                                 &mut bytes_required);

                // Then, allocate the buffer and actually copy.
                let mut buffer = vec![b'\x00'; bytes_required as usize];

                let mut bytes_used: CFIndex = 0;
                let chars_written = CFStringGetBytes(self.0,
                                                     CFRange { location: 0, length: char_len },
                                                     kCFStringEncodingUTF8,
                                                     0,
                                                     false as Boolean,
                                                     buffer.as_mut_ptr(),
                                                     buffer.len().to_CFIndex(),
                                                     &mut bytes_used) as usize;
                assert!(chars_written.to_CFIndex() == char_len);

                // This is dangerous; we over-allocate and null-terminate the string (during
                // initialization).
                assert!(bytes_used == buffer.len().to_CFIndex());
                fmt.write_str(str::from_utf8_unchecked(&buffer))
            }
        }
    }
}

impl fmt::Debug for CFString {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "\"{}\"", self)
    }
}


impl CFString {
    /// Creates a new `CFString` instance from a Rust string.
    #[inline]
    pub fn new(string: &str) -> CFString {
        unsafe {
            let string_ref = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                     string.as_ptr(),
                                                     string.len().to_CFIndex(),
                                                     kCFStringEncodingUTF8,
                                                     false as Boolean,
                                                     kCFAllocatorNull);
            CFString::wrap_under_create_rule(string_ref)
        }
    }

    /// Like `CFString::new`, but references a string that can be used as a backing store
    /// by virtue of being statically allocated.
    #[inline]
    pub fn from_static_string(string: &'static str) -> CFString {
        unsafe {
            let string_ref = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                           string.as_ptr(),
                                                           string.len().to_CFIndex(),
                                                           kCFStringEncodingUTF8,
                                                           false as Boolean,
                                                           kCFAllocatorNull);
            TCFType::wrap_under_create_rule(string_ref)
        }
    }

    /// Returns the number of characters in the string.
    #[inline]
    pub fn char_len(&self) -> CFIndex {
        unsafe {
            CFStringGetLength(self.0)
        }
    }
}

#[test]
fn string_and_back() {
    let original = "The quick brown fox jumped over the slow lazy dog.";
    let cfstr = CFString::from_static_string(original);
    let converted = cfstr.to_string();
    assert!(converted == original);
}
