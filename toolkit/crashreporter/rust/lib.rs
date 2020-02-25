/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate rustc_demangle;

use rustc_demangle::demangle;
use std::ffi::{CStr, CString};
use std::ptr;

/// Demangle `name` as a Rust symbol.
///
/// The resulting pointer should be freed with `free_demangled_name`.
#[no_mangle]
pub extern "C" fn rust_demangle(name: *const std::os::raw::c_char) -> *mut std::os::raw::c_char {
    let demangled = format!(
        "{:#}",
        demangle(&unsafe { CStr::from_ptr(name) }.to_string_lossy())
    );
    CString::new(demangled)
        .map(|s| s.into_raw())
        .unwrap_or(ptr::null_mut())
}

/// Free a string that was returned from `rust_demangle`.
#[no_mangle]
pub extern "C" fn free_rust_demangled_name(demangled: *mut std::os::raw::c_char) {
    if demangled != ptr::null_mut() {
        // Just take ownership here.
        unsafe { CString::from_raw(demangled) };
    }
}
