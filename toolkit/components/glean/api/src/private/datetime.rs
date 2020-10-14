// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use super::CommonMetricData;

use super::TimeUnit;
use crate::dispatcher;
use crate::ipc::need_ipc;
use chrono::{DateTime, FixedOffset};

/// A datetime metric of a certain resolution.
///
/// Datetimes are used to make record when something happened according to the
/// client's clock.
#[derive(Debug)]
pub enum DatetimeMetric {
    Parent(Arc<DatetimeMetricImpl>),
    Child(DatetimeMetricIpc),
}
#[derive(Debug)]
pub struct DatetimeMetricImpl(glean_core::metrics::DatetimeMetric);
#[derive(Debug)]
pub struct DatetimeMetricIpc();

impl DatetimeMetric {
    /// Create a new datetime metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            DatetimeMetric::Child(DatetimeMetricIpc {})
        } else {
            DatetimeMetric::Parent(Arc::new(DatetimeMetricImpl::new(meta, time_unit)))
        }
    }

    /// Set the datetime to the provided value, or the local now.
    ///
    /// ## Arguments
    ///
    /// - `value` - The date and time and timezone value to set.
    ///             If None we use the current local time.
    pub fn set(&self, value: Option<DateTime<FixedOffset>>) {
        match self {
            DatetimeMetric::Parent(p) => {
                let metric = Arc::clone(&p);
                dispatcher::launch(move || metric.set(value));
            }
            DatetimeMetric::Child(_) => {
                log::error!(
                    "Unable to set datetime metric {:?} in non-parent process. Ignoring.",
                    self
                );
                // TODO: Record an error.
            }
        }
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored value.
    /// This doesn't clear the stored value.
    ///
    /// ## Note
    ///
    /// This currently returns the value as an ISO 8601 time string.
    /// See [bug 1636176](https://bugzilla.mozilla.org/show_bug.cgi?id=1636176).
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
            DatetimeMetric::Parent(p) => {
                dispatcher::block_on_queue();
                p.test_get_value(storage_name)
            }
            DatetimeMetric::Child(_) => panic!(
                "Cannot get test value for {:?} in non-parent process!",
                self
            ),
        }
    }
}

impl DatetimeMetricImpl {
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        Self(glean_core::metrics::DatetimeMetric::new(meta, time_unit))
    }

    pub fn set(&self, value: Option<DateTime<FixedOffset>>) {
        crate::with_glean(move |glean| self.0.set(glean, value))
    }

    pub fn test_get_value(&self, storage_name: &str) -> Option<String> {
        crate::with_glean(move |glean| self.0.test_get_value_as_string(glean, storage_name))
    }
}
