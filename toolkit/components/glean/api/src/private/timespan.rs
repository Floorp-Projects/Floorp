// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::{Arc, RwLock};

use super::{CommonMetricData, Instant, MetricId, TimeUnit};

use crate::dispatcher;
use crate::ipc::need_ipc;

/// A timespan metric.
///
/// Timespans are used to make a measurement of how much time is spent in a particular task.
#[derive(Debug)]
pub enum TimespanMetric {
    Parent(Arc<TimespanMetricImpl>),
    Child(TimespanMetricIpc),
}

#[derive(Debug)]
pub struct TimespanMetricImpl {
    inner: RwLock<glean_core::metrics::TimespanMetric>,
}
#[derive(Debug)]
pub struct TimespanMetricIpc;

impl TimespanMetric {
    /// Create a new timespan metric.
    pub fn new(_id: MetricId, meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            TimespanMetric::Child(TimespanMetricIpc)
        } else {
            TimespanMetric::Parent(Arc::new(TimespanMetricImpl::new(meta, time_unit)))
        }
    }

    pub fn start(&self) {
        match self {
            TimespanMetric::Parent(p) => {
                let now = Instant::now();
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.start(now));
            }
            TimespanMetric::Child(_) => {
                log::error!(
                    "Unable to start timespan metric {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        }
    }

    pub fn stop(&self) {
        match self {
            TimespanMetric::Parent(p) => {
                let now = Instant::now();
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.stop(now));
            }
            TimespanMetric::Child(_) => {
                log::error!(
                    "Unable to stop timespan metric {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        }
    }

    pub fn cancel(&self) {
        match self {
            TimespanMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.cancel());
            }
            TimespanMetric::Child(_) => {
                log::error!(
                    "Unable to cancel timespan metric {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as an integer.
    /// This doesn't clear the stored value.
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<u64> {
        match self {
            TimespanMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name)
            }
            TimespanMetric::Child(_c) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl TimespanMetricImpl {
    fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        let inner = RwLock::new(glean_core::metrics::TimespanMetric::new(meta, time_unit));
        Self { inner }
    }

    fn start(&self, now: Instant) {
        crate::with_glean(move |glean| {
            let mut inner = self
                .inner
                .write()
                .expect("lock of wrapped metric was poisoned");
            inner.set_start(glean, now.as_nanos())
        })
    }

    fn stop(&self, now: Instant) {
        crate::with_glean(move |glean| {
            let mut inner = self
                .inner
                .write()
                .expect("lock of wrapped metric was poisoned");
            inner.set_stop(glean, now.as_nanos())
        })
    }

    fn cancel(&self) {
        let mut inner = self
            .inner
            .write()
            .expect("lock of wrapped metric was poisoned");
        inner.cancel()
    }

    fn test_get_value(&self, storage_name: &str) -> Option<u64> {
        crate::with_glean(move |glean| {
            let inner = self
                .inner
                .read()
                .expect("lock of wrapped metric was poisoned");
            inner.test_get_value(glean, storage_name)
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_timespan() {
        let _lock = lock_test();

        let metric = TimespanMetric::new(
            0.into(),
            CommonMetricData {
                name: "timespan_metric".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["store1".into()],
                disabled: false,
                ..Default::default()
            },
            TimeUnit::Nanosecond,
        );

        metric.start();
        // Stopping right away might not give us data, if the underlying clock source is not precise
        // enough.
        // So let's cancel and make sure nothing blows up.
        metric.cancel();

        assert_eq!(None, metric.test_get_value("store1"));
    }

    #[test]
    fn timespan_ipc() {
        let _lock = lock_test();
        let _raii = ipc::test_set_need_ipc(true);

        let child_metric = &metrics::test_only::can_we_time_it;

        // Instrumentation calls do not panic.
        child_metric.start();
        // Stopping right away might not give us data,
        // if the underlying clock source is not precise enough.
        // So let's cancel and make sure nothing blows up.
        child_metric.cancel();

        // (They also shouldn't do anything,
        // but that's not something we can inspect in this test)

        // Need to catch the panic so that our RAIIs drop nicely.
        let result = std::panic::catch_unwind(move || {
            child_metric.test_get_value("store1");
        });
        assert!(result.is_err());
    }
}
