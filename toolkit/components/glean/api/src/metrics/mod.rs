// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

// Re-export of `glean_core` types we can re-use.
// That way a user only needs to depend on this crate, not on glean_core (and there can't be a
// version mismatch).
pub use glean_core::{CommonMetricData, Lifetime};

mod string;

pub use string::StringMetric;
