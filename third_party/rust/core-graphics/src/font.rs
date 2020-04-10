// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::ptr;
use core_foundation::base::{CFRelease, CFRetain, CFTypeID, TCFType};
use core_foundation::array::{CFArray, CFArrayRef};
use core_foundation::data::{CFData, CFDataRef};
use core_foundation::number::CFNumber;
use core_foundation::string::{CFString, CFStringRef};
use core_foundation::dictionary::{CFDictionary, CFDictionaryRef};
use data_provider::CGDataProvider;
use geometry::CGRect;

use foreign_types::ForeignType;

use libc::{self, c_int, size_t};

pub type CGGlyph = libc::c_ushort;

foreign_type! {
    #[doc(hidden)]
    type CType = ::sys::CGFont;
    fn drop = |p| CFRelease(p as *mut _);
    fn clone = |p| CFRetain(p as *const _) as *mut _;
    pub struct CGFont;
    pub struct CGFontRef;
}

unsafe impl Send for CGFont {}
unsafe impl Sync for CGFont {}

impl CGFont {
    pub fn type_id() -> CFTypeID {
        unsafe {
            CGFontGetTypeID()
        }
    }

    pub fn from_data_provider(provider: CGDataProvider) -> Result<CGFont, ()> {
        unsafe {
            let font_ref = CGFontCreateWithDataProvider(provider.as_ptr());
            if !font_ref.is_null() {
                Ok(CGFont::from_ptr(font_ref))
            } else {
                Err(())
            }
        }
    }

    pub fn from_name(name: &CFString) -> Result<CGFont, ()> {
        unsafe {
            let font_ref = CGFontCreateWithFontName(name.as_concrete_TypeRef());
            if !font_ref.is_null() {
                Ok(CGFont::from_ptr(font_ref))
            } else {
                Err(())
            }
        }
    }

    pub fn create_copy_from_variations(&self, vars: &CFDictionary<CFString, CFNumber>) -> Result<CGFont, ()> {
        unsafe {
            let font_ref = CGFontCreateCopyWithVariations(self.as_ptr(),
                                                          vars.as_concrete_TypeRef());
            if !font_ref.is_null() {
                Ok(CGFont::from_ptr(font_ref))
            } else {
                Err(())
            }
        }
    }

    pub fn postscript_name(&self) -> CFString {
        unsafe {
            let string_ref = CGFontCopyPostScriptName(self.as_ptr());
            TCFType::wrap_under_create_rule(string_ref)
        }
    }

    pub fn get_glyph_b_boxes(&self, glyphs: &[CGGlyph], bboxes: &mut [CGRect]) -> bool {
        unsafe {
            assert!(bboxes.len() >= glyphs.len());
            CGFontGetGlyphBBoxes(self.as_ptr(),
                                 glyphs.as_ptr(),
                                 glyphs.len(),
                                 bboxes.as_mut_ptr())
        }
    }

    pub fn get_glyph_advances(&self, glyphs: &[CGGlyph], advances: &mut [c_int]) -> bool {
        unsafe {
            assert!(advances.len() >= glyphs.len());
            CGFontGetGlyphAdvances(self.as_ptr(),
                                   glyphs.as_ptr(),
                                   glyphs.len(),
                                   advances.as_mut_ptr())
        }
    }

    pub fn get_units_per_em(&self) -> c_int {
        unsafe {
            CGFontGetUnitsPerEm(self.as_ptr())
        }
    }

    pub fn copy_table_tags(&self) -> CFArray<u32> {
        unsafe {
            TCFType::wrap_under_create_rule(CGFontCopyTableTags(self.as_ptr()))
        }
    }

    pub fn copy_table_for_tag(&self, tag: u32) -> Option<CFData> {
        let data_ref = unsafe { CGFontCopyTableForTag(self.as_ptr(), tag) };
        if data_ref != ptr::null() {
            Some(unsafe { TCFType::wrap_under_create_rule(data_ref) })
        } else {
            None
        }
    }
}

#[link(name = "CoreGraphics", kind = "framework")]
extern {
    // TODO: basically nothing has bindings (even commented-out) besides what we use.
    fn CGFontCreateWithDataProvider(provider: ::sys::CGDataProviderRef) -> ::sys::CGFontRef;
    fn CGFontCreateWithFontName(name: CFStringRef) -> ::sys::CGFontRef;
    fn CGFontCreateCopyWithVariations(font: ::sys::CGFontRef, vars: CFDictionaryRef) -> ::sys::CGFontRef;
    fn CGFontGetTypeID() -> CFTypeID;

    fn CGFontCopyPostScriptName(font: ::sys::CGFontRef) -> CFStringRef;

    // These do the same thing as CFRetain/CFRelease, except
    // gracefully handle a NULL argument. We don't use them.
    //fn CGFontRetain(font: ::sys::CGFontRef);
    //fn CGFontRelease(font: ::sys::CGFontRef);

    fn CGFontGetGlyphBBoxes(font: ::sys::CGFontRef,
                            glyphs: *const CGGlyph,
                            count: size_t,
                            bboxes: *mut CGRect)
                            -> bool;
    fn CGFontGetGlyphAdvances(font: ::sys::CGFontRef,
                              glyphs: *const CGGlyph,
                              count: size_t,
                              advances: *mut c_int)
                              -> bool;
    fn CGFontGetUnitsPerEm(font: ::sys::CGFontRef) -> c_int;

    fn CGFontCopyTableTags(font: ::sys::CGFontRef) -> CFArrayRef;
    fn CGFontCopyTableForTag(font: ::sys::CGFontRef, tag: u32) -> CFDataRef;
}
