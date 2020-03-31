// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![deny(missing_docs)]

//! Glean is a modern approach for recording and sending Telemetry data.
//!
//! It's in use at Mozilla.
//!
//! All documentation can be found online:
//!
//! ## [The Glean SDK Book](https://mozilla.github.io/glean)

use std::collections::HashMap;
use std::path::{Path, PathBuf};

use chrono::{DateTime, FixedOffset};
use lazy_static::lazy_static;
use uuid::Uuid;

// This needs to be included first, and the space below prevents rustfmt from
// alphabetizing it.
mod macros;

mod common_metric_data;
mod database;
mod error;
mod error_recording;
mod event_database;
mod histogram;
mod internal_metrics;
mod internal_pings;
pub mod metrics;
pub mod ping;
pub mod storage;
mod util;

pub use crate::common_metric_data::{CommonMetricData, Lifetime};
use crate::database::Database;
pub use crate::error::{Error, Result};
pub use crate::error_recording::{test_get_num_recorded_errors, ErrorType};
use crate::event_database::EventDatabase;
use crate::internal_metrics::CoreMetrics;
use crate::internal_pings::InternalPings;
use crate::metrics::PingType;
use crate::ping::PingMaker;
use crate::storage::StorageManager;
use crate::util::{local_now_with_offset, sanitize_application_id};

const GLEAN_SCHEMA_VERSION: u32 = 1;
const DEFAULT_MAX_EVENTS: usize = 500;
lazy_static! {
    static ref KNOWN_CLIENT_ID: Uuid =
        Uuid::parse_str("c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0").unwrap();
}

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
}

/// The object holding meta information about a Glean instance.
///
/// ## Example
///
/// Create a new Glean instance, register a ping, record a simple counter and then send the final
/// ping.
///
/// ```rust,no_run
/// # use glean_core::{Glean, Configuration, CommonMetricData, metrics::*};
/// let cfg = Configuration {
///     data_path: "/tmp/glean".into(),
///     application_id: "glean.sample.app".into(),
///     upload_enabled: true,
///     max_events: None,
///     delay_ping_lifetime_io: false,
/// };
/// let mut glean = Glean::new(cfg).unwrap();
/// let ping = PingType::new("sample", true, false);
/// glean.register_ping_type(&ping);
///
/// let call_counter: CounterMetric = CounterMetric::new(CommonMetricData {
///     name: "calls".into(),
///     category: "local".into(),
///     send_in_pings: vec!["sample".into()],
///     ..Default::default()
/// });
///
/// call_counter.add(&glean, 1);
///
/// glean.submit_ping(&ping).unwrap();
/// ```
///
/// ## Note
///
/// In specific language bindings, this is usually wrapped in a singleton and all metric recording goes to a single instance of this object.
/// In the Rust core, it is possible to create multiple instances, which is used in testing.
#[derive(Debug)]
pub struct Glean {
    upload_enabled: bool,
    data_store: Database,
    event_data_store: EventDatabase,
    core_metrics: CoreMetrics,
    internal_pings: InternalPings,
    data_path: PathBuf,
    application_id: String,
    ping_registry: HashMap<String, PingType>,
    start_time: DateTime<FixedOffset>,
    max_events: usize,
    is_first_run: bool,
}

impl Glean {
    /// Create and initialize a new Glean object.
    ///
    /// This will create the necessary directories and files in `data_path`.
    /// This will also initialize the core metrics.
    pub fn new(cfg: Configuration) -> Result<Self> {
        log::info!("Creating new Glean");

        let application_id = sanitize_application_id(&cfg.application_id);

        // Creating the data store creates the necessary path as well.
        // If that fails we bail out and don't initialize further.
        let data_store = Database::new(&cfg.data_path, cfg.delay_ping_lifetime_io)?;
        let event_data_store = EventDatabase::new(&cfg.data_path)?;

        let mut glean = Self {
            upload_enabled: cfg.upload_enabled,
            data_store,
            event_data_store,
            core_metrics: CoreMetrics::new(),
            internal_pings: InternalPings::new(),
            data_path: PathBuf::from(cfg.data_path),
            application_id,
            ping_registry: HashMap::new(),
            start_time: local_now_with_offset(),
            max_events: cfg.max_events.unwrap_or(DEFAULT_MAX_EVENTS),
            is_first_run: false,
        };
        glean.on_change_upload_enabled(cfg.upload_enabled);
        Ok(glean)
    }

