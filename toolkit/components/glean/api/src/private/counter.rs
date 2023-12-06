// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;
use std::sync::Arc;

use glean::traits::Counter;

use super::{CommonMetricData, MetricId};
use crate::ipc::{need_ipc, with_ipc_payload};

/// A counter metric.
///
/// Used to count things.
/// The value can only be incremented, not decremented.
#[derive(Clone)]
pub enum CounterMetric {
    Parent {
        /// The metric's ID.
        ///
        /// **TEST-ONLY** - Do not use unless gated with `#[cfg(test)]`.
        id: MetricId,
        inner: Arc<glean::private::CounterMetric>,
    },
    Child(CounterMetricIpc),
}
#[derive(Clone, Debug)]
pub struct CounterMetricIpc(MetricId);

impl CounterMetric {
    /// Create a new counter metric.
    pub fn new(id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            CounterMetric::Child(CounterMetricIpc(id))
        } else {
            let inner = Arc::new(glean::private::CounterMetric::new(meta));
            CounterMetric::Parent { id, inner }
        }
    }

    /// Special-purpose ctor for use by codegen.
    /// Only useful if the metric is:
    ///   * not disabled
    ///   * lifetime: ping
    ///   * and is sent in precisely one ping.
    pub fn codegen_new(id: u32, category: &str, name: &str, ping: &str) -> Self {
        if need_ipc() {
            CounterMetric::Child(CounterMetricIpc(id.into()))
        } else {
            let inner = Arc::new(glean::private::CounterMetric::new(CommonMetricData {
                category: category.into(),
                name: name.into(),
                send_in_pings: vec![ping.into()],
                ..Default::default()
            }));
            CounterMetric::Parent {
                id: id.into(),
                inner,
            }
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
}

#[inherent]
impl Counter for CounterMetric {
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
                inner.add(amount);
            }
            CounterMetric::Child(c) => {
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.counters.get_mut(&c.0) {
                        *v += amount;
                    } else {
                        payload.counters.insert(c.0, amount);
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
    /// * `ping_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<i32> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            CounterMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            CounterMetric::Child(c) => {
                panic!("Cannot get test value for {:?} in non-parent process!", c.0)
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
            CounterMetric::Parent { inner, .. } => inner.test_get_num_recorded_errors(error),
            CounterMetric::Child(c) => panic!(
                "Cannot get the number of recorded errors for {:?} in non-parent process!",
                c.0
            ),
        }
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
