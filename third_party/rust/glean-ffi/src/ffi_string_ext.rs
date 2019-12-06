// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use ffi_support::FfiStr;
use glean_core::Result;

pub trait FallibleToString {
    /// Convert to a string or fail with an appropriate error.
    fn to_string_fallible(&self) -> Result<String>;
}

/// This allows to convert a `FfiStr` (effectively a null-terminated C-like string)
/// to a Rust string, failing when the pointer is null or contains invalid UTF-8 characters.
impl<'a> FallibleToString for FfiStr<'a> {
    fn to_string_fallible(&self) -> Result<String> {
        self.as_opt_str()
            .map(|s| s.to_string())
            .ok_or_else(glean_core::Error::utf8_error)
    }
}
