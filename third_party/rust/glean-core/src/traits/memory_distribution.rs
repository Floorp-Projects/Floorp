// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::metrics::DistributionData;
use crate::ErrorType;

/// A description for the
/// [`MemoryDistributionMetric`](crate::metrics::MemoryDistributionMetric) type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait MemoryDistribution {
    /// Accumulates the provided sample in the metric.
    ///
    /// # Arguments
    ///
    /// * `sample` - The sample to be recorded by the metric. The sample is assumed to be in the
    ///   configured memory unit of the metric.
    ///
    /// ## Notes
    ///
    /// Values bigger than 1 Terabyte (2<sup>40</sup> bytes) are truncated
    /// and an `ErrorType::InvalidValue` error is recorded.
    fn accumulate(&self, sample: u64);

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value as a DistributionData of the serialized value.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData>;

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    ///
    /// # Returns
    ///
    /// The number of errors recorded.
    fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32;
}
