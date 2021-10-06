/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Gecko profiler time.

use crate::gecko_bindings::{bindings, structs::mozilla};
use std::mem::MaybeUninit;

/// Profiler time for the marker API.
/// This should be used as the `MarkerTiming` parameter.
/// E.g.:
///
/// ```
/// let start = ProfilerTime::now();
/// // ...some code...
/// gecko_profiler::add_untyped_marker(
///     "marker name",
///     category,
///     MarkerOptions {
///         timing: MarkerTiming::interval_until_now_from(start),
///         ..Default::default()
///     },
/// );
/// ```
#[derive(Debug)]
pub struct ProfilerTime(pub(crate) mozilla::TimeStamp);

impl ProfilerTime {
    pub fn now() -> ProfilerTime {
        let mut marker_timing = MaybeUninit::<mozilla::TimeStamp>::uninit();
        unsafe {
            bindings::gecko_profiler_construct_timestamp_now(marker_timing.as_mut_ptr());
            ProfilerTime(marker_timing.assume_init())
        }
    }
}

impl Drop for ProfilerTime {
    fn drop(&mut self) {
        unsafe {
            bindings::gecko_profiler_destruct_timestamp(&mut self.0);
        }
    }
}
