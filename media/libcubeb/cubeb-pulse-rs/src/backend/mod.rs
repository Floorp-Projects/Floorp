// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

mod context;
mod cork_state;
mod stream;

use std::os::raw::c_char;
use std::ffi::CStr;

pub type Result<T> = ::std::result::Result<T, i32>;

pub use self::context::Context;
pub use self::stream::Device;
pub use self::stream::Stream;

// helper to convert *const c_char to Option<CStr>
fn try_cstr_from<'str>(s: *const c_char) -> Option<&'str CStr> {
    if s.is_null() { None } else { Some(unsafe { CStr::from_ptr(s) }) }
}
