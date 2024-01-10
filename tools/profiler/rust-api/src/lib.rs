/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///! Profiler Rust API

#[macro_use]
extern crate lazy_static;

pub mod gecko_bindings;
mod json_writer;
mod label;
mod marker;
mod profiler_state;
mod thread;
mod time;

pub use gecko_bindings::profiling_categories::*;
pub use json_writer::*;
#[cfg(feature = "enabled")]
pub use label::*;
pub use marker::options::*;
pub use marker::schema::MarkerSchema;
pub use marker::*;
pub use profiler_macros::gecko_profiler_fn_label;
pub use profiler_state::*;
pub use thread::*;
pub use time::*;

pub use serde::{Deserialize, Serialize};
