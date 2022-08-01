// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;
use std::collections::HashMap;
use std::convert::TryInto;
use std::sync::{
    atomic::{AtomicUsize, Ordering},
    RwLock,
};
use std::time::{Duration, Instant};

use super::{CommonMetricData, MetricId, TimeUnit};
use glean::{DistributionData, ErrorType, TimerId};

use crate::ipc::{need_ipc, with_ipc_payload};
use glean::traits::TimingDistribution;

/// A timing distribution metric.
///
/// Timing distributions are used to accumulate and store time measurements for analyzing distributions of the timing data.
pub enum TimingDistributionMetric {
    Parent {
        /// The metric's ID.
        ///
        /// No longer test-only, is also used for GIFFT.
        id: MetricId,
        inner: glean::private::TimingDistributionMetric,
    },
    Child(TimingDistributionMetricIpc),
}
#[derive(Debug)]
pub struct TimingDistributionMetricIpc {
    metric_id: MetricId,
    next_timer_id: AtomicUsize,
    instants: RwLock<HashMap<u64, Instant>>,
}

impl TimingDistributionMetric {
    /// Create a new timing distribution metric.
    pub fn new(id: MetricId, meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            TimingDistributionMetric::Child(TimingDistributionMetricIpc {
                metric_id: id,
                next_timer_id: AtomicUsize::new(0),
                instants: RwLock::new(HashMap::new()),
            })
        } else {
            let inner = glean::private::TimingDistributionMetric::new(meta, time_unit);
            TimingDistributionMetric::Parent { id, inner }
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            TimingDistributionMetric::Parent { id, .. } => {
                TimingDistributionMetric::Child(TimingDistributionMetricIpc {
                    metric_id: *id,
                    next_timer_id: AtomicUsize::new(0),
                    instants: RwLock::new(HashMap::new()),
                })
            }
            TimingDistributionMetric::Child(_) => {
                panic!("Can't get a child metric from a child metric")
            }
        }
    }

    pub(crate) fn accumulate_raw_samples_nanos(&self, samples: Vec<u64>) {
        match self {
            TimingDistributionMetric::Parent { inner, .. } => {
                inner.accumulate_raw_samples_nanos(samples);
            }
            TimingDistributionMetric::Child(_) => {
                // TODO: Instrument this error
                log::error!("Can't record samples for a timing distribution from a child metric");
            }
        }
    }

    /// Accumulates a time duration sample for the provided metric.
    ///
    /// Adds a count to the corresponding bucket in the timing distribution.
    /// Saturates at u64::MAX nanoseconds.
    ///
    /// Prefer start() and stop_and_accumulate() where possible.
    ///
    /// Users of this API are responsible for ensuring the timing source used
    /// to calculate the duration is monotonic and consistent across platforms.
    ///
    /// # Arguments
    ///
    /// * `duration` - The [`Duration`] of the accumulated sample.
    pub fn accumulate_raw_duration(&self, duration: Duration) {
        let sample = duration.as_nanos().try_into().unwrap_or_else(|_| {
            // TODO: Instrument this error
            log::warn!(
                "Elapsed nanoseconds larger than fits into 64-bytes. Saturating at u64::MAX."
            );
            u64::MAX
        });
        // May be unused in builds without gecko.
        let _sample_ms = duration.as_millis().try_into().unwrap_or_else(|_| {
            // TODO: Instrument this error
            log::warn!(
                "Elapsed milliseconds larger than fits into 32-bytes. Saturating at u32::MAX."
            );
            u32::MAX
        });
        match self {
            TimingDistributionMetric::Parent {
                id: _metric_id,
                inner,
            } => {
                #[cfg(feature = "with_gecko")]
                {
                    extern "C" {
                        fn GIFFT_TimingDistributionAccumulateRawMillis(metric_id: u32, sample: u32);
                    }
                    // SAFETY: using only primitives, no return value.
                    unsafe {
                        GIFFT_TimingDistributionAccumulateRawMillis(_metric_id.0, _sample_ms);
                    }
                }
                inner.accumulate_raw_samples_nanos(vec![sample]);
            }
            TimingDistributionMetric::Child(c) => {
                #[cfg(feature = "with_gecko")]
                {
                    extern "C" {
                        fn GIFFT_TimingDistributionAccumulateRawMillis(metric_id: u32, sample: u32);
                    }
                    // SAFETY: using only primitives, no return value.
                    unsafe {
                        GIFFT_TimingDistributionAccumulateRawMillis(c.metric_id.0, _sample_ms);
                    }
                }
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.timing_samples.get_mut(&c.metric_id) {
                        v.push(sample);
                    } else {
                        payload.timing_samples.insert(c.metric_id, vec![sample]);
                    }
                });
            }
        }
    }
}

