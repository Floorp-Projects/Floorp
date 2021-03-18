// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error_recording::{record_error, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A quantity metric.
///
/// Used to store explicit non-negative integers.
#[derive(Clone, Debug)]
pub struct QuantityMetric {
    meta: CommonMetricData,
}

impl MetricType for QuantityMetric {
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
impl QuantityMetric {
    /// Creates a new quantity metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self { meta }
    }

    /// Sets the value. Must be non-negative.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `value` - The value. Must be non-negative.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `value` is negative.
    pub fn set(&self, glean: &Glean, value: i64) {
        if !self.should_record(glean) {
            return;
        }

        if value < 0 {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidValue,
                format!("Set negative value {}", value),
                None,
            );
            return;
        }

        glean
            .storage()
            .record(glean, &self.meta, &Metric::Quantity(value))
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as an integer.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<i64> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Quantity(i)) => Some(i),
            _ => None,
        }
    }
}
