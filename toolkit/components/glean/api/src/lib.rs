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

// Must appear before `metrics` or its use of `ffi`'s macros will fail.
#[macro_use]
mod ffi;

pub mod factory;
pub mod metrics;
pub mod pings;
pub mod private;

pub mod ipc;

#[cfg(test)]
mod common_test;
