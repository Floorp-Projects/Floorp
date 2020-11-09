// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::metrics::DistributionData;

/// A description for the `MemoryDistributionMetric` type.
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

    /// Accumulates the provided signed samples in the metric.
    ///
    /// This is required so that the platform-specific code can provide us with
    /// 64 bit signed integers if no `u64` comparable type is available. This
    /// will take care of filtering and reporting errors for any provided negative
    /// sample.
    ///
    /// Please note that this assumes that the provided samples are already in the
    /// "unit" declared by the instance of the implementing metric type (e.g. if the
    /// implementing class is a [MemoryDistributionMetricType] and the instance this
    /// method was called on is using [MemoryUnit.Kilobyte], then `samples` are assumed
    /// to be in that unit).
    ///
    /// # Arguments
    ///
    /// * `samples` - The vector holding the samples to be recorded by the metric.
    ///
    /// ## Notes
    ///
    /// Discards any negative value in `samples` and report an `ErrorType::InvalidValue`
    /// for each of them.
    /// Values bigger than 1 Terabyte (2<sup>40</sup> bytes) are truncated
    /// and an `ErrorType::InvalidValue` error is recorded.
    fn accumulate_samples_signed(&self, samples: Vec<i64>);

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value as an integer.
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
    /// Gets the currently-stored histogram as a JSON String of the serialized value.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value_as_json_string<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<String>;
}
