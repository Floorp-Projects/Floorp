// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::ErrorType;
use std::time::Duration;

/// A description for the [`TimespanMetric`](crate::metrics::TimespanMetric) type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Timespan {
    /// Starts tracking time for the provided metric.
    ///
    /// This uses an internal monotonic timer.
    ///
    /// This records an error if it's already tracking time (i.e.
    /// [`start`](Timespan::start) was already called with no corresponding
    /// [`stop`](Timespan::stop)): in that case the original start time will be
    /// preserved.
    fn start(&self);

    /// Stops tracking time for the provided metric. Sets the metric to the elapsed time.
    ///
    /// This will record an error if no [`start`](Timespan::start) was called.
    fn stop(&self);

    /// Aborts a previous [`start`](Timespan::start) call. No error is recorded
    /// if no [`start`](Timespan::start) was called.
    fn cancel(&self);

    /// Explicitly sets the timespan value.
    ///
    /// This API should only be used if your library or application requires recording
    /// spans of time in a way that cannot make use of
    /// [`start`](Timespan::start)/[`stop`](Timespan::stop)/[`cancel`](Timespan::cancel).
    ///
    /// # Arguments
    ///
    /// * `elapsed` - The elapsed time to record.
    fn set_raw(&self, elapsed: Duration);

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
    fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<u64>;

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    fn test_get_num_recorded_errors<'a, S: Into<Option<&'a str>>>(
        &self,
        error: ErrorType,
        ping_name: S,
    ) -> i32;
}
