/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::slice;
use std::ptr;
use std::cell::UnsafeCell;

use comptr::ComPtr;

use winapi;

#[derive(Debug)]
pub struct FontFile {
    native: UnsafeCell<ComPtr<winapi::IDWriteFontFile>>,
}

impl FontFile {
    pub fn take(native: ComPtr<winapi::IDWriteFontFile>) -> FontFile {
        FontFile {
            native: UnsafeCell::new(native),
        }
    }

    pub unsafe fn as_ptr(&self) -> *mut winapi::IDWriteFontFile {
        (*self.native.get()).as_ptr()
    }

    // This is a helper to read the contents of this FontFile,
    // without requiring callers to deal with loaders, keys,
    // or streams.
    pub fn get_font_file_bytes(&self) -> Vec<u8> {
        unsafe {
            let mut ref_key: *const winapi::c_void = ptr::null();
            let mut ref_key_size: u32 = 0;
            let hr = (*self.native.get()).GetReferenceKey(&mut ref_key, &mut ref_key_size);
            assert!(hr == 0);

            let mut loader: ComPtr<winapi::IDWriteFontFileLoader> = ComPtr::new();
            let hr = (*self.native.get()).GetLoader(loader.getter_addrefs());
            assert!(hr == 0);

            let mut stream: ComPtr<winapi::IDWriteFontFileStream> = ComPtr::new();
            let hr = loader.CreateStreamFromKey(ref_key, ref_key_size, stream.getter_addrefs());
            assert!(hr == 0);

            let mut file_size: u64 = 0;
            let hr = stream.GetFileSize(&mut file_size);
            assert!(hr == 0);

            let mut fragment_start: *const winapi::c_void = ptr::null();
            let mut fragment_context: *mut winapi::c_void = ptr::null_mut();
            let hr = stream.ReadFileFragment(&mut fragment_start, 0, file_size, &mut fragment_context);
            assert!(hr == 0);

            let in_ptr = slice::from_raw_parts(fragment_start as *const u8, file_size as usize);
            let bytes = in_ptr.to_vec();

            stream.ReleaseFileFragment(fragment_context);

            bytes
        }
    }

}
