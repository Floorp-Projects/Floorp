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
//! # use glean_preview::{Configuration, Error, metrics::*};
//! # fn main() -> Result<(), Error> {
//! let cfg = Configuration {
//!     data_path: "/tmp/data".into(),
//!     application_id: "org.mozilla.glean_core.example".into(),
//!     upload_enabled: true,
//!     max_events: None,
//!     delay_ping_lifetime_io: false,
//! };
//! glean_preview::initialize(cfg)?;
//!
//! let prototype_ping = PingType::new("prototype", true, true);
//!
//! glean_preview::register_ping_type(&prototype_ping);
//!
//! prototype_ping.send();
//! # Ok(())
//! # }
//! ```

use once_cell::sync::OnceCell;
use std::sync::Mutex;

pub use glean_core::{Configuration, Error, Glean, Result};

pub mod metrics;

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
pub fn initialize(cfg: Configuration) -> Result<()> {
    let glean = Glean::new(cfg)?;
    setup_glean(glean)?;

    Ok(())
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

/// Send a ping.
///
/// See `glean_core::Glean.send_ping`.
///
/// ## Return value
///
/// Returns true if a ping was assembled and queued, false otherwise.
pub fn send_ping(ping: &metrics::PingType) -> bool {
    send_ping_by_name(&ping.name)
}

/// Send a ping by name.
///
/// See `glean_core::Glean.send_ping_by_name`.
///
/// ## Return value
///
/// Returns true if a ping was assembled and queued, false otherwise.
pub fn send_ping_by_name(ping: &str) -> bool {
    send_pings_by_name(&[ping.to_string()])
}

/// Send multiple pings by name
///
/// ## Return value
///
/// Returns true if at least one ping was assembled and queued, false otherwise.
pub fn send_pings_by_name(pings: &[String]) -> bool {
    with_glean(|glean| glean.send_pings_by_name(pings))
}
