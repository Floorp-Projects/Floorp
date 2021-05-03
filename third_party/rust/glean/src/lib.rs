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
//!     channel: None,
//!     server_endpoint: None,
//!     uploader: None,
//! };
//! glean::initialize(cfg, ClientInfoMetrics::unknown());
//!
//! let prototype_ping = PingType::new("prototype", true, true, vec!());
//!
//! glean::register_ping_type(&prototype_ping);
//!
//! prototype_ping.submit(None);
//! ```

use once_cell::sync::OnceCell;
use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

pub use configuration::Configuration;
use configuration::DEFAULT_GLEAN_ENDPOINT;
pub use core_metrics::ClientInfoMetrics;
use glean_core::global_glean;
pub use glean_core::{
    metrics::{Datetime, DistributionData, MemoryUnit, RecordedEvent, TimeUnit, TimerId},
    traits, CommonMetricData, Error, ErrorType, Glean, HistogramType, Lifetime, Result,
};
use private::RecordedExperimentData;

mod configuration;
mod core_metrics;
mod dispatcher;
mod glean_metrics;
pub mod net;
pub mod private;
mod system;

#[cfg(test)]
mod common_test;

const LANGUAGE_BINDING_NAME: &str = "Rust";

/// State to keep track for the Rust Language bindings.
///
/// This is useful for setting Glean SDK-owned metrics when
/// the state of the upload is toggled.
#[derive(Debug)]
struct RustBindingsState {
    /// The channel the application is being distributed on.
    channel: Option<String>,

    /// Client info metrics set by the application.
    client_info: ClientInfoMetrics,

    /// An instance of the upload manager
    upload_manager: net::UploadManager,
}

/// Set when `glean::initialize()` returns.
/// This allows to detect calls that happen before `glean::initialize()` was called.
/// Note: The initialization might still be in progress, as it runs in a separate thread.
static INITIALIZE_CALLED: AtomicBool = AtomicBool::new(false);

/// Keep track of the debug features before Glean is initialized.
static PRE_INIT_DEBUG_VIEW_TAG: OnceCell<Mutex<String>> = OnceCell::new();
static PRE_INIT_LOG_PINGS: AtomicBool = AtomicBool::new(false);
static PRE_INIT_SOURCE_TAGS: OnceCell<Mutex<Vec<String>>> = OnceCell::new();

/// Keep track of pings registered before Glean is initialized.
static PRE_INIT_PING_REGISTRATION: OnceCell<Mutex<Vec<private::PingType>>> = OnceCell::new();

/// A global singleton storing additional state for Glean.
///
/// Requires a Mutex, because in tests we can actual reset this.
static STATE: OnceCell<Mutex<RustBindingsState>> = OnceCell::new();

/// Get a reference to the global state object.
///
/// Panics if no global state object was set.
fn global_state() -> &'static Mutex<RustBindingsState> {
    STATE.get().unwrap()
}

/// Set or replace the global bindings State object.
fn setup_state(state: RustBindingsState) {
    // The `OnceCell` type wrapping our state is thread-safe and can only be set once.
    // Therefore even if our check for it being empty succeeds, setting it could fail if a
    // concurrent thread is quicker in setting it.
    // However this will not cause a bigger problem, as the second `set` operation will just fail.
    // We can log it and move on.
    //
    // For all wrappers this is not a problem, as the State object is intialized exactly once on
    // calling `initialize` on the global singleton and further operations check that it has been
    // initialized.
    if STATE.get().is_none() {
        if STATE.set(Mutex::new(state)).is_err() {
            log::error!(
                "Global Glean state object is initialized already. This probably happened concurrently."
            );
        }
    } else {
        // We allow overriding the global State object to support test mode.
        // In test mode the State object is fully destroyed and recreated.
        // This all happens behind a mutex and is therefore also thread-safe.
        let mut lock = STATE.get().unwrap().lock().unwrap();
        *lock = state;
    }
}

fn with_glean<F, R>(f: F) -> R
where
    F: FnOnce(&Glean) -> R,
{
    let glean = global_glean().expect("Global Glean object not initialized");
    let lock = glean.lock().unwrap();
    f(&lock)
}

