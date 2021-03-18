// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error_recording::{record_error, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A counter metric.
///
/// Used to count things.
/// The value can only be incremented, not decremented.
#[derive(Clone, Debug)]
pub struct CounterMetric {
    meta: CommonMetricData,
}

impl MetricType for CounterMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn meta_mut(&mut self) -> &mut CommonMetricData {
        &mut self.meta
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl CounterMetric {
    /// Creates a new counter metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self { meta }
    }

    /// Increases the counter by `amount`.
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
                &self.meta,
                ErrorType::InvalidValue,
                format!("Added negative or zero value {}", amount),
                None,
            );
            return;
        }

        glean
            .storage()
            .record_with(glean, &self.meta, |old_value| match old_value {
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
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Counter(i)) => Some(i),
            _ => None,
        }
    }
}