#[inherent]
impl TimingDistribution for TimingDistributionMetric {
    /// Starts tracking time for the provided metric.
    ///
    /// This records an error if it’s already tracking time (i.e.
    /// [`start`](TimingDistribution::start) was already called with no corresponding
    /// [`stop_and_accumulate`](TimingDistribution::stop_and_accumulate)): in that case the
    /// original start time will be preserved.
    ///
    /// # Returns
    ///
    /// A unique [`TimerId`] for the new timer.
    pub fn start(&self) -> TimerId {
        match self {
            TimingDistributionMetric::Parent { id: _id, inner } => {
                let timer_id = inner.start();
                #[cfg(feature = "with_gecko")]
                {
                    extern "C" {
                        fn GIFFT_TimingDistributionStart(metric_id: u32, timer_id: u64);
                    }
                    // SAFETY: using only primitives, no return value.
                    unsafe {
                        GIFFT_TimingDistributionStart(_id.0, timer_id.id);
                    }
                }
                timer_id.into()
            }
            TimingDistributionMetric::Child(c) => {
                // There is no glean-core on this process to give us a TimerId,
                // so we'll have to make our own and do our own bookkeeping.
                let id = c
                    .next_timer_id
                    .fetch_add(1, Ordering::SeqCst)
                    .try_into()
                    .unwrap();
                let mut map = c
                    .instants
                    .write()
                    .expect("lock of instants map was poisoned");
                if let Some(_v) = map.insert(id, Instant::now()) {
                    // TODO: report an error and find a different TimerId.
                }
                #[cfg(feature = "with_gecko")]
                {
                    extern "C" {
                        fn GIFFT_TimingDistributionStart(metric_id: u32, timer_id: u64);
                    }
                    // SAFETY: using only primitives, no return value.
                    unsafe {
                        GIFFT_TimingDistributionStart(c.metric_id.0, id);
                    }
                }
                id.into()
            }
        }
    }

    /// Stops tracking time for the provided metric and associated timer id.
    ///
    /// Adds a count to the corresponding bucket in the timing distribution.
    /// This will record an error if no [`start`](TimingDistribution::start) was
    /// called.
    ///
    /// # Arguments
    ///
    /// * `id` - The [`TimerId`] to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timespan metric.
    pub fn stop_and_accumulate(&self, id: TimerId) {
        match self {
            TimingDistributionMetric::Parent {
                id: _metric_id,
                inner,
            } => {
                #[cfg(feature = "with_gecko")]
                {
                    extern "C" {
                        fn GIFFT_TimingDistributionStopAndAccumulate(metric_id: u32, timer_id: u64);
                    }
                    // SAFETY: using only primitives, no return value.
                    unsafe {
                        GIFFT_TimingDistributionStopAndAccumulate(_metric_id.0, id.id);
                    }
                }
                inner.stop_and_accumulate(id);
            }
            TimingDistributionMetric::Child(c) => {
                #[cfg(feature = "with_gecko")]
                {
                    extern "C" {
                        fn GIFFT_TimingDistributionStopAndAccumulate(metric_id: u32, timer_id: u64);
                    }
                    // SAFETY: using only primitives, no return value.
                    unsafe {
                        GIFFT_TimingDistributionStopAndAccumulate(c.metric_id.0, id.id);
                    }
                }
                let mut map = c
                    .instants
                    .write()
                    .expect("Write lock must've been poisoned.");
                if let Some(start) = map.remove(&id.id) {
                    let now = Instant::now();
                    let sample = now
                        .checked_duration_since(start)
                        .map(|s| s.as_nanos().try_into());
                    let sample = match sample {
                        Some(Ok(sample)) => sample,
                        Some(Err(_)) => {
                            log::warn!("Elapsed time larger than fits into 64-bytes. Saturating at u64::MAX.");
                            u64::MAX
                        }
                        None => {
                            log::warn!("Time went backwards. Not recording.");
                            // TODO: report an error (timer id for stop was started, but time went backwards).
                            return;
                        }
                    };
                    with_ipc_payload(move |payload| {
                        if let Some(v) = payload.timing_samples.get_mut(&c.metric_id) {
                            v.push(sample);
                        } else {
                            payload.timing_samples.insert(c.metric_id, vec![sample]);
                        }
                    });
                } else {
                    // TODO: report an error (timer id for stop wasn't started).
                }
            }
        }
    }

