// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::sync::{
    atomic::{AtomicU64, Ordering},
    Arc, RwLock,
};

use super::{CommonMetricData, Instant, TimeUnit};

use crate::dispatcher;
use crate::ipc::{need_ipc, with_ipc_payload, MetricId};

/// Identifier for a running timer.
///
/// *Note*: This is not `Clone` (or `Copy`) and thus is consumable by the TimingDistribution API,
/// avoiding re-use of a `TimerId` object.
#[derive(Debug, Eq, PartialEq)]
pub struct TimerId(glean_core::metrics::TimerId);

/// A timing distribution metric.
///
/// Timing distributions are used to accumulate and store time measurements for analyzing distributions of the timing data.
#[derive(Debug)]
pub enum TimingDistributionMetric {
    Parent(Arc<TimingDistributionMetricImpl>),
    Child(TimingDistributionMetricIpc),
}
#[derive(Debug)]
pub struct TimingDistributionMetricImpl {
    inner: RwLock<glean_core::metrics::TimingDistributionMetric>,
}
#[derive(Debug)]
pub struct TimingDistributionMetricIpc {
    metric_id: MetricId,
    next_timer_id: AtomicU64,
    instants: RwLock<HashMap<u64, std::time::Instant>>,
}

impl TimingDistributionMetric {
    /// Create a new timing distribution metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            TimingDistributionMetric::Child(TimingDistributionMetricIpc {
                metric_id: MetricId::new(meta),
                next_timer_id: AtomicU64::new(0),
                instants: RwLock::new(HashMap::new()),
            })
        } else {
            TimingDistributionMetric::Parent(Arc::new(TimingDistributionMetricImpl::new(
                meta, time_unit,
            )))
        }
    }

    /// Start tracking time for the provided metric.
    ///
    /// ## Return
    ///
    /// Returns a new `TimerId` identifying the timer.
    pub fn start(&self) -> TimerId {
        match self {
            TimingDistributionMetric::Parent(p) => {
                // Since this is a synchronous return, we can't use the
                // Dispatcher, and we can't rely on the Global Glean to provide
                // mutual exclusion. Use the RwLock to guard the inner metric,
                // and synchronously create a new TimerId.
                p.start()
            }
            TimingDistributionMetric::Child(c) => {
                // There is no glean-core on this process to give us a TimerId,
                // so we'll have to make our own and do our own bookkeeping.
                let id = c.next_timer_id.fetch_add(1, Ordering::SeqCst);
                let mut map = c
                    .instants
                    .write()
                    .expect("lock of instants map was poisoned");
                if let Some(_v) = map.insert(id, std::time::Instant::now()) {
                    // TODO: report an error and find a different TimerId.
                }
                TimerId(id)
            }
        }
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
        match self {
            TimingDistributionMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.stop_and_accumulate(id));
            }
            TimingDistributionMetric::Child(c) => {
                let mut map = c
                    .instants
                    .write()
                    .expect("Write lock must've been poisoned.");
                if let Some(start) = map.remove(&id.0) {
                    let sample = start.elapsed().as_nanos();
                    with_ipc_payload(move |payload| {
                        if let Some(v) = payload.timing_samples.get_mut(&c.metric_id) {
                            v.push(sample);
                        } else {
                            payload
                                .timing_samples
                                .insert(c.metric_id.clone(), vec![sample]);
                        }
                    });
                } else {
                    // TODO: report an error (timer id for stop wasn't started).
                }
            }
        }
    }

    /// Cancel a previous `start` call.
    ///
    /// ## Arguments
    ///
    /// * `id` - The `TimerId` to associate with this timing. This allows
    ///          for concurrent timing of events associated with different ids to the
    ///          same timing distribution metric.
    pub fn cancel(&self, id: TimerId) {
        match self {
            TimingDistributionMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.cancel(id));
            }
            TimingDistributionMetric::Child(c) => {
                let mut map = c
                    .instants
                    .write()
                    .expect("Write lock must've been poisoned.");
                if map.remove(&id.0).is_none() {
                    // TODO: report an error (cancelled a non-started id).
                }
            }
        }
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
        match self {
            TimingDistributionMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name)
            }
            TimingDistributionMetric::Child(_c) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl TimingDistributionMetricImpl {
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        let inner = RwLock::new(glean_core::metrics::TimingDistributionMetric::new(
            meta, time_unit,
        ));
        Self { inner }
    }

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