    /// For tests make it easy to create a Glean object using only the required configuration.
    #[cfg(test)]
    pub(crate) fn with_options(
        data_path: &str,
        application_id: &str,
        upload_enabled: bool,
    ) -> Result<Self> {
        let cfg = Configuration {
            data_path: data_path.into(),
            application_id: application_id.into(),
            upload_enabled,
            max_events: None,
            delay_ping_lifetime_io: false,
        };

        Self::new(cfg)
    }

    /// Initialize the core metrics managed by Glean's Rust core.
    fn initialize_core_metrics(&mut self) {
        let need_new_client_id = match self
            .core_metrics
            .client_id
            .get_value(self, "glean_client_info")
        {
            None => true,
            Some(uuid) => uuid == *KNOWN_CLIENT_ID,
        };
        if need_new_client_id {
            self.core_metrics.client_id.generate_and_set(self);
        }

        if self
            .core_metrics
            .first_run_date
            .get_value(self, "glean_client_info")
            .is_none()
        {
            self.core_metrics.first_run_date.set(self, None);
            // The `first_run_date` field is generated on the very first run
            // and persisted across upload toggling. We can assume that, the only
            // time it is set, that's indeed our "first run".
            self.is_first_run = true;
        }
    }

    /// Called when Glean is initialized to the point where it can correctly
    /// assemble pings. Usually called from the language specific layer after all
    /// of the core metrics have been set and the ping types have been
    /// registered.
    ///
    /// # Return value
    ///
    /// `true` if at least one ping was generated, `false` otherwise.
    pub fn on_ready_to_submit_pings(&self) -> bool {
        self.event_data_store.flush_pending_events_on_startup(&self)
    }

    /// Set whether upload is enabled or not.
    ///
    /// When uploading is disabled, metrics aren't recorded at all and no
    /// data is uploaded.
    ///
    /// When disabling, all pending metrics, events and queued pings are cleared.
    ///
    /// When enabling, the core Glean metrics are recreated.
    ///
    /// If the value of this flag is not actually changed, this is a no-op.
    ///
    /// # Arguments
    ///
    /// * `flag` - When true, enable metric collection.
    ///
    /// # Returns
    ///
    /// * Returns true when the flag was different from the current value, and
    ///   actual work was done to clear or reinstate metrics.
    pub fn set_upload_enabled(&mut self, flag: bool) -> bool {
        log::info!("Upload enabled: {:?}", flag);

        // When upload is disabled, submit a deletion-request ping
        if !flag {
            if let Err(err) = self.internal_pings.deletion_request.submit(self) {
                log::error!("Failed to send deletion-request ping on optout: {}", err);
            }
        }

        if self.upload_enabled != flag {
            self.upload_enabled = flag;
            self.on_change_upload_enabled(flag);
            true
        } else {
            false
        }
    }

    /// Determine whether upload is enabled.
    ///
    /// When upload is disabled, no data will be recorded.
    pub fn is_upload_enabled(&self) -> bool {
        self.upload_enabled
    }

    /// Handles the changing of state when upload_enabled changes.
    ///
    /// Should only be called when the state actually changes.
    /// When disabling, all pending metrics, events and queued pings are cleared.
    ///
    /// When enabling, the core Glean metrics are recreated.
    ///
    /// # Arguments
    ///
    /// * `flag` - When true, enable metric collection.
    fn on_change_upload_enabled(&mut self, flag: bool) {
        if flag {
            self.initialize_core_metrics();
        } else {
            self.clear_metrics();
        }
    }

    /// Clear any pending metrics when telemetry is disabled.
    fn clear_metrics(&mut self) {
        // There is only one metric that we want to survive after clearing all
        // metrics: first_run_date. Here, we store its value so we can restore
        // it after clearing the metrics.
        let existing_first_run_date = self
            .core_metrics
            .first_run_date
            .get_value(self, "glean_client_info");

        // Clear any pending pings.
        let ping_maker = PingMaker::new();
        if let Err(err) = ping_maker.clear_pending_pings(self.get_data_path()) {
            log::error!("Error clearing pending pings: {}", err);
        }

        // Delete all stored metrics.
        // Note that this also includes the ping sequence numbers, so it has
        // the effect of resetting those to their initial values.
        self.data_store.clear_all();
        if let Err(err) = self.event_data_store.clear_all() {
            log::error!("Error clearing pending events: {}", err);
        }

        // This does not clear the experiments store (which isn't managed by the
        // StorageEngineManager), since doing so would mean we would have to have the
        // application tell us again which experiments are active if telemetry is
        // re-enabled.

        {
            // We need to briefly set upload_enabled to true here so that `set`
            // is not a no-op. This is safe, since nothing on the Rust side can
            // run concurrently to this since we hold a mutable reference to the
            // Glean object. Additionally, the pending pings have been cleared
            // from disk, so the PingUploader can't wake up and start sending
            // pings.
            self.upload_enabled = true;

            // Store a "dummy" KNOWN_CLIENT_ID in the client_id metric. This will
            // make it easier to detect if pings were unintentionally sent after
            // uploading is disabled.
            self.core_metrics.client_id.set(self, *KNOWN_CLIENT_ID);

            // Restore the first_run_date.
            if let Some(existing_first_run_date) = existing_first_run_date {
                self.core_metrics
                    .first_run_date
                    .set(self, Some(existing_first_run_date));
            }

            self.upload_enabled = false;
        }
    }

