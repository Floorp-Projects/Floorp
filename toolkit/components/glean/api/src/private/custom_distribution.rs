// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use super::{CommonMetricData, MetricId};
use glean::{DistributionData, ErrorType, HistogramType};

use crate::ipc::{need_ipc, with_ipc_payload};
use glean::traits::CustomDistribution;

/// A custom distribution metric.
///
/// Custom distributions are used to record the distribution of arbitrary values.
pub enum CustomDistributionMetric {
    Parent {
        /// The metric's ID.
        ///
        /// **TEST-ONLY** - Do not use unless gated with `#[cfg(test)]`.
        id: MetricId,
        inner: glean::private::CustomDistributionMetric,
    },
    Child(CustomDistributionMetricIpc),
}
#[derive(Debug)]
pub struct CustomDistributionMetricIpc(MetricId);

impl CustomDistributionMetric {
    /// Create a new timing distribution metric.
    pub fn new(
        id: MetricId,
        meta: CommonMetricData,
        range_min: u64,
        range_max: u64,
        bucket_count: u64,
        histogram_type: HistogramType,
    ) -> Self {
        if need_ipc() {
            CustomDistributionMetric::Child(CustomDistributionMetricIpc(id))
        } else {
            debug_assert!(
                range_min <= i64::MAX as u64,
                "sensible limits enforced by glean_parser"
            );
            debug_assert!(
                range_max <= i64::MAX as u64,
                "sensible limits enforced by glean_parser"
            );
            debug_assert!(
                bucket_count <= i64::MAX as u64,
                "sensible limits enforced by glean_parser"
            );
            let inner = glean::private::CustomDistributionMetric::new(
                meta,
                range_min as i64,
                range_max as i64,
                bucket_count as i64,
                histogram_type,
            );
            CustomDistributionMetric::Parent { id, inner }
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            CustomDistributionMetric::Parent { id, .. } => {
                CustomDistributionMetric::Child(CustomDistributionMetricIpc(*id))
            }
            CustomDistributionMetric::Child(_) => {
                panic!("Can't get a child metric from a child metric")
            }
        }
    }
}

#[inherent]
impl CustomDistribution for CustomDistributionMetric {
    pub fn accumulate_samples_signed(&self, samples: Vec<i64>) {
        match self {
            CustomDistributionMetric::Parent { inner, .. } => inner.accumulate_samples(samples),
            CustomDistributionMetric::Child(c) => {
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.custom_samples.get_mut(&c.0) {
                        v.extend(samples);
                    } else {
                        payload.custom_samples.insert(c.0, samples);
                    }
                });
            }
        }
    }

    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            CustomDistributionMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            CustomDistributionMetric::Child(c) => {
                panic!("Cannot get test value for {:?} in non-parent process!", c)
            }
        }
    }

    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        match self {
            CustomDistributionMetric::Parent { inner, .. } => {
                inner.test_get_num_recorded_errors(error)
            }
            CustomDistributionMetric::Child(c) => panic!(
                "Cannot get number of recorded errors for {:?} in non-parent process!",
                c
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_custom_distribution() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_custom_dist;

        metric.accumulate_samples_signed(vec![1, 2, 3]);

        assert!(metric.test_get_value("store1").is_some());
    }

    #[test]
    fn custom_distribution_child() {
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_custom_dist;
        parent_metric.accumulate_samples_signed(vec![1, 268435458]);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            child_metric.accumulate_samples_signed(vec![4, 268435460]);
        }

        let buf = ipc::take_buf().unwrap();
        assert!(buf.len() > 0);
        assert!(ipc::replay_from_buf(&buf).is_ok());

        let data = parent_metric
            .test_get_value("store1")
            .expect("should have some data");

        assert_eq!(2, data.values[&1], "Low bucket has 2 values");
        assert_eq!(
            2, data.values[&268435456],
            "Next higher bucket has 2 values"
        );
        assert_eq!(
            1 + 4 + 268435458 + 268435460,
            data.sum,
            "Sum of all recorded values"
        );
    }
}
