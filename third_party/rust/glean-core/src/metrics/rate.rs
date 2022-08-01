// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A rate value as given by its numerator and denominator.
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Rate {
    /// A rate's numerator
    pub numerator: i32,
    /// A rate's denominator
    pub denominator: i32,
}

impl From<(i32, i32)> for Rate {
    fn from((num, den): (i32, i32)) -> Self {
        Self {
            numerator: num,
            denominator: den,
        }
    }
}

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
    pub fn add_to_numerator(&self, amount: i32) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.add_to_numerator_sync(glean, amount))
    }

    #[doc(hidden)]
    pub fn add_to_numerator_sync(&self, glean: &Glean, amount: i32) {
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
    pub fn add_to_denominator(&self, amount: i32) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.add_to_denominator_sync(glean, amount))
    }

    #[doc(hidden)]
    pub fn add_to_denominator_sync(&self, glean: &Glean, amount: i32) {
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
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<Rate> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| self.get_value(glean, ping_name.as_deref()))
    }

    /// Get current value
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<Rate> {
        let queried_ping_name = ping_name
            .into()
            .unwrap_or_else(|| &self.meta().send_in_pings[0]);

        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Rate(n, d)) => Some((n, d).into()),
            _ => None,
        }
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
