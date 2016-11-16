// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core_foundation::base::{CFRelease, CFRetain, CFTypeID, CFTypeRef, TCFType};
use std::mem;

#[repr(C)]
pub struct __CGColorSpace;

pub type CGColorSpaceRef = *const __CGColorSpace;

pub struct CGColorSpace {
    obj: CGColorSpaceRef,
}

impl Drop for CGColorSpace {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl Clone for CGColorSpace {
    fn clone(&self) -> CGColorSpace {
        unsafe {
            TCFType::wrap_under_get_rule(self.as_concrete_TypeRef())
        }
    }
}

impl TCFType<CGColorSpaceRef> for CGColorSpace {
    #[inline]
    fn as_concrete_TypeRef(&self) -> CGColorSpaceRef {
        self.obj
    }

    #[inline]
    unsafe fn wrap_under_get_rule(reference: CGColorSpaceRef) -> CGColorSpace {
        let reference: CGColorSpaceRef = mem::transmute(CFRetain(mem::transmute(reference)));
        TCFType::wrap_under_create_rule(reference)
    }

    #[inline]
    fn as_CFTypeRef(&self) -> CFTypeRef {
        unsafe {
            mem::transmute(self.as_concrete_TypeRef())
        }
    }

    #[inline]
    unsafe fn wrap_under_create_rule(obj: CGColorSpaceRef) -> CGColorSpace {
        CGColorSpace {
            obj: obj,
        }
    }

    #[inline]
    fn type_id() -> CFTypeID {
        unsafe {
            CGColorSpaceGetTypeID()
        }
    }
}

impl CGColorSpace {
    pub fn create_device_rgb() -> CGColorSpace {
        unsafe {
            let result = CGColorSpaceCreateDeviceRGB();
            TCFType::wrap_under_create_rule(result)
        }
    }
}

#[link(name = "ApplicationServices", kind = "framework")]
extern {
    fn CGColorSpaceCreateDeviceRGB() -> CGColorSpaceRef;
    fn CGColorSpaceGetTypeID() -> CFTypeID;
}

