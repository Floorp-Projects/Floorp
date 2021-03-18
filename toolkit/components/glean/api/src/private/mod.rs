// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

use std::convert::TryFrom;
use std::time::{SystemTime, UNIX_EPOCH};

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
mod datetime;
mod event;
mod labeled;
mod labeled_counter;
mod memory_distribution;
mod ping;
pub(crate) mod string;
mod string_list;
mod timespan;
mod timing_distribution;
mod uuid;

pub use self::boolean::BooleanMetric;
pub use self::boolean::BooleanMetric as LabeledBooleanMetric;
pub use self::counter::CounterMetric;
pub use self::datetime::DatetimeMetric;
pub use self::event::{EventMetric, EventRecordingError, ExtraKeys, NoExtraKeys};
pub use self::labeled::LabeledMetric;
pub use self::labeled_counter::LabeledCounterMetric;
pub use self::memory_distribution::MemoryDistributionMetric;
pub use self::ping::Ping;
pub use self::string::StringMetric;
pub use self::string::StringMetric as LabeledStringMetric;
pub use self::string_list::StringListMetric;
pub use self::timespan::TimespanMetric;
pub use self::timing_distribution::TimingDistributionMetric;
pub use self::uuid::UuidMetric;

/// An instant in time.
///
/// Similar to [`std::time::Instant`](https://doc.rust-lang.org/std/time/struct.Instant.html),
/// but much simpler in that we explicitly expose that it's just an integer.
///
/// This is needed, as the current `glean-core` API expects timestamps as integers.
/// We probably should move this API into `glean-core` directly.
/// See [Bug 1619253](https://bugzilla.mozilla.org/show_bug.cgi?id=1619253).
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct Instant(u64);

impl Instant {
    /// Returns an instant corresponding to "now".
    fn now() -> Instant {
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("SystemTime before UNIX epoch!");
        let now = now.as_nanos();

        match u64::try_from(now) {
            Ok(now) => Instant(now),
            Err(_) => {
                // Greetings to 2554 from 2020!
                panic!("timestamp exceeds value range")
            }
        }
    }
}

/// Uniquely identifies a single metric within its metric type.
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
#[repr(transparent)]
pub struct MetricId(u32);

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
