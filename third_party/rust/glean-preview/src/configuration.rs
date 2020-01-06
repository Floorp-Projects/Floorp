// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/// The Glean configuration.
///
/// Optional values will be filled in with default values.
#[derive(Debug, Clone)]
pub struct Configuration {
    /// Whether upload should be enabled.
    pub upload_enabled: bool,
    /// Path to a directory to store all data in.
    pub data_path: String,
    /// The application ID (will be sanitized during initialization).
    pub application_id: String,
    /// The maximum number of events to store before sending a ping containing events.
    pub max_events: Option<usize>,
    /// Whether Glean should delay persistence of data from metrics with ping lifetime.
    pub delay_ping_lifetime_io: bool,
    /// The release channel the application is on, if known.
    pub channel: Option<String>,
}
