// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::{collections::HashMap, convert::TryFrom};

use serde::{Deserialize, Serialize};

/// Represents a list of metrics and their associated `disabled` property from the
/// remote-settings configuration store. The expected format of this data is stringified
/// JSON in the following format:
/// ```json
/// {
///     "category.metric_name": true
/// }
/// ```
#[derive(Serialize, Deserialize, Debug, Clone, Default)]
pub struct MetricsDisabledConfig {
    /// This is a `HashMap` consisting of base_identifiers as keys
    /// and bool values representing an override for the `disabled`
    /// property of the metric
    #[serde(flatten)]
    pub metrics_disabled: HashMap<String, bool>,
}

impl MetricsDisabledConfig {
    /// Creates a new MetricsDisabledConfig
    pub fn new() -> Self {
        Default::default()
    }
}

impl TryFrom<String> for MetricsDisabledConfig {
    type Error = crate::ErrorKind;

    fn try_from(json: String) -> Result<Self, Self::Error> {
        match serde_json::from_str(json.as_str()) {
            Ok(config) => Ok(config),
            Err(e) => Err(crate::ErrorKind::Json(e)),
        }
    }
}
