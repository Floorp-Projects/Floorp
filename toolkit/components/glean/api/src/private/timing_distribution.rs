// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::RwLock;

use super::{CommonMetricData, Instant, TimeUnit};

/// Identifier for a running timer.
///
/// *Note*: This is not `Clone` (or `Copy`) and thus is consumable by the TimingDistribution API,
/// avoiding re-use of a `TimerId` object.
pub struct TimerId(glean_core::metrics::TimerId);

/// A timing distribution metric.
///
/// Timing distributions are used to accumulate and store time measurements for analyzing distributions of the timing data.
#[derive(Debug)]
pub struct TimingDistributionMetric {
    inner: RwLock<glean_core::metrics::TimingDistributionMetric>,
}

impl TimingDistributionMetric {
    /// Create a new timing distribution metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        let inner = RwLock::new(glean_core::metrics::TimingDistributionMetric::new(
            meta, time_unit,
        ));
        Self { inner }
    }

    /// Start tracking time for the provided metric.
    ///
    /// ## Return
    ///
    /// Returns a new `TimerId` identifying the timer.
    pub fn start(&self) -> TimerId {
        let now = Instant::now();

        let mut inner = self
            .inner
            .write()
            .expect("lock of wrapped metric was poisoned");
        TimerId(inner.set_start(now.as_nanos()))
    }

    /// Stop tracking time for the provided metric and associated timer id.
    ///
    /// Add a count to the corresponding bucket in the timing distribution.
    ///
    /// ## Arguments
    ///
    /// * `id` - The `TimerId` to associate with this timing.
    ///          This allows for concurrent timing of events associated
    ///          with different ids to the same timespan metric.
    pub fn stop_and_accumulate(&self, id: TimerId) {
        crate::with_glean(|glean| {
            let TimerId(id) = id;
            let now = Instant::now();

            let mut inner = self
                .inner
                .write()
                .expect("lock of wrapped metric was poisoned");
            inner.set_stop_and_accumulate(glean, id, now.as_nanos())
        })
    }

    /// Cancel a previous `start` call.
    ///
    /// ## Arguments
    ///
    /// * `id` - The `TimerId` to associate with this timing. This allows
    ///          for concurrent timing of events associated with different ids to the
    ///          same timing distribution metric.
    pub fn cancel(&self, id: TimerId) {
        let TimerId(id) = id;
        let mut inner = self
            .inner
            .write()
            .expect("lock of wrapped metric was poisoned");
        inner.cancel(id)
    }

    /// **Test-only API.**
    ///
    /// Get the currently-stored histogram as a JSON String of the serialized value.
    /// This doesn't clear the stored value.
    ///
    /// ## Note
    ///
    /// This currently returns the value as a JSON encoded string.
    /// `glean_core` doesn't expose the underlying histogram type.
    /// This will eventually change to a proper `DistributionData` type.
    /// See [Bug 1634415](https://bugzilla.mozilla.org/show_bug.cgi?id=1634415).
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<String> {
        crate::with_glean(move |glean| {
            let inner = self
                .inner
                .read()
                .expect("lock of wrapped metric was poisoned");
            inner.test_get_value_as_json_string(glean, storage_name)
        })
    }
}
