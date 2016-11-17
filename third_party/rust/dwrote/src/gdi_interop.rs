/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ptr;
use std::cell::UnsafeCell;

use comptr::ComPtr;
use winapi;
use super::{DWriteFactory, BitmapRenderTarget};

#[derive(Debug)]
pub struct GdiInterop {
    native: UnsafeCell<ComPtr<winapi::IDWriteGdiInterop>>,
}

impl GdiInterop {
    pub fn create() -> GdiInterop {
        unsafe {
            let mut native: ComPtr<winapi::IDWriteGdiInterop> = ComPtr::new();
            let hr = (*DWriteFactory()).GetGdiInterop(native.getter_addrefs());
            assert!(hr == 0);
            GdiInterop::take(native)
        }
    }

    pub fn take(native: ComPtr<winapi::IDWriteGdiInterop>) -> GdiInterop {
        GdiInterop {
            native: UnsafeCell::new(native),
        }
    }

    pub fn create_bitmap_render_target(&self, width: u32, height: u32) -> BitmapRenderTarget {
        unsafe {
            let mut native: ComPtr<winapi::IDWriteBitmapRenderTarget> = ComPtr::new();
            let hr = (*self.native.get()).CreateBitmapRenderTarget(ptr::null_mut(),
                                                                   width, height,
                                                                   native.getter_addrefs());
            assert!(hr == 0);
            BitmapRenderTarget::take(native)
        }
    }
}
