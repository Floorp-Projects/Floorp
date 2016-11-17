/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use winapi::IDWriteLocalizedStrings;
use winapi::wchar_t;
use comptr::ComPtr;
use winapi::winerror::S_OK;
use winapi::minwindef::{BOOL, FALSE};
use kernel32;
use std::ffi::{OsStr};
use std::os::windows::ffi::{OsStrExt};

lazy_static! {
    static ref SYSTEM_LOCALE: Vec<wchar_t> = {
        unsafe {
            let mut locale: Vec<wchar_t> = vec![0; 85];
            kernel32::GetUserDefaultLocaleName(locale.as_mut_ptr(), locale.len() as i32 - 1);
            locale
        }
    };
    static ref EN_US_LOCALE: Vec<wchar_t> = {
        OsStr::new("en-us").encode_wide().collect()
    };
}

pub fn get_locale_string(strings: &mut ComPtr<IDWriteLocalizedStrings>) -> String {
    unsafe {
        let mut index: u32 = 0;
        let mut exists: BOOL = FALSE;
        let hr = strings.FindLocaleName((*SYSTEM_LOCALE).as_ptr(), &mut index, &mut exists);
        if hr != S_OK || exists == FALSE {
            let hr = strings.FindLocaleName((*EN_US_LOCALE).as_ptr(), &mut index, &mut exists);
            if hr != S_OK || exists == FALSE {
                // Ultimately fall back to first locale on list
                index = 0;
            }
        }

        let mut length: u32 = 0;
        let hr = strings.GetStringLength(index, &mut length);
        assert!(hr == 0);

        let mut name: Vec<wchar_t> = Vec::with_capacity(length as usize + 1);
        let hr = strings.GetString(index, name.as_mut_ptr(), length + 1);
        assert!(hr == 0);
        name.set_len(length as usize);

        String::from_utf16(&name).ok().unwrap()
    }
}

// ToWide from https://github.com/retep998/wio-rs/blob/master/src/wide.rs

pub trait ToWide {
    fn to_wide(&self) -> Vec<u16>;
    fn to_wide_null(&self) -> Vec<u16>;
}

impl<T> ToWide for T where T: AsRef<OsStr> {
    fn to_wide(&self) -> Vec<u16> {
        self.as_ref().encode_wide().collect()
    }
    fn to_wide_null(&self) -> Vec<u16> {
        self.as_ref().encode_wide().chain(Some(0)).collect()
    }
}
