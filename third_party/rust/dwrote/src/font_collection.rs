/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use comptr::ComPtr;
use winapi::um::dwrite::{IDWriteFontFamily, IDWriteFont, IDWriteFontCollection};
use winapi::um::dwrite::{IDWriteFontCollectionLoader};
use winapi::shared::minwindef::{BOOL, FALSE};
use winapi::shared::winerror::S_OK;
use std::cell::UnsafeCell;
use std::mem;
use std::ptr;
use std::sync::atomic::{ATOMIC_USIZE_INIT, AtomicUsize, Ordering};

use super::{CustomFontCollectionLoaderImpl, DWriteFactory, FontFamily, Font};
use super::{FontFace, FontDescriptor};
use helpers::*;
use com_helpers::Com;

static NEXT_ID: AtomicUsize = ATOMIC_USIZE_INIT;

pub struct FontCollectionFamilyIterator {
    collection: ComPtr<IDWriteFontCollection>,
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
            let mut family: ComPtr<IDWriteFontFamily> = ComPtr::new();
            let hr = self.collection.GetFontFamily(self.curr, family.getter_addrefs());
            assert!(hr == 0);
            self.curr += 1;
            Some(FontFamily::take(family))
        }
    }
}

pub struct FontCollection {
    native: UnsafeCell<ComPtr<IDWriteFontCollection>>,
}

impl FontCollection {
    pub fn system() -> FontCollection {
        unsafe {
            let mut native: ComPtr<IDWriteFontCollection> = ComPtr::new();
            let hr = (*DWriteFactory()).GetSystemFontCollection(native.getter_addrefs(), FALSE);
            assert!(hr == 0);

            FontCollection {
                native: UnsafeCell::new(native)
            }
        }
    }

    pub fn take(native: ComPtr<IDWriteFontCollection>) -> FontCollection {
        FontCollection {
            native: UnsafeCell::new(native)
        }
    }

    pub fn from_loader(collection_loader: ComPtr<IDWriteFontCollectionLoader>) -> FontCollection {
        unsafe {
            let factory = DWriteFactory();
            assert_eq!((*factory).RegisterFontCollectionLoader(collection_loader.clone().forget()),
                       S_OK);
            let mut collection: ComPtr<IDWriteFontCollection> = ComPtr::new();
            let id = NEXT_ID.fetch_add(1, Ordering::SeqCst);
            assert_eq!((*factory).CreateCustomFontCollection(
                            collection_loader.clone().forget(),
                            &id as *const usize as *const _,
                            mem::size_of::<AtomicUsize>() as u32,
                            collection.getter_addrefs()),
                       S_OK);
            FontCollection::take(collection)
        }
    }

    pub unsafe fn as_ptr(&self) -> *mut IDWriteFontCollection {
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
            let mut family: ComPtr<IDWriteFontFamily> = ComPtr::new();
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
            let mut font: ComPtr<IDWriteFont> = ComPtr::new();
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
            let mut exists: BOOL = FALSE;
            let hr = (*self.native.get()).FindFamilyName(family_name.to_wide_null().as_ptr(), &mut index, &mut exists);
            assert!(hr == 0);
            if exists == FALSE {
                return None;
            }

            let mut family: ComPtr<IDWriteFontFamily> = ComPtr::new();
            let hr = (*self.native.get()).GetFontFamily(index, family.getter_addrefs());
            assert!(hr == 0);

            Some(FontFamily::take(family))
        }
    }
}
