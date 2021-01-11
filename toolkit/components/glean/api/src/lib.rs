// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The public FOG APIs, for Rust consumers.

// Re-exporting for later use in generated code.
pub extern crate chrono;
pub extern crate once_cell;
pub extern crate uuid;

// Re-exporting for use in user tests.
pub use private::{DistributionData, ErrorType, RecordedEvent};

pub mod metrics;
pub mod pings;
pub mod private;

pub mod dispatcher;
pub mod ipc;

#[cfg(test)]
mod common_test;
mod ffi;

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
