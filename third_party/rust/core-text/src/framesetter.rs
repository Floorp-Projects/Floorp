// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::os::raw::c_void;
use core_foundation::attributed_string::CFAttributedStringRef;
use core_foundation::base::{CFRange, CFTypeID, TCFType};
use core_graphics::path::{CGPath, CGPathRef};
use foreign_types::{ForeignType, ForeignTypeRef};
use std::ptr::null;
use super::frame::{CTFrameRef, CTFrame};

#[repr(C)]
pub struct __CTFramesetter(c_void);

pub type CTFramesetterRef = *const __CTFramesetter;

declare_TCFType! {
    CTFramesetter, CTFramesetterRef
}
impl_TCFType!(CTFramesetter, CTFramesetterRef, CTFramesetterGetTypeID);
impl_CFTypeDescription!(CTFramesetter);

impl CTFramesetter {
    pub fn new_with_attributed_string(string: CFAttributedStringRef) -> Self {
        unsafe {
            let ptr = CTFramesetterCreateWithAttributedString(string);
            CTFramesetter::wrap_under_create_rule(ptr)
        }
    }

    pub fn create_frame(&self, string_range: CFRange, path: &CGPathRef) -> CTFrame {
        unsafe {
            let ptr = CTFramesetterCreateFrame(
                self.as_concrete_TypeRef(), string_range, path.as_ptr(), null());

            CTFrame::wrap_under_create_rule(ptr)
        }
    }
}

#[link(name = "CoreText", kind = "framework")]
extern {
    fn CTFramesetterGetTypeID() -> CFTypeID;
    fn CTFramesetterCreateWithAttributedString(string: CFAttributedStringRef) -> CTFramesetterRef;
    fn CTFramesetterCreateFrame(
        framesetter: CTFramesetterRef,
        string_range: CFRange,
        path: *mut <CGPath as ForeignType>::CType,
        attributes: *const c_void,
    ) -> CTFrameRef;
}