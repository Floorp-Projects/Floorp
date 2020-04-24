// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Firefox on Glean (FOG) is the name of the layer that integrates the [Glean SDK][glean-sdk] into Firefox Desktop.
//! It is currently being designed and implemented.
//!
//! The [Glean SDK][glean-sdk] is a data collection library built by Mozilla for use in its products.
//! Like [Telemetry][telemetry], it can be used to
//! (in accordance with our [Privacy Policy][privacy-policy])
//! send anonymous usage statistics to Mozilla in order to make better decisions.
//!
//! Documentation can be found online in the [Firefox Source Docs][docs].
//!
//! [glean-sdk]: https://github.com/mozilla/glean/
//! [book-of-glean]: https://mozilla.github.io/glean/book/index.html
//! [privacy-policy]: https://www.mozilla.org/privacy/
//! [docs]: https://firefox-source-docs.mozilla.org/toolkit/components/glean/

// No one is currently using the Glean SDK, so let's export it, so we know it gets
// compiled.
pub extern crate glean;

use std::ffi::CStr;
use std::os::raw::c_char;

use nserror::{nsresult, NS_OK};
use nsstring::nsACString;

use client_info::ClientInfo;
use glean_core::Configuration;

mod api;
mod client_info;
mod core_metrics;

/// Project FOG's entry point.
///
/// This assembles client information and the Glean configuration and then initializes the global
/// Glean instance.
#[no_mangle]
pub unsafe extern "C" fn fog_init(
    data_path: &nsACString,
    app_build: &nsACString,
    app_display_version: &nsACString,
    channel: *const c_char,
    os_version: &nsACString,
    architecture: &nsACString,
) -> nsresult {
    log::debug!("Initializing FOG.");

    let app_build = app_build.to_string();
    let app_display_version = app_display_version.to_string();

    let channel = CStr::from_ptr(channel);
    let channel = Some(channel.to_string_lossy().to_string());

    let os_version = os_version.to_string();
    let architecture = architecture.to_string();

    let client_info = ClientInfo {
        app_build,
        app_display_version,
        channel,
        os_version,
        architecture,
    };
    log::debug!("Client Info: {:#?}", client_info);

    let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
    let data_path = data_path.to_string();
    let configuration = Configuration {
        upload_enabled,
        data_path,
        application_id: "org-mozilla-firefox".to_string(),
        max_events: None,
        delay_ping_lifetime_io: false,
    };

    log::debug!("Configuration: {:#?}", configuration);

    if configuration.data_path.len() > 0 {
        if let Err(e) = api::initialize(configuration, client_info) {
            log::error!("Failed to init FOG due to {:?}", e);
        }
    }

    NS_OK
}
