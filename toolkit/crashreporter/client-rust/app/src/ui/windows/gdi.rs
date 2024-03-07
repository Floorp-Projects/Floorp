/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! GDI helpers.

use windows_sys::Win32::{
    Foundation::HWND,
    Graphics::Gdi::{self, GDI_ERROR, HDC, HGDIOBJ},
};

/// A GDI drawing context.
pub struct DC {
    hwnd: HWND,
    hdc: HDC,
}

impl DC {
    /// Create a new DC.
    pub fn new(hwnd: HWND) -> Option<Self> {
        let hdc = unsafe { Gdi::GetDC(hwnd) };
        (hdc != 0).then_some(DC { hwnd, hdc })
    }

    /// Call the given function with a gdi object selected.
    pub fn with_object_selected<R>(&self, object: HGDIOBJ, f: impl FnOnce(HDC) -> R) -> Option<R> {
        let old_object = unsafe { Gdi::SelectObject(self.hdc, object) };
        if old_object == 0 || old_object == GDI_ERROR as isize {
            return None;
        }
        let ret = f(self.hdc);
        // The prior object must be selected before releasing the DC. Ignore errors; this is
        // best-effort.
        unsafe { Gdi::SelectObject(self.hdc, old_object) };
        Some(ret)
    }
}

impl Drop for DC {
    fn drop(&mut self) {
        unsafe { Gdi::ReleaseDC(self.hwnd, self.hdc) };
    }
}
