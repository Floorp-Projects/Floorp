// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

use std::convert::TryFrom;
use std::time::{SystemTime, UNIX_EPOCH};

// Re-export of `glean_core` types we can re-use.
// That way a user only needs to depend on this crate, not on glean_core (and there can't be a
// version mismatch).
pub use glean_core::{
    metrics::DistributionData, metrics::MemoryUnit, metrics::TimeUnit, CommonMetricData, ErrorType,
    Lifetime,
};

mod boolean;
mod counter;
mod datetime;
mod event;
mod labeled;
mod memory_distribution;
mod ping;
pub(crate) mod string;
mod string_list;
mod timespan;
mod timing_distribution;
mod uuid;

pub use self::boolean::BooleanMetric;
pub use self::counter::CounterMetric;
pub use self::datetime::DatetimeMetric;
pub use self::event::{EventMetric, ExtraKeys, NoExtraKeys};
pub use self::labeled::LabeledMetric;
pub use self::memory_distribution::MemoryDistributionMetric;
pub use self::ping::Ping;
pub use self::string::StringMetric;
pub use self::string_list::StringListMetric;
pub use self::timespan::TimespanMetric;
pub use self::timing_distribution::{TimerId, TimingDistributionMetric};
pub use self::uuid::UuidMetric;

/// An instant in time.
///
/// Similar to [`std::time::Instant`](https://doc.rust-lang.org/std/time/struct.Instant.html),
/// but much simpler in that we explicitly expose that it's just an integer.
///
/// This is needed, as the current `glean-core` API expects timestamps as integers.
/// We probably should move this API into `glean-core` directly.
/// See [Bug 1619253](https://bugzilla.mozilla.org/show_bug.cgi?id=1619253).
struct Instant(u64);

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

    /// Get this instant as a timestamp in nanoseconds.
    fn as_nanos(&self) -> u64 {
        self.0
    }

    /// Get this instant as a timestamp in milliseconds.
    fn as_millis(&self) -> u64 {
        const NANOS_PER_MILLI: u64 = 1_000_000;
        self.0 / NANOS_PER_MILLI
    }
}
