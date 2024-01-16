// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use glean::traits::Quantity;

use super::CommonMetricData;

use crate::ipc::need_ipc;
use crate::private::MetricId;

/// A quantity metric.
///
/// Records a single numeric value of a specific unit.
#[derive(Clone)]
pub enum QuantityMetric {
    Parent(glean::private::QuantityMetric),
    Child(QuantityMetricIpc),
}
#[derive(Clone, Debug)]
pub struct QuantityMetricIpc;

impl QuantityMetric {
    /// Create a new quantity metric.
    pub fn new(_id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            QuantityMetric::Child(QuantityMetricIpc)
        } else {
            QuantityMetric::Parent(glean::private::QuantityMetric::new(meta))
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            QuantityMetric::Parent(_) => QuantityMetric::Child(QuantityMetricIpc),
            QuantityMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }
}

#[inherent]
impl Quantity for QuantityMetric {
    /// Set the value. Must be non-negative.
    ///
    /// # Arguments
    ///
    /// * `value` - The value. Must be non-negative.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `value` is negative.
    pub fn set(&self, value: i64) {
        match self {
            QuantityMetric::Parent(p) => {
                p.set(value);
            }
            QuantityMetric::Child(_) => {
                log::error!("Unable to set quantity metric in non-main process. This operation will be ignored.");
                // If we're in automation we can panic so the instrumentor knows they've gone wrong.
                // This is a deliberate violation of Glean's "metric APIs must not throw" design.
                assert!(!crate::ipc::is_in_automation(), "Attempted to set quantity metric in non-main process, which is forbidden. This panics in automation.");
                // TODO: Record an error.
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `ping_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<i64> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            QuantityMetric::Parent(p) => p.test_get_value(ping_name),
            QuantityMetric::Child(_) => {
                panic!("Cannot get test value for quantity metric in non-main process!",)
            }
        }
    }

    /// **Test-only API.**
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
            QuantityMetric::Parent(p) => p.test_get_num_recorded_errors(error),
            QuantityMetric::Child(_) => panic!(
                "Cannot get the number of recorded errors for quantity metric in non-main process!"
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn sets_quantity_metric() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_quantity;
        metric.set(14);

        assert_eq!(14, metric.test_get_value("store1").unwrap());
    }

    #[test]
    fn quantity_ipc() {
        // QuantityMetric doesn't support IPC.
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_quantity;

        parent_metric.set(15);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            // Instrumentation calls do not panic.
            child_metric.set(30);

            // (They also shouldn't do anything,
            // but that's not something we can inspect in this test)
        }

        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert_eq!(15, parent_metric.test_get_value(None).unwrap());
    }
}
