// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use serde::{Deserialize, Serialize};
use serde_json::{json, Map as JsonMap, Value as JsonValue};
use std::collections::HashMap;

use crate::error_recording::{record_error, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::util::{truncate_string_at_boundary, truncate_string_at_boundary_with_error};
use crate::CommonMetricData;
use crate::Glean;
use crate::Lifetime;
use crate::INTERNAL_STORAGE;

/// The maximum length of the experiment id, the branch id, and the keys of the
/// `extra` map. Identifiers longer than this number of characters are truncated.
const MAX_EXPERIMENTS_IDS_LEN: usize = 100;
/// The maximum length of the experiment `extra` values.  Values longer than this
/// limit will be truncated.
const MAX_EXPERIMENT_VALUE_LEN: usize = MAX_EXPERIMENTS_IDS_LEN;
/// The maximum number of extras allowed in the `extra` hash map.  Any items added
/// beyond this limit will be dropped. Note that truncation of a hash map is
/// nondeterministic in which items are truncated.
const MAX_EXPERIMENTS_EXTRAS_SIZE: usize = 20;

/// The data for a single experiment.
#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
pub struct RecordedExperimentData {
    pub branch: String,
    pub extra: Option<HashMap<String, String>>,
}

impl RecordedExperimentData {
    /// Gets the recorded experiment data as a JSON value.
    ///
    /// For JSON, we don't want to include `{"extra": null}` -- we just want to skip
    /// `extra` entirely. Unfortunately, we can't use a serde field annotation for this,
    /// since that would break bincode serialization, which doesn't support skipping
    /// fields. Therefore, we use a custom serialization function just for JSON here.
    pub fn as_json(&self) -> JsonValue {
        let mut value = JsonMap::new();
        value.insert("branch".to_string(), json!(self.branch));
        if self.extra.is_some() {
            value.insert("extra".to_string(), json!(self.extra));
        }
        JsonValue::Object(value)
    }
}

/// An experiment metric.
///
/// Used to store active experiments.
/// This is used through the `set_experiment_active`/`set_experiment_inactive` Glean SDK API.
#[derive(Clone, Debug)]
pub struct ExperimentMetric {
    meta: CommonMetricData,
}

impl MetricType for ExperimentMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn meta_mut(&mut self) -> &mut CommonMetricData {
        &mut self.meta
    }
}

impl ExperimentMetric {
    /// Creates a new experiment metric.
    ///
    /// # Arguments
    ///
    /// * `id` - the id of the experiment. Please note that this will be
    ///          truncated to `MAX_EXPERIMENTS_IDS_LEN`, if needed.
    pub fn new(glean: &Glean, id: String) -> Self {
        let mut error = None;

        // Make sure that experiment id is within the expected limit.
        let truncated_id = if id.len() > MAX_EXPERIMENTS_IDS_LEN {
            let msg = format!(
                "Value length {} for experiment id exceeds maximum of {}",
                id.len(),
                MAX_EXPERIMENTS_IDS_LEN
            );
            error = Some(msg);
            truncate_string_at_boundary(id, MAX_EXPERIMENTS_IDS_LEN)
        } else {
            id
        };

        let new_experiment = Self {
            meta: CommonMetricData {
                name: format!("{}#experiment", truncated_id),
                // We don't need a category, the name is already unique
                category: "".into(),
                send_in_pings: vec![INTERNAL_STORAGE.into()],
                lifetime: Lifetime::Application,
                ..Default::default()
            },
        };

        // Check for a truncation error to record
        if let Some(msg) = error {
            record_error(
                glean,
                &new_experiment.meta,
                ErrorType::InvalidValue,
                msg,
                None,
            );
        }

        new_experiment
    }

