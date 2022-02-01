// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use context::cubeb;
use std::os::raw::{c_long, c_void};
use stream::{cubeb_state, cubeb_stream};

pub type cubeb_data_callback = Option<
    unsafe extern "C" fn(
        *mut cubeb_stream,
        *mut c_void,
        *const c_void,
        *mut c_void,
        c_long,
    ) -> c_long,
>;
pub type cubeb_state_callback =
    Option<unsafe extern "C" fn(*mut cubeb_stream, *mut c_void, cubeb_state)>;
pub type cubeb_device_changed_callback = Option<unsafe extern "C" fn(*mut c_void)>;
pub type cubeb_device_collection_changed_callback =
    Option<unsafe extern "C" fn(*mut cubeb, *mut c_void)>;
