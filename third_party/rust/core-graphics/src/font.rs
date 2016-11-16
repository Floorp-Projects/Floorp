// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core_foundation::base::{CFRelease, CFRetain, CFTypeID, CFTypeRef, TCFType};
use core_foundation::string::{CFString, CFStringRef};
use data_provider::{CGDataProvider, CGDataProviderRef};

use libc;
use serde::de::{self, Deserialize, Deserializer};
use serde::ser::{Serialize, Serializer};
use std::mem;
use std::ptr;

pub type CGGlyph = libc::c_ushort;

#[repr(C)]
pub struct __CGFont;

pub type CGFontRef = *const __CGFont;

pub struct CGFont {
    obj: CGFontRef,
}

unsafe impl Send for CGFont {}
unsafe impl Sync for CGFont {}

impl Serialize for CGFont {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error> where S: Serializer {
        let postscript_name = self.postscript_name().to_string();
        postscript_name.serialize(serializer)
    }
}

impl Deserialize for CGFont {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: Deserializer {
        let postscript_name: String = try!(Deserialize::deserialize(deserializer));
        CGFont::from_name(&CFString::new(&*postscript_name)).map_err(|_| {
            de::Error::invalid_value("Couldn't find a font with that PostScript name!")
        })
    }
}

impl Clone for CGFont {
    #[inline]
    fn clone(&self) -> CGFont {
        unsafe {
            TCFType::wrap_under_get_rule(self.obj)
        }
    }
}

impl Drop for CGFont {
    fn drop(&mut self) {
        unsafe {
            let ptr = self.as_CFTypeRef();
            assert!(ptr != ptr::null());
            CFRelease(ptr);
        }
    }
}

impl TCFType<CGFontRef> for CGFont {
    #[inline]
    fn as_concrete_TypeRef(&self) -> CGFontRef {
        self.obj
    }

    #[inline]
    unsafe fn wrap_under_get_rule(reference: CGFontRef) -> CGFont {
        let reference: CGFontRef = mem::transmute(CFRetain(mem::transmute(reference)));
        TCFType::wrap_under_create_rule(reference)
    }

    #[inline]
    fn as_CFTypeRef(&self) -> CFTypeRef {
        unsafe {
            mem::transmute(self.as_concrete_TypeRef())
        }
    }

    #[inline]
    unsafe fn wrap_under_create_rule(obj: CGFontRef) -> CGFont {
        CGFont {
            obj: obj,
        }
    }

    #[inline]
    fn type_id() -> CFTypeID {
        unsafe {
            CGFontGetTypeID()
        }
    }
}

impl CGFont {
    pub fn from_data_provider(provider: CGDataProvider) -> Result<CGFont, ()> {
        unsafe {
            let font_ref = CGFontCreateWithDataProvider(provider.as_concrete_TypeRef());
            if font_ref != ptr::null() {
                Ok(TCFType::wrap_under_create_rule(font_ref))
            } else {
                Err(())
            }
        }
    }

    pub fn from_name(name: &CFString) -> Result<CGFont, ()> {
        unsafe {
            let font_ref = CGFontCreateWithFontName(name.as_concrete_TypeRef());
            if font_ref != ptr::null() {
                Ok(TCFType::wrap_under_create_rule(font_ref))
            } else {
                Err(())
            }
        }
    }

    pub fn postscript_name(&self) -> CFString {
        unsafe {
            let string_ref = CGFontCopyPostScriptName(self.obj);
            TCFType::wrap_under_create_rule(string_ref)
        }
    }
}

#[link(name = "ApplicationServices", kind = "framework")]
extern {
    // TODO: basically nothing has bindings (even commented-out) besides what we use.
    fn CGFontCreateWithDataProvider(provider: CGDataProviderRef) -> CGFontRef;
    fn CGFontCreateWithFontName(name: CFStringRef) -> CGFontRef;
    fn CGFontGetTypeID() -> CFTypeID;

    fn CGFontCopyPostScriptName(font: CGFontRef) -> CFStringRef;

    // These do the same thing as CFRetain/CFRelease, except
    // gracefully handle a NULL argument. We don't use them.
    //fn CGFontRetain(font: CGFontRef);
    //fn CGFontRelease(font: CGFontRef);
}
