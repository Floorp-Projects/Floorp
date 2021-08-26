// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![deny(broken_intra_doc_links)]
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
use once_cell::sync::Lazy;
use once_cell::sync::OnceCell;
use std::sync::Mutex;
use uuid::Uuid;

// This needs to be included first, and the space below prevents rustfmt from
// alphabetizing it.
mod macros;

mod common_metric_data;
mod coverage;
mod database;
mod debug;
mod error;
mod error_recording;
mod event_database;
mod histogram;
mod internal_metrics;
mod internal_pings;
pub mod metrics;
pub mod ping;
mod scheduler;
pub mod storage;
mod system;
pub mod traits;
pub mod upload;
mod util;

pub use crate::common_metric_data::{CommonMetricData, Lifetime};
use crate::database::Database;
use crate::debug::DebugOptions;
pub use crate::error::{Error, ErrorKind, Result};
pub use crate::error_recording::{test_get_num_recorded_errors, ErrorType};
use crate::event_database::EventDatabase;
pub use crate::histogram::HistogramType;
use crate::internal_metrics::{AdditionalMetrics, CoreMetrics, DatabaseMetrics};
use crate::internal_pings::InternalPings;
use crate::metrics::{Metric, MetricType, PingType};
use crate::ping::PingMaker;
use crate::storage::StorageManager;
use crate::upload::{PingUploadManager, PingUploadTask, UploadResult};
use crate::util::{local_now_with_offset, sanitize_application_id};

const GLEAN_VERSION: &str = env!("CARGO_PKG_VERSION");
const GLEAN_SCHEMA_VERSION: u32 = 1;
const DEFAULT_MAX_EVENTS: usize = 500;
static KNOWN_CLIENT_ID: Lazy<Uuid> =
    Lazy::new(|| Uuid::parse_str("c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0").unwrap());
// An internal ping name, not to be touched by anything else
pub(crate) const INTERNAL_STORAGE: &str = "glean_internal_info";

// The names of the pings directories.
pub(crate) const PENDING_PINGS_DIRECTORY: &str = "pending_pings";
pub(crate) const DELETION_REQUEST_PINGS_DIRECTORY: &str = "deletion_request";

/// The global Glean instance.
///
/// This is the singleton used by all wrappers to allow for a nice API.
/// All state for Glean is kept inside this object (such as the database handle and `upload_enabled` flag).
///
/// It should be initialized with `glean_core::initialize` at the start of the application using
/// Glean.
static GLEAN: OnceCell<Mutex<Glean>> = OnceCell::new();

/// Gets a reference to the global Glean object.
pub fn global_glean() -> Option<&'static Mutex<Glean>> {
    GLEAN.get()
}

/// Sets or replaces the global Glean object.
pub fn setup_glean(glean: Glean) -> Result<()> {
    // The `OnceCell` type wrapping our Glean is thread-safe and can only be set once.
    // Therefore even if our check for it being empty succeeds, setting it could fail if a
    // concurrent thread is quicker in setting it.
    // However this will not cause a bigger problem, as the second `set` operation will just fail.
    // We can log it and move on.
    //
    // For all wrappers this is not a problem, as the Glean object is intialized exactly once on
    // calling `initialize` on the global singleton and further operations check that it has been
    // initialized.
    if GLEAN.get().is_none() {
        if GLEAN.set(Mutex::new(glean)).is_err() {
            log::warn!(
                "Global Glean object is initialized already. This probably happened concurrently."
            )
        }
    } else {
        // We allow overriding the global Glean object to support test mode.
        // In test mode the Glean object is fully destroyed and recreated.
        // This all happens behind a mutex and is therefore also thread-safe..
        let mut lock = GLEAN.get().unwrap().lock().unwrap();
        *lock = glean;
    }
    Ok(())
}

