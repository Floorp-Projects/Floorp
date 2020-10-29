// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use uuid::Uuid;

use super::CommonMetricData;

use crate::{
    dispatcher,
    ipc::{need_ipc, MetricId},
};

/// A UUID metric.
///
/// Stores UUID values.
#[derive(Debug)]
pub enum UuidMetric {
    Parent(Arc<UuidMetricImpl>),
    Child(UuidMetricIpc),
}

#[derive(Clone, Debug)]
pub struct UuidMetricImpl(pub(crate) glean_core::metrics::UuidMetric);
#[derive(Debug)]
pub struct UuidMetricIpc(MetricId);

impl UuidMetric {
    /// Create a new UUID metric.
    pub fn new(meta: CommonMetricData) -> Self {
        if need_ipc() {
            UuidMetric::Child(UuidMetricIpc(MetricId::new(meta)))
        } else {
            UuidMetric::Parent(Arc::new(UuidMetricImpl::new(meta)))
        }
    }

    /// Set to the specified value.
    ///
    /// ## Arguments
    ///
    /// * `value` - The UUID to set the metric to.
    pub fn set(&self, value: Uuid) {
        match self {
            UuidMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.set(value));
            }
            UuidMetric::Child(_c) => {
                log::error!(
                    "Unable to set UUID metric {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        };
    }

    /// Generate a new random UUID and set the metric to it.
    pub fn generate_and_set(&self) {
        match self {
            UuidMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.generate_and_set());
            }
            UuidMetric::Child(_c) => {
                log::error!(
                    "Unable to set UUID metric {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        };
    }

    /// **Test-only API.**
    ///
    /// Get the stored UUID value.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<Uuid> {
        match self {
            UuidMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name).map(|uuid| {
                    Uuid::parse_str(&uuid).expect("can't parse internally created UUID value")
                })
            }
            UuidMetric::Child(_c) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl UuidMetricImpl {
    fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::UuidMetric::new(meta))
    }

    fn set(&self, value: Uuid) {
        crate::with_glean(move |glean| self.0.set(glean, value));
    }

    fn generate_and_set(&self) {
        crate::with_glean(move |glean| self.0.generate_and_set(glean));
    }

    fn test_get_value(&self, storage_name: &str) -> Option<String> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
    }
}
