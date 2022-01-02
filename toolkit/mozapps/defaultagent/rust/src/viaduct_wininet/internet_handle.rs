// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

//! Wrapping and automatically closing Internet handles.  Copy-pasted from
//! [comedy-rs](https://github.com/agashlin/comedy-rs/blob/c244b91e9237c887f6a7bc6cd03db98b51966494/src/handle.rs).

use winapi::shared::minwindef::DWORD;
use winapi::shared::ntdef::NULL;
use winapi::um::errhandlingapi::GetLastError;
use winapi::um::wininet::{InternetCloseHandle, HINTERNET};

/// Check and automatically close a Windows `HINTERNET`.
#[repr(transparent)]
#[derive(Debug)]
pub struct InternetHandle(HINTERNET);

impl InternetHandle {
    /// Take ownership of a `HINTERNET`, which will be closed with `InternetCloseHandle` upon drop.
    /// Returns an error in case of `NULL`.
    ///
    /// # Safety
    ///
    /// `h` should be the only copy of the handle. `GetLastError()` is called to
    /// return an error, so the last Windows API called on this thread should have been
    /// what produced the invalid handle.
    pub unsafe fn new(h: HINTERNET) -> Result<InternetHandle, DWORD> {
        if h == NULL {
            Err(GetLastError())
        } else {
            Ok(InternetHandle(h))
        }
    }

    /// Obtains the raw `HINTERNET` without transferring ownership.
    ///
    /// Do __not__ close this handle because it is still owned by the `InternetHandle`.
    ///
    /// Do __not__ use this handle beyond the lifetime of the `InternetHandle`.
    pub fn as_raw(&self) -> HINTERNET {
        self.0
    }
}

impl Drop for InternetHandle {
    fn drop(&mut self) {
        unsafe {
            InternetCloseHandle(self.0);
        }
    }
}
