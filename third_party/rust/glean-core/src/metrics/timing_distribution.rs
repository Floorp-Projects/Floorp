// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;

use crate::error_recording::{record_error, ErrorType};
use crate::histogram::{Functional, Histogram};
use crate::metrics::time_unit::TimeUnit;
use crate::metrics::{DistributionData, Metric, MetricType};
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

// The base of the logarithm used to determine bucketing
const LOG_BASE: f64 = 2.0;

// The buckets per each order of magnitude of the logarithm.
const BUCKETS_PER_MAGNITUDE: f64 = 8.0;

// Maximum time, which means we retain a maximum of 316 buckets.
// It is automatically adjusted based on the `time_unit` parameter
// so that:
//
// - `nanosecond` - 10 minutes
// - `microsecond` - ~6.94 days
// - `millisecond` - ~19 years
const MAX_SAMPLE_TIME: u64 = 1000 * 1000 * 1000 * 60 * 10;

/// Identifier for a running timer.
pub type TimerId = u64;

#[derive(Debug, Clone)]
struct Timings {
    next_id: TimerId,
    start_times: HashMap<TimerId, u64>,
}

/// Track different running timers, identified by a `TimerId`.
impl Timings {
    /// Create a new timing manager.
    fn new() -> Self {
        Self {
            next_id: 0,
            start_times: HashMap::new(),
        }
    }

    /// Start a new timer and set it to the `start_time`.
    /// Multiple timers can run simultaneously.
    ///
    /// Returns a new [`TimerId`] identifying the timer.
    fn set_start(&mut self, start_time: u64) -> TimerId {
        let id = self.next_id;
        self.next_id += 1;
        self.start_times.insert(id, start_time);
        id
    }

    /// Stop the timer and return the elapsed time.
    ///
    /// Returns an error if the `id` does not correspond to a running timer.
    /// Returns an error if the stop time is before the start time.
    ///
    /// ## Note
    ///
    /// This API exists to satisfy the FFI requirements, where the clock is handled on the
    /// application side and passed in as a timestamp.
    fn set_stop(&mut self, id: TimerId, stop_time: u64) -> Result<u64, (ErrorType, &str)> {
        let start_time = match self.start_times.remove(&id) {
            Some(start_time) => start_time,
            None => return Err((ErrorType::InvalidState, "Timing not running")),
        };

        let duration = match stop_time.checked_sub(start_time) {
            Some(duration) => duration,
            None => {
                return Err((
                    ErrorType::InvalidValue,
                    "Timer stopped with negative duration",
                ))
            }
        };

        Ok(duration)
    }

    /// Cancel and remove the timer.
    fn cancel(&mut self, id: TimerId) {
        self.start_times.remove(&id);
    }
}

/// A timing distribution metric.
///
/// Timing distributions are used to accumulate and store time measurement, for analyzing distributions of the timing data.
#[derive(Debug)]
pub struct TimingDistributionMetric {
    meta: CommonMetricData,
    time_unit: TimeUnit,
    timings: Timings,
}

/// Create a snapshot of the histogram with a time unit.
///
/// The snapshot can be serialized into the payload format.
pub(crate) fn snapshot(hist: &Histogram<Functional>) -> DistributionData {
    DistributionData {
        // **Caution**: This cannot use `Histogram::snapshot_values` and needs to use the more
        // specialized snapshot function.
        values: hist.snapshot(),
        sum: hist.sum(),
    }
}

