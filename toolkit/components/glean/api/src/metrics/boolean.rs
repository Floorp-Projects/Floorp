// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use glean_core::CommonMetricData;

/// A boolean metric.
///
/// Records a simple true or false value.
#[derive(Clone, Debug)]
pub struct BooleanMetric(glean_core::metrics::BooleanMetric);

impl BooleanMetric {
    /// Create a new boolean metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::BooleanMetric::new(meta))
    }

    /// Set to the specified boolean value.
    ///
    /// ## Arguments
    ///
    /// * `value` - the value to set.
    pub fn set(&self, value: bool) {
        crate::with_glean(move |glean| self.0.set(glean, value))
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value as a boolean.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<bool> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
    }
}
