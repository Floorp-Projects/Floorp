// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::metrics::DistributionData;
use crate::metrics::TimerId;
use crate::ErrorType;

/// A description for the [`TimingDistributionMetric`](crate::metrics::TimingDistributionMetric) type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait TimingDistribution {
    /// Starts tracking time for the provided metric.
    ///
    /// This records an error if it’s already tracking time (i.e.
    /// [`start`](TimingDistribution::start) was already called with no corresponding
    /// [`stop_and_accumulate`](TimingDistribution::stop_and_accumulate)): in that case the
    /// original start time will be preserved.
    ///
    /// # Returns
    ///
    /// A unique [`TimerId`] for the new timer.
    fn start(&self) -> TimerId;

    /// Stops tracking time for the provided metric and associated timer id.
    ///
    /// Adds a count to the corresponding bucket in the timing distribution.
    /// This will record an error if no [`start`](TimingDistribution::start) was
    /// called.
    ///
    /// # Arguments
    ///
    /// * `id` - The [`TimerId`] to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timespan metric.
    fn stop_and_accumulate(&self, id: TimerId);

    /// Aborts a previous [`start`](TimingDistribution::start) call. No
    /// error is recorded if no [`start`](TimingDistribution::start) was
    /// called.
    ///
    /// # Arguments
    ///
    /// * `id` - The [`TimerId`] to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timing distribution metric.
    fn cancel(&self, id: TimerId);

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value of the metric.
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
    /// Gets the number of recorded errors for the given error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors recorded.
    fn test_get_num_recorded_errors<'a, S: Into<Option<&'a str>>>(
        &self,
        error: ErrorType,
        ping_name: S,
    ) -> i32;
}
