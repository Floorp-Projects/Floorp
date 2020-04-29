// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

// Re-export of `glean_core` types we can re-use.
// That way a user only needs to depend on this crate, not on glean_core (and there can't be a
// version mismatch).
pub use glean_core::{CommonMetricData, ErrorType, Lifetime};

mod boolean;
mod counter;
mod labeled;
mod ping;
mod string;
mod string_list;
mod uuid;

pub use self::boolean::BooleanMetric;
pub use self::counter::CounterMetric;
pub use self::labeled::LabeledMetric;
pub use self::ping::Ping;
pub use self::string::StringMetric;
pub use self::string_list::StringListMetric;
pub use self::uuid::UuidMetric;
