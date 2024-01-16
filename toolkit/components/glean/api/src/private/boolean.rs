// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;
use std::sync::Arc;

use glean::traits::Boolean;

use super::CommonMetricData;

use crate::ipc::need_ipc;
use crate::private::MetricId;

/// A boolean metric.
///
/// Records a simple true or false value.
#[derive(Clone)]
pub enum BooleanMetric {
    Parent(Arc<glean::private::BooleanMetric>),
    Child(BooleanMetricIpc),
}
#[derive(Clone, Debug)]
pub struct BooleanMetricIpc;

impl BooleanMetric {
    /// Create a new boolean metric.
    pub fn new(_id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            BooleanMetric::Child(BooleanMetricIpc)
        } else {
            BooleanMetric::Parent(Arc::new(glean::private::BooleanMetric::new(meta)))
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            BooleanMetric::Parent(_) => BooleanMetric::Child(BooleanMetricIpc),
            BooleanMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }
}

#[inherent]
impl Boolean for BooleanMetric {
    /// Set to the specified boolean value.
    ///
    /// ## Arguments
    ///
    /// * `value` - the value to set.
    pub fn set(&self, value: bool) {
        match self {
            BooleanMetric::Parent(p) => {
                p.set(value);
            }
            BooleanMetric::Child(_) => {
                log::error!("Unable to set boolean metric in non-main process. This operation will be ignored.");
                // If we're in automation we can panic so the instrumentor knows they've gone wrong.
                // This is a deliberate violation of Glean's "metric APIs must not throw" design.
                assert!(!crate::ipc::is_in_automation(), "Attempted to set boolean metric in non-main process, which is forbidden. This panics in automation.");
                // TODO: Record an error.
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value as a boolean.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `ping_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<bool> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            BooleanMetric::Parent(p) => p.test_get_value(ping_name),
            BooleanMetric::Child(_) => {
                panic!("Cannot get test value for boolean metric in non-main process!",)
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
            BooleanMetric::Parent(p) => p.test_get_num_recorded_errors(error),
            BooleanMetric::Child(_) => panic!(
                "Cannot get the number of recorded errors for boolean metric in non-main process!"
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn sets_boolean_value() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_bool;
        metric.set(true);

        assert!(metric.test_get_value("store1").unwrap());
    }

    #[test]
    fn boolean_ipc() {
        // BooleanMetric doesn't support IPC.
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_bool;

        parent_metric.set(false);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            // Instrumentation calls do not panic.
            child_metric.set(true);

            // (They also shouldn't do anything,
            // but that's not something we can inspect in this test)
        }

        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert!(
            false == parent_metric.test_get_value("store1").unwrap(),
            "Boolean metrics should only work in the parent process"
        );
    }
}
