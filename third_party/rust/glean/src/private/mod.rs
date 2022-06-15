// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

mod event;
mod ping;

pub use event::EventMetric;
pub use glean_core::BooleanMetric;
pub use glean_core::CounterMetric;
pub use glean_core::CustomDistributionMetric;
pub use glean_core::DenominatorMetric;
pub use glean_core::MemoryDistributionMetric;
pub use glean_core::NumeratorMetric;
pub use glean_core::QuantityMetric;
pub use glean_core::RateMetric;
pub use glean_core::RecordedExperiment;
pub use glean_core::StringListMetric;
pub use glean_core::StringMetric;
pub use glean_core::TimespanMetric;
pub use glean_core::TimingDistributionMetric;
pub use glean_core::UrlMetric;
pub use glean_core::UuidMetric;
pub use glean_core::{AllowLabeled, LabeledMetric};
pub use glean_core::{Datetime, DatetimeMetric};
pub use ping::PingType;

// Re-export types that are used by the glean_parser-generated code.
#[doc(hidden)]
pub mod __export {
    pub use once_cell::sync::Lazy;
}
