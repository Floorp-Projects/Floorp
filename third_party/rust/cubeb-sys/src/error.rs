// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::os::raw::c_int;

pub const CUBEB_OK: c_int = 0;
pub const CUBEB_ERROR: c_int = -1;
pub const CUBEB_ERROR_INVALID_FORMAT: c_int = -2;
pub const CUBEB_ERROR_INVALID_PARAMETER: c_int = -3;
pub const CUBEB_ERROR_NOT_SUPPORTED: c_int = -4;
pub const CUBEB_ERROR_DEVICE_UNAVAILABLE: c_int = -5;
