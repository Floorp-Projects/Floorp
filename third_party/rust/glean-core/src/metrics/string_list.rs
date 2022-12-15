// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::util::truncate_string_at_boundary_with_error;
use crate::CommonMetricData;
use crate::Glean;

// Maximum length of any list
const MAX_LIST_LENGTH: usize = 20;
// Maximum length of any string in the list
const MAX_STRING_LENGTH: usize = 50;

/// A string list metric.
///
/// This allows appending a string value with arbitrary content to a list.
#[derive(Clone, Debug)]
pub struct StringListMetric {
    meta: Arc<CommonMetricData>,
}

impl MetricType for StringListMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl StringListMetric {
    /// Creates a new string list metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self {
            meta: Arc::new(meta),
        }
    }

    /// Adds a new string to the list.
    ///
    /// # Arguments
    ///
    /// * `value` - The string to add.
    ///
    /// ## Notes
    ///
    /// Truncates the value if it is longer than `MAX_STRING_LENGTH` bytes and logs an error.
    pub fn add(&self, value: String) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.add_sync(glean, value))
    }

    /// Adds a new string to the list synchronously
    #[doc(hidden)]
    pub fn add_sync<S: Into<String>>(&self, glean: &Glean, value: S) {
        if !self.should_record(glean) {
            return;
        }

        let value =
            truncate_string_at_boundary_with_error(glean, &self.meta, value, MAX_STRING_LENGTH);
        let mut error = None;
        glean
            .storage()
            .record_with(glean, &self.meta, |old_value| match old_value {
                Some(Metric::StringList(mut old_value)) => {
                    if old_value.len() == MAX_LIST_LENGTH {
                        let msg = format!(
                            "String list length of {} exceeds maximum of {}",
                            old_value.len() + 1,
                            MAX_LIST_LENGTH
                        );
                        error = Some(msg);
                    } else {
                        old_value.push(value.clone());
                    }
                    Metric::StringList(old_value)
                }
                _ => Metric::StringList(vec![value.clone()]),
            });

        if let Some(msg) = error {
            record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
        }
    }

    /// Sets to a specific list of strings.
    ///
    /// # Arguments
    ///
    /// * `value` - The list of string to set the metric to.
    ///
    /// ## Notes
    ///
    /// If passed an empty list, records an error and returns.
    ///
    /// Truncates the list if it is longer than `MAX_LIST_LENGTH` and logs an error.
    ///
    /// Truncates any value in the list if it is longer than `MAX_STRING_LENGTH` and logs an error.
    pub fn set(&self, values: Vec<String>) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_sync(glean, values))
    }

    /// Sets to a specific list of strings synchronously.
    #[doc(hidden)]
    pub fn set_sync(&self, glean: &Glean, value: Vec<String>) {
        if !self.should_record(glean) {
            return;
        }

        let value = if value.len() > MAX_LIST_LENGTH {
            let msg = format!(
                "StringList length {} exceeds maximum of {}",
                value.len(),
                MAX_LIST_LENGTH
            );
            record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
            value[0..MAX_LIST_LENGTH].to_vec()
        } else {
            value
        };

        let value = value
            .into_iter()
            .map(|elem| {
                truncate_string_at_boundary_with_error(glean, &self.meta, elem, MAX_STRING_LENGTH)
            })
            .collect();

        let value = Metric::StringList(value);
        glean.storage().record(glean, &self.meta, &value);
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently-stored values.
    ///
    /// This doesn't clear the stored value.
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<Vec<String>> {
        let queried_ping_name = ping_name
            .into()
            .unwrap_or_else(|| &self.meta().send_in_pings[0]);

        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::StringList(values)) => Some(values),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently-stored values.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<Vec<String>> {
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
    ///   metric for. Defaults to the first value in `send_in_pings`.
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
