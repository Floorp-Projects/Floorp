// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#[cfg(feature="servo")]
extern crate geckoservo;

extern crate mp4parse_capi;
extern crate nsstring;
extern crate rust_url_capi;
#[cfg(feature = "quantum_render")]
extern crate webrender_bindings;

use std::ffi::CStr;
use std::os::raw::c_char;

/// Used to implement `nsIDebug2::RustPanic` for testing purposes.
#[no_mangle]
pub extern "C" fn intentional_panic(message: *const c_char) {
    panic!("{}", unsafe { CStr::from_ptr(message) }.to_string_lossy());
}
