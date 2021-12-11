// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use uuid::Uuid;

use super::{CommonMetricData, MetricId};

use crate::ipc::need_ipc;

/// A UUID metric.
///
/// Stores UUID values.
pub enum UuidMetric {
    Parent(glean::private::UuidMetric),
    Child(UuidMetricIpc),
}

#[derive(Debug)]
pub struct UuidMetricIpc;

impl UuidMetric {
    /// Create a new UUID metric.
    pub fn new(_id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            UuidMetric::Child(UuidMetricIpc)
        } else {
            UuidMetric::Parent(glean::private::UuidMetric::new(meta))
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            UuidMetric::Parent(_) => UuidMetric::Child(UuidMetricIpc),
            UuidMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }
}

#[inherent(pub)]
impl glean::traits::Uuid for UuidMetric {
    /// Set to the specified value.
    ///
    /// ## Arguments
    ///
    /// * `value` - The UUID to set the metric to.
    fn set(&self, value: Uuid) {
        match self {
            UuidMetric::Parent(p) => {
                glean::traits::Uuid::set(&*p, value);
            }
            UuidMetric::Child(_c) => {
                log::error!("Unable to set the uuid metric in non-main process. Ignoring.");
                // TODO: Record an error.
            }
        };
    }

    /// Generate a new random UUID and set the metric to it.
    ///
    /// ## Return value
    ///
    /// Returns the stored UUID value or `Uuid::nil` if called from
    /// a non-parent process.
    fn generate_and_set(&self) -> Uuid {
        match self {
            UuidMetric::Parent(p) => glean::traits::Uuid::generate_and_set(&*p),
            UuidMetric::Child(_c) => {
                log::error!("Unable to set the uuid metric in non-main process. Ignoring.");
                // TODO: Record an error.
                Uuid::nil()
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the stored UUID value.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, storage_name: S) -> Option<Uuid> {
        match self {
            UuidMetric::Parent(p) => p.test_get_value(storage_name),
            UuidMetric::Child(_c) => panic!("Cannot get test value for in non-parent process!"),
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
    fn test_get_num_recorded_errors<'a, S: Into<Option<&'a str>>>(
        &self,
        error: glean::ErrorType,
        ping_name: S,
    ) -> i32 {
        match self {
            UuidMetric::Parent(p) => p.test_get_num_recorded_errors(error, ping_name),
            UuidMetric::Child(_c) => {
                panic!("Cannot get test value for UuidMetric in non-parent process!")
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn sets_uuid_value() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_uuid;
        let expected = Uuid::new_v4();
        metric.set(expected.clone());

        assert_eq!(expected, metric.test_get_value("store1").unwrap());
    }

    #[test]
    fn uuid_ipc() {
        // UuidMetric doesn't support IPC.
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_uuid;
        let expected = Uuid::new_v4();
        parent_metric.set(expected.clone());

        {
            let child_metric = parent_metric.child_metric();

            // Instrumentation calls do not panic.
            child_metric.set(Uuid::new_v4());

            // (They also shouldn't do anything,
            // but that's not something we can inspect in this test)
        }

        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert_eq!(
            expected,
            parent_metric.test_get_value("store1").unwrap(),
            "UUID metrics should only work in the parent process"
        );
    }
}
