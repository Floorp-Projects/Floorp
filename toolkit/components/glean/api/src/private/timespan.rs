// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::{Arc, RwLock};

use super::{CommonMetricData, Instant, TimeUnit};

use crate::dispatcher;
use crate::ipc::{need_ipc, with_ipc_payload, MetricId, TimespanCommand};

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
pub struct TimespanMetricIpc(MetricId);

impl TimespanMetric {
    /// Create a new timespan metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            TimespanMetric::Child(TimespanMetricIpc(MetricId::new(meta)))
        } else {
            TimespanMetric::Parent(Arc::new(TimespanMetricImpl::new(meta, time_unit)))
        }
    }

    pub fn start(&self) {
        let now = Instant::now();

        match self {
            TimespanMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.start(now));
            }
            TimespanMetric::Child(c) => {
                let cmd = TimespanCommand::Start(now);
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.timespans.get_mut(&c.0) {
                        v.push(cmd);
                    } else {
                        let mut v = vec![];
                        v.push(cmd);
                        payload.timespans.insert(c.0.clone(), v);
                    }
                });
            }
        }
    }

    pub fn stop(&self) {
        let now = Instant::now();

        match self {
            TimespanMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.stop(now));
            }
            TimespanMetric::Child(c) => {
                let cmd = TimespanCommand::Stop(now);
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.timespans.get_mut(&c.0) {
                        v.push(cmd);
                    } else {
                        let mut v = vec![];
                        v.push(cmd);
                        payload.timespans.insert(c.0.clone(), v);
                    }
                });
            }
        }
    }

    pub fn cancel(&self) {
        match self {
            TimespanMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.cancel());
            }
            TimespanMetric::Child(c) => {
                let cmd = TimespanCommand::Cancel;
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.timespans.get_mut(&c.0) {
                        v.push(cmd);
                    } else {
                        let mut v = vec![];
                        v.push(cmd);
                        payload.timespans.insert(c.0.clone(), v);
                    }
                });
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
