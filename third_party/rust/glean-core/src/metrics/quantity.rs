// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
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
    /// * `value` - The value. Must be non-negative.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `value` is negative.
    pub fn set(&self, value: i64) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_sync(glean, value))
    }

    /// Sets the value synchronously. Must be non-negative.
    #[doc(hidden)]
    pub fn set_sync(&self, glean: &Glean, value: i64) {
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

    /// Get current value.
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<i64> {
        let queried_ping_name = ping_name
            .into()
            .unwrap_or_else(|| &self.meta().send_in_pings[0]);

        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Quantity(i)) => Some(i),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as an integer.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<i64> {
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
