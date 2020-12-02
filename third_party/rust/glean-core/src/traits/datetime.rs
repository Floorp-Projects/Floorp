// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![allow(clippy::too_many_arguments)]

/// A description for the `DatetimeMetric` type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Datetime {
    /// Sets the metric to a date/time including the timezone offset.
    ///
    /// # Arguments
    ///
    /// * `year` - the year to set the metric to.
    /// * `month` - the month to set the metric to (1-12).
    /// * `day` - the day to set the metric to (1-based).
    /// * `hour` - the hour to set the metric to.
    /// * `minute` - the minute to set the metric to.
    /// * `second` - the second to set the metric to.
    /// * `nano` - the nanosecond fraction to the last whole second.
    /// * `offset_seconds` - the timezone difference, in seconds, for the Eastern
    ///   Hemisphere. Negative seconds mean Western Hemisphere.
    fn set_with_details(
        &self,
        year: i32,
        month: u32,
        day: u32,
        hour: u32,
        minute: u32,
        second: u32,
        nano: u32,
        offset_seconds: i32,
    );

    /// Sets the metric to a date/time which including the timezone offset.
    ///
    /// # Arguments
    ///
    /// * `value` - Some date/time value, with offset, to set the metric to.
    ///             If none, the current local time is used.
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
    /// Gets the currently stored value as a String.
    ///
    /// The precision of this value is truncated to the `time_unit` precision.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value_as_string<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<String>;
}
