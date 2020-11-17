// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

mod boolean;
mod counter;
mod ping;
mod recorded_experiment_data;
mod string;

pub use boolean::BooleanMetric;
pub use counter::CounterMetric;
pub use ping::PingType;
pub use recorded_experiment_data::RecordedExperimentData;
pub use string::StringMetric;
