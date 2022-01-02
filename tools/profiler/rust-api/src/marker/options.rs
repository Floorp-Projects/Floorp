/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Different options for the marker API.
//! See [`MarkerOptions`] and its fields.

use crate::gecko_bindings::{bindings, structs::mozilla};
use crate::ProfilerTime;
use std::mem::MaybeUninit;

/// Marker option that contains marker timing information.
/// This class encapsulates the logic for correctly storing a marker based on its
/// constructor types. Use the static methods to create the MarkerTiming. This is
/// a transient object that is being used to enforce the constraints of the
/// combinations of the data.
///
/// Implementation details: This is a RAII object that constructs and destroys a
/// C++ MarkerTiming object pointed to a specified reference. It allocates the
/// marker timing on stack and it's safe to move around because it's a
/// trivially-copyable object that only contains a few numbers.
#[derive(Debug)]
pub struct MarkerTiming(pub(crate) MaybeUninit<mozilla::MarkerTiming>);

impl MarkerTiming {
    /// Instant marker timing at a specific time.
    pub fn instant_at(time: ProfilerTime) -> MarkerTiming {
        let mut marker_timing = MaybeUninit::<mozilla::MarkerTiming>::uninit();
        unsafe {
            bindings::gecko_profiler_construct_marker_timing_instant_at(
                marker_timing.as_mut_ptr(),
                &time.0,
            );
        }
        MarkerTiming(marker_timing)
    }

    /// Instant marker timing at this time.
    pub fn instant_now() -> MarkerTiming {
        let mut marker_timing = MaybeUninit::<mozilla::MarkerTiming>::uninit();
        unsafe {
            bindings::gecko_profiler_construct_marker_timing_instant_now(
                marker_timing.as_mut_ptr(),
            );
        }
        MarkerTiming(marker_timing)
    }

    /// Interval marker timing with start and end times.
    pub fn interval(start_time: ProfilerTime, end_time: ProfilerTime) -> MarkerTiming {
        let mut marker_timing = MaybeUninit::<mozilla::MarkerTiming>::uninit();
        unsafe {
            bindings::gecko_profiler_construct_marker_timing_interval(
                marker_timing.as_mut_ptr(),
                &start_time.0,
                &end_time.0,
            );
        }
        MarkerTiming(marker_timing)
    }

    /// Interval marker with a start time and end time as "now".
    pub fn interval_until_now_from(start_time: ProfilerTime) -> MarkerTiming {
        let mut marker_timing = MaybeUninit::<mozilla::MarkerTiming>::uninit();
        unsafe {
            bindings::gecko_profiler_construct_marker_timing_interval_until_now_from(
                marker_timing.as_mut_ptr(),
                &start_time.0,
            );
        }
        MarkerTiming(marker_timing)
    }

    /// Interval start marker with only start time. This is a partial marker and
    /// it requires another marker with `instant_end` to be complete.
    pub fn interval_start(time: ProfilerTime) -> MarkerTiming {
        let mut marker_timing = MaybeUninit::<mozilla::MarkerTiming>::uninit();
        unsafe {
            bindings::gecko_profiler_construct_marker_timing_interval_start(
                marker_timing.as_mut_ptr(),
                &time.0,
            );
        }
        MarkerTiming(marker_timing)
    }

    /// Interval end marker with only end time. This is a partial marker and
    /// it requires another marker with `interval_start` to be complete.
    pub fn interval_end(time: ProfilerTime) -> MarkerTiming {
        let mut marker_timing = MaybeUninit::<mozilla::MarkerTiming>::uninit();
        unsafe {
            bindings::gecko_profiler_construct_marker_timing_interval_end(
                marker_timing.as_mut_ptr(),
                &time.0,
            );
        }
        MarkerTiming(marker_timing)
    }
}

impl Default for MarkerTiming {
    fn default() -> Self {
        MarkerTiming::instant_now()
    }
}

impl Drop for MarkerTiming {
    fn drop(&mut self) {
        unsafe {
            bindings::gecko_profiler_destruct_marker_timing(self.0.as_mut_ptr());
        }
    }
}

/// Marker option that contains marker stack information.
pub type MarkerStack = mozilla::StackCaptureOptions;

impl Default for MarkerStack {
    fn default() -> Self {
        MarkerStack::NoStack
    }
}

/// This class combines each of the possible marker options above.
/// Use Default::default() for the options that you don't want to provide or the
/// options you want to leave as default. Example usage:
///
/// ```rust
///  MarkerOptions {
///     timing: MarkerTiming::instant_now(),
///     ..Default::default()
///  }
/// ```
#[derive(Debug, Default)]
pub struct MarkerOptions {
    pub timing: MarkerTiming,
    pub stack: MarkerStack,
}
