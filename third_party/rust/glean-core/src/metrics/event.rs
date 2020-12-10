// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;

use serde_json::{json, Value as JsonValue};

use crate::error_recording::{record_error, ErrorType};
use crate::event_database::RecordedEvent;
use crate::metrics::MetricType;
use crate::util::truncate_string_at_boundary_with_error;
use crate::CommonMetricData;
use crate::Glean;

const MAX_LENGTH_EXTRA_KEY_VALUE: usize = 100;

/// An event metric.
///
/// Events allow recording of e.g. individual occurences of user actions, say
/// every time a view was open and from where. Each time you record an event, it
/// records a timestamp, the event's name and a set of custom values.
#[derive(Clone, Debug)]
pub struct EventMetric {
    meta: CommonMetricData,
    allowed_extra_keys: Vec<String>,
}

impl MetricType for EventMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn meta_mut(&mut self) -> &mut CommonMetricData {
        &mut self.meta
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl EventMetric {
    /// Creates a new event metric.
    pub fn new(meta: CommonMetricData, allowed_extra_keys: Vec<String>) -> Self {
        Self {
            meta,
            allowed_extra_keys,
        }
    }

    /// Records an event.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `timestamp` - A monotonically increasing timestamp, in milliseconds.
    ///   This must be provided since the actual recording of the event may
    ///   happen some time later than the moment the event occurred.
    /// * `extra` - A [`HashMap`] of (key, value) pairs. The key is an index into
    ///   the metric's `allowed_extra_keys` vector where the key's string is
    ///   looked up. If any key index is out of range, an error is reported and
    ///   no event is recorded.
    pub fn record<M: Into<Option<HashMap<i32, String>>>>(
        &self,
        glean: &Glean,
        timestamp: u64,
        extra: M,
    ) {
        if !self.should_record(glean) {
            return;
        }

        let extra = extra.into();
        let extra_strings: Option<HashMap<String, String>> = if let Some(extra) = extra {
            if extra.is_empty() {
                None
            } else {
                let mut extra_strings = HashMap::new();
                for (k, v) in extra.into_iter() {
                    match self.allowed_extra_keys.get(k as usize) {
                        Some(k) => extra_strings.insert(
                            k.to_string(),
                            truncate_string_at_boundary_with_error(
                                glean,
                                &self.meta,
                                v,
                                MAX_LENGTH_EXTRA_KEY_VALUE,
                            ),
                        ),
                        None => {
                            let msg = format!("Invalid key index {}", k);
                            record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
                            return;
                        }
                    };
                }
                Some(extra_strings)
            }
        } else {
            None
        };

        glean
            .event_storage()
            .record(glean, &self.meta, timestamp, extra_strings);
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Tests whether there are currently stored events for this event metric.
    ///
    /// This doesn't clear the stored value.
    pub fn test_has_value(&self, glean: &Glean, store_name: &str) -> bool {
        glean.event_storage().test_has_value(&self.meta, store_name)
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Get the vector of currently stored events for this event metric.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, store_name: &str) -> Option<Vec<RecordedEvent>> {
        glean.event_storage().test_get_value(&self.meta, store_name)
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored events for this event metric as a JSON-encoded string.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value_as_json_string(&self, glean: &Glean, store_name: &str) -> String {
        match self.test_get_value(glean, store_name) {
            Some(value) => json!(value),
            None => json!(JsonValue::Null),
        }
        .to_string()
    }
}
