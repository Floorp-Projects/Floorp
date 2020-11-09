// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::metrics::DistributionData;
use crate::metrics::TimerId;

/// A description for the `TimingDistributionMetric` type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait TimingDistribution {
    /// Starts tracking time for the provided metric.
    ///
    /// This records an error if it’s already tracking time (i.e. start was already
    /// called with no corresponding [stop]): in that case the original
    /// start time will be preserved.
    ///
    /// # Arguments
    ///
    /// * `start_time` - Timestamp in nanoseconds.
    ///
    /// # Returns
    ///
    /// A unique `TimerId` for the new timer.
    fn set_start(&mut self, start_time: u64);

    /// Stops tracking time for the provided metric and associated timer id.
    ///
    /// Adds a count to the corresponding bucket in the timing distribution.
    /// This will record an error if no `start` was called.
    ///
    /// # Arguments
    ///
    /// * `id` - The `TimerId` to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timespan metric.
    /// * `stop_time` - Timestamp in nanoseconds.
    fn set_stop_and_accumulate(&mut self, id: TimerId, stop_time: u64);

    /// Aborts a previous `set_start` call. No error is recorded if no `set_start`
    /// was called.
    ///
    /// # Arguments
    ///
    /// * `id` - The `TimerId` to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timing distribution metric.
    fn cancel(&mut self, id: TimerId);

    /// Accumulates the provided signed samples in the metric.
    ///
    /// This is required so that the platform-specific code can provide us with
    /// 64 bit signed integers if no `u64` comparable type is available. This
    /// will take care of filtering and reporting errors for any provided negative
    /// sample.
    ///
    /// Please note that this assumes that the provided samples are already in the
    /// "unit" declared by the instance of the implementing metric type (e.g. if the
    /// implementing class is a [TimingDistributionMetricType] and the instance this
    /// method was called on is using [TimeUnit.Second], then `samples` are assumed
    /// to be in that unit).
    ///
    /// # Arguments
    ///
    /// * `samples` - The vector holding the samples to be recorded by the metric.
    ///
    /// ## Notes
    ///
    /// Discards any negative value in `samples` and report an `ErrorType::InvalidValue`
    /// for each of them. Reports an `ErrorType::InvalidOverflow` error for samples that
    /// are longer than `MAX_SAMPLE_TIME`.
    fn accumulate_samples_signed(&mut self, samples: Vec<i64>);

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value as an integer.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData>;

    /// **Exported for test purposes.**
    ///
    /// Gets the currently-stored histogram as a JSON String of the serialized value.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value_as_json_string<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<String>;
}
