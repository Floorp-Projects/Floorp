/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use comptr::ComPtr;
use winapi;
use winapi::FALSE;
use std::cell::UnsafeCell;

use super::{DWriteFactory, FontFamily, Font, FontFace, FontDescriptor};
use helpers::*;

#[derive(Debug)]
pub struct FontCollectionFamilyIterator {
    collection: ComPtr<winapi::IDWriteFontCollection>,
    curr: u32,
    count: u32,
}

impl Iterator for FontCollectionFamilyIterator {
    type Item = FontFamily;
    fn next(&mut self) -> Option<FontFamily> {
        if self.curr == self.count {
            return None;
        }

        unsafe {
            let mut family: ComPtr<winapi::IDWriteFontFamily> = ComPtr::new();
            let hr = self.collection.GetFontFamily(self.curr, family.getter_addrefs());
            assert!(hr == 0);
            self.curr += 1;
            Some(FontFamily::take(family))
        }
    }
}

pub struct FontCollection {
    native: UnsafeCell<ComPtr<winapi::IDWriteFontCollection>>,
}

impl FontCollection {
    pub fn system() -> FontCollection {
        unsafe {
            let mut native: ComPtr<winapi::IDWriteFontCollection> = ComPtr::new();
            let hr = (*DWriteFactory()).GetSystemFontCollection(native.getter_addrefs(), FALSE);
            assert!(hr == 0);

            FontCollection {
                native: UnsafeCell::new(native)
            }
        }
    }

    pub fn take(native: ComPtr<winapi::IDWriteFontCollection>) -> FontCollection {
        FontCollection {
            native: UnsafeCell::new(native)
        }
    }

    pub unsafe fn as_ptr(&self) -> *mut winapi::IDWriteFontCollection {
        (*self.native.get()).as_ptr()
    }

    
    pub fn families_iter(&self) -> FontCollectionFamilyIterator {
        unsafe {
            FontCollectionFamilyIterator {
                collection: (*self.native.get()).clone(),
                curr: 0,
                count: (*self.native.get()).GetFontFamilyCount(),
            }
        }
    }

    pub fn get_font_family_count(&self) -> u32 {
        unsafe {
            (*self.native.get()).GetFontFamilyCount()
        }
    }

    pub fn get_font_family(&self, index: u32) -> FontFamily {
        unsafe {
            let mut family: ComPtr<winapi::IDWriteFontFamily> = ComPtr::new();
            let hr = (*self.native.get()).GetFontFamily(index, family.getter_addrefs());
            assert!(hr == 0);
            FontFamily::take(family)
        }
    }

    // Find a font matching the given font descriptor in this
    // font collection.  
    pub fn get_font_from_descriptor(&self, desc: &FontDescriptor) -> Option<Font> {
        if let Some(family) = self.get_font_family_by_name(&desc.family_name) {
            let font = family.get_first_matching_font(desc.weight, desc.stretch, desc.style);
            // Exact matches only here
            if font.weight() == desc.weight &&
                font.stretch() == desc.stretch &&
                font.style() == desc.style
            {
                return Some(font);
            }
        }

        None
    }

    pub fn get_font_from_face(&self, face: &FontFace) -> Option<Font> {
        unsafe {
            let mut font: ComPtr<winapi::IDWriteFont> = ComPtr::new();
            let hr = (*self.native.get()).GetFontFromFontFace(face.as_ptr(), font.getter_addrefs());
            if hr != 0 {
                return None;
            }
            Some(Font::take(font))
        }
    }

    pub fn get_font_family_by_name(&self, family_name: &str) -> Option<FontFamily> {
        unsafe {
            let mut index: u32 = 0;
            let mut exists: winapi::BOOL = winapi::FALSE;
            let hr = (*self.native.get()).FindFamilyName(family_name.to_wide_null().as_ptr(), &mut index, &mut exists);
            assert!(hr == 0);
            if exists == winapi::FALSE {
                return None;
            }

            let mut family: ComPtr<winapi::IDWriteFontFamily> = ComPtr::new();
            let hr = (*self.native.get()).GetFontFamily(index, family.getter_addrefs());
            assert!(hr == 0);

            Some(FontFamily::take(family))
        }
    }
}
