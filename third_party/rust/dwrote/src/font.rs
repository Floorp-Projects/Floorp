/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::cell::UnsafeCell;

use comptr::ComPtr;
use winapi::shared::minwindef::{BOOL, FALSE, TRUE};
use winapi::shared::winerror::S_OK;
use winapi::um::dwrite::{DWRITE_FONT_METRICS, DWRITE_INFORMATIONAL_STRING_FULL_NAME, DWRITE_INFORMATIONAL_STRING_ID};
use winapi::um::dwrite::{DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_CID_NAME};
use winapi::um::dwrite::{DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, IDWriteFontFace};
use winapi::um::dwrite::{IDWriteLocalizedStrings, IDWriteFont, IDWriteFontFamily};
use winapi::um::dwrite_1::IDWriteFont1;
use std::mem;

use super::*;
use helpers::*;

pub struct Font {
    native: UnsafeCell<ComPtr<IDWriteFont>>,
}

impl Font {
    pub fn take(native: ComPtr<IDWriteFont>) -> Font {
        Font {
            native: UnsafeCell::new(native),
        }
    }

    pub unsafe fn as_ptr(&self) -> *mut IDWriteFont {
        (*self.native.get()).as_ptr()
    }

    pub fn to_descriptor(&self) -> FontDescriptor {
        FontDescriptor {
            family_name: self.family_name(),
            stretch: self.stretch(),
            style: self.style(),
            weight: self.weight(),
        }
    }

    pub fn stretch(&self) -> FontStretch {
        unsafe {
            mem::transmute::<u32, FontStretch>((*self.native.get()).GetStretch())
        }
    }

    pub fn style(&self) -> FontStyle {
        unsafe {
            mem::transmute::<u32, FontStyle>((*self.native.get()).GetStyle())
        }
    }

    pub fn weight(&self) -> FontWeight {
        unsafe {
            FontWeight::from_u32((*self.native.get()).GetWeight())
        }
    }

    pub fn is_monospace(&self) -> Option<bool> {
        unsafe {
            let font1: Option<ComPtr<IDWriteFont1>> = (*self.native.get()).query_interface(&IDWriteFont1::uuidof());
            font1.map(|font| font.IsMonospacedFont() == TRUE)
        }
    }

    pub fn simulations(&self) -> FontSimulations {
        unsafe {
            mem::transmute::<u32, FontSimulations>((*self.native.get()).GetSimulations())
        }
    }

    pub fn family_name(&self) -> String {
        unsafe {
            let mut family: ComPtr<IDWriteFontFamily> = ComPtr::new();
            let hr = (*self.native.get()).GetFontFamily(family.getter_addrefs());
            assert!(hr == 0);

            FontFamily::take(family).name()
        }
    }

    pub fn face_name(&self) -> String {
        unsafe {
            let mut names: ComPtr<IDWriteLocalizedStrings> = ComPtr::new();
            let hr = (*self.native.get()).GetFaceNames(names.getter_addrefs());
            assert!(hr == 0);

            get_locale_string(&mut names)
        }
    }

    pub fn informational_string(&self, id: InformationalStringId) -> Option<String> {
        unsafe {
            let mut names: ComPtr<IDWriteLocalizedStrings> = ComPtr::new();
            let mut exists = FALSE;
            let id = id as DWRITE_INFORMATIONAL_STRING_ID;
            let hr = (*self.native.get()).GetInformationalStrings(id,
                                                                  names.getter_addrefs(),
                                                                  &mut exists);
            assert!(hr == S_OK);
            if exists == TRUE {
                Some(get_locale_string(&mut names))
            } else {
                None
            }
        }
    }

    pub fn create_font_face(&self) -> FontFace {
        // FIXME create_font_face should cache the FontFace and return it,
        // there's a 1:1 relationship
        unsafe {
            let mut face: ComPtr<IDWriteFontFace> = ComPtr::new();
            let hr = (*self.native.get()).CreateFontFace(face.getter_addrefs());
            assert!(hr == 0);
            FontFace::take(face)
        }
    }

    pub fn metrics(&self) -> DWRITE_FONT_METRICS {
        unsafe {
            let mut metrics = mem::zeroed();
            (*self.native.get()).GetMetrics(&mut metrics);
            metrics
        }
    }
}

impl Clone for Font {
    fn clone(&self) -> Font {
        unsafe {
            Font {
                native: UnsafeCell::new((*self.native.get()).clone()),
            }
        }
    }
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum InformationalStringId {
    FullName = DWRITE_INFORMATIONAL_STRING_FULL_NAME,
    PostscriptName = DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME,
    PostscriptCidName = DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_CID_NAME,
}
