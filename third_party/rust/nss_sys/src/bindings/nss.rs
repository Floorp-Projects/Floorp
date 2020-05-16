/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::*;
use std::os::raw::c_char;

extern "C" {
    pub fn NSS_VersionCheck(importedVersion: *const c_char) -> PRBool;
    pub fn NSS_InitContext(
        configdir: *const c_char,
        certPrefix: *const c_char,
        keyPrefix: *const c_char,
        secmodName: *const c_char,
        initParams: *mut NSSInitParameters,
        flags: PRUint32,
    ) -> *mut NSSInitContext;
}

pub const NSS_INIT_READONLY: u32 = 1;
pub const NSS_INIT_NOCERTDB: u32 = 2;
pub const NSS_INIT_NOMODDB: u32 = 4;
pub const NSS_INIT_FORCEOPEN: u32 = 8;
pub const NSS_INIT_OPTIMIZESPACE: u32 = 32;

// Opaque types
pub type NSSInitContext = u8;
pub type NSSInitParameters = [u64; 10usize];
