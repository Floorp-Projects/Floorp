// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use backend::PulseContext;
use cubeb_backend::{capi, ffi};
use std::os::raw::{c_char, c_int};

/// Entry point from C code.
#[no_mangle]
pub unsafe extern "C" fn pulse_rust_init(
    c: *mut *mut ffi::cubeb,
    context_name: *const c_char,
) -> c_int {
    capi::capi_init::<PulseContext>(c, context_name)
}
