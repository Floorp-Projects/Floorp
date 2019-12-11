// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

mod context;
mod cork_state;
mod stream;
mod intern;

pub use self::context::PulseContext;
use self::intern::Intern;
pub use self::stream::Device;
pub use self::stream::PulseStream;
use std::ffi::CStr;
use std::os::raw::c_char;

// helper to convert *const c_char to Option<CStr>
fn try_cstr_from<'str>(s: *const c_char) -> Option<&'str CStr> {
    if s.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(s) })
    }
}
