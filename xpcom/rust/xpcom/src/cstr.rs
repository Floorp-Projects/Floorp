/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use libc::c_char;

/// Creates a static C string by adding a nul terminator to `$str`.
#[macro_export]
macro_rules! c_str {
    ($str:expr) => {
        $crate::NulTerminatedCStr(concat!($str, '\0').as_bytes())
    };
}

/// A nul-terminated, static C string. This should only be created via the
/// `c_str` macro.
pub struct NulTerminatedCStr(pub &'static [u8]);

impl NulTerminatedCStr {
    /// Returns a raw pointer to this string, asserting that it's
    /// nul-terminated. This pointer can be passed to any C function expecting a
    /// `const char*`, or any XPIDL method expecting an `in string`.
    #[inline]
    pub fn as_ptr(&self) -> *const c_char {
        assert_eq!(self.0.last(), Some(&0), "C strings must be nul-terminated");
        self.0 as *const [u8] as *const c_char
    }
}
