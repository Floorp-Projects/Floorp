/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///! Profiler Rust API
pub mod gecko_bindings;
mod label;
mod profiler_state;
mod thread;

pub use gecko_bindings::profiling_categories::*;
pub use label::*;
pub use profiler_macros::gecko_profiler_fn_label;
pub use profiler_state::*;
pub use thread::*;
