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

use ffi_support::FfiStr;
use std::ffi::CString;
use std::sync::Mutex;

pub use glean_core::{Configuration, Error, Result};

pub mod metrics;

lazy_static::lazy_static! {
    static ref GLEAN_HANDLE: Mutex<u64> = Mutex::new(0);
}

fn with_glean<F, R>(f: F) -> R
where
    F: Fn(u64) -> R,
{
    let lock = GLEAN_HANDLE.lock().unwrap();
    debug_assert!(*lock != 0);
    f(*lock)
}

/// Create and initialize a new Glean object.
///
/// See `glean_core::Glean::new`.
pub fn initialize(cfg: Configuration) -> Result<()> {
    unsafe {
        let data_dir = CString::new(cfg.data_path).unwrap();
        let package_name = CString::new(cfg.application_id).unwrap();
        let upload_enabled = cfg.upload_enabled;
        let max_events = cfg.max_events.map(|m| m as i32);

        let ffi_cfg = glean_ffi::FfiConfiguration {
            data_dir: FfiStr::from_cstr(&data_dir),
            package_name: FfiStr::from_cstr(&package_name),
            upload_enabled: upload_enabled as u8,
            max_events: max_events.as_ref(),
            delay_ping_lifetime_io: false as u8,
        };

        let handle = glean_ffi::glean_initialize(&ffi_cfg);
        if handle == 0 {
            return Err(glean_core::Error::utf8_error());
        }

        let mut lock = GLEAN_HANDLE.lock().unwrap();
        *lock = handle;

        Ok(())
    }
}

/// Set whether upload is enabled or not.
///
/// See `glean_core::Glean.set_upload_enabled`.
pub fn set_upload_enabled(flag: bool) -> bool {
    with_glean(|glean_handle| {
        glean_ffi::glean_set_upload_enabled(glean_handle, flag as u8);
        glean_ffi::glean_is_upload_enabled(glean_handle) != 0
    })
}

/// Determine whether upload is enabled.
///
/// See `glean_core::Glean.is_upload_enabled`.
pub fn is_upload_enabled() -> bool {
    with_glean(|glean_handle| glean_ffi::glean_is_upload_enabled(glean_handle) != 0)
}

/// Register a new [`PingType`](metrics/struct.PingType.html).
pub fn register_ping_type(ping: &metrics::PingType) {
    with_glean(|glean_handle| {
        glean_ffi::ping_type::glean_register_ping_type(glean_handle, ping.handle);
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
    send_pings_by_name(&[ping])
}

/// Send multiple pings by name
///
/// ## Return value
///
/// Returns true if at least one ping was assembled and queued, false otherwise.
pub fn send_pings_by_name(pings: &[&str]) -> bool {
    let array: Vec<CString> = pings.iter().map(|s| CString::new(*s).unwrap()).collect();
    let ptr_array: Vec<*const _> = array.iter().map(|s| s.as_ptr()).collect();
    with_glean(|glean_handle| {
        let res = glean_ffi::glean_send_pings_by_name(
            glean_handle,
            ptr_array.as_ptr(),
            array.len() as i32,
        );
        res != 0
    })
}
