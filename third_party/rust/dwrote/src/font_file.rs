/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::slice;
use std::ptr;
use std::cell::UnsafeCell;

use comptr::ComPtr;

use winapi;

use font_file_loader_impl::DataFontHelper;
use font_face::FontFace;
use super::DWriteFactory;

#[derive(Debug)]
pub struct FontFile {
    native: UnsafeCell<ComPtr<winapi::IDWriteFontFile>>,
    data_key: usize,
    face_type: winapi::DWRITE_FONT_FACE_TYPE,
}

impl FontFile {
    pub fn new_from_data(data: &[u8]) -> Option<FontFile> {
        let (font_file, key) = DataFontHelper::register_font_data(data);

        let mut ff = FontFile {
            native: UnsafeCell::new(font_file),
            data_key: key,
            face_type: winapi::DWRITE_FONT_FACE_TYPE_UNKNOWN,
        };

        if ff.analyze() == false {
            DataFontHelper::unregister_font_data(key);
            return None;
        }

        Some(ff)
    }

    fn analyze(&mut self) -> bool {
        let mut face_type = winapi::DWRITE_FONT_FACE_TYPE_UNKNOWN;
        unsafe {
            let mut supported = 0;
            let mut _file_type = winapi::DWRITE_FONT_FILE_TYPE_UNKNOWN;
            let mut _num_faces = 0;

            let hr = (*self.as_ptr()).Analyze(&mut supported, &mut _file_type, &mut face_type, &mut _num_faces);
            if hr != 0 || supported == 0 {
                return false;
            }
        }
        self.face_type = face_type;
        true
    }

    pub fn take(native: ComPtr<winapi::IDWriteFontFile>) -> FontFile {
        let mut ff = FontFile {
            native: UnsafeCell::new(native),
            data_key: 0,
            face_type: winapi::DWRITE_FONT_FACE_TYPE_UNKNOWN,
        };
        ff.analyze();
        ff
    }

    pub fn data_key(&self) -> Option<usize> {
        if self.data_key != 0 {
            Some(self.data_key)
        } else {
            None
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

    pub fn create_face(&self, face_index: u32, simulations: winapi::DWRITE_FONT_SIMULATIONS) -> FontFace {
        unsafe {
            let mut face: ComPtr<winapi::IDWriteFontFace> = ComPtr::new();
            let ptr = self.as_ptr();
            let hr = (*DWriteFactory()).CreateFontFace(self.face_type, 1, &ptr,
                                                       face_index, simulations, face.getter_addrefs());
            assert!(hr == 0);
            FontFace::take(face)
        }
    }
}
