// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use uuid::Uuid;

use glean_core::CommonMetricData;

#[derive(Clone, Debug)]
pub struct UuidMetric(glean_core::metrics::UuidMetric);

impl UuidMetric {
    /// Create a new UUID metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::UuidMetric::new(meta))
    }

    /// Set to the specified value.
    ///
    /// ## Arguments
    ///
    /// * `value` - The UUID to set the metric to.
    pub fn set(&self, value: Uuid) {
        crate::with_glean(move |glean| self.0.set(glean, value))
    }

    /// Generate a new random UUID and set the metric to it.
    pub fn generate_and_set(&self) -> Uuid {
        crate::with_glean(move |glean| self.0.generate_and_set(glean))
    }

    /// **Test-only API.**
    ///
    /// Get the stored UUID value.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<Uuid> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
            .map(|uuid| Uuid::parse_str(&uuid).expect("can't parse internally created UUID value"))
    }
}
