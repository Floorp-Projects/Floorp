// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};
use serde_json::{json, Map as JsonMap, Value as JsonValue};

/// Deserialized experiment data.
#[derive(Clone, Serialize, Deserialize, Debug, PartialEq, Eq)]
pub struct RecordedExperiment {
    /// The experiment's branch as set through [`set_experiment_active`](crate::glean_set_experiment_active).
    pub branch: String,
    /// Any extra data associated with this experiment through [`set_experiment_active`](crate::glean_set_experiment_active).
    /// Note: `Option` required to keep backwards-compatibility.
    pub extra: Option<HashMap<String, String>>,
}

impl RecordedExperiment {
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
