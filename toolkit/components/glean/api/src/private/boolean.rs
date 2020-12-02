// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use super::CommonMetricData;

use crate::dispatcher;
use crate::ipc::need_ipc;
use crate::private::MetricId;

/// A boolean metric.
///
/// Records a simple true or false value.
#[derive(Debug)]
pub enum BooleanMetric {
    Parent(Arc<BooleanMetricImpl>),
    Child(BooleanMetricIpc),
}
#[derive(Clone, Debug)]
pub struct BooleanMetricImpl(pub(crate) glean_core::metrics::BooleanMetric);
#[derive(Debug)]
pub struct BooleanMetricIpc;

impl BooleanMetric {
    /// Create a new boolean metric.
    pub fn new(_id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            BooleanMetric::Child(BooleanMetricIpc)
        } else {
            BooleanMetric::Parent(Arc::new(BooleanMetricImpl::new(meta)))
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            BooleanMetric::Parent(_) => BooleanMetric::Child(BooleanMetricIpc),
            BooleanMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }

    /// Set to the specified boolean value.
    ///
    /// ## Arguments
    ///
    /// * `value` - the value to set.
    pub fn set(&self, value: bool) {
        match self {
            BooleanMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.set(value));
            }
            BooleanMetric::Child(_) => {
                log::error!(
                    "Unable to set boolean metric {:?} in non-parent process. Ignoring.",
                    self
                );
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
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<bool> {
        match self {
            BooleanMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name)
            }
            BooleanMetric::Child(_) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl BooleanMetricImpl {
    pub fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::BooleanMetric::new(meta))
    }

    pub fn set(&self, value: bool) {
        crate::with_glean(move |glean| self.0.set(glean, value))
    }

    pub fn test_get_value(&self, storage_name: &str) -> Option<bool> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
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

            // Need to catch the panic so that our RAIIs drop nicely.
            let result = std::panic::catch_unwind(move || {
                child_metric.test_get_value("store1");
            });
            assert!(result.is_err());
        }

        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert!(
            false == parent_metric.test_get_value("store1").unwrap(),
            "Boolean metrics should only work in the parent process"
        );
    }
}