/// The Glean configuration.
///
/// Optional values will be filled in with default values.
#[derive(Debug, Clone)]
pub struct Configuration {
    /// Whether upload should be enabled.
    pub upload_enabled: bool,
    /// Path to a directory to store all data in.
    pub data_path: PathBuf,
    /// The application ID (will be sanitized during initialization).
    pub application_id: String,
    /// The name of the programming language used by the binding creating this instance of Glean.
    pub language_binding_name: String,
    /// The maximum number of events to store before sending a ping containing events.
    pub max_events: Option<usize>,
    /// Whether Glean should delay persistence of data from metrics with ping lifetime.
    pub delay_ping_lifetime_io: bool,
    /// The application's build identifier. If this is different from the one provided for a previous init,
    /// and use_core_mps is `true`, we will trigger a "metrics" ping.
    pub app_build: String,
    /// Whether Glean should schedule "metrics" pings.
    pub use_core_mps: bool,
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
///     language_binding_name: "Rust".into(),
///     upload_enabled: true,
///     max_events: None,
///     delay_ping_lifetime_io: false,
///     app_build: "".into(),
///     use_core_mps: false,
/// };
/// let mut glean = Glean::new(cfg).unwrap();
/// let ping = PingType::new("sample", true, false, vec![]);
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
/// glean.submit_ping(&ping, None);
/// ```
///
/// ## Note
///
/// In specific language bindings, this is usually wrapped in a singleton and all metric recording goes to a single instance of this object.
/// In the Rust core, it is possible to create multiple instances, which is used in testing.
#[derive(Debug)]
pub struct Glean {
    upload_enabled: bool,
    data_store: Option<Database>,
    event_data_store: EventDatabase,
    core_metrics: CoreMetrics,
    additional_metrics: AdditionalMetrics,
    database_metrics: DatabaseMetrics,
    internal_pings: InternalPings,
    data_path: PathBuf,
    application_id: String,
    ping_registry: HashMap<String, PingType>,
    start_time: DateTime<FixedOffset>,
    max_events: usize,
    is_first_run: bool,
    upload_manager: PingUploadManager,
    debug: DebugOptions,
    app_build: String,
    schedule_metrics_pings: bool,
}

impl Glean {
    /// Creates and initializes a new Glean object for use in a subprocess.
    ///
    /// Importantly, this will not send any pings at startup, since that
    /// sort of management should only happen in the main process.
    pub fn new_for_subprocess(cfg: &Configuration, scan_directories: bool) -> Result<Self> {
        log::info!("Creating new Glean v{}", GLEAN_VERSION);

        let application_id = sanitize_application_id(&cfg.application_id);
        if application_id.is_empty() {
            return Err(ErrorKind::InvalidConfig.into());
        }

        let event_data_store = EventDatabase::new(&cfg.data_path)?;

        // Create an upload manager with rate limiting of 15 pings every 60 seconds.
        let mut upload_manager = PingUploadManager::new(&cfg.data_path, &cfg.language_binding_name);
        upload_manager.set_rate_limiter(
            /* seconds per interval */ 60, /* max pings per interval */ 15,
        );

        // We only scan the pending ping directories when calling this from a subprocess,
        // when calling this from ::new we need to scan the directories after dealing with the upload state.
        if scan_directories {
            let _scanning_thread = upload_manager.scan_pending_pings_directories();
        }

        let (start_time, start_time_is_corrected) = local_now_with_offset();
        let this = Self {
            upload_enabled: cfg.upload_enabled,
            // In the subprocess, we want to avoid accessing the database entirely.
            // The easiest way to ensure that is to just not initialize it.
            data_store: None,
            event_data_store,
            core_metrics: CoreMetrics::new(),
            additional_metrics: AdditionalMetrics::new(),
            database_metrics: DatabaseMetrics::new(),
            internal_pings: InternalPings::new(),
            upload_manager,
            data_path: PathBuf::from(&cfg.data_path),
            application_id,
            ping_registry: HashMap::new(),
            start_time,
            max_events: cfg.max_events.unwrap_or(DEFAULT_MAX_EVENTS),
            is_first_run: false,
            debug: DebugOptions::new(),
            app_build: cfg.app_build.to_string(),
            // Subprocess doesn't use "metrics" pings so has no need for a scheduler.
            schedule_metrics_pings: false,
        };

        // Can't use `local_now_with_offset_and_record` above, because we needed a valid `Glean` first.
        if start_time_is_corrected {
            this.additional_metrics
                .invalid_timezone_offset
                .add(&this, 1);
        }

        Ok(this)
    }