    /// Records an experiment as active.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `branch` -  the active branch of the experiment. Please note that this will be
    ///               truncated to `MAX_EXPERIMENTS_IDS_LEN`, if needed.
    /// * `extra` - an optional, user defined String to String map used to provide richer
    ///             experiment context if needed.
    pub fn set_active(
        &self,
        glean: &Glean,
        branch: String,
        extra: Option<HashMap<String, String>>,
    ) {
        if !self.should_record(glean) {
            return;
        }

        // Make sure that branch id is within the expected limit.
        let truncated_branch = if branch.len() > MAX_EXPERIMENTS_IDS_LEN {
            truncate_string_at_boundary_with_error(
                glean,
                &self.meta,
                branch,
                MAX_EXPERIMENTS_IDS_LEN,
            )
        } else {
            branch
        };

        // Apply limits to extras
        let truncated_extras = extra.map(|extra| {
            if extra.len() > MAX_EXPERIMENTS_EXTRAS_SIZE {
                let msg = format!(
                    "Extra hash map length {} exceeds maximum of {}",
                    extra.len(),
                    MAX_EXPERIMENTS_EXTRAS_SIZE
                );
                record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
            }

            let mut temp_map = HashMap::new();
            for (key, value) in extra.into_iter().take(MAX_EXPERIMENTS_EXTRAS_SIZE) {
                let truncated_key = if key.len() > MAX_EXPERIMENTS_IDS_LEN {
                    truncate_string_at_boundary_with_error(
                        glean,
                        &self.meta,
                        key,
                        MAX_EXPERIMENTS_IDS_LEN,
                    )
                } else {
                    key
                };
                let truncated_value = if value.len() > MAX_EXPERIMENT_VALUE_LEN {
                    truncate_string_at_boundary_with_error(
                        glean,
                        &self.meta,
                        value,
                        MAX_EXPERIMENT_VALUE_LEN,
                    )
                } else {
                    value
                };

                temp_map.insert(truncated_key, truncated_value);
            }
            temp_map
        });

        let value = Metric::Experiment(RecordedExperimentData {
            branch: truncated_branch,
            extra: truncated_extras,
        });
        glean.storage().record(glean, &self.meta, &value)
    }

    /// Records an experiment as inactive.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance this metric belongs to.
    pub fn set_inactive(&self, glean: &Glean) {
        if !self.should_record(glean) {
            return;
        }

        if let Err(e) = glean.storage().remove_single_metric(
            Lifetime::Application,
            INTERNAL_STORAGE,
            &self.meta.name,
        ) {
            log::error!("Failed to set experiment as inactive: {:?}", e);
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored experiment data as a JSON representation of
    /// the RecordedExperimentData.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value_as_json_string(&self, glean: &Glean) -> Option<String> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            INTERNAL_STORAGE,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Experiment(e)) => Some(json!(e).to_string()),
            _ => None,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn stable_serialization() {
        let experiment_empty = RecordedExperimentData {
            branch: "branch".into(),
            extra: None,
        };

        let mut data = HashMap::new();
        data.insert("a key".to_string(), "a value".to_string());
        let experiment_data = RecordedExperimentData {
            branch: "branch".into(),
            extra: Some(data),
        };

        let experiment_empty_bin = bincode::serialize(&experiment_empty).unwrap();
        let experiment_data_bin = bincode::serialize(&experiment_data).unwrap();

        assert_eq!(
            experiment_empty,
            bincode::deserialize(&experiment_empty_bin).unwrap()
        );
        assert_eq!(
            experiment_data,
            bincode::deserialize(&experiment_data_bin).unwrap()
        );
    }

    #[test]
    #[rustfmt::skip] // Let's not add newlines unnecessary
    fn deserialize_old_encoding() {
        // generated by `bincode::serialize` as of Glean commit ac27fceb7c0d5a7288d7d569e8c5c5399a53afb2
        // empty was generated from: `RecordedExperimentData { branch: "branch".into(), extra: None, }`
        let empty_bin = vec![6, 0, 0, 0, 0, 0, 0, 0, 98, 114, 97, 110, 99, 104];
        // data was generated from: RecordedExperimentData { branch: "branch".into(), extra: Some({"a key": "a value"}), };
        let data_bin  = vec![6, 0, 0, 0, 0, 0, 0, 0, 98, 114, 97, 110, 99, 104,
                             1, 1, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
                             97, 32, 107, 101, 121, 7, 0, 0, 0, 0, 0, 0, 0, 97,
                             32, 118, 97, 108, 117, 101];


        let mut data = HashMap::new();
        data.insert("a key".to_string(), "a value".to_string());
        let experiment_data = RecordedExperimentData { branch: "branch".into(), extra: Some(data), };

        // We can't actually decode old experiment data.
        // Luckily Glean did store experiments in the database before commit ac27fceb7c0d5a7288d7d569e8c5c5399a53afb2.
        let experiment_empty: Result<RecordedExperimentData, _> = bincode::deserialize(&empty_bin);
        assert!(experiment_empty.is_err());

        assert_eq!(experiment_data, bincode::deserialize(&data_bin).unwrap());
    }
}
