// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

// Minimal null-terminated wide string support from wio.

use std::ffi::OsStr;
use std::os::windows::ffi::OsStrExt;

pub trait ToWideNull {
    fn to_wide_null(&self) -> Vec<u16>;
}

impl<T: AsRef<OsStr>> ToWideNull for T {
    fn to_wide_null(&self) -> Vec<u16> {
        self.as_ref().encode_wide().chain(Some(0)).collect()
    }
}
