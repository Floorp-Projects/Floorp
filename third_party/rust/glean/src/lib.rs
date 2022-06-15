// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![deny(rustdoc::broken_intra_doc_links)]
#![deny(missing_docs)]

//! Glean is a modern approach for recording and sending Telemetry data.
//!
//! It's in use at Mozilla.
//!
//! All documentation can be found online:
//!
//! ## [The Glean SDK Book](https://mozilla.github.io/glean)
//!
//! ## Example
//!
//! Initialize Glean, register a ping and then send it.
//!
//! ```rust,no_run
//! # use glean::{Configuration, ClientInfoMetrics, Error, private::*};
//! let cfg = Configuration {
//!     data_path: "/tmp/data".into(),
//!     application_id: "org.mozilla.glean_core.example".into(),
//!     upload_enabled: true,
//!     max_events: None,
//!     delay_ping_lifetime_io: false,
//!     server_endpoint: None,
//!     uploader: None,
//!     use_core_mps: false,
//! };
//! glean::initialize(cfg, ClientInfoMetrics::unknown());
//!
//! let prototype_ping = PingType::new("prototype", true, true, vec!());
//!
//! prototype_ping.submit(None);
//! ```

use std::collections::HashMap;

pub use configuration::Configuration;
use configuration::DEFAULT_GLEAN_ENDPOINT;
pub use core_metrics::ClientInfoMetrics;
pub use glean_core::{
    metrics::{Datetime, DistributionData, MemoryUnit, Rate, RecordedEvent, TimeUnit, TimerId},
    traits, CommonMetricData, Error, ErrorType, Glean, HistogramType, Lifetime, RecordedExperiment,
    Result,
};

mod configuration;
mod core_metrics;
pub mod net;
pub mod private;
mod system;

#[cfg(test)]
mod common_test;

const LANGUAGE_BINDING_NAME: &str = "Rust";

/// Creates and initializes a new Glean object.
///
/// See [`glean_core::Glean::new`] for more information.
///
/// # Arguments
///
/// * `cfg` - the [`Configuration`] options to initialize with.
/// * `client_info` - the [`ClientInfoMetrics`] values used to set Glean
///   core metrics.
pub fn initialize(cfg: Configuration, client_info: ClientInfoMetrics) {
    initialize_internal(cfg, client_info);
}

struct GleanEvents {
    /// An instance of the upload manager
    upload_manager: net::UploadManager,
}

impl glean_core::OnGleanEvents for GleanEvents {
    fn on_initialize_finished(&self) {
        // intentionally left empty
    }

    fn trigger_upload(&self) {
        self.upload_manager.trigger_upload();
    }

    fn start_metrics_ping_scheduler(&self) -> bool {
        // We rely on the glean-core MPS.
        // We always trigger an upload as it might have submitted a ping.
        true
    }

    fn cancel_uploads(&self) {
        // intentionally left empty
    }
}

fn initialize_internal(cfg: Configuration, client_info: ClientInfoMetrics) -> Option<()> {
    // Initialize the ping uploader.
    let upload_manager = net::UploadManager::new(
        cfg.server_endpoint
            .unwrap_or_else(|| DEFAULT_GLEAN_ENDPOINT.to_string()),
        cfg.uploader
            .unwrap_or_else(|| Box::new(net::HttpUploader) as Box<dyn net::PingUploader>),
    );

    // Now make this the global object available to others.
    let callbacks = Box::new(GleanEvents { upload_manager });

    let core_cfg = glean_core::InternalConfiguration {
        upload_enabled: cfg.upload_enabled,
        data_path: cfg.data_path.display().to_string(),
        application_id: cfg.application_id.clone(),
        language_binding_name: LANGUAGE_BINDING_NAME.into(),
        max_events: cfg.max_events.map(|m| m as u32),
        delay_ping_lifetime_io: cfg.delay_ping_lifetime_io,
        app_build: client_info.app_build.clone(),
        use_core_mps: cfg.use_core_mps,
    };

    glean_core::glean_initialize(core_cfg, client_info.into(), callbacks);
    Some(())
}

/// Shuts down Glean in an orderly fashion.
pub fn shutdown() {
    glean_core::shutdown()
}

/// Sets whether upload is enabled or not.
///
/// See [`glean_core::Glean::set_upload_enabled`].
pub fn set_upload_enabled(enabled: bool) {
    glean_core::glean_set_upload_enabled(enabled)
}

/// Collects and submits a ping for eventual uploading by name.
///
/// Note that this needs to be public in order for RLB consumers to
/// use Glean debugging facilities.
///
/// See [`glean_core::Glean.submit_ping_by_name`].
pub fn submit_ping_by_name(ping: &str, reason: Option<&str>) {
    let ping = ping.to_string();
    let reason = reason.map(|s| s.to_string());
    glean_core::glean_submit_ping_by_name(ping, reason)
}

