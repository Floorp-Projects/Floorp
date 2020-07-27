// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The public Glean SDK API, for Rust consumers.
//!
//! ## Example:
//!
//! ```rust,ignore
//! assert!(glean::is_upload_enabled())
//! ```

// Re-exporting for later use in generated code.
pub extern crate chrono;
pub extern crate once_cell;
pub extern crate uuid;

pub mod metrics;
pub mod ping_upload;
pub mod pings;
pub mod private;

pub mod ipc;

/// Run a closure with a mutable reference to the locked global Glean object.
fn with_glean<F, R>(f: F) -> R
where
    F: FnOnce(&mut glean_core::Glean) -> R,
{
    let mut lock = glean_core::global_glean()
        .expect("Global Glean object not initialized")
        .lock()
        .unwrap();
    f(&mut lock)
}

/// Determine whether upload is enabled.
///
/// See `glean_core::Glean.is_upload_enabled`.
pub fn is_upload_enabled() -> bool {
    with_glean(|glean| glean.is_upload_enabled())
}