fn with_glean_mut<F, R>(f: F) -> R
where
    F: FnOnce(&mut Glean) -> R,
{
    let glean = global_glean().expect("Global Glean object not initialized");
    let mut lock = glean.lock().unwrap();
    f(&mut lock)
}

/// Launches a new task on the global dispatch queue with a reference to the Glean singleton.
fn launch_with_glean(callback: impl FnOnce(&Glean) + Send + 'static) {
    dispatcher::launch(|| crate::with_glean(callback));
}

/// Launches a new task on the global dispatch queue with a mutable reference to the
/// Glean singleton.
fn launch_with_glean_mut(callback: impl FnOnce(&mut Glean) + Send + 'static) {
    dispatcher::launch(|| crate::with_glean_mut(callback));
}

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
    if was_initialize_called() {
        log::error!("Glean should not be initialized multiple times");
        return;
    }

    std::thread::Builder::new()
        .name("glean.init".into())
        .spawn(move || {
            let core_cfg = glean_core::Configuration {
                upload_enabled: cfg.upload_enabled,
                data_path: cfg.data_path,
                application_id: cfg.application_id.clone(),
                language_binding_name: LANGUAGE_BINDING_NAME.into(),
                max_events: cfg.max_events,
                delay_ping_lifetime_io: cfg.delay_ping_lifetime_io,
            };

            let glean = match Glean::new(core_cfg) {
                Ok(glean) => glean,
                Err(err) => {
                    log::error!("Failed to initialize Glean: {}", err);
                    return;
                }
            };

            // glean-core already takes care of logging errors: other bindings
            // simply do early returns, as we're doing.
            if glean_core::setup_glean(glean).is_err() {
                return;
            }

            log::info!("Glean initialized");

            // Initialize the ping uploader.
            let upload_manager = net::UploadManager::new(
                cfg.server_endpoint
                    .unwrap_or_else(|| DEFAULT_GLEAN_ENDPOINT.to_string()),
                cfg.uploader
                    .unwrap_or_else(|| Box::new(net::HttpUploader) as Box<dyn net::PingUploader>),
            );

            // Now make this the global object available to others.
            setup_state(RustBindingsState {
                channel: cfg.channel,
                client_info,
                upload_manager,
            });

            let upload_enabled = cfg.upload_enabled;

            with_glean_mut(|glean| {
                let state = global_state().lock().unwrap();

                // The debug view tag might have been set before initialize,
                // get the cached value and set it.
                if let Some(tag) = PRE_INIT_DEBUG_VIEW_TAG.get() {
                    let lock = tag.try_lock();
                    if let Ok(ref debug_tag) = lock {
                        glean.set_debug_view_tag(debug_tag);
                    }
                }
                // The log pings debug option might have been set before initialize,
                // get the cached value and set it.
                let log_pigs = PRE_INIT_LOG_PINGS.load(Ordering::SeqCst);
                if log_pigs {
                    glean.set_log_pings(log_pigs);
                }
                // The source tags might have been set before initialize,
                // get the cached value and set them.
                if let Some(tags) = PRE_INIT_SOURCE_TAGS.get() {
                    let lock = tags.try_lock();
                    if let Ok(ref source_tags) = lock {
                        glean.set_source_tags(source_tags.to_vec());
                    }
                }

                // Get the current value of the dirty flag so we know whether to
                // send a dirty startup baseline ping below.  Immediately set it to
                // `false` so that dirty startup pings won't be sent if Glean
                // initialization does not complete successfully.
                // TODO Bug 1672956 will decide where to set this flag again.
                let dirty_flag = glean.is_dirty_flag_set();
                glean.set_dirty_flag(false);

                // Register builtin pings.
                // Unfortunately we need to manually list them here to guarantee
                // they are registered synchronously before we need them.
                // We don't need to handle the deletion-request ping. It's never touched
                // from the language implementation.
                //
                // Note: this will actually double-register them.
                // On instantiation they will launch a task to register themselves.
                // That task could fail to run if the dispatcher queue is full.
                // We still register them here synchronously.
                glean.register_ping_type(&glean_metrics::pings::baseline.ping_type);
                glean.register_ping_type(&glean_metrics::pings::metrics.ping_type);
                glean.register_ping_type(&glean_metrics::pings::events.ping_type);

                // Perform registration of pings that were attempted to be
                // registered before init.
                if let Some(tags) = PRE_INIT_PING_REGISTRATION.get() {
                    let lock = tags.try_lock();
                    if let Ok(pings) = lock {
                        for ping in &*pings {
                            glean.register_ping_type(&ping.ping_type);
                        }
                    }
                }

                // If this is the first time ever the Glean SDK runs, make sure to set
                // some initial core metrics in case we need to generate early pings.
                // The next times we start, we would have them around already.
                let is_first_run = glean.is_first_run();
                if is_first_run {
                    initialize_core_metrics(&glean, &state.client_info, state.channel.clone());
                }

                // Deal with any pending events so we can start recording new ones
                let pings_submitted = glean.on_ready_to_submit_pings();

                // We need to kick off upload in these cases:
                // 1. Pings were submitted through Glean and it is ready to upload those pings;
                // 2. Upload is disabled, to upload a possible deletion-request ping.
                if pings_submitted || !upload_enabled {
                    state.upload_manager.trigger_upload();
                }

                // Set up information and scheduling for Glean owned pings. Ideally, the "metrics"
                // ping startup check should be performed before any other ping, since it relies
                // on being dispatched to the API context before any other metric.
                // TODO: start the metrics ping scheduler, will happen in bug 1672951.

                // Check if the "dirty flag" is set. That means the product was probably
                // force-closed. If that's the case, submit a 'baseline' ping with the
                // reason "dirty_startup". We only do that from the second run.
                if !is_first_run && dirty_flag {
                    // The `submit_ping_by_name_sync` function cannot be used, otherwise
                    // startup will cause a dead-lock, since that function requests a
                    // write lock on the `glean` object.
                    // Note that unwrapping below is safe: the function will return an
                    // `Ok` value for a known ping.
                    if glean
                        .submit_ping_by_name("baseline", Some("dirty_startup"))
                        .unwrap()
                    {
                        state.upload_manager.trigger_upload();
                    }
                }

                // From the second time we run, after all startup pings are generated,
                // make sure to clear `lifetime: application` metrics and set them again.
                // Any new value will be sent in newly generated pings after startup.
                if !is_first_run {
                    glean.clear_application_lifetime_metrics();
                    initialize_core_metrics(&glean, &state.client_info, state.channel.clone());
                }
            });

            // Signal Dispatcher that init is complete
            match dispatcher::flush_init() {
                Ok(task_count) if task_count > 0 => {
                    with_glean(|glean| {
                        glean_metrics::error::preinit_tasks_overflow
                            .add_sync(&glean, task_count as i32);
                    });
                }
                Ok(_) => {}
                Err(err) => log::error!("Unable to flush the preinit queue: {}", err),
            }
        })
        .expect("Failed to spawn Glean's init thread");

    // Mark the initialization as called: this needs to happen outside of the
    // dispatched block!
    INITIALIZE_CALLED.store(true, Ordering::SeqCst);
}

