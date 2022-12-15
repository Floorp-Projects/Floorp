// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use uuid::Uuid;

use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// An UUID metric.
///
/// Stores UUID v4 (randomly generated) values.
#[derive(Clone, Debug)]
pub struct UuidMetric {
    meta: Arc<CommonMetricData>,
}

impl MetricType for UuidMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl UuidMetric {
    /// Creates a new UUID metric
    pub fn new(meta: CommonMetricData) -> Self {
        Self {
            meta: Arc::new(meta),
        }
    }

    /// Sets to the specified value.
    ///
    /// # Arguments
    ///
    /// * `value` - The [`Uuid`] to set the metric to.
    pub fn set(&self, value: String) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_sync(glean, &value))
    }

    /// Sets to the specified value synchronously.
    #[doc(hidden)]
    pub fn set_sync<S: Into<String>>(&self, glean: &Glean, value: S) {
        if !self.should_record(glean) {
            return;
        }

        let value = value.into();

        if let Ok(uuid) = uuid::Uuid::parse_str(&value) {
            let value = Metric::Uuid(uuid.to_hyphenated().to_string());
            glean.storage().record(glean, &self.meta, &value)
        } else {
            let msg = format!("Unexpected UUID value '{}'", value);
            record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
        }
    }

    /// Sets to the specified value, from a string.
    ///
    /// This should only be used from FFI. When calling directly from Rust, it
    /// is better to use [`set`](UuidMetric::set).
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `value` - The [`Uuid`] to set the metric to.
    #[doc(hidden)]
    pub fn set_from_uuid_sync(&self, glean: &Glean, value: Uuid) {
        self.set_sync(glean, value.to_string())
    }

    /// Generates a new random [`Uuid`'] and sets the metric to it.
    pub fn generate_and_set(&self) -> String {
        let uuid = Uuid::new_v4();

        let value = uuid.to_string();
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_sync(glean, value));

        uuid.to_string()
    }

    /// Generates a new random [`Uuid`'] and sets the metric to it synchronously.
    #[doc(hidden)]
    pub fn generate_and_set_sync(&self, storage: &Glean) -> Uuid {
        let uuid = Uuid::new_v4();
        self.set_sync(storage, uuid.to_string());
        uuid
    }

    /// Gets the current-stored value as a string, or None if there is no value.
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<Uuid> {
        let queried_ping_name = ping_name
            .into()
            .unwrap_or_else(|| &self.meta().send_in_pings[0]);

        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Uuid(uuid)) => Uuid::parse_str(&uuid).ok(),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as a string.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<String> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| {
            self.get_value(glean, ping_name.as_deref())
                .map(|uuid| uuid.to_string())
        })
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
