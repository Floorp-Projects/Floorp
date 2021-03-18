// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error_recording::{record_error, ErrorType};
use crate::histogram::{Bucketing, Histogram, HistogramType};
use crate::metrics::{DistributionData, Metric, MetricType};
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A custom distribution metric.
///
/// Memory distributions are used to accumulate and store memory sizes.
#[derive(Debug)]
pub struct CustomDistributionMetric {
    meta: CommonMetricData,
    range_min: u64,
    range_max: u64,
    bucket_count: u64,
    histogram_type: HistogramType,
}

/// Create a snapshot of the histogram.
///
/// The snapshot can be serialized into the payload format.
pub(crate) fn snapshot<B: Bucketing>(hist: &Histogram<B>) -> DistributionData {
    DistributionData {
        values: hist.snapshot_values(),
        sum: hist.sum(),
    }
}

impl MetricType for CustomDistributionMetric {
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
impl CustomDistributionMetric {
    /// Creates a new memory distribution metric.
    pub fn new(
        meta: CommonMetricData,
        range_min: u64,
        range_max: u64,
        bucket_count: u64,
        histogram_type: HistogramType,
    ) -> Self {
        Self {
            meta,
            range_min,
            range_max,
            bucket_count,
            histogram_type,
        }
    }

    /// Accumulates the provided signed samples in the metric.
    ///
    /// This is required so that the platform-specific code can provide us with
    /// 64 bit signed integers if no `u64` comparable type is available. This
    /// will take care of filtering and reporting errors for any provided negative
    /// sample.
    ///
    /// # Arguments
    ///
    /// - `samples` - The vector holding the samples to be recorded by the metric.
    ///
    /// ## Notes
    ///
    /// Discards any negative value in `samples` and report an [`ErrorType::InvalidValue`]
    /// for each of them.
    pub fn accumulate_samples_signed(&self, glean: &Glean, samples: Vec<i64>) {
        if !self.should_record(glean) {
            return;
        }

        let mut num_negative_samples = 0;

        // Generic accumulation function to handle the different histogram types and count negative
        // samples.
        fn accumulate<B: Bucketing, F>(
            samples: &[i64],
            mut hist: Histogram<B>,
            metric: F,
        ) -> (i32, Metric)
        where
            F: Fn(Histogram<B>) -> Metric,
        {
            let mut num_negative_samples = 0;
            for &sample in samples.iter() {
                if sample < 0 {
                    num_negative_samples += 1;
                } else {
                    let sample = sample as u64;
                    hist.accumulate(sample);
                }
            }
            (num_negative_samples, metric(hist))
        }

        glean.storage().record_with(glean, &self.meta, |old_value| {
            let (num_negative, hist) = match self.histogram_type {
                HistogramType::Linear => {
                    let hist = if let Some(Metric::CustomDistributionLinear(hist)) = old_value {
                        hist
                    } else {
                        Histogram::linear(
                            self.range_min,
                            self.range_max,
                            self.bucket_count as usize,
                        )
                    };
                    accumulate(&samples, hist, Metric::CustomDistributionLinear)
                }
                HistogramType::Exponential => {
                    let hist = if let Some(Metric::CustomDistributionExponential(hist)) = old_value
                    {
                        hist
                    } else {
                        Histogram::exponential(
                            self.range_min,
                            self.range_max,
                            self.bucket_count as usize,
                        )
                    };
                    accumulate(&samples, hist, Metric::CustomDistributionExponential)
                }
            };

            num_negative_samples = num_negative;
            hist
        });

        if num_negative_samples > 0 {
            let msg = format!("Accumulated {} negative samples", num_negative_samples);
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidValue,
                msg,
                num_negative_samples,
            );
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored histogram.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<DistributionData> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            // Boxing the value, in order to return either of the possible buckets
            Some(Metric::CustomDistributionExponential(hist)) => Some(snapshot(&hist)),
            Some(Metric::CustomDistributionLinear(hist)) => Some(snapshot(&hist)),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored histogram as a JSON String of the serialized value.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value_as_json_string(
        &self,
        glean: &Glean,
        storage_name: &str,
    ) -> Option<String> {
        self.test_get_value(glean, storage_name)
            .map(|snapshot| serde_json::to_string(&snapshot).unwrap())
    }
}