/// Indicate that an experiment is running.  Glean will then add an
/// experiment annotation to the environment which is sent with pings. This
/// infomration is not persisted between runs.
///
/// See [`glean_core::Glean::set_experiment_active`].
pub fn set_experiment_active(
    experiment_id: String,
    branch: String,
    extra: Option<HashMap<String, String>>,
) {
    glean_core::glean_set_experiment_active(experiment_id, branch, extra.unwrap_or_default())
}

/// Indicate that an experiment is no longer running.
///
/// See [`glean_core::Glean::set_experiment_inactive`].
pub fn set_experiment_inactive(experiment_id: String) {
    glean_core::glean_set_experiment_inactive(experiment_id)
}

/// Performs the collection/cleanup operations required by becoming active.
///
/// This functions generates a baseline ping with reason `active`
/// and then sets the dirty bit.
/// This should be called whenever the consuming product becomes active (e.g.
/// getting to foreground).
pub fn handle_client_active() {
    glean_core::glean_handle_client_active()
}

/// Performs the collection/cleanup operations required by becoming inactive.
///
/// This functions generates a baseline and an events ping with reason
/// `inactive` and then clears the dirty bit.
/// This should be called whenever the consuming product becomes inactive (e.g.
/// getting to background).
pub fn handle_client_inactive() {
    glean_core::glean_handle_client_inactive()
}

/// TEST ONLY FUNCTION.
/// Checks if an experiment is currently active.
pub fn test_is_experiment_active(experiment_id: String) -> bool {
    glean_core::glean_test_get_experiment_data(experiment_id).is_some()
}

/// TEST ONLY FUNCTION.
/// Returns the [`RecordedExperiment`] for the given `experiment_id` or panics if
/// the id isn't found.
pub fn test_get_experiment_data(experiment_id: String) -> Option<RecordedExperiment> {
    glean_core::glean_test_get_experiment_data(experiment_id)
}

/// Destroy the global Glean state.
pub(crate) fn destroy_glean(clear_stores: bool) {
    glean_core::glean_test_destroy_glean(clear_stores)
}

/// TEST ONLY FUNCTION.
/// Resets the Glean state and triggers init again.
pub fn test_reset_glean(cfg: Configuration, client_info: ClientInfoMetrics, clear_stores: bool) {
    destroy_glean(clear_stores);
    initialize_internal(cfg, client_info);
    glean_core::join_init();
}

/// Sets a debug view tag.
///
/// When the debug view tag is set, pings are sent with a `X-Debug-ID` header with the
/// value of the tag and are sent to the ["Ping Debug Viewer"](https://mozilla.github.io/glean/book/dev/core/internal/debug-pings.html).
///
/// # Arguments
///
/// * `tag` - A valid HTTP header value. Must match the regex: "[a-zA-Z0-9-]{1,20}".
///
/// # Returns
///
/// This will return `false` in case `tag` is not a valid tag and `true` otherwise.
/// If called before Glean is initialized it will always return `true`.
pub fn set_debug_view_tag(tag: &str) -> bool {
    glean_core::glean_set_debug_view_tag(tag.to_string())
}

/// Sets the log pings debug option.
///
/// When the log pings debug option is `true`,
/// we log the payload of all succesfully assembled pings.
///
/// # Arguments
///
/// * `value` - The value of the log pings option
pub fn set_log_pings(value: bool) {
    glean_core::glean_set_log_pings(value)
}

/// Sets source tags.
///
/// Overrides any existing source tags.
/// Source tags will show in the destination datasets, after ingestion.
///
/// **Note** If one or more tags are invalid, all tags are ignored.
///
/// # Arguments
///
/// * `tags` - A vector of at most 5 valid HTTP header values. Individual
///   tags must match the regex: "[a-zA-Z0-9-]{1,20}".
pub fn set_source_tags(tags: Vec<String>) {
    glean_core::glean_set_source_tags(tags);
}

/// Returns a timestamp corresponding to "now" with millisecond precision.
pub fn get_timestamp_ms() -> u64 {
    glean_core::get_timestamp_ms()
}

/// Asks the database to persist ping-lifetime data to disk. Probably expensive to call.
/// Only has effect when Glean is configured with `delay_ping_lifetime_io: true`.
/// If Glean hasn't been initialized this will dispatch and return Ok(()),
/// otherwise it will block until the persist is done and return its Result.
pub fn persist_ping_lifetime_data() {
    glean_core::persist_ping_lifetime_data();
}

#[cfg(test)]
mod test;
