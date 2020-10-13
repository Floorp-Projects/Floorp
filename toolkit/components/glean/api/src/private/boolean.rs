// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use super::CommonMetricData;

use crate::dispatcher;
use crate::ipc::need_ipc;

/// A boolean metric.
///
/// Records a simple true or false value.
#[derive(Debug)]
pub enum BooleanMetric {
    Parent(Arc<BooleanMetricImpl>),
    Child(BooleanMetricIpc),
}
#[derive(Clone, Debug)]
pub struct BooleanMetricImpl(pub(crate) glean_core::metrics::BooleanMetric);
#[derive(Debug)]
pub struct BooleanMetricIpc();

impl BooleanMetric {
    /// Create a new boolean metric.
    pub fn new(meta: CommonMetricData) -> Self {
        if need_ipc() {
            BooleanMetric::Child(BooleanMetricIpc {})
        } else {
            BooleanMetric::Parent(Arc::new(BooleanMetricImpl::new(meta)))
        }
    }

    /// Set to the specified boolean value.
    ///
    /// ## Arguments
    ///
    /// * `value` - the value to set.
    pub fn set(&self, value: bool) {
        match self {
            BooleanMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.set(value));
            }
            BooleanMetric::Child(_) => {
                log::error!(
                    "Unable to set boolean metric {:?} in non-parent process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value as a boolean.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<bool> {
        match self {
            BooleanMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name)
            }
            BooleanMetric::Child(_) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl BooleanMetricImpl {
    pub fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::BooleanMetric::new(meta))
    }

    pub fn set(&self, value: bool) {
        crate::with_glean(move |glean| self.0.set(glean, value))
    }

    pub fn test_get_value(&self, storage_name: &str) -> Option<bool> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
    }
}
