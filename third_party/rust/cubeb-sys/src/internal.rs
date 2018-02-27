// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::{fmt, mem};
use std::os::raw::{c_char, c_uint};

#[repr(C)]
pub struct cubeb_layout_map {
    name: *const c_char,
    channels: c_uint,
    layout: cubeb_channel_layout,
}

impl Default for cubeb_layout_map {
    fn default() -> Self { unsafe { mem::zeroed() } }
}

// Explicit Debug impl to work around bug in ctest
impl fmt::Debug for cubeb_layout_map {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("cubeb_layout_map")
            .field("name", &self.name)
            .field("channels", &self.channels)
            .field("layout", &self.layout)
            .finish()
    }
}
