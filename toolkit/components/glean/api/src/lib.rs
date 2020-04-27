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
pub extern crate once_cell;
pub extern crate uuid;

pub mod metrics;

/// Run a closure with a mutable reference to the locked global Glean object.
fn with_glean<F, R>(f: F) -> R
where
    F: FnOnce(&glean_core::Glean) -> R,
{
    let lock = glean_core::global_glean().lock().unwrap();
    f(&lock)
}

/// Determine whether upload is enabled.
///
/// See `glean_core::Glean.is_upload_enabled`.
pub fn is_upload_enabled() -> bool {
    with_glean(|glean| glean.is_upload_enabled())
}