/// Shuts down Glean.
///
/// This currently only attempts to shut down the
/// internal dispatcher.
pub fn shutdown() {
    if global_glean().is_none() {
        log::warn!("Shutdown called before Glean is initialized");
        if let Err(e) = dispatcher::kill() {
            log::error!("Can't kill dispatcher thread: {:?}", e);
        }

        return;
    }

    crate::launch_with_glean_mut(|glean| {
        glean.set_dirty_flag(false);
    });

    if let Err(e) = dispatcher::shutdown() {
        log::error!("Can't shutdown dispatcher thread: {:?}", e);
    }
}

/// Unblock the global dispatcher to start processing queued tasks.
///
/// This should _only_ be called if it is guaranteed that `initialize` will never be called.
///
/// **Note**: Exported as a FFI function to be used by other language bindings (e.g. Kotlin/Swift)
/// to unblock the RLB-internal dispatcher.
/// This allows the usage of both the RLB and other language bindings (e.g. Kotlin/Swift)
/// within the same application.
#[no_mangle]
#[inline(never)]
pub extern "C" fn rlb_flush_dispatcher() {
    log::trace!("FLushing RLB dispatcher through the FFI");

    let was_initialized = was_initialize_called();

    // Panic in debug mode
    debug_assert!(!was_initialized);

    // In release do a check and bail out
    if was_initialized {
        log::error!(
            "Tried to flush the dispatcher from outside, but Glean was initialized in the RLB."
        );
        return;
    }

    if let Err(err) = dispatcher::flush_init() {
        log::error!("Unable to flush the preinit queue: {}", err);
    }
}

