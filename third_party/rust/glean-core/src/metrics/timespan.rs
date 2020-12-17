// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::time::Duration;

use crate::error_recording::{record_error, ErrorType};
use crate::metrics::time_unit::TimeUnit;
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A timespan metric.
///
/// Timespans are used to make a measurement of how much time is spent in a particular task.
#[derive(Debug)]
pub struct TimespanMetric {
    meta: CommonMetricData,
    time_unit: TimeUnit,
    start_time: Option<u64>,
}

impl MetricType for TimespanMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn meta_mut(&mut self) -> &mut CommonMetricData {
        &mut self.meta
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl TimespanMetric {
    /// Creates a new timespan metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        Self {
            meta,
            time_unit,
            start_time: None,
        }
    }

    /// Starts tracking time for the provided metric.
    ///
    /// This records an error if it's already tracking time (i.e. start was
    /// already called with no corresponding
    /// [`set_stop`](TimespanMetric::set_stop)): in that case the original start
    /// time will be preserved.
    pub fn set_start(&mut self, glean: &Glean, start_time: u64) {
        if !self.should_record(glean) {
            return;
        }

        if self.start_time.is_some() {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidState,
                "Timespan already started",
                None,
            );
            return;
        }

        self.start_time = Some(start_time);
    }

    /// Stops tracking time for the provided metric. Sets the metric to the elapsed time.
    ///
    /// This will record an error if no [`set_start`](TimespanMetric::set_start) was called.
    pub fn set_stop(&mut self, glean: &Glean, stop_time: u64) {
        if !self.should_record(glean) {
            // Reset timer when disabled, so that we don't record timespans across
            // disabled/enabled toggling.
            self.start_time = None;
            return;
        }

        if self.start_time.is_none() {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidState,
                "Timespan not running",
                None,
            );
            return;
        }

        let start_time = self.start_time.take().unwrap();
        let duration = match stop_time.checked_sub(start_time) {
            Some(duration) => duration,
            None => {
                record_error(
                    glean,
                    &self.meta,
                    ErrorType::InvalidValue,
                    "Timespan was negative",
                    None,
                );
                return;
            }
        };
        let duration = Duration::from_nanos(duration);
        self.set_raw(glean, duration);
    }

    /// Aborts a previous [`set_start`](TimespanMetric::set_start) call. No
    /// error is recorded if no [`set_start`](TimespanMetric::set_start) was
    /// called.
    pub fn cancel(&mut self) {
        self.start_time = None;
    }

    /// Explicitly sets the timespan value.
    ///
    /// This API should only be used if your library or application requires
    /// recording times in a way that can not make use of
    /// [`set_start`](TimespanMetric::set_start)/[`set_stop`](TimespanMetric::set_stop)/[`cancel`](TimespanMetric::cancel).
    ///
    /// Care should be taken using this if the ping lifetime might contain more
    /// than one timespan measurement. To be safe,
    /// [`set_raw`](TimespanMetric::set_raw) should generally be followed by
    /// sending a custom ping containing the timespan.
    ///
    /// # Arguments
    ///
    /// * `elapsed` - The elapsed time to record.
    pub fn set_raw(&self, glean: &Glean, elapsed: Duration) {
        if !self.should_record(glean) {
            return;
        }

        if self.start_time.is_some() {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidState,
                "Timespan already running. Raw value not recorded.",
                None,
            );
            return;
        }

        let mut report_value_exists: bool = false;
        glean.storage().record_with(glean, &self.meta, |old_value| {
            match old_value {
                Some(old @ Metric::Timespan(..)) => {
                    // If some value already exists, report an error.
                    // We do this out of the storage since recording an
                    // error accesses the storage as well.
                    report_value_exists = true;
                    old
                }
                _ => Metric::Timespan(elapsed, self.time_unit),
            }
        });

        if report_value_exists {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidState,
                "Timespan value already recorded. New value discarded.",
                None,
            );
        };
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as an integer.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<u64> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Timespan(time, time_unit)) => Some(time_unit.duration_convert(time)),
            _ => None,
        }
    }
}
