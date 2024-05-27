// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use log::LevelFilter;

use crate::net::PingUploader;

use std::collections::HashMap;
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
    /// The server pings are sent to.
    pub server_endpoint: Option<String>,
    /// The instance of the uploader used to send pings.
    pub uploader: Option<Box<dyn PingUploader + 'static>>,
    /// Whether Glean should schedule "metrics" pings for you.
    pub use_core_mps: bool,
    /// Whether Glean should limit its storage to only that of registered pings.
    /// Unless you know that all your and your libraries' pings are appropriately registered
    /// _before_ init, you shouldn't use this.
    pub trim_data_to_registered_pings: bool,
    /// The internal logging level.
    pub log_level: Option<LevelFilter>,
    /// The rate pings may be uploaded before they are throttled.
    pub rate_limit: Option<crate::PingRateLimit>,
    /// (Experimental) Whether to add a wallclock timestamp to all events.
    pub enable_event_timestamps: bool,
    /// An experimentation identifier derived by the application to be sent with all pings, it should
    /// be noted that this has an underlying StringMetric and so should conform to the limitations that
    /// StringMetric places on length, etc.
    pub experimentation_id: Option<String>,
    /// Whether to enable internal pings. Default: true
    pub enable_internal_pings: bool,
    /// A ping schedule map.
    /// Maps a ping name to a list of pings to schedule along with it.
    /// Only used if the ping's own ping schedule list is empty.
    pub ping_schedule: HashMap<String, Vec<String>>,
}

/// Configuration builder.
///
/// Let's you build a configuration from the required fields
/// and let you set optional fields individually.
#[derive(Debug)]
pub struct Builder {
    /// Required: Whether upload should be enabled.
    pub upload_enabled: bool,
    /// Required: Path to a directory to store all data in.
    pub data_path: PathBuf,
    /// Required: The application ID (will be sanitized during initialization).
    pub application_id: String,
    /// Optional: The maximum number of events to store before sending a ping containing events.
    /// Default: `None`
    pub max_events: Option<usize>,
    /// Optional: Whether Glean should delay persistence of data from metrics with ping lifetime.
    /// Default: `false`
    pub delay_ping_lifetime_io: bool,
    /// Optional: The server pings are sent to.
    /// Default: `None`
    pub server_endpoint: Option<String>,
    /// Optional: The instance of the uploader used to send pings.
    /// Default: `None`
    pub uploader: Option<Box<dyn PingUploader + 'static>>,
    /// Optional: Whether Glean should schedule "metrics" pings for you.
    /// Default: `false`
    pub use_core_mps: bool,
    /// Optional: Whether Glean should limit its storage to only that of registered pings.
    /// Unless you know that all your and your libraries' pings are appropriately registered
    /// _before_ init, you shouldn't use this.
    /// Default: `false`
    pub trim_data_to_registered_pings: bool,
    /// Optional: The internal logging level.
    /// Default: `None`
    pub log_level: Option<LevelFilter>,
    /// Optional: The internal ping upload rate limit.
    /// Default: `None`
    pub rate_limit: Option<crate::PingRateLimit>,
    /// (Experimental) Whether to add a wallclock timestamp to all events.
    pub enable_event_timestamps: bool,
    /// An experimentation identifier derived by the application to be sent with all pings, it should
    /// be noted that this has an underlying StringMetric and so should conform to the limitations that
    /// StringMetric places on length, etc.
    pub experimentation_id: Option<String>,
    /// Whether to enable internal pings. Default: true
    pub enable_internal_pings: bool,
    /// A ping schedule map.
    /// Maps a ping name to a list of pings to schedule along with it.
    /// Only used if the ping's own ping schedule list is empty.
    pub ping_schedule: HashMap<String, Vec<String>>,
}

impl Builder {
    /// A new configuration builder.
    pub fn new<P: Into<PathBuf>, S: Into<String>>(
        upload_enabled: bool,
        data_path: P,
        application_id: S,
    ) -> Self {
        Self {
            upload_enabled,
            data_path: data_path.into(),
            application_id: application_id.into(),
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: None,
            uploader: None,
            use_core_mps: false,
            trim_data_to_registered_pings: false,
            log_level: None,
            rate_limit: None,
            enable_event_timestamps: true,
            experimentation_id: None,
            enable_internal_pings: true,
            ping_schedule: HashMap::new(),
        }
    }

    /// Generate the full configuration.
    pub fn build(self) -> Configuration {
        Configuration {
            upload_enabled: self.upload_enabled,
            data_path: self.data_path,
            application_id: self.application_id,
            max_events: self.max_events,
            delay_ping_lifetime_io: self.delay_ping_lifetime_io,
            server_endpoint: self.server_endpoint,
            uploader: self.uploader,
            use_core_mps: self.use_core_mps,
            trim_data_to_registered_pings: self.trim_data_to_registered_pings,
            log_level: self.log_level,
            rate_limit: self.rate_limit,
            enable_event_timestamps: self.enable_event_timestamps,
            experimentation_id: self.experimentation_id,
            enable_internal_pings: self.enable_internal_pings,
            ping_schedule: self.ping_schedule,
        }
    }

    /// Set the maximum number of events to store before sending a ping containing events.
    pub fn with_max_events(mut self, max_events: usize) -> Self {
        self.max_events = Some(max_events);
        self
    }

    /// Set whether Glean should delay persistence of data from metrics with ping lifetime.
    pub fn with_delay_ping_lifetime_io(mut self, value: bool) -> Self {
        self.delay_ping_lifetime_io = value;
        self
    }

    /// Set the server pings are sent to.
    pub fn with_server_endpoint<S: Into<String>>(mut self, server_endpoint: S) -> Self {
        self.server_endpoint = Some(server_endpoint.into());
        self
    }

    /// Set the instance of the uploader used to send pings.
    pub fn with_uploader<U: PingUploader + 'static>(mut self, uploader: U) -> Self {
        self.uploader = Some(Box::new(uploader));
        self
    }

    /// Set whether Glean should schedule "metrics" pings for you.
    pub fn with_use_core_mps(mut self, value: bool) -> Self {
        self.use_core_mps = value;
        self
    }

    /// Set whether Glean should limit its storage to only that of registered pings.
    pub fn with_trim_data_to_registered_pings(mut self, value: bool) -> Self {
        self.trim_data_to_registered_pings = value;
        self
    }

    /// Set whether to add a wallclock timestamp to all events (experimental).
    pub fn with_event_timestamps(mut self, value: bool) -> Self {
        self.enable_event_timestamps = value;
        self
    }

    /// Set whether to add a wallclock timestamp to all events (experimental).
    pub fn with_experimentation_id(mut self, value: String) -> Self {
        self.experimentation_id = Some(value);
        self
    }

    /// Set whether to enable internal pings.
    pub fn with_internal_pings(mut self, value: bool) -> Self {
        self.enable_internal_pings = value;
        self
    }

    /// Set the ping schedule map.
    pub fn with_ping_schedule(mut self, value: HashMap<String, Vec<String>>) -> Self {
        self.ping_schedule = value;
        self
    }
}
