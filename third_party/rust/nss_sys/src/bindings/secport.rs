/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::*;
use std::os::raw::{c_int, c_void};

pub type size_t = usize;

extern "C" {
    pub fn PORT_FreeArena(arena: *mut PLArenaPool, zero: PRBool);
    pub fn NSS_SecureMemcmp(a: *const c_void, b: *const c_void, n: size_t) -> c_int;
}
