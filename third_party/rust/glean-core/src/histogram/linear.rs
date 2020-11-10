// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::cmp;
use std::collections::HashMap;

use once_cell::sync::OnceCell;
use serde::{Deserialize, Serialize};

use super::{Bucketing, Histogram};

/// Create the possible ranges in a linear distribution from `min` to `max` with
/// `bucket_count` buckets.
///
/// This algorithm calculates `bucket_count` number of buckets of equal sizes between `min` and `max`.
///
/// Bucket limits are the minimal bucket value.
/// That means values in a bucket `i` are `bucket[i] <= value < bucket[i+1]`.
/// It will always contain an underflow bucket (`< 1`).
fn linear_range(min: u64, max: u64, count: usize) -> Vec<u64> {
    let mut ranges = Vec::with_capacity(count);
    ranges.push(0);

    let min = cmp::max(1, min);
    let count = count as u64;
    for i in 1..count {
        let range = (min * (count - 1 - i) + max * (i - 1)) / (count - 2);
        ranges.push(range);
    }

    ranges
}

/// A linear bucketing algorithm.
///
/// Buckets are pre-computed at instantiation with a linear  distribution from `min` to `max`
/// and `bucket_count` buckets.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct PrecomputedLinear {
    // Don't serialize the (potentially large) array of ranges, instead compute them on first
    // access.
    #[serde(skip)]
    bucket_ranges: OnceCell<Vec<u64>>,
    min: u64,
    max: u64,
    bucket_count: usize,
}

impl Bucketing for PrecomputedLinear {
    /// Get the bucket for the sample.
    ///
    /// This uses a binary search to locate the index `i` of the bucket such that:
    /// bucket[i] <= sample < bucket[i+1]
    fn sample_to_bucket_minimum(&self, sample: u64) -> u64 {
        let limit = match self.ranges().binary_search(&sample) {
            // Found an exact match to fit it in
            Ok(i) => i,
            // Sorted it fits after the bucket's limit, therefore it fits into the previous bucket
            Err(i) => i - 1,
        };

        self.ranges()[limit]
    }

    fn ranges(&self) -> &[u64] {
        // Create the linear range on first access.
        self.bucket_ranges
            .get_or_init(|| linear_range(self.min, self.max, self.bucket_count))
    }
}

impl Histogram<PrecomputedLinear> {
    /// Create a histogram with `bucket_count` linear buckets in the range `min` to `max`.
    pub fn linear(min: u64, max: u64, bucket_count: usize) -> Histogram<PrecomputedLinear> {
        Histogram {
            values: HashMap::new(),
            count: 0,
            sum: 0,
            bucketing: PrecomputedLinear {
                bucket_ranges: OnceCell::new(),
                min,
                max,
                bucket_count,
            },
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    const DEFAULT_BUCKET_COUNT: usize = 100;
    const DEFAULT_RANGE_MIN: u64 = 0;
    const DEFAULT_RANGE_MAX: u64 = 100;

    #[test]
    fn can_count() {
        let mut hist = Histogram::linear(1, 500, 10);
        assert!(hist.is_empty());

        for i in 1..=10 {
            hist.accumulate(i);
        }

        assert_eq!(10, hist.count());
        assert_eq!(55, hist.sum());
    }

    #[test]
    fn overflow_values_accumulate_in_the_last_bucket() {
        let mut hist =
            Histogram::linear(DEFAULT_RANGE_MIN, DEFAULT_RANGE_MAX, DEFAULT_BUCKET_COUNT);

        hist.accumulate(DEFAULT_RANGE_MAX + 100);
        assert_eq!(1, hist.values[&DEFAULT_RANGE_MAX]);
    }

    #[test]
    fn short_linear_buckets_are_correct() {
        let test_buckets = vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 10];

        assert_eq!(test_buckets, linear_range(1, 10, 10));
        // There's always a zero bucket, so we increase the lower limit.
        assert_eq!(test_buckets, linear_range(0, 10, 10));
    }

    #[test]
    fn long_linear_buckets_are_correct() {
        // Hand calculated values using current default range 0 - 60000 and bucket count of 100.
        // NOTE: The final bucket, regardless of width, represents the overflow bucket to hold any
        // values beyond the maximum (in this case the maximum is 60000)
        let test_buckets = vec![
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
            46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67,
            68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
            90, 91, 92, 93, 94, 95, 96, 97, 98, 100,
        ];

        assert_eq!(
            test_buckets,
            linear_range(DEFAULT_RANGE_MIN, DEFAULT_RANGE_MAX, DEFAULT_BUCKET_COUNT)
        );
    }

    #[test]
    fn default_buckets_correctly_accumulate() {
        let mut hist =
            Histogram::linear(DEFAULT_RANGE_MIN, DEFAULT_RANGE_MAX, DEFAULT_BUCKET_COUNT);

        for i in &[1, 10, 100, 1000, 10000] {
            hist.accumulate(*i);
        }

        assert_eq!(11111, hist.sum());
        assert_eq!(5, hist.count());

        assert_eq!(None, hist.values.get(&0));
        assert_eq!(1, hist.values[&1]);
        assert_eq!(1, hist.values[&10]);
        assert_eq!(3, hist.values[&100]);
    }

    #[test]
    fn accumulate_large_numbers() {
        let mut hist = Histogram::linear(1, 500, 10);

        hist.accumulate(u64::max_value());
        hist.accumulate(u64::max_value());

        assert_eq!(2, hist.count());
        // Saturate before overflowing
        assert_eq!(u64::max_value(), hist.sum());
        assert_eq!(2, hist.values[&500]);
    }
}
