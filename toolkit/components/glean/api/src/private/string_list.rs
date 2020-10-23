// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use super::CommonMetricData;

use crate::dispatcher;
use crate::ipc::{need_ipc, with_ipc_payload, MetricId};

/// A string list metric.
///
/// This allows appending a string value with arbitrary content to a list.
#[derive(Debug)]
pub enum StringListMetric {
    Parent(Arc<StringListMetricImpl>),
    Child(StringListMetricIpc),
}
#[derive(Clone, Debug)]
pub struct StringListMetricImpl(glean_core::metrics::StringListMetric);
#[derive(Debug)]
pub struct StringListMetricIpc(MetricId);

impl StringListMetric {
    /// Create a new string list metric.
    pub fn new(meta: CommonMetricData) -> Self {
        if need_ipc() {
            StringListMetric::Child(StringListMetricIpc(MetricId::new(meta)))
        } else {
            StringListMetric::Parent(Arc::new(StringListMetricImpl::new(meta)))
        }
    }

    /// Add a new string to the list.
    ///
    /// ## Arguments
    ///
    /// * `value` - The string to add.
    ///
    /// ## Notes
    ///
    /// Truncates the value if it is longer than `MAX_STRING_LENGTH` bytes and logs an error.
    /// See [String list metric limits](https://mozilla.github.io/glean/book/user/metrics/string_list.html#limits).
    pub fn add<S: Into<String>>(&self, value: S) {
        match self {
            StringListMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                let value = value.into();
                dispatcher::launch(move || metric.add(value));
            }
            StringListMetric::Child(c) => {
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.string_lists.get_mut(&c.0) {
                        v.push(value.into());
                    } else {
                        let mut v = vec![];
                        v.push(value.into());
                        payload.string_lists.insert(c.0.clone(), v);
                    }
                });
            }
        }
    }

    /// Set to a specific list of strings.
    ///
    /// ## Arguments
    ///
    /// * `value` - The list of string to set the metric to.
    ///
    /// ## Notes
    ///
    /// If passed an empty list, records an error and returns.
    /// Truncates the list if it is longer than `MAX_LIST_LENGTH` and logs an error.
    /// Truncates any value in the list if it is longer than `MAX_STRING_LENGTH` and logs an error.
    pub fn set(&self, value: Vec<String>) {
        match self {
            StringListMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.set(value));
            }
            StringListMetric::Child(_c) => {
                log::error!(
                    "Unable to set string list metric {:?} in non-main process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored values.
    /// This doesn't clear the stored value.
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<Vec<String>> {
        match self {
            StringListMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name)
            }
            StringListMetric::Child(_c) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl StringListMetricImpl {
    pub fn new(meta: CommonMetricData) -> Self {
        Self(glean_core::metrics::StringListMetric::new(meta))
    }

    pub fn add<S: Into<String>>(&self, value: S) {
        crate::with_glean(move |glean| self.0.add(glean, value))
    }

    pub fn set(&self, value: Vec<String>) {
        crate::with_glean(move |glean| self.0.set(glean, value))
    }

    pub fn test_get_value(&self, storage_name: &str) -> Option<Vec<String>> {
        crate::with_glean(move |glean| self.0.test_get_value(glean, storage_name))
    }
}
