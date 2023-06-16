// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

use serde::{Deserialize, Serialize};

// Re-export of `glean` types we can re-use.
// That way a user only needs to depend on this crate, not on glean (and there can't be a
// version mismatch).
pub use glean::{
    traits, CommonMetricData, DistributionData, ErrorType, Lifetime, MemoryUnit, RecordedEvent,
    TimeUnit, TimerId,
};

mod boolean;
mod counter;
mod custom_distribution;
mod datetime;
mod denominator;
mod event;
mod labeled;
mod labeled_counter;
mod memory_distribution;
mod numerator;
mod ping;
mod quantity;
mod rate;
pub(crate) mod string;
mod string_list;
mod text;
mod timespan;
mod timing_distribution;
mod url;
mod uuid;

pub use self::boolean::BooleanMetric;
pub use self::boolean::BooleanMetric as LabeledBooleanMetric;
pub use self::counter::CounterMetric;
pub use self::custom_distribution::CustomDistributionMetric;
pub use self::datetime::DatetimeMetric;
pub use self::denominator::DenominatorMetric;
pub use self::event::{EventMetric, EventRecordingError, ExtraKeys, NoExtraKeys};
pub use self::labeled::LabeledMetric;
pub use self::labeled_counter::LabeledCounterMetric;
pub use self::memory_distribution::MemoryDistributionMetric;
pub use self::numerator::NumeratorMetric;
pub use self::ping::Ping;
pub use self::quantity::QuantityMetric;
pub use self::rate::RateMetric;
pub use self::string::StringMetric;
pub use self::string::StringMetric as LabeledStringMetric;
pub use self::string_list::StringListMetric;
pub use self::text::TextMetric;
pub use self::timespan::TimespanMetric;
pub use self::timing_distribution::TimingDistributionMetric;
pub use self::url::UrlMetric;
pub use self::uuid::UuidMetric;

/// Uniquely identifies a single metric within its metric type.
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
#[repr(transparent)]
pub struct MetricId(pub(crate) u32);

impl MetricId {
    pub fn new(id: u32) -> Self {
        Self(id)
    }
}

impl From<u32> for MetricId {
    fn from(id: u32) -> Self {
        Self(id)
    }
}
