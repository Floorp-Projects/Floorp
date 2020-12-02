// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use uuid::Uuid;

use super::{CommonMetricData, MetricId};

use crate::{dispatcher, ipc::need_ipc};

/// A UUID metric.
///
/// Stores UUID values.
#[derive(Debug)]
pub enum UuidMetric {
    Parent(Arc<UuidMetricImpl>),
    Child(UuidMetricIpc),
}

#[derive(Clone, Debug)]
pub struct UuidMetricImpl(pub(crate) glean_core::metrics::UuidMetric);
#[derive(Debug)]
pub struct UuidMetricIpc;

impl UuidMetric {
    /// Create a new UUID metric.
    pub fn new(_id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            UuidMetric::Child(UuidMetricIpc)
        } else {
            UuidMetric::Parent(Arc::new(UuidMetricImpl::new(meta)))
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            UuidMetric::Parent(_) => UuidMetric::Child(UuidMetricIpc),
            UuidMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }

    /// Set to the specified value.
    ///
    /// ## Arguments
    ///
    /// * `value` - The UUID to set the metric to.
    pub fn set(&self, value: Uuid) {
        match self {
            UuidMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.set(value));
            }
            UuidMetric::Child(_c) => {
                log::error!(
                    "Unable to set UUID metric {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        };
    }

    /// Generate a new random UUID and set the metric to it.
    pub fn generate_and_set(&self) {
        match self {
            UuidMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.generate_and_set());
            }
            UuidMetric::Child(_c) => {
                log::error!(
                    "Unable to set UUID metric {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        };
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
    pub fn test_get_value(&self, storage_name: &str) -> Option<Uuid> {
        match self {
            UuidMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name).map(|uuid| {
                    Uuid::parse_str(&uuid).expect("can't parse internally created UUID value")
                })
            }
            UuidMetric::Child(_c) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl UuidMetricImpl {
    fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::UuidMetric::new(meta))
    }

    fn set(&self, value: Uuid) {
        crate::with_glean(move |glean| self.0.set(glean, value));
    }

    fn generate_and_set(&self) {
        crate::with_glean(move |glean| self.0.generate_and_set(glean));
    }

    fn test_get_value(&self, storage_name: &str) -> Option<String> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
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

            // Need to catch the panic so that our RAIIs drop nicely.
            let result = std::panic::catch_unwind(move || {
                child_metric.test_get_value("store1");
            });
            assert!(result.is_err());
        }

        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert_eq!(
            expected,
            parent_metric.test_get_value("store1").unwrap(),
            "UUID metrics should only work in the parent process"
        );
    }
}