    /// Get the application ID as specified on instantiation.
    pub fn get_application_id(&self) -> &str {
        &self.application_id
    }

    /// Get the data path of this instance.
    pub fn get_data_path(&self) -> &Path {
        &self.data_path
    }

    /// Get a handle to the database.
    pub fn storage(&self) -> &Database {
        &self.data_store
    }

    /// Get a handle to the event database.
    pub fn event_storage(&self) -> &EventDatabase {
        &self.event_data_store
    }

    /// Get the maximum number of events to store before sending a ping.
    pub fn get_max_events(&self) -> usize {
        self.max_events
    }

    /// Take a snapshot for the given store and optionally clear it.
    ///
    /// ## Arguments
    ///
    /// * `store_name` - The store to snapshot.
    /// * `clear_store` - Whether to clear the store after snapshotting.
    ///
    /// ## Return value
    ///
    /// Returns the snapshot in a string encoded as JSON.
    /// If the snapshot is empty, it returns an empty string.
    pub fn snapshot(&mut self, store_name: &str, clear_store: bool) -> String {
        StorageManager
            .snapshot(&self.storage(), store_name, clear_store)
            .unwrap_or_else(|| String::from(""))
    }

    fn make_path(&self, ping_name: &str, doc_id: &str) -> String {
        format!(
            "/submit/{}/{}/{}/{}",
            self.get_application_id(),
            ping_name,
            GLEAN_SCHEMA_VERSION,
            doc_id
        )
    }

    /// Collect and submit a ping for eventual uploading.
    ///
    /// The ping content is assembled as soon as possible, but upload is not
    /// guaranteed to happen immediately, as that depends on the upload
    /// policies.
    ///
    /// If the ping currently contains no content, it will not be sent.
    ///
    /// Returns true if a ping was assembled and queued, false otherwise.
    /// Returns an error if collecting or writing the ping to disk failed.
    pub fn submit_ping(&self, ping: &PingType) -> Result<bool> {
        if !self.is_upload_enabled() {
            log::error!("Glean must be enabled before sending pings.");
            return Ok(false);
        }

        let ping_maker = PingMaker::new();
        let doc_id = Uuid::new_v4().to_string();
        let url_path = self.make_path(&ping.name, &doc_id);
        match ping_maker.collect(self, &ping) {
            None => {
                log::info!(
                    "No content for ping '{}', therefore no ping queued.",
                    ping.name
                );
                Ok(false)
            }
            Some(content) => {
                if let Err(e) = ping_maker.store_ping(
                    &doc_id,
                    &ping.name,
                    &self.get_data_path(),
                    &url_path,
                    &content,
                ) {
                    log::warn!("IO error while writing ping to file: {}", e);
                    return Err(e.into());
                }

                log::info!(
                    "The ping '{}' was submitted and will be sent as soon as possible",
                    ping.name
                );
                Ok(true)
            }
        }
    }

    /// Collect and submit a ping for eventual uploading by name.
    ///
    /// See `submit_ping` for detailed information.
    ///
    /// Returns true if at least one ping was assembled and queued, false otherwise.
    pub fn submit_pings_by_name(&self, ping_names: &[String]) -> bool {
        // TODO: 1553813: glean-ac collects and stores pings in parallel and then joins them all before queueing the worker.
        // This here is writing them out sequentially.

        let mut result = false;

        for ping_name in ping_names {
            if let Ok(true) = self.submit_ping_by_name(ping_name) {
                result = true;
            }
        }
        result
    }

    /// Collect and submit a ping by name for eventual uploading.
    ///
    /// The ping content is assembled as soon as possible, but upload is not
    /// guaranteed to happen immediately, as that depends on the upload
    /// policies.
    ///
    /// If the ping currently contains no content, it will not be sent.
    ///
    /// Returns true if a ping was assembled and queued, false otherwise.
    /// Returns an error if collecting or writing the ping to disk failed.
    pub fn submit_ping_by_name(&self, ping_name: &str) -> Result<bool> {
        match self.get_ping_by_name(ping_name) {
            None => {
                log::error!("Attempted to submit unknown ping '{}'", ping_name);
                Ok(false)
            }
            Some(ping) => self.submit_ping(ping),
        }
    }