    /// Creates and initializes a new Glean object.
    ///
    /// This will create the necessary directories and files in
    /// [`cfg.data_path`](Configuration::data_path). This will also initialize
    /// the core metrics.
    pub fn new(cfg: Configuration) -> Result<Self> {
        let mut glean = Self::new_for_subprocess(&cfg, false)?;

        // Creating the data store creates the necessary path as well.
        // If that fails we bail out and don't initialize further.
        glean.data_store = Some(Database::new(&cfg.data_path, cfg.delay_ping_lifetime_io)?);

        // The upload enabled flag may have changed since the last run, for
        // example by the changing of a config file.
        if cfg.upload_enabled {
            // If upload is enabled, just follow the normal code path to
            // instantiate the core metrics.
            glean.on_upload_enabled();
        } else {
            // If upload is disabled, and we've never run before, only set the
            // client_id to KNOWN_CLIENT_ID, but do not send a deletion request
            // ping.
            // If we have run before, and if the client_id is not equal to
            // the KNOWN_CLIENT_ID, do the full upload disabled operations to
            // clear metrics, set the client_id to KNOWN_CLIENT_ID, and send a
            // deletion request ping.
            match glean
                .core_metrics
                .client_id
                .get_value(&glean, "glean_client_info")
            {
                None => glean.clear_metrics(),
                Some(uuid) => {
                    if uuid != *KNOWN_CLIENT_ID {
                        // Temporarily enable uploading so we can submit a
                        // deletion request ping.
                        glean.upload_enabled = true;
                        glean.on_upload_disabled(true);
                    }
                }
            }
        }

        // We set this only for non-subprocess situations.
        glean.schedule_metrics_pings = cfg.use_core_mps;

        // We only scan the pendings pings directories **after** dealing with the upload state.
        // If upload is disabled, we delete all pending pings files
        // and we need to do that **before** scanning the pending pings folder
        // to ensure we don't enqueue pings before their files are deleted.
        let _scanning_thread = glean.upload_manager.scan_pending_pings_directories();

        Ok(glean)
    }

    /// For tests make it easy to create a Glean object using only the required configuration.
    #[cfg(test)]
    pub(crate) fn with_options(
        data_path: &str,
        application_id: &str,
        upload_enabled: bool,
    ) -> Self {
        let cfg = Configuration {
            data_path: data_path.into(),
            application_id: application_id.into(),
            language_binding_name: "Rust".into(),
            upload_enabled,
            max_events: None,
            delay_ping_lifetime_io: false,
            app_build: "unknown".into(),
            use_core_mps: false,
        };

        let mut glean = Self::new(cfg).unwrap();

        // Disable all upload manager policies for testing
        glean.upload_manager = PingUploadManager::no_policy(data_path);

        glean
    }

    /// Destroys the database.
    ///
    /// After this Glean needs to be reinitialized.
    pub fn destroy_db(&mut self) {
        self.data_store = None;
    }

    /// Initializes the core metrics managed by Glean's Rust core.
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
            self.core_metrics.first_run_hour.set(self, None);
            // The `first_run_date` field is generated on the very first run
            // and persisted across upload toggling. We can assume that, the only
            // time it is set, that's indeed our "first run".
            self.is_first_run = true;
        }

