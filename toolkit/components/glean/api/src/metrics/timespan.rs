// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::RwLock;

use super::CommonMetricData;

use super::{Instant, TimeUnit};

/// A timespan metric.
///
/// Timespans are used to make a measurement of how much time is spent in a particular task.
#[derive(Debug)]
pub struct TimespanMetric {
    inner: RwLock<glean_core::metrics::TimespanMetric>,
}

impl TimespanMetric {
    /// Create a new timespan metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        let inner = RwLock::new(glean_core::metrics::TimespanMetric::new(meta, time_unit));
        Self { inner }
    }

    /// Start tracking time for the provided metric.
    ///
    /// This records an error if it's already tracking time (i.e. start was already
    /// called with no corresponding `stop`): in that case the original
    /// start time will be preserved.
    pub fn start(&self) {
        crate::with_glean(move |glean| {
            let now = Instant::now();

            let mut inner = self
                .inner
                .write()
                .expect("lock of wrapped metric was poisoned");
            inner.set_start(glean, now.as_nanos())
        })
    }

    /// Stop tracking time for the provided metric.
    ///
    /// Sets the metric to the elapsed time.
    ///
    /// This will record an error if no `start` was called.
    pub fn stop(&self) {
        crate::with_glean(move |glean| {
            let now = Instant::now();

            let mut inner = self
                .inner
                .write()
                .expect("lock of wrapped metric was poisoned");
            inner.set_stop(glean, now.as_nanos())
        })
    }

    /// Cancel a previous `start` call.
    ///
    /// No error is recorded if no `start` was called.
    pub fn cancel(&self) {
        let mut inner = self
            .inner
            .write()
            .expect("lock of wrapped metric was poisoned");
        inner.cancel()
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value as an integer.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<u64> {
        crate::with_glean(move |glean| {
            let inner = self
                .inner
                .read()
                .expect("lock of wrapped metric was poisoned");
            inner.test_get_value(glean, storage_name)
        })
    }
}
