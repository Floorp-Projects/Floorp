// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

use std::collections::HashMap;
use std::sync::atomic::Ordering;

use chrono::{DateTime, FixedOffset};
use serde::{Deserialize, Serialize};
use serde_json::json;
pub use serde_json::Value as JsonValue;

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
mod object;
mod ping;
mod quantity;
mod rate;
mod recorded_experiment;
mod remote_settings_config;
mod string;
mod string_list;
mod text;
mod time_unit;
mod timespan;
mod timing_distribution;
mod url;
mod uuid;

use crate::common_metric_data::CommonMetricDataInternal;
pub use crate::event_database::RecordedEvent;
use crate::histogram::{Functional, Histogram, PrecomputedExponential, PrecomputedLinear};
pub use crate::metrics::datetime::Datetime;
use crate::util::get_iso_time_string;
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
pub use self::object::ObjectMetric;
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

pub use self::remote_settings_config::RemoteSettingsConfig;

/// A snapshot of all buckets and the accumulated sum of a distribution.
//
// Note: Be careful when changing this structure.
// The serialized form ends up in the ping payload.
// New fields might require to be skipped on serialization.
#[derive(Debug, Serialize)]
pub struct DistributionData {
    /// A map containig the bucket index mapped to the accumulated count.
    ///
    /// This can contain buckets with a count of `0`.
    pub values: HashMap<i64, i64>,

    /// The accumulated sum of all the samples in the distribution.
    pub sum: i64,

    /// The total number of entries in the distribution.
    #[serde(skip)]
    pub count: i64,
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
    /// An Object metric. See [`ObjectMetric`] for more information.
    Object(String),
}

/// A [`MetricType`] describes common behavior across all metrics.
pub trait MetricType {
    /// Access the stored metadata
    fn meta(&self) -> &CommonMetricDataInternal;

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
        if !glean.is_upload_enabled() {
            return false;
        }

        // Technically nothing prevents multiple calls to should_record() to run in parallel,
        // meaning both are reading self.meta().disabled and later writing it. In between it can
        // also read remote_settings_config, which also could be modified in between those 2 reads.
        // This means we could write the wrong remote_settings_epoch | current_disabled value. All in all
        // at worst we would see that metric enabled/disabled wrongly once.
        // But since everything is tunneled through the dispatcher, this should never ever happen.

        // Get the current disabled field from the metric metadata, including
        // the encoded remote_settings epoch
        let disabled_field = self.meta().disabled.load(Ordering::Relaxed);
        // Grab the epoch from the upper nibble
        let epoch = disabled_field >> 4;
        // Get the disabled flag from the lower nibble
        let disabled = disabled_field & 0xF;
        // Get the current remote_settings epoch to see if we need to bother with the
        // more expensive HashMap lookup
        let remote_settings_epoch = glean.remote_settings_epoch.load(Ordering::Acquire);
        if epoch == remote_settings_epoch {
            return disabled == 0;
        }
        // The epoch's didn't match so we need to look up the disabled flag
        // by the base_identifier from the in-memory HashMap
        let remote_settings_config = &glean.remote_settings_config.lock().unwrap();
        // Get the value from the remote configuration if it is there, otherwise return the default value.
        let current_disabled = {
            let base_id = self.meta().base_identifier();
            let identifier = base_id
                .split_once('/')
                .map(|split| split.0)
                .unwrap_or(&base_id);
            // NOTE: The `!` preceding the `*is_enabled` is important for inverting the logic since the
            // underlying property in the metrics.yaml is `disabled` and the outward API is treating it as
            // if it were `enabled` to make it easier to understand.

            if !remote_settings_config.metrics_enabled.is_empty() {
                if let Some(is_enabled) = remote_settings_config.metrics_enabled.get(identifier) {
                    u8::from(!*is_enabled)
                } else {
                    u8::from(self.meta().inner.disabled)
                }
            } else {
                u8::from(self.meta().inner.disabled)
            }
        };

        // Re-encode the epoch and enabled status and update the metadata
        let new_disabled = (remote_settings_epoch << 4) | (current_disabled & 0xF);
        self.meta().disabled.store(new_disabled, Ordering::Relaxed);

        // Return a boolean indicating whether or not the metric should be recorded
        current_disabled == 0
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
            Metric::Object(_) => "object",
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
            Metric::Object(s) => {
                serde_json::from_str(s).expect("object storage should have been json")
            }
        }
    }
}
