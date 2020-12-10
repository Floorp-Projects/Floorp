// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::HashMap;
use serde::Deserialize;

/// Deserialized experiment data.
#[derive(Clone, Deserialize, Debug)]
pub struct RecordedExperimentData {
    /// The experiment's branch as set through [`set_experiment_active`](crate::set_experiment_active).
    pub branch: String,
    /// Any extra data associated with this experiment through [`set_experiment_active`](crate::set_experiment_active).
    pub extra: Option<HashMap<String, String>>,
}
