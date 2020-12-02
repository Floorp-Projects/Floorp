// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::time::Duration;

/// A description for the `TimespanMetric` type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Timespan {
    /// Starts tracking time for the provided metric.
    ///
    /// This records an error if it's already tracking time (i.e. start was already
    /// called with no corresponding `stop`): in that case the original
    /// start time will be preserved.
    fn set_start(&mut self, start_time: u64);

    /// Stops tracking time for the provided metric. Sets the metric to the elapsed time.
    ///
    /// This will record an error if no `start` was called.
    fn set_stop(&mut self, stop_time: u64);

    /// Aborts a previous `start` call. No error is recorded if no `start` was called.
    fn cancel(&mut self);

    /// Explicitly sets the timespan value.
    ///
    /// This API should only be used if your library or application requires recording
    /// times in a way that can not make use of `start`/`stop`/`cancel`.
    ///
    /// Care should be taken using this if the ping lifetime might contain more than one
    /// timespan measurement. To be safe, `set_raw` should generally be followed by
    /// sending a custom ping containing the timespan.
    ///
    /// # Arguments
    ///
    /// * `elapsed` - The elapsed time to record.
    /// * `overwrite` - Whether or not to overwrite existing data.
    fn set_raw(&self, elapsed: Duration, overwrite: bool);

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
}
