// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error_recording::{record_error, ErrorType};
use crate::metrics::CounterMetric;
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::metrics::RateMetric;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A Denominator metric (a kind of count shared among Rate metrics).
///
/// Used to count things.
/// The value can only be incremented, not decremented.
#[derive(Clone, Debug)]
pub struct DenominatorMetric {
    counter: CounterMetric,
    numerators: Vec<RateMetric>,
}

impl MetricType for DenominatorMetric {
    fn meta(&self) -> &CommonMetricData {
        self.counter.meta()
    }

    fn meta_mut(&mut self) -> &mut CommonMetricData {
        self.counter.meta_mut()
    }
}

impl DenominatorMetric {
    /// Creates a new denominator metric.
    pub fn new(meta: CommonMetricData, numerators: Vec<CommonMetricData>) -> Self {
        Self {
            counter: CounterMetric::new(meta),
            numerators: numerators.into_iter().map(RateMetric::new).collect(),
        }
    }

    /// Increases the denominator by `amount`.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `amount` - The amount to increase by. Should be positive.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `amount` is 0 or negative.
    pub fn add(&self, glean: &Glean, amount: i32) {
        if !self.should_record(glean) {
            return;
        }

        if amount <= 0 {
            record_error(
                glean,
                self.meta(),
                ErrorType::InvalidValue,
                format!("Added negative or zero value {}", amount),
                None,
            );
            return;
        }

        for num in &self.numerators {
            num.add_to_denominator(glean, amount);
        }

        glean
            .storage()
            .record_with(glean, self.counter.meta(), |old_value| match old_value {
                Some(Metric::Counter(old_value)) => {
                    Metric::Counter(old_value.saturating_add(amount))
                }
                _ => Metric::Counter(amount),
            })
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as an integer.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<i32> {
        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            storage_name,
            &self.meta().identifier(glean),
            self.meta().lifetime,
        ) {
            Some(Metric::Counter(i)) => Some(i),
            _ => None,
        }
    }
}
