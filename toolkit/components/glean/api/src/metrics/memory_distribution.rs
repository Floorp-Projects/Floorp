// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use super::{CommonMetricData, DistributionData, MemoryUnit};

/// A memory distribution metric.
///
/// Memory distributions are used to accumulate and store memory measurements for analyzing distributions of the memory data.
#[derive(Debug)]
pub struct MemoryDistributionMetric {
    inner: glean_core::metrics::MemoryDistributionMetric,
}

impl MemoryDistributionMetric {
    /// Create a new memory distribution metric.
    pub fn new(meta: CommonMetricData, memory_unit: MemoryUnit) -> Self {
        let inner = glean_core::metrics::MemoryDistributionMetric::new(meta, memory_unit);
        Self { inner }
    }

    /// Accumulates the provided sample in the metric.
    ///
    /// ## Arguments
    ///
    /// * `sample` - The sample to be recorded by the metric. The sample is assumed to be in the
    ///   configured memory unit of the metric.
    ///
    /// ## Notes
    ///
    /// Values bigger than 1 Terabyte (2<sup>40</sup> bytes) are truncated
    /// and an `ErrorType::InvalidValue` error is recorded.
    pub fn accumulate(&self, sample: u64) {
        crate::with_glean(|glean| self.inner.accumulate(glean, sample))
    }

    /// **Test-only API.**
    ///
    /// Get the currently-stored histogram as a DistributionData of the serialized value.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<DistributionData> {
        crate::with_glean(move |glean| self.inner.test_get_value(glean, storage_name))
    }
}
