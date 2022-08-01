// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use crate::error_recording::{test_get_num_recorded_errors, ErrorType};
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
    meta: Arc<CommonMetricData>,
}

impl MetricType for BooleanMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn with_name(&self, name: String) -> Self {
        let mut meta = (*self.meta).clone();
        meta.name = name;
        Self {
            meta: Arc::new(meta),
        }
    }

    fn with_dynamic_label(&self, label: String) -> Self {
        let mut meta = (*self.meta).clone();
        meta.dynamic_label = Some(label);
        Self {
            meta: Arc::new(meta),
        }
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl BooleanMetric {
    /// Creates a new boolean metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self {
            meta: Arc::new(meta),
        }
    }

    /// Sets to the specified boolean value.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `value` - the value to set.
    #[doc(hidden)]
    pub fn set_sync(&self, glean: &Glean, value: bool) {
        if !self.should_record(glean) {
            return;
        }

        let value = Metric::Boolean(value);
        glean.storage().record(glean, &self.meta, &value)
    }

    /// Sets to the specified boolean value.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `value` - the value to set.
    pub fn set(&self, value: bool) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_sync(glean, value))
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as a boolean.
    ///
    /// This doesn't clear the stored value.
    #[doc(hidden)]
    pub fn get_value(&self, glean: &Glean, ping_name: Option<&str>) -> Option<bool> {
        let queried_ping_name = ping_name.unwrap_or_else(|| &self.meta().send_in_pings[0]);

        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Boolean(b)) => Some(b),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as an integer.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<bool> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| self.get_value(glean, ping_name.as_deref()))
    }

    /// **Exported for test purposes.**
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
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        crate::block_on_dispatcher();

        crate::core::with_glean(|glean| {
            test_get_num_recorded_errors(glean, self.meta(), error).unwrap_or(0)
        })
    }
}