impl MetricType for TimingDistributionMetric {
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
impl TimingDistributionMetric {
    /// Creates a new timing distribution metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        Self {
            meta,
            time_unit,
            timings: Timings::new(),
        }
    }

    /// Starts tracking time for the provided metric.
    ///
    /// This records an error if it’s already tracking time (i.e.
    /// [`set_start`](TimingDistributionMetric::set_start) was already called with no
    /// corresponding [`set_stop_and_accumulate`](TimingDistributionMetric::set_stop_and_accumulate)): in
    /// that case the original start time will be preserved.
    ///
    /// # Arguments
    ///
    /// * `start_time` - Timestamp in nanoseconds.
    ///
    /// # Returns
    ///
    /// A unique [`TimerId`] for the new timer.
    pub fn set_start(&mut self, start_time: u64) -> TimerId {
        self.timings.set_start(start_time)
    }

    /// Stops tracking time for the provided metric and associated timer id.
    ///
    /// Adds a count to the corresponding bucket in the timing distribution.
    /// This will record an error if no
    /// [`set_start`](TimingDistributionMetric::set_start) was called.
    ///
    /// # Arguments
    ///
    /// * `id` - The [`TimerId`] to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timespan metric.
    /// * `stop_time` - Timestamp in nanoseconds.
    pub fn set_stop_and_accumulate(&mut self, glean: &Glean, id: TimerId, stop_time: u64) {
        // Duration is in nanoseconds.
        let mut duration = match self.timings.set_stop(id, stop_time) {
            Err((err_type, err_msg)) => {
                record_error(glean, &self.meta, err_type, err_msg, None);
                return;
            }
            Ok(duration) => duration,
        };

        let min_sample_time = self.time_unit.as_nanos(1);
        let max_sample_time = self.time_unit.as_nanos(MAX_SAMPLE_TIME);

        duration = if duration < min_sample_time {
            // If measurement is less than the minimum, just truncate. This is
            // not recorded as an error.
            min_sample_time
        } else if duration > max_sample_time {
            let msg = format!(
                "Sample is longer than the max for a time_unit of {:?} ({} ns)",
                self.time_unit, max_sample_time
            );
            record_error(glean, &self.meta, ErrorType::InvalidOverflow, msg, None);
            max_sample_time
        } else {
            duration
        };

        if !self.should_record(glean) {
            return;
        }

        glean
            .storage()
            .record_with(glean, &self.meta, |old_value| match old_value {
                Some(Metric::TimingDistribution(mut hist)) => {
                    hist.accumulate(duration);
                    Metric::TimingDistribution(hist)
                }
                _ => {
                    let mut hist = Histogram::functional(LOG_BASE, BUCKETS_PER_MAGNITUDE);
                    hist.accumulate(duration);
                    Metric::TimingDistribution(hist)
                }
            });
    }

    /// Aborts a previous [`set_start`](TimingDistributionMetric::set_start)
    /// call. No error is recorded if no
    /// [`set_start`](TimingDistributionMetric.set_start) was called.
    ///
    /// # Arguments
    ///
    /// * `id` - The [`TimerId`] to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timing distribution metric.
    pub fn cancel(&mut self, id: TimerId) {
        self.timings.cancel(id);
    }

    /// Accumulates the provided signed samples in the metric.
    ///
    /// This is required so that the platform-specific code can provide us with
    /// 64 bit signed integers if no `u64` comparable type is available. This
    /// will take care of filtering and reporting errors for any provided negative
    /// sample.
    ///
    /// Please note that this assumes that the provided samples are already in
    /// the "unit" declared by the instance of the metric type (e.g. if the
    /// instance this method was called on is using [`TimeUnit::Second`], then
    /// `samples` are assumed to be in that unit).
    ///
    /// # Arguments
    ///
    /// * `samples` - The vector holding the samples to be recorded by the metric.
    ///
    /// ## Notes
    ///
    /// Discards any negative value in `samples` and report an [`ErrorType::InvalidValue`]
    /// for each of them. Reports an [`ErrorType::InvalidOverflow`] error for samples that
    /// are longer than `MAX_SAMPLE_TIME`.
    pub fn accumulate_samples_signed(&mut self, glean: &Glean, samples: Vec<i64>) {
        if !self.should_record(glean) {
            return;
        }

        let mut num_negative_samples = 0;
        let mut num_too_long_samples = 0;
        let max_sample_time = self.time_unit.as_nanos(MAX_SAMPLE_TIME);

        glean.storage().record_with(glean, &self.meta, |old_value| {
            let mut hist = match old_value {
                Some(Metric::TimingDistribution(hist)) => hist,
                _ => Histogram::functional(LOG_BASE, BUCKETS_PER_MAGNITUDE),
            };

            for &sample in samples.iter() {
                if sample < 0 {
                    num_negative_samples += 1;
                } else {
                    let mut sample = sample as u64;

                    // Check the range prior to converting the incoming unit to
                    // nanoseconds, so we can compare against the constant
                    // MAX_SAMPLE_TIME.
                    if sample == 0 {
                        sample = 1;
                    } else if sample > MAX_SAMPLE_TIME {
                        num_too_long_samples += 1;
                        sample = MAX_SAMPLE_TIME;
                    }

                    sample = self.time_unit.as_nanos(sample);

                    hist.accumulate(sample);
                }
            }

            Metric::TimingDistribution(hist)
        });

        if num_negative_samples > 0 {
            let msg = format!("Accumulated {} negative samples", num_negative_samples);
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidValue,
                msg,
                num_negative_samples,
            );
        }

        if num_too_long_samples > 0 {
            let msg = format!(
                "{} samples are longer than the maximum of {}",
                num_too_long_samples, max_sample_time
            );
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidOverflow,
                msg,
                num_too_long_samples,
            );
        }
    }

    /// Accumulates the provided samples in the metric.
    ///
    /// # Arguments
    ///
    /// * `samples` - A list of samples recorded by the metric.
    ///               Samples must be in nanoseconds.
    /// ## Notes
    ///
    /// Reports an [`ErrorType::InvalidOverflow`] error for samples that
    /// are longer than `MAX_SAMPLE_TIME`.
    pub fn accumulate_raw_samples_nanos(&mut self, glean: &Glean, samples: &[u64]) {
        if !self.should_record(glean) {
            return;
        }

        let mut num_too_long_samples = 0;
        let min_sample_time = self.time_unit.as_nanos(1);
        let max_sample_time = self.time_unit.as_nanos(MAX_SAMPLE_TIME);

        glean.storage().record_with(glean, &self.meta, |old_value| {
            let mut hist = match old_value {
                Some(Metric::TimingDistribution(hist)) => hist,
                _ => Histogram::functional(LOG_BASE, BUCKETS_PER_MAGNITUDE),
            };

            for &sample in samples.iter() {
                let mut sample = sample;

                if sample < min_sample_time {
                    sample = min_sample_time;
                } else if sample > max_sample_time {
                    num_too_long_samples += 1;
                    sample = max_sample_time;
                }

                // `sample` is in nanoseconds.
                hist.accumulate(sample);
            }

            Metric::TimingDistribution(hist)
        });

        if num_too_long_samples > 0 {
            let msg = format!(
                "{} samples are longer than the maximum of {}",
                num_too_long_samples, max_sample_time
            );
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidOverflow,
                msg,
                num_too_long_samples,
            );
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as an integer.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<DistributionData> {
        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::TimingDistribution(hist)) => Some(snapshot(&hist)),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently-stored histogram as a JSON String of the serialized value.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value_as_json_string(
        &self,
        glean: &Glean,
        storage_name: &str,
    ) -> Option<String> {
        self.test_get_value(glean, storage_name)
            .map(|snapshot| serde_json::to_string(&snapshot).unwrap())
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn can_snapshot() {
        use serde_json::json;

        let mut hist = Histogram::functional(2.0, 8.0);

        for i in 1..=10 {
            hist.accumulate(i);
        }

        let snap = snapshot(&hist);

        let expected_json = json!({
            "sum": 55,
            "values": {
                "1": 1,
                "2": 1,
                "3": 1,
                "4": 1,
                "5": 1,
                "6": 1,
                "7": 1,
                "8": 1,
                "9": 1,
                "10": 1,
                "11": 0,
            },
        });

        assert_eq!(expected_json, json!(snap));
    }

    #[test]
    fn can_snapshot_sparse() {
        use serde_json::json;

        let mut hist = Histogram::functional(2.0, 8.0);

        hist.accumulate(1024);
        hist.accumulate(1024);
        hist.accumulate(1116);
        hist.accumulate(1448);

        let snap = snapshot(&hist);

        let expected_json = json!({
            "sum": 4612,
            "values": {
                "1024": 2,
                "1116": 1,
                "1217": 0,
                "1327": 0,
                "1448": 1,
                "1579": 0,
            },
        });

        assert_eq!(expected_json, json!(snap));
    }
}
