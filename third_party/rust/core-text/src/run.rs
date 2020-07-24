// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::borrow::Cow;
use std::os::raw::c_void;
use std::slice;
use core_foundation::base::{CFIndex, CFTypeID, TCFType, CFType, CFRange};
use core_foundation::dictionary::{CFDictionary, CFDictionaryRef};
use core_foundation::string::CFString;
use core_graphics::font::CGGlyph;
use core_graphics::geometry::CGPoint;

#[repr(C)]
pub struct __CTRun(c_void);

pub type CTRunRef = *const __CTRun;

declare_TCFType! {
    CTRun, CTRunRef
}
impl_TCFType!(CTRun, CTRunRef, CTRunGetTypeID);
impl_CFTypeDescription!(CTRun);

impl CTRun {
    pub fn attributes(&self) -> Option<CFDictionary<CFString, CFType>> {
        unsafe {
            let attrs = CTRunGetAttributes(self.0);
            if attrs.is_null() {
                return None;
            }
            Some(TCFType::wrap_under_get_rule(attrs))
        }
    }
    pub fn glyph_count(&self) -> CFIndex {
        unsafe {
            CTRunGetGlyphCount(self.0)
        }
    }

    pub fn glyphs(&self) -> Cow<[CGGlyph]> {
        unsafe {
            // CTRunGetGlyphsPtr can return null under some not understood circumstances.
            // If it does the Apple documentation tells us to allocate our own buffer and call
            // CTRunGetGlyphs
            let count = CTRunGetGlyphCount(self.0);
            let glyphs_ptr = CTRunGetGlyphsPtr(self.0);
            if !glyphs_ptr.is_null() {
                Cow::from(slice::from_raw_parts(glyphs_ptr, count as usize))
            } else {
                let mut vec = Vec::with_capacity(count as usize);
                // "If the length of the range is set to 0, then the copy operation will continue
                // from the start index of the range to the end of the run"
                CTRunGetGlyphs(self.0, CFRange::init(0, 0), vec.as_mut_ptr());
                vec.set_len(count as usize);
                Cow::from(vec)
            }
        }
    }

    pub fn positions(&self) -> Cow<[CGPoint]> {
        unsafe {
            // CTRunGetPositionsPtr can return null under some not understood circumstances.
            // If it does the Apple documentation tells us to allocate our own buffer and call
            // CTRunGetPositions
            let count = CTRunGetGlyphCount(self.0);
            let positions_ptr = CTRunGetPositionsPtr(self.0);
            if !positions_ptr.is_null() {
                Cow::from(slice::from_raw_parts(positions_ptr, count as usize))
            } else {
                let mut vec = Vec::with_capacity(count as usize);
                // "If the length of the range is set to 0, then the copy operation will continue
                // from the start index of the range to the end of the run"
                CTRunGetPositions(self.0, CFRange::init(0, 0), vec.as_mut_ptr());
                vec.set_len(count as usize);
                Cow::from(vec)
            }
        }
    }

}

#[test]
fn create_runs() {
    use core_foundation::attributed_string::CFMutableAttributedString;
    use string_attributes::*;
    use line::*;
    use font;
    let mut string = CFMutableAttributedString::new();
    string.replace_str(&CFString::new("Food"), CFRange::init(0, 0));
    let len = string.char_len();
    unsafe {
        string.set_attribute(CFRange::init(0, len), kCTFontAttributeName, font::new_from_name("Helvetica", 16.).unwrap());
    }
    let line = CTLine::new_with_attributed_string(string.as_concrete_TypeRef());
    let runs = line.glyph_runs();
    assert_eq!(runs.len(), 1);
    for run in runs.iter() {
        assert_eq!(run.glyph_count(), 4);
        let font = run.attributes().unwrap().get(CFString::new("NSFont")).downcast::<font::CTFont>().unwrap();
        assert_eq!(font.pt_size(), 16.);

        let positions = run.positions();
        assert_eq!(positions.len(), 4);
        assert!(positions[0].x < positions[1].x);

        let glyphs = run.glyphs();
        assert_eq!(glyphs.len(), 4);
        assert_ne!(glyphs[0], glyphs[1]);
        assert_eq!(glyphs[1], glyphs[2]);
    }
}

#[link(name = "CoreText", kind = "framework")]
extern {
    fn CTRunGetTypeID() -> CFTypeID;
    fn CTRunGetAttributes(run: CTRunRef) -> CFDictionaryRef;
    fn CTRunGetGlyphCount(run: CTRunRef) -> CFIndex;
    fn CTRunGetPositionsPtr(run: CTRunRef) -> *const CGPoint;
    fn CTRunGetPositions(run: CTRunRef, range: CFRange, buffer: *const CGPoint);
    fn CTRunGetGlyphsPtr(run: CTRunRef) -> *const CGGlyph;
    fn CTRunGetGlyphs(run: CTRunRef, range: CFRange, buffer: *const CGGlyph);
}
