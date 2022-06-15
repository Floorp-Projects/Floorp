// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

use std::collections::HashMap;

use chrono::{DateTime, FixedOffset};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value as JsonValue};

mod boolean;
mod counter;
mod custom_distribution;
mod datetime;
mod denominator;
mod event;
mod experiment;
pub(crate) mod labeled;
mod memory_distribution;
mod memory_unit;
mod numerator;
mod ping;
mod quantity;
mod rate;
mod recorded_experiment;
mod string;
mod string_list;
mod text;
mod time_unit;
mod timespan;
mod timing_distribution;
mod url;
mod uuid;

pub use crate::event_database::RecordedEvent;
use crate::histogram::{Functional, Histogram, PrecomputedExponential, PrecomputedLinear};
pub use crate::metrics::datetime::Datetime;
use crate::util::get_iso_time_string;
use crate::CommonMetricData;
use crate::Glean;

pub use self::boolean::BooleanMetric;
pub use self::counter::CounterMetric;
pub use self::custom_distribution::CustomDistributionMetric;
pub use self::datetime::DatetimeMetric;
pub use self::denominator::DenominatorMetric;
pub use self::event::EventMetric;
pub(crate) use self::experiment::ExperimentMetric;
pub use self::labeled::{LabeledBoolean, LabeledCounter, LabeledMetric, LabeledString};
pub use self::memory_distribution::MemoryDistributionMetric;
pub use self::memory_unit::MemoryUnit;
pub use self::numerator::NumeratorMetric;
pub use self::ping::PingType;
pub use self::quantity::QuantityMetric;
pub use self::rate::{Rate, RateMetric};
pub use self::string::StringMetric;
pub use self::string_list::StringListMetric;
pub use self::text::TextMetric;
pub use self::time_unit::TimeUnit;
pub use self::timespan::TimespanMetric;
pub use self::timing_distribution::TimerId;
pub use self::timing_distribution::TimingDistributionMetric;
pub use self::url::UrlMetric;
pub use self::uuid::UuidMetric;
pub use crate::histogram::HistogramType;
pub use recorded_experiment::RecordedExperiment;

/// A snapshot of all buckets and the accumulated sum of a distribution.
#[derive(Debug, Serialize)]
pub struct DistributionData {
    /// A map containig the bucket index mapped to the accumulated count.
    ///
    /// This can contain buckets with a count of `0`.
    pub values: HashMap<i64, i64>,

    /// The accumulated sum of all the samples in the distribution.
    pub sum: i64,
}

/// The available metrics.
///
/// This is the in-memory and persisted layout of a metric.
///
/// ## Note
///
/// The order of metrics in this enum is important, as it is used for serialization.
/// Do not reorder the variants.
///
/// **Any new metric must be added at the end.**
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub enum Metric {
    /// A boolean metric. See [`BooleanMetric`] for more information.
    Boolean(bool),
    /// A counter metric. See [`CounterMetric`] for more information.
    Counter(i32),
    /// A custom distribution with precomputed exponential bucketing.
    /// See [`CustomDistributionMetric`] for more information.
    CustomDistributionExponential(Histogram<PrecomputedExponential>),
    /// A custom distribution with precomputed linear bucketing.
    /// See [`CustomDistributionMetric`] for more information.
    CustomDistributionLinear(Histogram<PrecomputedLinear>),
    /// A datetime metric. See [`DatetimeMetric`] for more information.
    Datetime(DateTime<FixedOffset>, TimeUnit),
    /// An experiment metric. See `ExperimentMetric` for more information.
    Experiment(recorded_experiment::RecordedExperiment),
    /// A quantity metric. See [`QuantityMetric`] for more information.
    Quantity(i64),
    /// A string metric. See [`StringMetric`] for more information.
    String(String),
    /// A string list metric. See [`StringListMetric`] for more information.
    StringList(Vec<String>),
    /// A UUID metric. See [`UuidMetric`] for more information.
    Uuid(String),
    /// A timespan metric. See [`TimespanMetric`] for more information.
    Timespan(std::time::Duration, TimeUnit),
    /// A timing distribution. See [`TimingDistributionMetric`] for more information.
    TimingDistribution(Histogram<Functional>),
    /// A memory distribution. See [`MemoryDistributionMetric`] for more information.
    MemoryDistribution(Histogram<Functional>),
    /// **DEPRECATED**: A JWE metric..
    /// Note: This variant MUST NOT be removed to avoid backwards-incompatible changes to the
    /// serialization. This type has no underlying implementation anymore.
    Jwe(String),
    /// A rate metric. See [`RateMetric`] for more information.
    Rate(i32, i32),
    /// A URL metric. See [`UrlMetric`] for more information.
    Url(String),
    /// A Text metric. See [`TextMetric`] for more information.
    Text(String),
}

