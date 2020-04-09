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
use core_foundation::array::{CFArray, CFArrayRef};
use core_foundation::base::{CFTypeID, TCFType};
use run::CTRun;

#[repr(C)]
pub struct __CTLine(c_void);

pub type CTLineRef = *const __CTLine;

declare_TCFType! {
    CTLine, CTLineRef
}
impl_TCFType!(CTLine, CTLineRef, CTLineGetTypeID);
impl_CFTypeDescription!(CTLine);

impl CTLine {
    pub fn new_with_attributed_string(string: CFAttributedStringRef) -> Self {
        unsafe {
            let ptr = CTLineCreateWithAttributedString(string);
            CTLine::wrap_under_create_rule(ptr)
        }
    }

    pub fn glyph_runs(&self) -> CFArray<CTRun> {
        unsafe {
            TCFType::wrap_under_get_rule(CTLineGetGlyphRuns(self.0))
        }
    }
}

#[link(name = "CoreText", kind = "framework")]
extern {
    fn CTLineGetTypeID() -> CFTypeID;
    fn CTLineGetGlyphRuns(line: CTLineRef) -> CFArrayRef;
    fn CTLineCreateWithAttributedString(string: CFAttributedStringRef) -> CTLineRef;
}
