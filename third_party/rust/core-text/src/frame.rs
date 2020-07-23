// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::os::raw::c_void;
use core_foundation::base::{CFTypeID, TCFType};
use core_graphics::context::{CGContext, CGContextRef};
use foreign_types::{ForeignType, ForeignTypeRef};

#[repr(C)]
pub struct __CTFrame(c_void);

pub type CTFrameRef = *const __CTFrame;

declare_TCFType! {
    CTFrame, CTFrameRef
}
impl_TCFType!(CTFrame, CTFrameRef, CTFrameGetTypeID);
impl_CFTypeDescription!(CTFrame);

impl CTFrame {
    pub fn draw(&self, context: &CGContextRef) {
        unsafe {
            CTFrameDraw(self.as_concrete_TypeRef(), context.as_ptr());
        }
    }
}

#[link(name = "CoreText", kind = "framework")]
extern {
    fn CTFrameGetTypeID() -> CFTypeID;
    fn CTFrameDraw(frame: CTFrameRef, context: *mut <CGContext as ForeignType>::CType);
}