/// A [`MetricType`] describes common behavior across all metrics.
pub trait MetricType {
    /// Access the stored metadata
    fn meta(&self) -> &CommonMetricData;

    /// Create a new metric from this with a new name.
    fn with_name(&self, _name: String) -> Self
    where
        Self: Sized,
    {
        unimplemented!()
    }

    /// Create a new metric from this with a specific label.
    fn with_dynamic_label(&self, _label: String) -> Self
    where
        Self: Sized,
    {
        unimplemented!()
    }

    /// Whether this metric should currently be recorded
    ///
    /// This depends on the metrics own state, as determined by its metadata,
    /// and whether upload is enabled on the Glean object.
    fn should_record(&self, glean: &Glean) -> bool {
        glean.is_upload_enabled() && self.meta().should_record()
    }
}

impl Metric {
    /// Gets the ping section the metric fits into.
    ///
    /// This determines the section of the ping to place the metric data in when
    /// assembling the ping payload.
    pub fn ping_section(&self) -> &'static str {
        match self {
            Metric::Boolean(_) => "boolean",
            Metric::Counter(_) => "counter",
            // Custom distributions are in the same section, no matter what bucketing.
            Metric::CustomDistributionExponential(_) => "custom_distribution",
            Metric::CustomDistributionLinear(_) => "custom_distribution",
            Metric::Datetime(_, _) => "datetime",
            Metric::Experiment(_) => panic!("Experiments should not be serialized through this"),
            Metric::Quantity(_) => "quantity",
            Metric::Rate(..) => "rate",
            Metric::String(_) => "string",
            Metric::StringList(_) => "string_list",
            Metric::Timespan(..) => "timespan",
            Metric::TimingDistribution(_) => "timing_distribution",
            Metric::Url(_) => "url",
            Metric::Uuid(_) => "uuid",
            Metric::MemoryDistribution(_) => "memory_distribution",
            Metric::Jwe(_) => "jwe",
            Metric::Text(_) => "text",
        }
    }

    /// The JSON representation of the metric's data
    pub fn as_json(&self) -> JsonValue {
        match self {
            Metric::Boolean(b) => json!(b),
            Metric::Counter(c) => json!(c),
            Metric::CustomDistributionExponential(hist) => {
                json!(custom_distribution::snapshot(hist))
            }
            Metric::CustomDistributionLinear(hist) => json!(custom_distribution::snapshot(hist)),
            Metric::Datetime(d, time_unit) => json!(get_iso_time_string(*d, *time_unit)),
            Metric::Experiment(e) => e.as_json(),
            Metric::Quantity(q) => json!(q),
            Metric::Rate(num, den) => {
                json!({"numerator": num, "denominator": den})
            }
            Metric::String(s) => json!(s),
            Metric::StringList(v) => json!(v),
            Metric::Timespan(time, time_unit) => {
                json!({"value": time_unit.duration_convert(*time), "time_unit": time_unit})
            }
            Metric::TimingDistribution(hist) => json!(timing_distribution::snapshot(hist)),
            Metric::Url(s) => json!(s),
            Metric::Uuid(s) => json!(s),
            Metric::MemoryDistribution(hist) => json!(memory_distribution::snapshot(hist)),
            Metric::Jwe(s) => json!(s),
            Metric::Text(s) => json!(s),
        }
    }
}
