// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! A simple histogram implementation for exponential histograms.

use std::collections::HashMap;
use std::convert::TryFrom;

use serde::{Deserialize, Serialize};

use crate::error::{Error, ErrorKind};

pub use exponential::PrecomputedExponential;
pub use functional::Functional;
pub use linear::PrecomputedLinear;

mod exponential;
mod functional;
mod linear;

/// Different kinds of histograms.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum HistogramType {
    /// A histogram with linear distributed buckets.
    Linear,
    /// A histogram with exponential distributed buckets.
    Exponential,
}

impl TryFrom<i32> for HistogramType {
    type Error = Error;

    fn try_from(value: i32) -> Result<HistogramType, Self::Error> {
        match value {
            0 => Ok(HistogramType::Linear),
            1 => Ok(HistogramType::Exponential),
            e => Err(ErrorKind::HistogramType(e).into()),
        }
    }
}

/// A histogram.
///
/// Stores the counts per bucket and tracks the count of added samples and the total sum.
/// The bucketing algorithm can be changed.
///
/// ## Example
///
/// ```rust,ignore
/// let mut hist = Histogram::exponential(1, 500, 10);
///
/// for i in 1..=10 {
///     hist.accumulate(i);
/// }
///
/// assert_eq!(10, hist.count());
/// assert_eq!(55, hist.sum());
/// ```
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Histogram<B> {
    /// Mapping bucket's minimum to sample count.
    values: HashMap<u64, u64>,

    /// The count of samples added.
    count: u64,
    /// The total sum of samples.
    sum: u64,

    /// The bucketing algorithm used.
    bucketing: B,
}

/// A bucketing algorithm for histograms.
///
/// It's responsible to calculate the bucket a sample goes into.
/// It can calculate buckets on-the-fly or pre-calculate buckets and re-use that when needed.
pub trait Bucketing {
    /// Get the bucket's minimum value the sample falls into.
    fn sample_to_bucket_minimum(&self, sample: u64) -> u64;

    /// The computed bucket ranges for this bucketing algorithm.
    fn ranges(&self) -> &[u64];
}

impl<B: Bucketing> Histogram<B> {
    /// Gets the number of buckets in this histogram.
    pub fn bucket_count(&self) -> usize {
        self.values.len()
    }

    /// Adds a single value to this histogram.
    pub fn accumulate(&mut self, sample: u64) {
        let bucket_min = self.bucketing.sample_to_bucket_minimum(sample);
        let entry = self.values.entry(bucket_min).or_insert(0);
        *entry += 1;
        self.sum = self.sum.saturating_add(sample);
        self.count += 1;
    }

    /// Gets the total sum of values recorded in this histogram.
    pub fn sum(&self) -> u64 {
        self.sum
    }

    /// Gets the total count of values recorded in this histogram.
    pub fn count(&self) -> u64 {
        self.count
    }

    /// Gets the filled values.
    pub fn values(&self) -> &HashMap<u64, u64> {
        &self.values
    }

    /// Checks if this histogram recorded any values.
    pub fn is_empty(&self) -> bool {
        self.count() == 0
    }

    /// Gets a snapshot of all values from the first bucket until one past the last filled bucket,
    /// filling in empty buckets with 0.
    pub fn snapshot_values(&self) -> HashMap<u64, u64> {
        let mut res = self.values.clone();

        let max_bucket = self.values.keys().max().cloned().unwrap_or(0);

        for &min_bucket in self.bucketing.ranges() {
            // Fill in missing entries.
            let _ = res.entry(min_bucket).or_insert(0);
            // stop one after the last filled bucket
            if min_bucket > max_bucket {
                break;
            }
        }
        res
    }
}
