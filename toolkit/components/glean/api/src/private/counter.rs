// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use super::CommonMetricData;

use crate::dispatcher;
use crate::ipc::{need_ipc, with_ipc_payload};
use crate::private::MetricId;

/// A counter metric.
///
/// Used to count things.
/// The value can only be incremented, not decremented.
#[derive(Debug)]
pub enum CounterMetric {
    Parent {
        /// The metric's ID.
        ///
        /// **TEST-ONLY** - Do not use unless gated with `#[cfg(test)]`.
        id: MetricId,
        inner: Arc<CounterMetricImpl>,
    },
    Child(CounterMetricIpc),
}
#[derive(Clone, Debug)]
pub struct CounterMetricImpl(pub(crate) glean_core::metrics::CounterMetric);
#[derive(Debug)]
pub struct CounterMetricIpc(MetricId);

impl CounterMetric {
    /// Create a new counter metric.
    pub fn new(id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            CounterMetric::Child(CounterMetricIpc(id))
        } else {
            let inner = Arc::new(CounterMetricImpl::new(meta));
            CounterMetric::Parent { id, inner }
        }
    }

    #[cfg(test)]
    pub(crate) fn metric_id(&self) -> MetricId {
        match self {
            CounterMetric::Parent { id, .. } => *id,
            CounterMetric::Child(c) => c.0,
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            CounterMetric::Parent { id, .. } => CounterMetric::Child(CounterMetricIpc(*id)),
            CounterMetric::Child(_) => panic!("Can't get a child metric from a child metric"),
        }
    }

    /// Increase the counter by `amount`.
    ///
    /// ## Arguments
    ///
    /// * `amount` - The amount to increase by. Should be positive.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `amount` is 0 or negative.
    pub fn add(&self, amount: i32) {
        match self {
            CounterMetric::Parent { inner, .. } => {
                let metric = Arc::clone(&inner);
                dispatcher::launch(move || metric.add(amount));
            }
            CounterMetric::Child(c) => {
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.counters.get_mut(&c.0) {
                        *v += amount;
                    } else {
                        payload.counters.insert(c.0.clone(), amount);
                    }
                });
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value as an integer.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<i32> {
        match self {
            CounterMetric::Parent { inner, .. } => {
                dispatcher::block_on_queue();
                inner.test_get_value(storage_name)
            }
            CounterMetric::Child(_c) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl CounterMetricImpl {
    pub fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::CounterMetric::new(meta))
    }

    pub fn add(&self, amount: i32) {
        crate::with_glean(move |glean| self.0.add(glean, amount))
    }

    pub fn test_get_value(&self, storage_name: &str) -> Option<i32> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn sets_counter_value_parent() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_counter;
        metric.add(1);

        assert_eq!(1, metric.test_get_value("store1").unwrap());
    }

    #[test]
    fn sets_counter_value_child() {
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_counter;
        parent_metric.add(3);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);
            let metric_id = child_metric.metric_id();

            child_metric.add(42);

            // Need to catch the panic so that our RAIIs drop nicely.
            let result = std::panic::catch_unwind(move || {
                child_metric.test_get_value("store1");
            });
            assert!(result.is_err());

            ipc::with_ipc_payload(move |payload| {
                assert!(
                    42 == *payload.counters.get(&metric_id).unwrap(),
                    "Stored the correct value in the ipc payload"
                );
            });
        }

        assert!(
            false == ipc::need_ipc(),
            "RAII dropped, should not need ipc any more"
        );
        assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

        assert!(
            45 == parent_metric.test_get_value("store1").unwrap(),
            "Values from the 'processes' should be summed"
        );
    }
}
