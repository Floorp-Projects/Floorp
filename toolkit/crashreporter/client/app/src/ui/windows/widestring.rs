/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ffi::OsStr;
use std::os::windows::ffi::OsStrExt;
use windows_sys::core::PCWSTR;

/// Windows wide strings.
///
/// These are utf16 encoded with a terminating null character (0).
pub struct WideString(Vec<u16>);

impl WideString {
    pub fn new(os_str: impl AsRef<OsStr>) -> Self {
        // TODO: doesn't check whether the OsStr contains a null character, which could be treated
        // as an error (as `CString::new` does).
        WideString(
            os_str
                .as_ref()
                .encode_wide()
                .chain(std::iter::once(0))
                // Remove unicode BIDI markers (from fluent) which aren't rendered correctly.
                .filter(|c| *c != 0x2068 && *c != 0x2069)
                .collect(),
        )
    }

    pub fn pcwstr(&self) -> PCWSTR {
        self.0.as_ptr()
    }

    pub fn as_slice(&self) -> &[u16] {
        &self.0
    }
}
