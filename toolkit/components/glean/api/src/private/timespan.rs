// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use super::{CommonMetricData, MetricId, TimeUnit};

use glean_core::traits::Timespan;

use crate::ipc::need_ipc;

/// A timespan metric.
///
/// Timespans are used to make a measurement of how much time is spent in a particular task.
pub enum TimespanMetric {
    Parent(glean::private::TimespanMetric),
    Child,
}

impl TimespanMetric {
    /// Create a new timespan metric.
    pub fn new(_id: MetricId, meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            TimespanMetric::Child
        } else {
            TimespanMetric::Parent(glean::private::TimespanMetric::new(meta, time_unit))
        }
    }
}

#[inherent(pub)]
impl Timespan for TimespanMetric {
    fn start(&self) {
        match self {
            TimespanMetric::Parent(p) => Timespan::start(p),
            TimespanMetric::Child => {
                log::error!("Unable to start timespan metric in non-main process. Ignoring.");
                // TODO: Record an error.
            }
        }
    }

    fn stop(&self) {
        match self {
            TimespanMetric::Parent(p) => Timespan::stop(p),
            TimespanMetric::Child => {
                log::error!("Unable to stop timespan metric in non-main process. Ignoring.");
                // TODO: Record an error.
            }
        }
    }

    fn cancel(&self) {
        match self {
            TimespanMetric::Parent(p) => Timespan::cancel(p),
            TimespanMetric::Child => {
                log::error!("Unable to cancel timespan metric in non-main process. Ignoring.");
                // TODO: Record an error.
            }
        }
    }

    fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<u64> {
        match self {
            TimespanMetric::Parent(p) => p.test_get_value(ping_name),
            TimespanMetric::Child => {
                panic!("Cannot get test value for in non-parent process!");
            }
        }
    }

    fn test_get_num_recorded_errors<'a, S: Into<Option<&'a str>>>(
        &self,
        error: glean::ErrorType,
        ping_name: S,
    ) -> i32 {
        match self {
            TimespanMetric::Parent(p) => p.test_get_num_recorded_errors(error, ping_name),
            TimespanMetric::Child => {
                panic!("Cannot get the number of recorded errors for timespan metric in non-parent process!");
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_timespan() {
        let _lock = lock_test();

        let metric = TimespanMetric::new(
            0.into(),
            CommonMetricData {
                name: "timespan_metric".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["store1".into()],
                disabled: false,
                ..Default::default()
            },
            TimeUnit::Nanosecond,
        );

        metric.start();
        // Stopping right away might not give us data, if the underlying clock source is not precise
        // enough.
        // So let's cancel and make sure nothing blows up.
        metric.cancel();

        assert_eq!(None, metric.test_get_value("store1"));
    }

    #[test]
    fn timespan_ipc() {
        let _lock = lock_test();
        let _raii = ipc::test_set_need_ipc(true);

        let child_metric = &metrics::test_only::can_we_time_it;

        // Instrumentation calls do not panic.
        child_metric.start();
        // Stopping right away might not give us data,
        // if the underlying clock source is not precise enough.
        // So let's cancel and make sure nothing blows up.
        child_metric.cancel();

        // (They also shouldn't do anything,
        // but that's not something we can inspect in this test)
    }
}
