// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use super::{CommonMetricData, MetricId};

use super::TimeUnit;
use crate::dispatcher;
use crate::ipc::need_ipc;
use chrono::{DateTime, FixedOffset};

/// A datetime metric of a certain resolution.
///
/// Datetimes are used to make record when something happened according to the
/// client's clock.
#[derive(Debug)]
pub enum DatetimeMetric {
    Parent(Arc<DatetimeMetricImpl>),
    Child(DatetimeMetricIpc),
}
#[derive(Debug)]
pub struct DatetimeMetricImpl(glean_core::metrics::DatetimeMetric);
#[derive(Debug)]
pub struct DatetimeMetricIpc;

impl DatetimeMetric {
    /// Create a new datetime metric.
    pub fn new(_id: MetricId, meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            DatetimeMetric::Child(DatetimeMetricIpc)
        } else {
            DatetimeMetric::Parent(Arc::new(DatetimeMetricImpl::new(meta, time_unit)))
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            DatetimeMetric::Parent { .. } => DatetimeMetric::Child(DatetimeMetricIpc),
            DatetimeMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }

    /// Set the datetime to the provided value, or the local now.
    ///
    /// ## Arguments
    ///
    /// - `value` - The date and time and timezone value to set.
    ///             If None we use the current local time.
    pub fn set(&self, value: Option<DateTime<FixedOffset>>) {
        match self {
            DatetimeMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.set(value));
            }
            DatetimeMetric::Child(_) => {
                log::error!(
                    "Unable to set datetime metric {:?} in non-parent process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
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
                let metric = Arc::clone(&p);
                dispatcher::launch(move || {
                    metric.set_with_details(
                        year,
                        month,
                        day,
                        hour,
                        minute,
                        second,
                        nano,
                        offset_seconds,
                    )
                });
            }
            DatetimeMetric::Child(_) => {
                log::error!(
                    "Unable to set datetime metric {:?} in non-parent process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value.
    /// This doesn't clear the stored value.
    ///
    /// ## Note
    ///
    /// This currently returns the value as an ISO 8601 time string.
    /// See [bug 1636176](https://bugzilla.mozilla.org/show_bug.cgi?id=1636176).
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, storage_name: S) -> Option<String> {
        match self {
            DatetimeMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name)
            }
            DatetimeMetric::Child(_) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl DatetimeMetricImpl {
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        Self(glean_core::metrics::DatetimeMetric::new(meta, time_unit))
    }

    pub fn set(&self, value: Option<DateTime<FixedOffset>>) {
        crate::with_glean(move |glean| self.0.set(glean, value))
    }

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
        crate::with_glean(move |glean| {
            self.0.set_with_details(
                glean,
                year,
                month,
                day,
                hour,
                minute,
                second,
                nano,
                offset_seconds,
            );
        })
    }

    fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, storage_name: S) -> Option<String> {
        // FIXME(bug 1677448): This hack goes away when the type is implemented in RLB.
        let storage = storage_name.into().expect("storage name required.");
        crate::with_glean(move |glean| self.0.test_get_value_as_string(glean, storage))
    }
}

#[cfg(test)]
mod test {
    use chrono::{FixedOffset, TimeZone};

    use crate::{common_test::*, ipc, metrics};

    #[test]
    #[ignore] // TODO: Enable them back when bug 1677448 lands.
    fn sets_datetime_value() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_date;

        let a_datetime = FixedOffset::east(5 * 3600)
            .ymd(2020, 05, 07)
            .and_hms(11, 58, 00);
        metric.set(Some(a_datetime));

        assert_eq!(
            "2020-05-07T11:58:00+05:00",
            metric.test_get_value("store1").unwrap()
        );
    }

    #[test]
    #[ignore] // TODO: Enable them back when bug 1677448 lands.
    fn sets_datetime_value_with_details() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_date;

        metric.set_with_details(2020, 05, 07, 11, 58, 0, 0, 5 * 3600);

        assert_eq!(
            "2020-05-07T11:58:00+05:00",
            metric.test_get_value("store1").unwrap()
        );
    }

    #[test]
    #[ignore] // TODO: Enable them back when bug 1677448 lands.
    fn datetime_ipc() {
        // DatetimeMetric doesn't support IPC.
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_date;

        // Instrumentation calls do not panic.
        let a_datetime = FixedOffset::east(5 * 3600)
            .ymd(2020, 10, 13)
            .and_hms(16, 41, 00);
        parent_metric.set(Some(a_datetime));

        {
            let child_metric = parent_metric.child_metric();

            let _raii = ipc::test_set_need_ipc(true);

            let a_datetime = FixedOffset::east(0).ymd(2018, 4, 7).and_hms(12, 01, 00);
            child_metric.set(Some(a_datetime));

            // (They also shouldn't do anything,
            // but that's not something we can inspect in this test)

            // Need to catch the panic so that our RAIIs drop nicely.
            let result = std::panic::catch_unwind(move || {
                child_metric.test_get_value("store1");
            });
            assert!(result.is_err());
        }

        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert_eq!(
            "2020-10-13T16:41:00+05:00",
            parent_metric.test_get_value("store1").unwrap()
        );
    }
}
