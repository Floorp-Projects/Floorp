// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use super::{CommonMetricData, MetricId};

use super::TimeUnit;
use crate::ipc::need_ipc;
use chrono::{FixedOffset, TimeZone};
use glean::traits::Datetime;

/// A datetime metric of a certain resolution.
///
/// Datetimes are used to make record when something happened according to the
/// client's clock.
#[derive(Clone)]
pub enum DatetimeMetric {
    Parent(glean::private::DatetimeMetric),
    Child(DatetimeMetricIpc),
}
#[derive(Debug, Clone)]
pub struct DatetimeMetricIpc;

impl DatetimeMetric {
    /// Create a new datetime metric.
    pub fn new(_id: MetricId, meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            DatetimeMetric::Child(DatetimeMetricIpc)
        } else {
            DatetimeMetric::Parent(glean::private::DatetimeMetric::new(meta, time_unit))
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            DatetimeMetric::Parent { .. } => DatetimeMetric::Child(DatetimeMetricIpc),
            DatetimeMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }

    /// Sets the metric to a date/time including the timezone offset.
    ///
    /// # Arguments
    ///
    /// * `year` - the year to set the metric to.
    /// * `month` - the month to set the metric to (1-12).
    /// * `day` - the day to set the metric to (1-based).
    /// * `hour` - the hour to set the metric to (0-23).
    /// * `minute` - the minute to set the metric to.
    /// * `second` - the second to set the metric to.
    /// * `nano` - the nanosecond fraction to the last whole second.
    /// * `offset_seconds` - the timezone difference, in seconds, for the Eastern
    ///   Hemisphere. Negative seconds mean Western Hemisphere.
    #[cfg_attr(not(feature = "with-gecko"), allow(dead_code))]
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn set_with_details(
        &self,
        year: i32,
        month: u32,
        day: u32,
        hour: u32,
        minute: u32,
        second: u32,
        nano: u32,
        offset_seconds: i32,
    ) {
        match self {
            DatetimeMetric::Parent(p) => {
                let tz = FixedOffset::east_opt(offset_seconds);
                if tz.is_none() {
                    log::error!(
                        "Unable to set datetime metric with invalid offset seconds {}",
                        offset_seconds
                    );
                    // TODO: Record an error
                    return;
                }

                let value = FixedOffset::east(offset_seconds)
                    .ymd_opt(year, month, day)
                    .and_hms_nano_opt(hour, minute, second, nano);
                match value.single() {
                    Some(d) => p.set(Some(d.into())),
                    _ => {
                        log::error!("Unable to construct datetime")
                        // TODO: Record an error
                    }
                }
            }
            DatetimeMetric::Child(_) => {
                log::error!("Unable to set datetime metric in non-parent process. Ignoring.");
                // TODO: Record an error.
            }
        }
    }
}

#[inherent]
impl Datetime for DatetimeMetric {
    /// Sets the metric to a date/time which including the timezone offset.
    ///
    /// ## Arguments
    ///
    /// - `value` - The date and time and timezone value to set.
    ///             If None we use the current local time.
    pub fn set(&self, value: Option<glean::Datetime>) {
        match self {
            DatetimeMetric::Parent(p) => {
                p.set(value);
            }
            DatetimeMetric::Child(_) => {
                log::error!(
                    "Unable to set datetime metric DatetimeMetric in non-parent process. Ignoring."
                );
                // TODO: Record an error.
            }
        }
    }

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
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<glean::Datetime> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            DatetimeMetric::Parent(p) => p.test_get_value(ping_name),
            DatetimeMetric::Child(_) => {
                panic!("Cannot get test value for DatetimeMetric in non-parent process!")
            }
        }
    }

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
    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            DatetimeMetric::Parent(p) => p.test_get_num_recorded_errors(error),
            DatetimeMetric::Child(_) => panic!("Cannot get the number of recorded errors for DatetimeMetric in non-parent process!"),
        }
    }
}

#[cfg(test)]
mod test {
    use chrono::{DateTime, FixedOffset, TimeZone};

    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn sets_datetime_value() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_date;

        let a_datetime = FixedOffset::east(5 * 3600)
            .ymd(2020, 05, 07)
            .and_hms(11, 58, 00);
        metric.set(Some(a_datetime.into()));

        let expected: glean::Datetime = DateTime::parse_from_rfc3339("2020-05-07T11:58:00+05:00")
            .unwrap()
            .into();
        assert_eq!(expected, metric.test_get_value("store1").unwrap());
    }

    #[test]
    fn sets_datetime_value_with_details() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_date;

        metric.set_with_details(2020, 05, 07, 11, 58, 0, 0, 5 * 3600);

        let expected: glean::Datetime = DateTime::parse_from_rfc3339("2020-05-07T11:58:00+05:00")
            .unwrap()
            .into();
        assert_eq!(expected, metric.test_get_value("store1").unwrap());
    }

    #[test]
    fn datetime_ipc() {
        // DatetimeMetric doesn't support IPC.
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_date;

        // Instrumentation calls do not panic.
        let a_datetime = FixedOffset::east(5 * 3600)
            .ymd(2020, 10, 13)
            .and_hms(16, 41, 00);
        parent_metric.set(Some(a_datetime.into()));

        {
            let child_metric = parent_metric.child_metric();

            let _raii = ipc::test_set_need_ipc(true);

            let a_datetime = FixedOffset::east(0).ymd(2018, 4, 7).and_hms(12, 01, 00);
            child_metric.set(Some(a_datetime.into()));

            // (They also shouldn't do anything,
            // but that's not something we can inspect in this test)
        }

        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        let expected: glean::Datetime = DateTime::parse_from_rfc3339("2020-10-13T16:41:00+05:00")
            .unwrap()
            .into();
        assert_eq!(expected, parent_metric.test_get_value("store1").unwrap());
    }
}
