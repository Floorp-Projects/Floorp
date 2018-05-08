// Copyright 2013-2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use libc::c_void;

use base::{CFAllocatorRef, CFTypeID, CFComparisonResult};

#[repr(C)]
pub struct __CFBoolean(c_void);

pub type CFBooleanRef = *const __CFBoolean;

pub type CFNumberType = u32;

// members of enum CFNumberType
// static kCFNumberSInt8Type:     CFNumberType = 1;
// static kCFNumberSInt16Type:    CFNumberType = 2;
pub static kCFNumberSInt32Type:    CFNumberType = 3;
pub static kCFNumberSInt64Type:    CFNumberType = 4;
pub static kCFNumberFloat32Type:   CFNumberType = 5;
pub static kCFNumberFloat64Type:   CFNumberType = 6;
// static kCFNumberCharType:      CFNumberType = 7;
// static kCFNumberShortType:     CFNumberType = 8;
// static kCFNumberIntType:       CFNumberType = 9;
// static kCFNumberLongType:      CFNumberType = 10;
// static kCFNumberLongLongType:  CFNumberType = 11;
// static kCFNumberFloatType:     CFNumberType = 12;
// static kCFNumberDoubleType:    CFNumberType = 13;
// static kCFNumberCFIndexType:   CFNumberType = 14;
// static kCFNumberNSIntegerType: CFNumberType = 15;
// static kCFNumberCGFloatType:   CFNumberType = 16;
// static kCFNumberMaxType:       CFNumberType = 16;

// This is an enum due to zero-sized types warnings.
// For more details see https://github.com/rust-lang/rust/issues/27303
pub enum __CFNumber {}

pub type CFNumberRef = *const __CFNumber;

extern {
    /*
     * CFNumber.h
     */
    pub static kCFBooleanTrue: CFBooleanRef;
    pub static kCFBooleanFalse: CFBooleanRef;

    pub fn CFBooleanGetTypeID() -> CFTypeID;
    pub fn CFNumberCreate(allocator: CFAllocatorRef, theType: CFNumberType, valuePtr: *const c_void)
                          -> CFNumberRef;
    //fn CFNumberGetByteSize
    pub fn CFNumberGetValue(number: CFNumberRef, theType: CFNumberType, valuePtr: *mut c_void) -> bool;
    pub fn CFNumberCompare(date: CFNumberRef, other: CFNumberRef, context: *mut c_void) -> CFComparisonResult;
    pub fn CFNumberGetTypeID() -> CFTypeID;
}
