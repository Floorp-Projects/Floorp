// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::net::PingUploader;

use std::path::PathBuf;

/// The default server pings are sent to.
pub(crate) const DEFAULT_GLEAN_ENDPOINT: &str = "https://incoming.telemetry.mozilla.org";

/// The Glean configuration.
///
/// Optional values will be filled in with default values.
#[derive(Debug)]
pub struct Configuration {
    /// Whether upload should be enabled.
    pub upload_enabled: bool,
    /// Path to a directory to store all data in.
    pub data_path: PathBuf,
    /// The application ID (will be sanitized during initialization).
    pub application_id: String,
    /// The maximum number of events to store before sending a ping containing events.
    pub max_events: Option<usize>,
    /// Whether Glean should delay persistence of data from metrics with ping lifetime.
    pub delay_ping_lifetime_io: bool,
    /// The release channel the application is on, if known.
    pub channel: Option<String>,
    /// The server pings are sent to.
    pub server_endpoint: Option<String>,
    /// The instance of the uploader used to send pings.
    pub uploader: Option<Box<dyn PingUploader + 'static>>,
    /// Whether Glean should schedule "metrics" pings for you.
    pub use_core_mps: bool,
}
