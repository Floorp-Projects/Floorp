// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::os::raw::{c_float, c_short};

extern "C" {
    pub fn cubeb_pan_stereo_buffer_float(buf: *mut c_float, frames: u32, pan: c_float);
    pub fn cubeb_pan_stereo_buffer_int(buf: *mut c_short, frames: u32, pan: c_float);
}