/// Block on the dispatcher emptying.
///
/// This will panic if called before Glean is initialized.
fn block_on_dispatcher() {
    assert!(
        was_initialize_called(),
        "initialize was never called. Can't block on the dispatcher queue."
    );
    dispatcher::block_on_queue()
}

/// Checks if [`initialize`] was ever called.
///
/// # Returns
///
/// `true` if it was, `false` otherwise.
fn was_initialize_called() -> bool {
    INITIALIZE_CALLED.load(Ordering::SeqCst)
}

fn initialize_core_metrics(
    glean: &Glean,
    client_info: &ClientInfoMetrics,
    channel: Option<String>,
) {
    core_metrics::internal_metrics::app_build.set_sync(glean, &client_info.app_build[..]);
    core_metrics::internal_metrics::app_display_version
        .set_sync(glean, &client_info.app_display_version[..]);
    if let Some(app_channel) = channel {
        core_metrics::internal_metrics::app_channel.set_sync(glean, app_channel);
    }
    core_metrics::internal_metrics::os_version.set_sync(glean, system::get_os_version());
    core_metrics::internal_metrics::architecture.set_sync(glean, system::ARCH.to_string());
    core_metrics::internal_metrics::device_manufacturer.set_sync(glean, "unknown".to_string());
    core_metrics::internal_metrics::device_model.set_sync(glean, "unknown".to_string());
}

/// Sets whether upload is enabled or not.
///
/// See [`glean_core::Glean::set_upload_enabled`].
pub fn set_upload_enabled(enabled: bool) {
    if !was_initialize_called() {
        let msg =
            "Changing upload enabled before Glean is initialized is not supported.\n \
            Pass the correct state into `Glean.initialize()`.\n \
            See documentation at https://mozilla.github.io/glean/book/user/general-api.html#initializing-the-glean-sdk";
        log::error!("{}", msg);
        return;
    }

    // Changing upload enabled always happens asynchronous.
    // That way it follows what a user expect when calling it inbetween other calls:
    // it executes in the right order.
    //
    // Because the dispatch queue is halted until Glean is fully initialized
    // we can safely enqueue here and it will execute after initialization.
    crate::launch_with_glean_mut(move |glean| {
        let state = global_state().lock().unwrap();
        let old_enabled = glean.is_upload_enabled();
        glean.set_upload_enabled(enabled);

        // TODO: Cancel upload and any outstanding metrics ping scheduler
        // task. Will happen on bug 1672951.

        if !old_enabled && enabled {
            // If uploading is being re-enabled, we have to restore the
            // application-lifetime metrics.
            initialize_core_metrics(&glean, &state.client_info, state.channel.clone());
        }

        if old_enabled && !enabled {
            // If uploading is disabled, we need to send the deletion-request ping:
            // note that glean-core takes care of generating it.
            state.upload_manager.trigger_upload();
        }
    });
}

