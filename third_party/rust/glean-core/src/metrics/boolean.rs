// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A boolean metric.
///
/// Records a simple flag.
#[derive(Clone, Debug)]
pub struct BooleanMetric {
    meta: CommonMetricData,
}

impl MetricType for BooleanMetric {
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
impl BooleanMetric {
    /// Creates a new boolean metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self { meta }
    }

    /// Sets to the specified boolean value.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `value` - the value to set.
    pub fn set(&self, glean: &Glean, value: bool) {
        if !self.should_record(glean) {
            return;
        }

        let value = Metric::Boolean(value);
        glean.storage().record(glean, &self.meta, &value)
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as a boolean.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<bool> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Boolean(b)) => Some(b),
            _ => None,
        }
    }
}
