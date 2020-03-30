// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::ffi::CStr;
use std::os::raw::c_char;

use nserror::{nsresult, NS_OK};

use glean_core::Configuration;

#[no_mangle]
pub unsafe extern "C" fn fog_init(
    app_build: *const c_char,
    app_display_version: *const c_char,
    channel: *const c_char,
) -> nsresult {
    log::debug!("Initializing FOG.");

    let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
    let data_path = "/tmp".to_string(); // need to pass in something
    let configuration = Configuration {
        upload_enabled,
        data_path,
        application_id: "org-mozilla-firefox".to_string(),
        max_events: None,
        delay_ping_lifetime_io: false,
    };

    log::debug!("Configuration: {:#?}", configuration);

    NS_OK
}