/// Register a new [`PingType`](private::PingType).
pub fn register_ping_type(ping: &private::PingType) {
    // If this happens after Glean.initialize is called (and returns),
    // we dispatch ping registration on the thread pool.
    // Registering a ping should not block the application.
    // Submission itself is also dispatched, so it will always come after the registration.
    if was_initialize_called() {
        let ping = ping.clone();
        crate::launch_with_glean_mut(move |glean| {
            glean.register_ping_type(&ping.ping_type);
        })
    } else {
        // We need to keep track of pings, so they get re-registered after a reset or
        // if ping registration is attempted before Glean initializes.
        // This state is kept across Glean resets, which should only ever happen in test mode.
        // It's a set and keeping them around forever should not have much of an impact.
        let m = PRE_INIT_PING_REGISTRATION.get_or_init(Default::default);
        let mut lock = m.lock().unwrap();
        lock.push(ping.clone());
    }
}

/// Collects and submits a ping for eventual uploading.
///
/// See [`glean_core::Glean.submit_ping`].
pub(crate) fn submit_ping(ping: &private::PingType, reason: Option<&str>) {
    submit_ping_by_name(&ping.name, reason)
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
    dispatcher::launch(move || {
        submit_ping_by_name_sync(&ping, reason.as_deref());
    })
}

/// Collect and submit a ping (by its name) for eventual upload, synchronously.
///
/// The ping will be looked up in the known instances of [`private::PingType`]. If the
/// ping isn't known, an error is logged and the ping isn't queued for uploading.
///
/// The ping content is assembled as soon as possible, but upload is not
/// guaranteed to happen immediately, as that depends on the upload
/// policies.
///
/// If the ping currently contains no content, it will not be assembled and
/// queued for sending, unless explicitly specified otherwise in the registry
/// file.
///
/// # Arguments
///
/// * `ping_name` - the name of the ping to submit.
/// * `reason` - the reason the ping is being submitted.
pub(crate) fn submit_ping_by_name_sync(ping: &str, reason: Option<&str>) {
    if !was_initialize_called() {
        log::error!("Glean must be initialized before submitting pings.");
        return;
    }

    let submitted_ping = with_glean(|glean| {
        if !glean.is_upload_enabled() {
            log::info!("Glean disabled: not submitting any pings.");
            // This won't actually return from `submit_ping_by_name`, but
            // returning `false` here skips spinning up the uploader below,
            // which is basically the same.
            return Some(false);
        }

        glean.submit_ping_by_name(&ping, reason.as_deref()).ok()
    });

    if let Some(true) = submitted_ping {
        let state = global_state().lock().unwrap();
        state.upload_manager.trigger_upload();
    }
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
    crate::launch_with_glean(move |glean| {
        glean.set_experiment_active(experiment_id.to_owned(), branch.to_owned(), extra)
    })
}

/// Indicate that an experiment is no longer running.
///
/// See [`glean_core::Glean::set_experiment_inactive`].
pub fn set_experiment_inactive(experiment_id: String) {
    crate::launch_with_glean(move |glean| glean.set_experiment_inactive(experiment_id))
}

/// Performs the collection/cleanup operations required by becoming active.
///
/// This functions generates a baseline ping with reason `active`
/// and then sets the dirty bit.
/// This should be called whenever the consuming product becomes active (e.g.
/// getting to foreground).
pub fn handle_client_active() {
    crate::launch_with_glean_mut(|glean| {
        glean.handle_client_active();

        // The above call may generate pings, so we need to trigger
        // the uploader. It's fine to trigger it if no ping was generated:
        // it will bail out.
        let state = global_state().lock().unwrap();
        state.upload_manager.trigger_upload();
    });

    // The previous block of code may send a ping containing the `duration` metric,
    // in `glean.handle_client_active`. We intentionally start recording a new
    // `duration` after that happens, so that the measurement gets reported when
    // calling `handle_client_inactive`.
    core_metrics::internal_metrics::baseline_duration.start();
}

