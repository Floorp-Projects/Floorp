// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core_foundation::base::{CFRelease, CFRetain, CFTypeID};
use core_foundation::string::CFStringRef;
use foreign_types::ForeignType;

foreign_type! {
    #[doc(hidden)]
    type CType = ::sys::CGColorSpace;
    fn drop = |p| CFRelease(p as *mut _);
    fn clone = |p| CFRetain(p as *const _) as *mut _;
    pub struct CGColorSpace;
    pub struct CGColorSpaceRef;
}

impl CGColorSpace {
    pub fn type_id() -> CFTypeID {
        unsafe {
            CGColorSpaceGetTypeID()
        }
    }

    pub fn create_with_name(name: CFStringRef) -> Option<CGColorSpace> {
        unsafe {
            let p = CGColorSpaceCreateWithName(name);
            if !p.is_null() {Some(CGColorSpace::from_ptr(p))} else {None}
        }
    }

    #[inline]
    pub fn create_device_rgb() -> CGColorSpace {
        unsafe {
            let result = CGColorSpaceCreateDeviceRGB();
            CGColorSpace::from_ptr(result)
        }
    }

    #[inline]
    pub fn create_device_gray() -> CGColorSpace {
        unsafe {
            let result = CGColorSpaceCreateDeviceGray();
            CGColorSpace::from_ptr(result)
        }
    }
}

#[link(name = "CoreGraphics", kind = "framework")]
extern {
    pub static kCGColorSpaceSRGB: CFStringRef;
    pub static kCGColorSpaceAdobeRGB1998: CFStringRef;
    pub static kCGColorSpaceGenericGray: CFStringRef;
    pub static kCGColorSpaceGenericRGB: CFStringRef;
    pub static kCGColorSpaceGenericCMYK: CFStringRef;
    pub static kCGColorSpaceGenericRGBLinear: CFStringRef;
    pub static kCGColorSpaceGenericGrayGamma2_2: CFStringRef;

    fn CGColorSpaceCreateDeviceRGB() -> ::sys::CGColorSpaceRef;
    fn CGColorSpaceCreateDeviceGray() -> ::sys::CGColorSpaceRef;
    fn CGColorSpaceCreateWithName(name: CFStringRef) -> ::sys::CGColorSpaceRef;
    fn CGColorSpaceGetTypeID() -> CFTypeID;
}

