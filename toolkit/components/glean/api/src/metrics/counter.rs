// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use super::CommonMetricData;

use crate::ipc::{need_ipc, with_ipc_payload, MetricId};

/// A counter metric.
///
/// Used to count things.
/// The value can only be incremented, not decremented.
#[derive(Debug)]
pub enum CounterMetric {
    Parent(CounterMetricImpl),
    Child(CounterMetricIpc),
}
#[derive(Clone, Debug)]
pub struct CounterMetricImpl(pub(crate) glean_core::metrics::CounterMetric);
#[derive(Debug)]
pub struct CounterMetricIpc(MetricId);

impl CounterMetric {
    /// Create a new counter metric.
    pub fn new(meta: CommonMetricData) -> Self {
        if need_ipc() {
            CounterMetric::Child(CounterMetricIpc(MetricId::new(meta)))
        } else {
            CounterMetric::Parent(CounterMetricImpl::new(meta))
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
            CounterMetric::Parent(p) => p.add(amount),
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
            CounterMetric::Parent(p) => p.test_get_value(storage_name),
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