/// Performs the collection/cleanup operations required by becoming inactive.
///
/// This functions generates a baseline and an events ping with reason
/// `inactive` and then clears the dirty bit.
/// This should be called whenever the consuming product becomes inactive (e.g.
/// getting to background).
pub fn handle_client_inactive() {
    // This needs to be called before the `handle_client_inactive` api: it stops
    // measuring the duration of the previous activity time, before any ping is sent
    // by the next call.
    core_metrics::internal_metrics::baseline_duration.stop();

    crate::launch_with_glean_mut(|glean| {
        glean.handle_client_inactive();

        // The above call may generate pings, so we need to trigger
        // the uploader. It's fine to trigger it if no ping was generated:
        // it will bail out.
        let state = global_state().lock().unwrap();
        state.upload_manager.trigger_upload();
    })
}

/// TEST ONLY FUNCTION.
/// Checks if an experiment is currently active.
#[allow(dead_code)]
pub(crate) fn test_is_experiment_active(experiment_id: String) -> bool {
    block_on_dispatcher();
    with_glean(|glean| glean.test_is_experiment_active(experiment_id.to_owned()))
}

/// TEST ONLY FUNCTION.
/// Returns the [`RecordedExperimentData`] for the given `experiment_id` or panics if
/// the id isn't found.
#[allow(dead_code)]
pub(crate) fn test_get_experiment_data(experiment_id: String) -> RecordedExperimentData {
    block_on_dispatcher();
    with_glean(|glean| {
        let json_data = glean
            .test_get_experiment_data_as_json(experiment_id.to_owned())
            .unwrap_or_else(|| panic!("No experiment found for id: {}", experiment_id));
        serde_json::from_str::<RecordedExperimentData>(&json_data).unwrap()
    })
}

/// Destroy the global Glean state.
pub(crate) fn destroy_glean(clear_stores: bool) {
    // Destroy the existing glean instance from glean-core.
    if was_initialize_called() {
        // We need to check if the Glean object (from glean-core) is
        // initialized, otherwise this will crash on the first test
        // due to bug 1675215 (this check can be removed once that
        // bug is fixed).
        if global_glean().is_some() {
            with_glean_mut(|glean| {
                if clear_stores {
                    glean.test_clear_all_stores()
                }
                glean.destroy_db()
            });
        }
        // Allow us to go through initialization again.
        INITIALIZE_CALLED.store(false, Ordering::SeqCst);
        // Reset the dispatcher.
        dispatcher::reset_dispatcher();
    }
}

/// TEST ONLY FUNCTION.
/// Resets the Glean state and triggers init again.
pub fn test_reset_glean(cfg: Configuration, client_info: ClientInfoMetrics, clear_stores: bool) {
    destroy_glean(clear_stores);

    // Always log pings for tests
    //Glean.setLogPings(true)
    initialize(cfg, client_info);
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
    if was_initialize_called() {
        with_glean_mut(|glean| glean.set_debug_view_tag(tag))
    } else {
        // Glean has not been initialized yet. Cache the provided tag value.
        let m = PRE_INIT_DEBUG_VIEW_TAG.get_or_init(Default::default);
        let mut lock = m.lock().unwrap();
        *lock = tag.to_string();
        // When setting the debug view tag before initialization,
        // we don't validate the tag, thus this function always returns true.
        true
    }
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
    if was_initialize_called() {
        with_glean_mut(|glean| glean.set_log_pings(value));
    } else {
        PRE_INIT_LOG_PINGS.store(value, Ordering::SeqCst);
    }
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
///
/// # Returns
///
/// This will return `false` in case `value` contains invalid tags and `true`
/// otherwise or if the tag is set before Glean is initialized.
pub fn set_source_tags(tags: Vec<String>) -> bool {
    if was_initialize_called() {
        with_glean_mut(|glean| glean.set_source_tags(tags))
    } else {
        // Glean has not been initialized yet. Cache the provided source tags.
        let m = PRE_INIT_SOURCE_TAGS.get_or_init(Default::default);
        let mut lock = m.lock().unwrap();
        *lock = tags;
        // When setting the source tags before initialization,
        // we don't validate the tags, thus this function always returns true.
        true
    }
}

/// Returns a timestamp corresponding to "now" with millisecond precision.
pub fn get_timestamp_ms() -> u64 {
    glean_core::get_timestamp_ms()
}

#[cfg(test)]
mod test;
