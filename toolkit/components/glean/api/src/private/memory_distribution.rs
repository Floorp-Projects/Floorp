// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;
use std::convert::TryInto;

use super::{CommonMetricData, DistributionData, MemoryUnit, MetricId};

use glean::traits::MemoryDistribution;

use crate::ipc::{need_ipc, with_ipc_payload};

/// A memory distribution metric.
///
/// Memory distributions are used to accumulate and store memory measurements for analyzing distributions of the memory data.
#[derive(Clone)]
pub enum MemoryDistributionMetric {
    Parent {
        /// The metric's ID.
        ///
        /// **TEST-ONLY** - Do not use unless gated with `#[cfg(test)]`.
        id: MetricId,
        inner: glean::private::MemoryDistributionMetric,
    },
    Child(MemoryDistributionMetricIpc),
}
#[derive(Clone, Debug)]
pub struct MemoryDistributionMetricIpc(MetricId);

impl MemoryDistributionMetric {
    /// Create a new memory distribution metric.
    pub fn new(id: MetricId, meta: CommonMetricData, memory_unit: MemoryUnit) -> Self {
        if need_ipc() {
            MemoryDistributionMetric::Child(MemoryDistributionMetricIpc(id))
        } else {
            let inner = glean::private::MemoryDistributionMetric::new(meta, memory_unit);
            MemoryDistributionMetric::Parent { id, inner }
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            MemoryDistributionMetric::Parent { id, .. } => {
                MemoryDistributionMetric::Child(MemoryDistributionMetricIpc(*id))
            }
            MemoryDistributionMetric::Child(_) => {
                panic!("Can't get a child metric from a child metric")
            }
        }
    }
}

#[inherent]
impl MemoryDistribution for MemoryDistributionMetric {
    /// Accumulates the provided sample in the metric.
    ///
    /// ## Arguments
    ///
    /// * `sample` - The sample to be recorded by the metric. The sample is assumed to be in the
    ///   configured memory unit of the metric.
    ///
    /// ## Notes
    ///
    /// Values bigger than 1 Terabyte (2<sup>40</sup> bytes) are truncated
    /// and an `ErrorType::InvalidValue` error is recorded.
    pub fn accumulate(&self, sample: u64) {
        match self {
            MemoryDistributionMetric::Parent { inner, .. } => {
                // values are capped at 2**40.
                // If the value doesn't fit into `i64` it's definitely to large
                // and cause an error.
                // glean-core handles that.
                let sample = sample.try_into().unwrap_or_else(|_| {
                    log::warn!(
                        "Memory size too large to fit into into 64-bytes. Saturating at i64::MAX."
                    );
                    i64::MAX
                });
                inner.accumulate(sample);
            }
            MemoryDistributionMetric::Child(c) => {
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.memory_samples.get_mut(&c.0) {
                        v.push(sample);
                    } else {
                        payload.memory_samples.insert(c.0, vec![sample]);
                    }
                });
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently-stored histogram as a DistributionData of the serialized value.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `ping_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            MemoryDistributionMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            MemoryDistributionMetric::Child(c) => {
                panic!("Cannot get test value for {:?} in non-parent process!", c.0)
            }
        }
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors recorded.
    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            MemoryDistributionMetric::Parent { inner, .. } => {
                inner.test_get_num_recorded_errors(error)
            }
            MemoryDistributionMetric::Child(c) => panic!(
                "Cannot get the number of recorded errors for {:?} in non-parent process!",
                c.0
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_memory_distribution() {
        let _lock = lock_test();

        let metric = MemoryDistributionMetric::new(
            0.into(),
            CommonMetricData {
                name: "memory_distribution_metric".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["store1".into()],
                disabled: false,
                ..Default::default()
            },
            MemoryUnit::Kilobyte,
        );

        metric.accumulate(42);

        let metric_data = metric.test_get_value("store1").unwrap();
        assert_eq!(1, metric_data.values[&42494]);
        assert_eq!(0, metric_data.values[&44376]);
        assert_eq!(43008, metric_data.sum);
    }

    #[test]
    fn memory_distribution_child() {
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_memory_dist;
        parent_metric.accumulate(42);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);
            child_metric.accumulate(13 * 9);
        }

        let metric_data = parent_metric.test_get_value("store1").unwrap();
        assert_eq!(1, metric_data.values[&42494]);
        assert_eq!(0, metric_data.values[&44376]);
        assert_eq!(43008, metric_data.sum);

        // Single-process IPC machine goes brrrrr...
        let buf = ipc::take_buf().unwrap();
        assert!(buf.len() > 0);
        assert!(ipc::replay_from_buf(&buf).is_ok());

        let data = parent_metric.test_get_value(None).expect("must have data");
        assert_eq!(2, data.values.values().fold(0, |acc, count| acc + count));
        assert_eq!(1, data.values[&42494]);
        assert_eq!(1, data.values[&115097]);
        assert_eq!(162816, data.sum);
    }
}
