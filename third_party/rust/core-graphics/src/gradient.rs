// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(non_upper_case_globals)]

use base::CGFloat;
use color::CGColor;
use color_space::CGColorSpace;

use core_foundation::array::{ CFArray, CFArrayRef };
use core_foundation::base::{CFRelease, CFRetain, TCFType};
use foreign_types::ForeignType;

use libc::size_t;

bitflags! {
    #[repr(C)]
    pub struct CGGradientDrawingOptions: u32 {
        const CGGradientDrawsBeforeStartLocation = (1 << 0);
        const CGGradientDrawsAfterEndLocation = (1 << 1);
    }
}

foreign_type! {
    #[doc(hidden)]
    type CType = ::sys::CGGradient;
    fn drop = |p| CFRelease(p as *mut _);
    fn clone = |p| CFRetain(p as *const _) as *mut _;
    pub struct CGGradient;
    pub struct CGGradientRef;
}

impl CGGradient {
    pub fn create_with_color_components(color_space: &CGColorSpace, components: &[CGFloat], locations: &[CGFloat], count: usize) -> CGGradient {
        unsafe {
            let result = CGGradientCreateWithColorComponents(color_space.as_ptr(), components.as_ptr(), locations.as_ptr(), count);
            assert!(!result.is_null());
            Self::from_ptr(result)
        }
    }

    pub fn create_with_colors(color_space: &CGColorSpace, colors: &CFArray<CGColor>, locations: &[CGFloat]) -> CGGradient {
        unsafe {
            let result = CGGradientCreateWithColors(color_space.as_ptr(), colors.as_concrete_TypeRef(), locations.as_ptr());
            assert!(!result.is_null());
            Self::from_ptr(result)
        }
    }
}

#[link(name = "CoreGraphics", kind = "framework")]
extern {
    fn CGGradientCreateWithColorComponents(color_space: ::sys::CGColorSpaceRef, components: *const CGFloat, locations: *const CGFloat, count: size_t) -> ::sys::CGGradientRef;
    fn CGGradientCreateWithColors(color_space: ::sys::CGColorSpaceRef, colors: CFArrayRef, locations: *const CGFloat) -> ::sys::CGGradientRef;
}

