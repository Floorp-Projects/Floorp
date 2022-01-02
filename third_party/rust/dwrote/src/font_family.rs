/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::cell::UnsafeCell;
use std::ptr;
use winapi::um::dwrite::IDWriteLocalizedStrings;
use winapi::um::dwrite::{IDWriteFont, IDWriteFontCollection, IDWriteFontFamily};
use wio::com::ComPtr;

use super::*;
use helpers::*;

pub struct FontFamily {
    native: UnsafeCell<ComPtr<IDWriteFontFamily>>,
}

impl FontFamily {
    pub fn take(native: ComPtr<IDWriteFontFamily>) -> FontFamily {
        FontFamily {
            native: UnsafeCell::new(native),
        }
    }

    pub unsafe fn as_ptr(&self) -> *mut IDWriteFontFamily {
        (*self.native.get()).as_raw()
    }

    pub fn name(&self) -> String {
        unsafe {
            let mut family_names: *mut IDWriteLocalizedStrings = ptr::null_mut();
            let hr = (*self.native.get()).GetFamilyNames(&mut family_names);
            assert!(hr == 0);

            get_locale_string(&mut ComPtr::from_raw(family_names))
        }
    }

    pub fn get_first_matching_font(
        &self,
        weight: FontWeight,
        stretch: FontStretch,
        style: FontStyle,
    ) -> Font {
        unsafe {
            let mut font: *mut IDWriteFont = ptr::null_mut();
            let hr = (*self.native.get()).GetFirstMatchingFont(
                weight.t(),
                stretch.t(),
                style.t(),
                &mut font,
            );
            assert!(hr == 0);
            Font::take(ComPtr::from_raw(font))
        }
    }

    pub fn get_font_collection(&self) -> FontCollection {
        unsafe {
            let mut collection: *mut IDWriteFontCollection = ptr::null_mut();
            let hr = (*self.native.get()).GetFontCollection(&mut collection);
            assert!(hr == 0);
            FontCollection::take(ComPtr::from_raw(collection))
        }
    }

    pub fn get_font_count(&self) -> u32 {
        unsafe { (*self.native.get()).GetFontCount() }
    }

    pub fn get_font(&self, index: u32) -> Font {
        unsafe {
            let mut font: *mut IDWriteFont = ptr::null_mut();
            let hr = (*self.native.get()).GetFont(index, &mut font);
            assert!(hr == 0);
            Font::take(ComPtr::from_raw(font))
        }
    }
}
