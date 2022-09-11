// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::os::raw::{c_char, c_int, c_void};

cubeb_enum! {
    pub enum cubeb_log_level {
        CUBEB_LOG_DISABLED = 0,
        CUBEB_LOG_NORMAL = 1,
        CUBEB_LOG_VERBOSE = 2,
    }
}

pub type cubeb_log_callback = Option<unsafe extern "C" fn(*const c_char, ...)>;

extern "C" {
    pub fn cubeb_set_log_callback(
        log_level: cubeb_log_level,
        log_callback: cubeb_log_callback,
    ) -> c_int;

    pub static g_cubeb_log_level: cubeb_log_level;
    pub static g_cubeb_log_callback: cubeb_log_callback;

    pub fn cubeb_async_log_reset_threads(_: c_void) -> c_void;
    pub fn cubeb_async_log(msg: *const c_char, ...) -> c_void;
}
