/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use windows_sys::Win32::{Foundation::S_OK, Graphics::Gdi, UI::Controls};

/// Windows font handle (`HFONT`).
pub struct Font(Gdi::HFONT);

impl Font {
    /// Get the system theme caption font.
    ///
    /// Panics if the font cannot be retrieved.
    pub fn caption() -> Self {
        unsafe {
            let mut font = std::mem::zeroed::<Gdi::LOGFONTW>();
            success!(hresult
                Controls::GetThemeSysFont(0, Controls::TMT_CAPTIONFONT as i32, &mut font)
            );
            Font(success!(pointer Gdi::CreateFontIndirectW(&font)))
        }
    }

    /// Get the system theme bold caption font.
    ///
    /// Returns `None` if the font cannot be retrieved.
    pub fn caption_bold() -> Option<Self> {
        unsafe {
            let mut font = std::mem::zeroed::<Gdi::LOGFONTW>();
            if Controls::GetThemeSysFont(0, Controls::TMT_CAPTIONFONT as i32, &mut font) != S_OK {
                return None;
            }
            font.lfWeight = Gdi::FW_BOLD as i32;

            let ptr = Gdi::CreateFontIndirectW(&font);
            if ptr == 0 {
                return None;
            }
            Some(Font(ptr))
        }
    }
}

impl std::ops::Deref for Font {
    type Target = Gdi::HFONT;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl Drop for Font {
    fn drop(&mut self) {
        unsafe { Gdi::DeleteObject(self.0 as _) };
    }
}