    /// Get a [`PingType`](metrics/struct.PingType.html) by name.
    ///
    /// ## Return value
    ///
    /// Returns the `PingType` if a ping of the given name was registered before.
    /// Returns `None` otherwise.
    pub fn get_ping_by_name(&self, ping_name: &str) -> Option<&PingType> {
        self.ping_registry.get(ping_name)
    }

    /// Register a new [`PingType`](metrics/struct.PingType.html).
    pub fn register_ping_type(&mut self, ping: &PingType) {
        if self.ping_registry.contains_key(&ping.name) {
            log::error!("Duplicate ping named '{}'", ping.name)
        }

        self.ping_registry.insert(ping.name.clone(), ping.clone());
    }

    /// Get create time of the Glean object.
    pub(crate) fn start_time(&self) -> DateTime<FixedOffset> {
        self.start_time
    }

    /// Indicate that an experiment is running.
    /// Glean will then add an experiment annotation to the environment
    /// which is sent with pings. This information is not persisted between runs.
    ///
    /// ## Arguments
    ///
    /// * `experiment_id` - The id of the active experiment (maximum 30 bytes).
    /// * `branch` - The experiment branch (maximum 30 bytes).
    /// * `extra` - Optional metadata to output with the ping.
    pub fn set_experiment_active(
        &self,
        experiment_id: String,
        branch: String,
        extra: Option<HashMap<String, String>>,
    ) {
        let metric = metrics::ExperimentMetric::new(&self, experiment_id);
        metric.set_active(&self, branch, extra);
    }

    /// Indicate that an experiment is no longer running.
    ///
    /// ## Arguments
    ///
    /// * `experiment_id` - The id of the active experiment to deactivate (maximum 30 bytes).
    pub fn set_experiment_inactive(&self, experiment_id: String) {
        let metric = metrics::ExperimentMetric::new(&self, experiment_id);
        metric.set_inactive(&self);
    }

    /// Persist Lifetime::Ping data that might be in memory
    /// in case `delay_ping_lifetime_io` is set or was set
    /// at a previous time.
    ///
    /// If there is no data to persist, this function does nothing.
    pub fn persist_ping_lifetime_data(&self) -> Result<()> {
        self.data_store.persist_ping_lifetime_data()
    }

    /// ** This is not meant to be used directly.**
    ///
    /// Clear all the metrics that have `Lifetime::Application`.
    pub fn clear_application_lifetime_metrics(&self) {
        log::debug!("Clearing Lifetime::Application metrics");
        self.data_store.clear_lifetime(Lifetime::Application);
    }

    /// Return whether or not this is the first run on this profile.
    pub fn is_first_run(&self) -> bool {
        self.is_first_run
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Check if an experiment is currently active.
    ///
    /// ## Arguments
    ///
    /// * `experiment_id` - The id of the experiment (maximum 30 bytes).
    ///
    /// ## Return value
    ///
    /// True if the experiment is active, false otherwise.
    pub fn test_is_experiment_active(&self, experiment_id: String) -> bool {
        self.test_get_experiment_data_as_json(experiment_id)
            .is_some()
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Get stored data for the requested experiment.
    ///
    /// ## Arguments
    ///
    /// * `experiment_id` - The id of the active experiment (maximum 30 bytes).
    ///
    /// ## Return value
    ///
    /// If the requested experiment is active, a JSON string with the following format:
    /// { 'branch': 'the-branch-name', 'extra': {'key': 'value', ...}}
    /// Otherwise, None.
    pub fn test_get_experiment_data_as_json(&self, experiment_id: String) -> Option<String> {
        let metric = metrics::ExperimentMetric::new(&self, experiment_id);
        metric.test_get_value_as_json_string(&self)
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Delete all stored metrics.
    /// Note that this also includes the ping sequence numbers, so it has
    /// the effect of resetting those to their initial values.
    pub fn test_clear_all_stores(&self) {
        self.data_store.clear_all();
        // We don't care about this failing, maybe the data does just not exist.
        let _ = self.event_data_store.clear_all();
    }
}

// Split unit tests to a separate file, to reduce the file of this one.
#[cfg(test)]
#[cfg(test)]
#[path = "lib_unit_tests.rs"]
mod tests;