    /// Aborts a previous [`start`](TimingDistribution::start) call. No
    /// error is recorded if no [`start`](TimingDistribution::start) was
    /// called.
    ///
    /// # Arguments
    ///
    /// * `id` - The [`TimerId`] to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timing distribution metric.
    pub fn cancel(&self, id: TimerId) {
        match self {
            TimingDistributionMetric::Parent {
                id: _metric_id,
                inner,
            } => {
                #[cfg(feature = "with_gecko")]
                {
                    extern "C" {
                        fn GIFFT_TimingDistributionCancel(metric_id: u32, timer_id: u64);
                    }
                    // SAFETY: using only primitives, no return value.
                    unsafe {
                        GIFFT_TimingDistributionCancel(_metric_id.0, id.id);
                    }
                }
                inner.cancel(id);
            }
            TimingDistributionMetric::Child(c) => {
                let mut map = c
                    .instants
                    .write()
                    .expect("Write lock must've been poisoned.");
                if map.remove(&id.id).is_none() {
                    // TODO: report an error (cancelled a non-started id).
                }
                #[cfg(feature = "with_gecko")]
                {
                    extern "C" {
                        fn GIFFT_TimingDistributionCancel(metric_id: u32, timer_id: u64);
                    }
                    // SAFETY: using only primitives, no return value.
                    unsafe {
                        GIFFT_TimingDistributionCancel(c.metric_id.0, id.id);
                    }
                }
            }
        }
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value of the metric.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            TimingDistributionMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            TimingDistributionMetric::Child(c) => {
                panic!("Cannot get test value for {:?} in non-parent process!", c)
            }
        }
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors recorded.
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        match self {
            TimingDistributionMetric::Parent { inner, .. } => {
                inner.test_get_num_recorded_errors(error)
            }
            TimingDistributionMetric::Child(c) => panic!(
                "Cannot get number of recorded errors for {:?} in non-parent process!",
                c
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_timing_distribution() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_timing_dist;

        let id = metric.start();
        // Stopping right away might not give us data, if the underlying clock source is not precise
        // enough.
        // So let's cancel and make sure nothing blows up.
        metric.cancel(id);

        // We can't inspect the values yet.
        assert!(metric.test_get_value("store1").is_none());
    }

    #[test]
    fn timing_distribution_child() {
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_timing_dist;
        let id = parent_metric.start();
        std::thread::sleep(std::time::Duration::from_millis(10));
        parent_metric.stop_and_accumulate(id);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            let id = child_metric.start();
            let id2 = child_metric.start();
            assert_ne!(id, id2);
            std::thread::sleep(std::time::Duration::from_millis(10));
            child_metric.stop_and_accumulate(id);

            child_metric.cancel(id2);
        }

        let buf = ipc::take_buf().unwrap();
        assert!(buf.len() > 0);
        assert!(ipc::replay_from_buf(&buf).is_ok());

        let data = parent_metric
            .test_get_value("store1")
            .expect("should have some data");

        // No guarantees from timers means no guarantees on buckets.
        // But we can guarantee it's only two samples.
        assert_eq!(
            2,
            data.values.values().fold(0, |acc, count| acc + count),
            "record 2 values, one parent, one child measurement"
        );
        assert!(0 < data.sum, "record some time");
    }
}
