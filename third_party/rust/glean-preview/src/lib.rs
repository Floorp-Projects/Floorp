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
//!
//! ## Example
//!
//! Initialize Glean, register a ping and then send it.
//!
//! ```rust,no_run
//! # use glean_preview::{Configuration, ClientInfoMetrics, Error, metrics::*};
//! # fn main() -> Result<(), Error> {
//! let cfg = Configuration {
//!     data_path: "/tmp/data".into(),
//!     application_id: "org.mozilla.glean_core.example".into(),
//!     upload_enabled: true,
//!     max_events: None,
//!     delay_ping_lifetime_io: false,
//!     channel: None,
//! };
//! glean_preview::initialize(cfg, ClientInfoMetrics::unknown())?;
//!
//! let prototype_ping = PingType::new("prototype", true, true);
//!
//! glean_preview::register_ping_type(&prototype_ping);
//!
//! prototype_ping.submit();
//! # Ok(())
//! # }
//! ```

use once_cell::sync::OnceCell;
use std::sync::Mutex;

pub use configuration::Configuration;
pub use core_metrics::ClientInfoMetrics;
pub use glean_core::{CommonMetricData, Error, Glean, Lifetime, Result};

mod configuration;
mod core_metrics;
pub mod metrics;
mod system;

static GLEAN: OnceCell<Mutex<Glean>> = OnceCell::new();

/// Get a reference to the global Glean object.
///
/// Panics if no global Glean object was set.
fn global_glean() -> &'static Mutex<Glean> {
    GLEAN.get().unwrap()
}

/// Set or replace the global Glean object.
fn setup_glean(glean: Glean) -> Result<()> {
    if GLEAN.get().is_none() {
        GLEAN.set(Mutex::new(glean)).unwrap();
    } else {
        let mut lock = GLEAN.get().unwrap().lock().unwrap();
        *lock = glean;
    }
    Ok(())
}

fn with_glean<F, R>(f: F) -> R
where
    F: Fn(&Glean) -> R,
{
    let lock = global_glean().lock().unwrap();
    f(&lock)
}

fn with_glean_mut<F, R>(f: F) -> R
where
    F: Fn(&mut Glean) -> R,
{
    let mut lock = global_glean().lock().unwrap();
    f(&mut lock)
}

/// Create and initialize a new Glean object.
///
/// See `glean_core::Glean::new`.
pub fn initialize(cfg: Configuration, client_info: ClientInfoMetrics) -> Result<()> {
    let channel = cfg.channel;
    let cfg = glean_core::Configuration {
        upload_enabled: cfg.upload_enabled,
        data_path: cfg.data_path,
        application_id: cfg.application_id,
        max_events: cfg.max_events,
        delay_ping_lifetime_io: cfg.delay_ping_lifetime_io,
    };
    let glean = Glean::new(cfg)?;

    // First initialize core metrics
    initialize_core_metrics(&glean, client_info, channel);
    // Now make this the global object available to others.
    setup_glean(glean)?;

    Ok(())
}

fn initialize_core_metrics(glean: &Glean, client_info: ClientInfoMetrics, channel: Option<String>) {
    let core_metrics = core_metrics::InternalMetrics::new();

    core_metrics.app_build.set(glean, client_info.app_build);
    core_metrics
        .app_display_version
        .set(glean, client_info.app_display_version);
    if let Some(app_channel) = channel {
        core_metrics.app_channel.set(glean, app_channel);
    }
    core_metrics.os.set(glean, system::OS.to_string());
    core_metrics.os_version.set(glean, "unknown".to_string());
    core_metrics
        .architecture
        .set(glean, system::ARCH.to_string());
    core_metrics
        .device_manufacturer
        .set(glean, "unknown".to_string());
    core_metrics.device_model.set(glean, "unknown".to_string());
}

/// Set whether upload is enabled or not.
///
/// See `glean_core::Glean.set_upload_enabled`.
pub fn set_upload_enabled(flag: bool) -> bool {
    with_glean_mut(|glean| {
        glean.set_upload_enabled(flag);
        glean.is_upload_enabled()
    })
}

/// Determine whether upload is enabled.
///
/// See `glean_core::Glean.is_upload_enabled`.
pub fn is_upload_enabled() -> bool {
    with_glean(|glean| glean.is_upload_enabled())
}

/// Register a new [`PingType`](metrics/struct.PingType.html).
pub fn register_ping_type(ping: &metrics::PingType) {
    with_glean_mut(|glean| {
        glean.register_ping_type(&ping.ping_type);
    })
}

/// Collect and submit a ping for eventual uploading.
///
/// See `glean_core::Glean.submit_ping`.
///
/// ## Return value
///
/// Returns true if a ping was assembled and queued, false otherwise.
pub fn submit_ping(ping: &metrics::PingType) -> bool {
    submit_ping_by_name(&ping.name)
}

/// Collect and submit a ping for eventual uploading by name.
///
/// See `glean_core::Glean.submit_ping_by_name`.
///
/// ## Return value
///
/// Returns true if a ping was assembled and queued, false otherwise.
pub fn submit_ping_by_name(ping: &str) -> bool {
    submit_pings_by_name(&[ping.to_string()])
}

/// Collect and submit multiple pings by name for eventual uploading.
///
/// ## Return value
///
/// Returns true if at least one ping was assembled and queued, false otherwise.
pub fn submit_pings_by_name(pings: &[String]) -> bool {
    with_glean(|glean| glean.send_pings_by_name(pings))
}
