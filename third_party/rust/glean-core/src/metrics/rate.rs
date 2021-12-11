// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error_recording::{record_error, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A rate metric.
///
/// Used to determine the proportion of things via two counts:
/// * A numerator defining the amount of times something happened,
/// * A denominator counting the amount of times someting could have happened.
///
/// Both numerator and denominator can only be incremented, not decremented.
#[derive(Clone, Debug)]
pub struct RateMetric {
    meta: CommonMetricData,
}

impl MetricType for RateMetric {
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
impl RateMetric {
    /// Creates a new rate metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self { meta }
    }

    /// Increases the numerator by `amount`.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `amount` - The amount to increase by. Should be non-negative.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `amount` is negative.
    pub fn add_to_numerator(&self, glean: &Glean, amount: i32) {
        if !self.should_record(glean) {
            return;
        }

        if amount < 0 {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidValue,
                format!("Added negative value {} to numerator", amount),
                None,
            );
            return;
        }

        glean
            .storage()
            .record_with(glean, &self.meta, |old_value| match old_value {
                Some(Metric::Rate(num, den)) => Metric::Rate(num.saturating_add(amount), den),
                _ => Metric::Rate(amount, 0), // Denominator will show up eventually. Probably.
            });
    }

    /// Increases the denominator by `amount`.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `amount` - The amount to increase by. Should be non-negative.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `amount` is negative.
    pub fn add_to_denominator(&self, glean: &Glean, amount: i32) {
        if !self.should_record(glean) {
            return;
        }

        if amount < 0 {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidValue,
                format!("Added negative value {} to denominator", amount),
                None,
            );
            return;
        }

        glean
            .storage()
            .record_with(glean, &self.meta, |old_value| match old_value {
                Some(Metric::Rate(num, den)) => Metric::Rate(num, den.saturating_add(amount)),
                _ => Metric::Rate(0, amount),
            });
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as a pair of integers.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<(i32, i32)> {
        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Rate(n, d)) => Some((n, d)),
            _ => None,
        }
    }
}
