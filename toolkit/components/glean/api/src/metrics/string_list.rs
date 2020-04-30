// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use super::CommonMetricData;

/// A string list metric.
///
/// This allows appending a string value with arbitrary content to a list.
#[derive(Clone, Debug)]
pub struct StringListMetric(glean_core::metrics::StringListMetric);

impl StringListMetric {
    /// Create a new string list metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::StringListMetric::new(meta))
    }

    /// Add a new string to the list.
    ///
    /// ## Arguments
    ///
    /// * `value` - The string to add.
    ///
    /// ## Notes
    ///
    /// Truncates the value if it is longer than `MAX_STRING_LENGTH` bytes and logs an error.
    /// See [String list metric limits](https://mozilla.github.io/glean/book/user/metrics/string_list.html#limits).
    pub fn add<S: Into<String>>(&self, value: S) {
        crate::with_glean(move |glean| self.0.add(glean, value))
    }

    /// Set to a specific list of strings.
    ///
    /// ## Arguments
    ///
    /// * `value` - The list of string to set the metric to.
    ///
    /// ## Notes
    ///
    /// If passed an empty list, records an error and returns.
    /// Truncates the list if it is longer than `MAX_LIST_LENGTH` and logs an error.
    /// Truncates any value in the list if it is longer than `MAX_STRING_LENGTH` and logs an error.
    pub fn set(&self, value: Vec<String>) {
        crate::with_glean(move |glean| self.0.set(glean, value))
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored values.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<Vec<String>> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
    }
}
