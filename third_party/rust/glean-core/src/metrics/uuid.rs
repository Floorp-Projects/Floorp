// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use uuid::Uuid;

use crate::error_recording::{record_error, ErrorType};
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
    meta: CommonMetricData,
}

impl MetricType for UuidMetric {
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
impl UuidMetric {
    /// Creates a new UUID metric
    pub fn new(meta: CommonMetricData) -> Self {
        Self { meta }
    }

    /// Sets to the specified value.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `value` - The [`Uuid`] to set the metric to.
    pub fn set(&self, glean: &Glean, value: Uuid) {
        if !self.should_record(glean) {
            return;
        }

        let s = value.to_string();
        let value = Metric::Uuid(s);
        glean.storage().record(glean, &self.meta, &value)
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
    pub fn set_from_str(&self, glean: &Glean, value: &str) {
        if !self.should_record(glean) {
            return;
        }

        if let Ok(uuid) = uuid::Uuid::parse_str(value) {
            self.set(glean, uuid);
        } else {
            let msg = format!("Unexpected UUID value '{}'", value);
            record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
        }
    }

    /// Generates a new random [`Uuid`'] and sets the metric to it.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    pub fn generate_and_set(&self, storage: &Glean) -> Uuid {
        let uuid = Uuid::new_v4();
        self.set(storage, uuid);
        uuid
    }

    /// Gets the stored Uuid value.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `storage_name` - the storage name to look into.
    ///
    /// # Returns
    ///
    /// The stored value or `None` if nothing stored.
    pub(crate) fn get_value(&self, glean: &Glean, storage_name: &str) -> Option<Uuid> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta().identifier(glean),
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
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<Uuid> {
        self.get_value(glean, storage_name)
    }
}
