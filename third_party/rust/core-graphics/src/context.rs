// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use base::CGFloat;
use color_space::{CGColorSpace, CGColorSpaceRef};
use core_foundation::base::{CFRelease, CFRetain, CFTypeID, CFTypeRef, TCFType};
use libc::{c_void, size_t};
use std::mem;
use std::ptr;
use std::slice;

#[repr(C)]
pub struct __CGContext;

pub type CGContextRef = *const __CGContext;

pub struct CGContext {
    obj: CGContextRef,
}

impl Drop for CGContext {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl Clone for CGContext {
    fn clone(&self) -> CGContext {
        unsafe {
            TCFType::wrap_under_get_rule(self.as_concrete_TypeRef())
        }
    }
}

impl TCFType<CGContextRef> for CGContext {
    #[inline]
    fn as_concrete_TypeRef(&self) -> CGContextRef {
        self.obj
    }

    #[inline]
    unsafe fn wrap_under_get_rule(reference: CGContextRef) -> CGContext {
        let reference: CGContextRef = mem::transmute(CFRetain(mem::transmute(reference)));
        TCFType::wrap_under_create_rule(reference)
    }

    #[inline]
    fn as_CFTypeRef(&self) -> CFTypeRef {
        unsafe {
            mem::transmute(self.as_concrete_TypeRef())
        }
    }

    #[inline]
    unsafe fn wrap_under_create_rule(obj: CGContextRef) -> CGContext {
        CGContext {
            obj: obj,
        }
    }

    #[inline]
    fn type_id() -> CFTypeID {
        unsafe {
            CGContextGetTypeID()
        }
    }
}

impl CGContext {
    pub fn create_bitmap_context(width: size_t,
                                 height: size_t,
                                 bits_per_component: size_t,
                                 bytes_per_row: size_t,
                                 space: &CGColorSpace,
                                 bitmap_info: u32)
                                 -> CGContext {
        unsafe {
            let result = CGBitmapContextCreate(ptr::null_mut(),
                                               width,
                                               height,
                                               bits_per_component,
                                               bytes_per_row,
                                               space.as_concrete_TypeRef(),
                                               bitmap_info);
            TCFType::wrap_under_create_rule(result)
        }
    }

    pub fn data(&mut self) -> &mut [u8] {
        unsafe {
            slice::from_raw_parts_mut(
                    CGBitmapContextGetData(self.as_concrete_TypeRef()) as *mut u8,
                    (self.height() * self.bytes_per_row()) as usize)
        }
    }

    pub fn width(&self) -> size_t {
        unsafe {
            CGBitmapContextGetWidth(self.as_concrete_TypeRef())
        }
    }

    pub fn height(&self) -> size_t {
        unsafe {
            CGBitmapContextGetHeight(self.as_concrete_TypeRef())
        }
    }

    pub fn bytes_per_row(&self) -> size_t {
        unsafe {
            CGBitmapContextGetBytesPerRow(self.as_concrete_TypeRef())
        }
    }

    pub fn set_rgb_fill_color(&self, red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat) {
        unsafe {
            CGContextSetRGBFillColor(self.as_concrete_TypeRef(), red, green, blue, alpha)
        }
    }

    pub fn set_allows_font_smoothing(&self, allows_font_smoothing: bool) {
        unsafe {
            CGContextSetAllowsFontSmoothing(self.as_concrete_TypeRef(), allows_font_smoothing)
        }
    }

    pub fn set_should_smooth_fonts(&self, should_smooth_fonts: bool) {
        unsafe {
            CGContextSetShouldSmoothFonts(self.as_concrete_TypeRef(), should_smooth_fonts)
        }
    }

    pub fn set_allows_antialiasing(&self, allows_antialiasing: bool) {
        unsafe {
            CGContextSetAllowsAntialiasing(self.as_concrete_TypeRef(), allows_antialiasing)
        }
    }

    pub fn set_should_antialias(&self, should_antialias: bool) {
        unsafe {
            CGContextSetShouldAntialias(self.as_concrete_TypeRef(), should_antialias)
        }
    }
}

#[link(name = "ApplicationServices", kind = "framework")]
extern {
    fn CGBitmapContextCreate(data: *mut c_void,
                             width: size_t,
                             height: size_t,
                             bitsPerComponent: size_t,
                             bytesPerRow: size_t,
                             space: CGColorSpaceRef,
                             bitmapInfo: u32)
                             -> CGContextRef;
    fn CGBitmapContextGetData(context: CGContextRef) -> *mut c_void;
    fn CGBitmapContextGetWidth(context: CGContextRef) -> size_t;
    fn CGBitmapContextGetHeight(context: CGContextRef) -> size_t;
    fn CGBitmapContextGetBytesPerRow(context: CGContextRef) -> size_t;
    fn CGContextGetTypeID() -> CFTypeID;
    fn CGContextSetAllowsFontSmoothing(c: CGContextRef, allowsFontSmoothing: bool);
    fn CGContextSetShouldSmoothFonts(c: CGContextRef, shouldSmoothFonts: bool);
    fn CGContextSetAllowsAntialiasing(c: CGContextRef, allowsAntialiasing: bool);
    fn CGContextSetShouldAntialias(c: CGContextRef, shouldAntialias: bool);
    fn CGContextSetRGBFillColor(context: CGContextRef,
                                red: CGFloat,
                                green: CGFloat,
                                blue: CGFloat,
                                alpha: CGFloat);
}

