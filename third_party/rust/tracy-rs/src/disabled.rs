/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A set of empty stub macros that are defined when the enable_profiler feature
// is not enabled. This ensures that we don't include callsite definitions in
// the resulting binary.

#[macro_export]
macro_rules! profile_scope {
    ($string:expr) => {}
}

#[macro_export]
macro_rules! tracy_frame_marker {
    () => {}
}

#[macro_export]
macro_rules! tracy_begin_frame {
    ($name:expr) => {}
}

#[macro_export]
macro_rules! tracy_end_frame {
    ($name:expr) => {}
}

#[macro_export]
macro_rules! tracy_plot {
    ($name:expr, $value:expr) => {}
}

pub unsafe fn load(_: &str) -> bool {
    println!("Can't load the tracy profiler unless enable_profiler feature is enabled!");
    false
}

pub fn register_thread_with_profiler(_: String) {
}
