// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use super::{CommonMetricData, MetricId};

use crate::ipc::need_ipc;

use glean::traits::ObjectSerialize;

/// An object metric.
pub enum ObjectMetric<K> {
    Parent {
        id: MetricId,
        inner: glean::private::ObjectMetric<K>,
    },
    Child,
}

impl<K: ObjectSerialize> ObjectMetric<K> {
    /// Create a new object metric.
    pub fn new(id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            ObjectMetric::Child
        } else {
            let inner = glean::private::ObjectMetric::new(meta);
            ObjectMetric::Parent { id, inner }
        }
    }

    pub fn set(&self, value: K) {
        match self {
            ObjectMetric::Parent { inner, .. } => {
                inner.set(value);
            }
            ObjectMetric::Child => {
                log::error!("Unable to set object metric in non-main process. This operation will be ignored.");
                // TODO: Record an error.
            }
        };
    }

    pub fn set_string(&self, value: String) {
        match self {
            ObjectMetric::Parent { inner, .. } => {
                inner.set_string(value);
            }
            ObjectMetric::Child => {
                log::error!("Unable to set object metric in non-main process. This operation will be ignored.");
                // TODO: Record an error.
            }
        };
    }

    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<serde_json::Value> {
        match self {
            ObjectMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            ObjectMetric::Child => {
                panic!("Cannot get test value for object metric in non-parent process!",)
            }
        }
    }

    pub fn test_get_value_as_str<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<String> {
        self.test_get_value(ping_name)
            .map(|val| serde_json::to_string(&val).unwrap())
    }

    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            ObjectMetric::Parent { inner, .. } => inner.test_get_num_recorded_errors(error),
            ObjectMetric::Child => {
                panic!("Cannot get the number of recorded errors in non-parent process!")
            }
        }
    }
}
