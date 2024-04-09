// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::{Arc, RwLock};
use std::time::Duration;

use crate::common_metric_data::CommonMetricDataInternal;
use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::metrics::time_unit::TimeUnit;
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A timespan metric.
///
/// Timespans are used to make a measurement of how much time is spent in a particular task.
///
// Implementation note:
// Because we dispatch this, we handle this with interior mutability.
// The whole struct is clonable, but that's comparable cheap, as it does not clone the data.
// Cloning `CommonMetricData` is not free, as it contains strings, so we also wrap that in an Arc.
#[derive(Clone, Debug)]
pub struct TimespanMetric {
    meta: Arc<CommonMetricDataInternal>,
    time_unit: TimeUnit,
    start_time: Arc<RwLock<Option<u64>>>,
}

impl MetricType for TimespanMetric {
    fn meta(&self) -> &CommonMetricDataInternal {
        &self.meta
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
            meta: Arc::new(meta.into()),
            time_unit,
            start_time: Arc::new(RwLock::new(None)),
        }
    }

    /// Starts tracking time for the provided metric.
    ///
    /// This records an error if it's already tracking time (i.e. start was
    /// already called with no corresponding
    /// [`set_stop`](TimespanMetric::set_stop)): in that case the original start
    /// time will be preserved.
    pub fn start(&self) {
        let start_time = time::precise_time_ns();

        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_start(glean, start_time));
    }

    /// Set start time synchronously.
    #[doc(hidden)]
    pub fn set_start(&self, glean: &Glean, start_time: u64) {
        if !self.should_record(glean) {
            return;
        }

        let mut lock = self
            .start_time
            .write()
            .expect("Lock poisoned for timespan metric on start.");

        if lock.is_some() {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidState,
                "Timespan already started",
                None,
            );
            return;
        }

        *lock = Some(start_time);
    }

    /// Stops tracking time for the provided metric. Sets the metric to the elapsed time.
    ///
    /// This will record an error if no [`set_start`](TimespanMetric::set_start) was called.
    pub fn stop(&self) {
        let stop_time = time::precise_time_ns();

        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_stop(glean, stop_time));
    }

    /// Set stop time synchronously.
    #[doc(hidden)]
    pub fn set_stop(&self, glean: &Glean, stop_time: u64) {
        // Need to write in either case, so get the lock first.
        let mut lock = self
            .start_time
            .write()
            .expect("Lock poisoned for timespan metric on stop.");

        if !self.should_record(glean) {
            // Reset timer when disabled, so that we don't record timespans across
            // disabled/enabled toggling.
            *lock = None;
            return;
        }

        if lock.is_none() {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidState,
                "Timespan not running",
                None,
            );
            return;
        }

        let start_time = lock.take().unwrap();
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
        self.set_raw_inner(glean, duration);
    }

    /// Aborts a previous [`set_start`](TimespanMetric::set_start) call. No
    /// error is recorded if no [`set_start`](TimespanMetric::set_start) was
    /// called.
    pub fn cancel(&self) {
        let metric = self.clone();
        crate::dispatcher::launch(move || {
            let mut lock = metric
                .start_time
                .write()
                .expect("Lock poisoned for timespan metric on cancel.");
            *lock = None;
        });
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
    pub fn set_raw(&self, elapsed: Duration) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_raw_sync(glean, elapsed));
    }

    /// Explicitly sets the timespan value in nanoseconds.
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
    /// * `elapsed_nanos` - The elapsed time to record, in nanoseconds.
    pub fn set_raw_nanos(&self, elapsed_nanos: i64) {
        let elapsed = Duration::from_nanos(elapsed_nanos.try_into().unwrap_or(0));
        self.set_raw(elapsed)
    }

    /// Explicitly sets the timespan value synchronously.
    #[doc(hidden)]
    pub fn set_raw_sync(&self, glean: &Glean, elapsed: Duration) {
        if !self.should_record(glean) {
            return;
        }

        let lock = self
            .start_time
            .read()
            .expect("Lock poisoned for timespan metric on set_raw.");

        if lock.is_some() {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidState,
                "Timespan already running. Raw value not recorded.",
                None,
            );
            return;
        }

        self.set_raw_inner(glean, elapsed);
    }

    fn set_raw_inner(&self, glean: &Glean, elapsed: Duration) {
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
    ///
    /// # Arguments
    ///
    /// * `ping_name` - the optional name of the ping to retrieve the metric
    ///                 for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The stored value or `None` if nothing stored.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<i64> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| {
            self.get_value(glean, ping_name.as_deref()).map(|val| {
                val.try_into()
                    .expect("Timespan can't be represented as i64")
            })
        })
    }

    /// Get the current value
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<u64> {
        let queried_ping_name = ping_name
            .into()
            .unwrap_or_else(|| &self.meta().inner.send_in_pings[0]);

        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.inner.lifetime,
        ) {
            Some(Metric::Timespan(time, time_unit)) => Some(time_unit.duration_convert(time)),
            _ => None,
        }
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        crate::block_on_dispatcher();

        crate::core::with_glean(|glean| {
            test_get_num_recorded_errors(glean, self.meta(), error).unwrap_or(0)
        })
    }
}