        self.set_application_lifetime_core_metrics();
    }

    /// Initializes the database metrics managed by Glean's Rust core.
    fn initialize_database_metrics(&mut self) {
        log::trace!("Initializing database metrics");

        if let Some(size) = self
            .data_store
            .as_ref()
            .and_then(|database| database.file_size())
        {
            log::trace!("Database file size: {}", size.get());
            self.database_metrics.size.accumulate(self, size.get())
        }
    }

    /// Signals that the environment is ready to submit pings.
    ///
    /// Should be called when Glean is initialized to the point where it can correctly assemble pings.
    /// Usually called from the language binding after all of the core metrics have been set
    /// and the ping types have been registered.
    ///
    /// # Returns
    ///
    /// Whether at least one ping was generated.
    pub fn on_ready_to_submit_pings(&self) -> bool {
        self.event_data_store.flush_pending_events_on_startup(self)
    }

    /// Sets whether upload is enabled or not.
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
    /// Whether the flag was different from the current value,
    /// and actual work was done to clear or reinstate metrics.
    pub fn set_upload_enabled(&mut self, flag: bool) -> bool {
        log::info!("Upload enabled: {:?}", flag);

        if self.upload_enabled != flag {
            if flag {
                self.on_upload_enabled();
            } else {
                self.on_upload_disabled(false);
            }
            true
        } else {
            false
        }
    }

    /// Determines whether upload is enabled.
    ///
    /// When upload is disabled, no data will be recorded.
    pub fn is_upload_enabled(&self) -> bool {
        self.upload_enabled
    }

    /// Handles the changing of state from upload disabled to enabled.
    ///
    /// Should only be called when the state actually changes.
    ///
    /// The `upload_enabled` flag is set to true and the core Glean metrics are
    /// recreated.
    fn on_upload_enabled(&mut self) {
        self.upload_enabled = true;
        self.initialize_core_metrics();
        self.initialize_database_metrics();
    }

    /// Handles the changing of state from upload enabled to disabled.
    ///
    /// Should only be called when the state actually changes.
    ///
    /// A deletion_request ping is sent, all pending metrics, events and queued
    /// pings are cleared, and the client_id is set to KNOWN_CLIENT_ID.
    /// Afterward, the upload_enabled flag is set to false.
    fn on_upload_disabled(&mut self, during_init: bool) {
        // The upload_enabled flag should be true here, or the deletion ping
        // won't be submitted.
        let reason = if during_init {
            Some("at_init")
        } else {
            Some("set_upload_enabled")
        };
        if !self.internal_pings.deletion_request.submit(self, reason) {
            log::error!("Failed to submit deletion-request ping on optout.");
        }
        self.clear_metrics();
        self.upload_enabled = false;
    }

    /// Clear any pending metrics when telemetry is disabled.
    fn clear_metrics(&mut self) {
        // Clear the pending pings queue and acquire the lock
        // so that it can't be accessed until this function is done.
        let _lock = self.upload_manager.clear_ping_queue();

        // There are only two metrics that we want to survive after clearing all
        // metrics: first_run_date and first_run_hour. Here, we store their values
        // so we can restore them after clearing the metrics.
        let existing_first_run_date = self
            .core_metrics
            .first_run_date
            .get_value(self, "glean_client_info");

        let existing_first_run_hour = self.core_metrics.first_run_hour.get_value(self, "metrics");

        // Clear any pending pings.
        let ping_maker = PingMaker::new();
        if let Err(err) = ping_maker.clear_pending_pings(self.get_data_path()) {
            log::warn!("Error clearing pending pings: {}", err);
        }

        // Delete all stored metrics.
        // Note that this also includes the ping sequence numbers, so it has
        // the effect of resetting those to their initial values.
        if let Some(data) = self.data_store.as_ref() {
            data.clear_all()
        }
        if let Err(err) = self.event_data_store.clear_all() {
            log::warn!("Error clearing pending events: {}", err);
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

            // Restore the first_run_hour.
            if let Some(existing_first_run_hour) = existing_first_run_hour {
                self.core_metrics
                    .first_run_hour
                    .set(self, Some(existing_first_run_hour));
            }

            self.upload_enabled = false;
        }
    }

    /// Gets the application ID as specified on instantiation.
    pub fn get_application_id(&self) -> &str {
        &self.application_id
    }

    /// Gets the data path of this instance.
    pub fn get_data_path(&self) -> &Path {
        &self.data_path
    }

    /// Gets a handle to the database.
    pub fn storage(&self) -> &Database {
        self.data_store.as_ref().expect("No database found")
    }

    /// Gets a handle to the event database.
    pub fn event_storage(&self) -> &EventDatabase {
        &self.event_data_store
    }

    /// Gets the maximum number of events to store before sending a ping.
    pub fn get_max_events(&self) -> usize {
        self.max_events
    }

    /// Gets the next task for an uploader.
    ///
    /// This can be one of:
    ///
    /// * [`Wait`](PingUploadTask::Wait) - which means the requester should ask
    ///   again later;
    /// * [`Upload(PingRequest)`](PingUploadTask::Upload) - which means there is
    ///   a ping to upload. This wraps the actual request object;
    /// * [`Done`](PingUploadTask::Done) - which means requester should stop
    ///   asking for now.
    ///
    /// # Returns
    ///
    /// A [`PingUploadTask`] representing the next task.
    pub fn get_upload_task(&self) -> PingUploadTask {
        self.upload_manager.get_upload_task(self, self.log_pings())
    }

    /// Processes the response from an attempt to upload a ping.
    ///
    /// # Arguments
    ///
    /// * `uuid` - The UUID of the ping in question.
    /// * `status` - The upload result.
    pub fn process_ping_upload_response(&self, uuid: &str, status: UploadResult) {
        self.upload_manager
            .process_ping_upload_response(self, uuid, status);
    }

    /// Takes a snapshot for the given store and optionally clear it.
    ///
    /// # Arguments
    ///
    /// * `store_name` - The store to snapshot.
    /// * `clear_store` - Whether to clear the store after snapshotting.
    ///
    /// # Returns
    ///
    /// The snapshot in a string encoded as JSON. If the snapshot is empty, returns an empty string.
    pub fn snapshot(&mut self, store_name: &str, clear_store: bool) -> String {
        StorageManager
            .snapshot(self.storage(), store_name, clear_store)
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

    /// Collects and submits a ping for eventual uploading.
    ///
    /// The ping content is assembled as soon as possible, but upload is not
    /// guaranteed to happen immediately, as that depends on the upload policies.
    ///
    /// If the ping currently contains no content, it will not be sent,
    /// unless it is configured to be sent if empty.
    ///
    /// # Arguments
    ///
    /// * `ping` - The ping to submit
    /// * `reason` - A reason code to include in the ping
    ///
    /// # Returns
    ///
    /// Whether the ping was succesfully assembled and queued.
    pub fn submit_ping(&self, ping: &PingType, reason: Option<&str>) -> bool {
        if !self.is_upload_enabled() {
            log::info!("Glean disabled: not submitting any pings.");
            return false;
        }

        let ping_maker = PingMaker::new();
        let doc_id = Uuid::new_v4().to_string();
        let url_path = self.make_path(&ping.name, &doc_id);
        match ping_maker.collect(self, ping, reason, &doc_id, &url_path) {
            None => {
                log::info!(
                    "No content for ping '{}', therefore no ping queued.",
                    ping.name
                );
                false
            }
            Some(ping) => {
                // This metric is recorded *after* the ping is collected (since
                // that is the only way to know *if* it will be submitted). The
                // implication of this is that the count for a metrics ping will
                // be included in the *next* metrics ping.
                self.additional_metrics
                    .pings_submitted
                    .get(ping.name)
                    .add(self, 1);

                if let Err(e) = ping_maker.store_ping(self.get_data_path(), &ping) {
                    log::warn!("IO error while writing ping to file: {}. Enqueuing upload of what we have in memory.", e);
                    self.additional_metrics.io_errors.add(self, 1);
                    // `serde_json::to_string` only fails if serialization of the content
                    // fails or it contains maps with non-string keys.
                    // However `ping.content` is already a `JsonValue`,
                    // so both scenarios should be impossible.
                    let content =
                        ::serde_json::to_string(&ping.content).expect("ping serialization failed");
                    self.upload_manager.enqueue_ping(
                        self,
                        ping.doc_id,
                        ping.url_path,
                        &content,
                        Some(ping.headers),
                    );
                    return true;
                }

                self.upload_manager.enqueue_ping_from_file(self, &doc_id);

                log::info!(
                    "The ping '{}' was submitted and will be sent as soon as possible",
                    ping.name
                );

                true
            }
        }
    }

    /// Collects and submits a ping by name for eventual uploading.
    ///
    /// The ping content is assembled as soon as possible, but upload is not
    /// guaranteed to happen immediately, as that depends on the upload policies.
    ///
    /// If the ping currently contains no content, it will not be sent,
    /// unless it is configured to be sent if empty.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - The name of the ping to submit
    /// * `reason` - A reason code to include in the ping
    ///
    /// # Returns
    ///
    /// Whether the ping was succesfully assembled and queued.
    ///
    /// # Errors
    ///
    /// If collecting or writing the ping to disk failed.
    pub fn submit_ping_by_name(&self, ping_name: &str, reason: Option<&str>) -> bool {
        match self.get_ping_by_name(ping_name) {
            None => {
                log::error!("Attempted to submit unknown ping '{}'", ping_name);
                false
            }
            Some(ping) => self.submit_ping(ping, reason),
        }
    }

    /// Gets a [`PingType`] by name.
    ///
    /// # Returns
    ///
    /// The [`PingType`] of a ping if the given name was registered before, [`None`]
    /// otherwise.
    pub fn get_ping_by_name(&self, ping_name: &str) -> Option<&PingType> {
        self.ping_registry.get(ping_name)
    }

    /// Register a new [`PingType`](metrics/struct.PingType.html).
    pub fn register_ping_type(&mut self, ping: &PingType) {
        if self.ping_registry.contains_key(&ping.name) {
            log::debug!("Duplicate ping named '{}'", ping.name)
        }

        self.ping_registry.insert(ping.name.clone(), ping.clone());
    }

    /// Get create time of the Glean object.
    pub(crate) fn start_time(&self) -> DateTime<FixedOffset> {
        self.start_time
    }

    /// Indicates that an experiment is running.
    ///
    /// Glean will then add an experiment annotation to the environment
    /// which is sent with pings. This information is not persisted between runs.
    ///
    /// # Arguments
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
        let metric = metrics::ExperimentMetric::new(self, experiment_id);
        metric.set_active(self, branch, extra);
    }

    /// Indicates that an experiment is no longer running.
    ///
    /// # Arguments
    ///
    /// * `experiment_id` - The id of the active experiment to deactivate (maximum 30 bytes).
    pub fn set_experiment_inactive(&self, experiment_id: String) {
        let metric = metrics::ExperimentMetric::new(self, experiment_id);
        metric.set_inactive(self);
    }

    /// Persists [`Lifetime::Ping`] data that might be in memory in case
    /// [`delay_ping_lifetime_io`](Configuration::delay_ping_lifetime_io) is set
    /// or was set at a previous time.
    ///
    /// If there is no data to persist, this function does nothing.
    pub fn persist_ping_lifetime_data(&self) -> Result<()> {
        if let Some(data) = self.data_store.as_ref() {
            return data.persist_ping_lifetime_data();
        }

        Ok(())
    }

    /// Sets internally-handled application lifetime metrics.
    fn set_application_lifetime_core_metrics(&self) {
        self.core_metrics.os.set(self, system::OS);
    }

    /// **This is not meant to be used directly.**
    ///
    /// Clears all the metrics that have [`Lifetime::Application`].
    pub fn clear_application_lifetime_metrics(&self) {
        log::trace!("Clearing Lifetime::Application metrics");
        if let Some(data) = self.data_store.as_ref() {
            data.clear_lifetime(Lifetime::Application);
        }

        // Set internally handled app lifetime metrics again.
        self.set_application_lifetime_core_metrics();
    }

    /// Whether or not this is the first run on this profile.
    pub fn is_first_run(&self) -> bool {
        self.is_first_run
    }

    /// Sets a debug view tag.
    ///
    /// This will return `false` in case `value` is not a valid tag.
    ///
    /// When the debug view tag is set, pings are sent with a `X-Debug-ID` header with the value of the tag
    /// and are sent to the ["Ping Debug Viewer"](https://mozilla.github.io/glean/book/dev/core/internal/debug-pings.html).
    ///
    /// # Arguments
    ///
    /// * `value` - A valid HTTP header value. Must match the regex: "[a-zA-Z0-9-]{1,20}".
    pub fn set_debug_view_tag(&mut self, value: &str) -> bool {
        self.debug.debug_view_tag.set(value.into())
    }

    /// Return the value for the debug view tag or [`None`] if it hasn't been set.
    ///
    /// The `debug_view_tag` may be set from an environment variable
    /// (`GLEAN_DEBUG_VIEW_TAG`) or through the [`set_debug_view_tag`] function.
    pub(crate) fn debug_view_tag(&self) -> Option<&String> {
        self.debug.debug_view_tag.get()
    }

    /// Sets source tags.
    ///
    /// This will return `false` in case `value` contains invalid tags.
    ///
    /// Ping tags will show in the destination datasets, after ingestion.
    ///
    /// **Note** If one or more tags are invalid, all tags are ignored.
    ///
    /// # Arguments
    ///
    /// * `value` - A vector of at most 5 valid HTTP header values. Individual tags must match the regex: "[a-zA-Z0-9-]{1,20}".
    pub fn set_source_tags(&mut self, value: Vec<String>) -> bool {
        self.debug.source_tags.set(value)
    }

    /// Return the value for the source tags or [`None`] if it hasn't been set.
    ///
    /// The `source_tags` may be set from an environment variable (`GLEAN_SOURCE_TAGS`)
    /// or through the [`set_source_tags`] function.
    pub(crate) fn source_tags(&self) -> Option<&Vec<String>> {
        self.debug.source_tags.get()
    }

    /// Sets the log pings debug option.
    ///
    /// This will return `false` in case we are unable to set the option.
    ///
    /// When the log pings debug option is `true`,
    /// we log the payload of all succesfully assembled pings.
    ///
    /// # Arguments
    ///
    /// * `value` - The value of the log pings option
    pub fn set_log_pings(&mut self, value: bool) -> bool {
        self.debug.log_pings.set(value)
    }

    /// Return the value for the log pings debug option or [`None`] if it hasn't been set.
    ///
    /// The `log_pings` option may be set from an environment variable (`GLEAN_LOG_PINGS`)
    /// or through the [`set_log_pings`] function.
    pub(crate) fn log_pings(&self) -> bool {
        self.debug.log_pings.get().copied().unwrap_or(false)
    }

    fn get_dirty_bit_metric(&self) -> metrics::BooleanMetric {
        metrics::BooleanMetric::new(CommonMetricData {
            name: "dirtybit".into(),
            // We don't need a category, the name is already unique
            category: "".into(),
            send_in_pings: vec![INTERNAL_STORAGE.into()],
            lifetime: Lifetime::User,
            ..Default::default()
        })
    }

    /// **This is not meant to be used directly.**
    ///
    /// Sets the value of a "dirty flag" in the permanent storage.
    ///
    /// The "dirty flag" is meant to have the following behaviour, implemented
    /// by the consumers of the FFI layer:
    ///
    /// - on mobile: set to `false` when going to background or shutting down,
    ///   set to `true` at startup and when going to foreground.
    /// - on non-mobile platforms: set to `true` at startup and `false` at
    ///   shutdown.
    ///
    /// At startup, before setting its new value, if the "dirty flag" value is
    /// `true`, then Glean knows it did not exit cleanly and can implement
    /// coping mechanisms (e.g. sending a `baseline` ping).
    pub fn set_dirty_flag(&self, new_value: bool) {
        self.get_dirty_bit_metric().set(self, new_value);
    }

    /// **This is not meant to be used directly.**
    ///
    /// Checks the stored value of the "dirty flag".
    pub fn is_dirty_flag_set(&self) -> bool {
        let dirty_bit_metric = self.get_dirty_bit_metric();
        match StorageManager.snapshot_metric(
            self.storage(),
            INTERNAL_STORAGE,
            &dirty_bit_metric.meta().identifier(self),
            dirty_bit_metric.meta().lifetime,
        ) {
            Some(Metric::Boolean(b)) => b,
            _ => false,
        }
    }

    /// Performs the collection/cleanup operations required by becoming active.
    ///
    /// This functions generates a baseline ping with reason `active`
    /// and then sets the dirty bit.
    pub fn handle_client_active(&mut self) {
        if !self.internal_pings.baseline.submit(self, Some("active")) {
            log::info!("baseline ping not submitted on active");
        }

        self.set_dirty_flag(true);
    }

    /// Performs the collection/cleanup operations required by becoming inactive.
    ///
    /// This functions generates a baseline and an events ping with reason
    /// `inactive` and then clears the dirty bit.
    pub fn handle_client_inactive(&mut self) {
        if !self.internal_pings.baseline.submit(self, Some("inactive")) {
            log::info!("baseline ping not submitted on inactive");
        }

        if !self.internal_pings.events.submit(self, Some("inactive")) {
            log::info!("events ping not submitted on inactive");
        }

        self.set_dirty_flag(false);
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Checks if an experiment is currently active.
    ///
    /// # Arguments
    ///
    /// * `experiment_id` - The id of the experiment (maximum 30 bytes).
    ///
    /// # Returns
    ///
    /// Whether the experiment is active.
    pub fn test_is_experiment_active(&self, experiment_id: String) -> bool {
        self.test_get_experiment_data_as_json(experiment_id)
            .is_some()
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets stored data for the requested experiment.
    ///
    /// # Arguments
    ///
    /// * `experiment_id` - The id of the active experiment (maximum 30 bytes).
    ///
    /// # Returns
    ///
    /// A JSON string with the following format:
    ///
    ///   `{ 'branch': 'the-branch-name', 'extra': {'key': 'value', ...}}`
    ///
    /// if the requested experiment is active, `None` otherwise.
    pub fn test_get_experiment_data_as_json(&self, experiment_id: String) -> Option<String> {
        let metric = metrics::ExperimentMetric::new(self, experiment_id);
        metric.test_get_value_as_json_string(self)
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Deletes all stored metrics.
    ///
    /// Note that this also includes the ping sequence numbers, so it has
    /// the effect of resetting those to their initial values.
    pub fn test_clear_all_stores(&self) {
        if let Some(data) = self.data_store.as_ref() {
            data.clear_all()
        }
        // We don't care about this failing, maybe the data does just not exist.
        let _ = self.event_data_store.clear_all();
    }

    /// Instructs the Metrics Ping Scheduler's thread to exit cleanly.
    /// If Glean was configured with `use_core_mps: false`, this has no effect.
    pub fn cancel_metrics_ping_scheduler(&self) {
        if self.schedule_metrics_pings {
            scheduler::cancel();
        }
    }

    /// Instructs the Metrics Ping Scheduler to being scheduling metrics pings.
    /// If Glean wsa configured with `use_core_mps: false`, this has no effect.
    pub fn start_metrics_ping_scheduler(&self) {
        if self.schedule_metrics_pings {
            scheduler::schedule(self);
        }
    }
}

/// Returns a timestamp corresponding to "now" with millisecond precision.
pub fn get_timestamp_ms() -> u64 {
    const NANOS_PER_MILLI: u64 = 1_000_000;
    zeitstempel::now() / NANOS_PER_MILLI
}

// Split unit tests to a separate file, to reduce the file of this one.
#[cfg(test)]
#[path = "lib_unit_tests.rs"]
mod tests;
