/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::cell::UnsafeCell;

use comptr::ComPtr;
use winapi;
use super::DWriteFactory;

#[derive(Debug)]
pub struct RenderingParams {
    native: UnsafeCell<ComPtr<winapi::IDWriteRenderingParams>>,
}

impl RenderingParams {
    pub fn create_for_primary_monitor() -> RenderingParams {
        unsafe {
            let mut native: ComPtr<winapi::IDWriteRenderingParams> = ComPtr::new();
            let hr = (*DWriteFactory()).CreateRenderingParams(native.getter_addrefs());
            assert!(hr == 0);
            RenderingParams::take(native)
        }
    }

    pub fn take(native: ComPtr<winapi::IDWriteRenderingParams>) -> RenderingParams {
        RenderingParams {
            native: UnsafeCell::new(native),
        }
    }

    pub unsafe fn as_ptr(&self) -> *mut winapi::IDWriteRenderingParams {
        (*self.native.get()).as_ptr()
    }
}
