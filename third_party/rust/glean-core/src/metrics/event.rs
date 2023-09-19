// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;

use crate::common_metric_data::CommonMetricDataInternal;
use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::event_database::RecordedEvent;
use crate::metrics::MetricType;
use crate::util::truncate_string_at_boundary_with_error;
use crate::CommonMetricData;
use crate::Glean;

use chrono::Utc;

const MAX_LENGTH_EXTRA_KEY_VALUE: usize = 500;

/// An event metric.
///
/// Events allow recording of e.g. individual occurences of user actions, say
/// every time a view was open and from where. Each time you record an event, it
/// records a timestamp, the event's name and a set of custom values.
#[derive(Clone, Debug)]
pub struct EventMetric {
    meta: CommonMetricDataInternal,
    allowed_extra_keys: Vec<String>,
}

impl MetricType for EventMetric {
    fn meta(&self) -> &CommonMetricDataInternal {
        &self.meta
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
            meta: meta.into(),
            allowed_extra_keys,
        }
    }

    /// Records an event.
    ///
    /// # Arguments
    ///
    /// * `extra` - A [`HashMap`] of `(key, value)` pairs.
    ///             Keys must be one of the allowed extra keys.
    ///             If any key is not allowed, an error is reported and no event is recorded.
    pub fn record(&self, extra: HashMap<String, String>) {
        let timestamp = crate::get_timestamp_ms();
        self.record_with_time(timestamp, extra);
    }

    /// Record a new event with a provided timestamp.
    ///
    /// It's the caller's responsibility to ensure the timestamp comes from the same clock source.
    ///
    /// # Arguments
    ///
    /// * `timestamp` - The event timestamp, in milliseconds.
    /// * `extra` - A [`HashMap`] of `(key, value)` pairs.
    ///             Keys must be one of the allowed extra keys.
    ///             If any key is not allowed, an error is reported and no event is recorded.
    pub fn record_with_time(&self, timestamp: u64, extra: HashMap<String, String>) {
        let metric = self.clone();

        // Precise timestamp based on wallclock. Will be used if `enable_event_timestamps` is true.
        let now = Utc::now();
        let precise_timestamp = now.timestamp_millis() as u64;

        crate::launch_with_glean(move |glean| {
            let sent = metric.record_sync(glean, timestamp, extra, precise_timestamp);
            if sent {
                let state = crate::global_state().lock().unwrap();
                if let Err(e) = state.callbacks.trigger_upload() {
                    log::error!("Triggering upload failed. Error: {}", e);
                }
            }
        });
    }

    /// Validate that extras are empty or all extra keys are allowed.
    ///
    /// If at least one key is not allowed, record an error and fail.
    fn validate_extra(
        &self,
        glean: &Glean,
        extra: HashMap<String, String>,
    ) -> Result<Option<HashMap<String, String>>, ()> {
        if extra.is_empty() {
            return Ok(None);
        }

        let mut extra_strings = HashMap::new();
        for (k, v) in extra.into_iter() {
            if !self.allowed_extra_keys.contains(&k) {
                let msg = format!("Invalid key index {}", k);
                record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
                return Err(());
            }

            let value = truncate_string_at_boundary_with_error(
                glean,
                &self.meta,
                v,
                MAX_LENGTH_EXTRA_KEY_VALUE,
            );
            extra_strings.insert(k, value);
        }

        Ok(Some(extra_strings))
    }

    /// Records an event.
    ///
    /// ## Returns
    ///
    /// `true` if a ping was submitted and should be uploaded.
    /// `false` otherwise.
    #[doc(hidden)]
    pub fn record_sync(
        &self,
        glean: &Glean,
        timestamp: u64,
        extra: HashMap<String, String>,
        precise_timestamp: u64,
    ) -> bool {
        if !self.should_record(glean) {
            return false;
        }

        let mut extra_strings = match self.validate_extra(glean, extra) {
            Ok(extra) => extra,
            Err(()) => return false,
        };

        if glean.with_timestamps() {
            if extra_strings.is_none() {
                extra_strings.replace(Default::default());
            }
            let map = extra_strings.get_or_insert(Default::default());
            map.insert("glean_timestamp".to_string(), precise_timestamp.to_string());
        }

        glean
            .event_storage()
            .record(glean, &self.meta, timestamp, extra_strings)
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Get the vector of currently stored events for this event metric.
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<Vec<RecordedEvent>> {
        let queried_ping_name = ping_name
            .into()
            .unwrap_or_else(|| &self.meta().inner.send_in_pings[0]);

        glean
            .event_storage()
            .test_get_value(&self.meta, queried_ping_name)
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Get the vector of currently stored events for this event metric.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<Vec<RecordedEvent>> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| self.get_value(glean, ping_name.as_deref()))
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. inner to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        crate::block_on_dispatcher();

        crate::core::with_glean(|glean| {
            test_get_num_recorded_errors(glean, self.meta(), error).unwrap_or(0)
        })
    }
}
