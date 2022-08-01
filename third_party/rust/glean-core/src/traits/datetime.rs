// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![allow(clippy::too_many_arguments)]

use crate::ErrorType;

/// A description for the [`DatetimeMetric`](crate::metrics::DatetimeMetric) type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Datetime {
    /// Sets the metric to a date/time which including the timezone offset.
    ///
    /// # Arguments
    ///
    /// * `value` - Some [`Datetime`](crate::metrics::Datetime), with offset, to
    ///             set the metric to. If [`None`], the current local time is
    ///             used.
    fn set(&self, value: Option<crate::metrics::Datetime>);

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value as a Datetime.
    ///
    /// The precision of this value is truncated to the `time_unit` precision.
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
    ) -> Option<crate::metrics::Datetime>;

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
    fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32;
}
