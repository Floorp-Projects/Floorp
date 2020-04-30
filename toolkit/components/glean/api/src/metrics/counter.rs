// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use super::CommonMetricData;

/// A counter metric.
///
/// Used to count things.
/// The value can only be incremented, not decremented.
#[derive(Clone, Debug)]
pub struct CounterMetric(glean_core::metrics::CounterMetric);

impl CounterMetric {
    /// Create a new counter metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::CounterMetric::new(meta))
    }

    /// Increase the counter by `amount`.
    ///
    /// ## Arguments
    ///
    /// * `amount` - The amount to increase by. Should be positive.
    ///
    /// ## Notes
    ///
    /// Logs an error if the `amount` is 0 or negative.
    pub fn add(&self, amount: i32) {
        crate::with_glean(move |glean| self.0.add(glean, amount))
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value as an integer.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<i32> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
    }
}
