// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::os::raw::c_char;

use nserror::{nsresult, NS_OK};

#[no_mangle]
pub unsafe extern "C" fn fog_init(
    app_build: *const c_char,
    app_display_version: *const c_char,
    channel: *const c_char,
) -> nsresult {
    log::debug!("Initializing FOG.");

    NS_OK
}
