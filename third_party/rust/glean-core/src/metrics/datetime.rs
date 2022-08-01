// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fmt;
use std::sync::Arc;

use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::metrics::time_unit::TimeUnit;
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::util::{get_iso_time_string, local_now_with_offset};
use crate::CommonMetricData;
use crate::Glean;

use chrono::{DateTime, Datelike, FixedOffset, TimeZone, Timelike};

/// A datetime type.
///
/// Used to feed data to the `DatetimeMetric`.
pub type ChronoDatetime = DateTime<FixedOffset>;

/// Representation of a date, time and timezone.
#[derive(Clone, PartialEq, Eq)]
pub struct Datetime {
    /// The year, e.g. 2021.
    pub year: i32,
    /// The month, 1=January.
    pub month: u32,
    /// The day of the month.
    pub day: u32,
    /// The hour. 0-23
    pub hour: u32,
    /// The minute. 0-59.
    pub minute: u32,
    /// The second. 0-60.
    pub second: u32,
    /// The nanosecond part of the time.
    pub nanosecond: u32,
    /// The timezone offset from UTC in seconds.
    /// Negative for west, positive for east of UTC.
    pub offset_seconds: i32,
}

impl fmt::Debug for Datetime {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Datetime({:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}{}{:02}{:02})",
            self.year,
            self.month,
            self.day,
            self.hour,
            self.minute,
            self.second,
            self.nanosecond,
            if self.offset_seconds < 0 { "-" } else { "+" },
            self.offset_seconds / 3600,        // hour part
            (self.offset_seconds % 3600) / 60, // minute part
        )
    }
}

impl Default for Datetime {
    fn default() -> Self {
        Datetime {
            year: 1970,
            month: 1,
            day: 1,
            hour: 0,
            minute: 0,
            second: 0,
            nanosecond: 0,
            offset_seconds: 0,
        }
    }
}

/// A datetime metric.
///
/// Used to record an absolute date and time, such as the time the user first ran
/// the application.
#[derive(Clone, Debug)]
pub struct DatetimeMetric {
    meta: Arc<CommonMetricData>,
    time_unit: TimeUnit,
}

impl MetricType for DatetimeMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }
}

impl From<ChronoDatetime> for Datetime {
    fn from(dt: ChronoDatetime) -> Self {
        let date = dt.date();
        let time = dt.time();
        let tz = dt.timezone();
        Self {
            year: date.year(),
            month: date.month(),
            day: date.day(),
            hour: time.hour(),
            minute: time.minute(),
            second: time.second(),
            nanosecond: time.nanosecond(),
            offset_seconds: tz.local_minus_utc(),
        }
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl DatetimeMetric {
    /// Creates a new datetime metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        Self {
            meta: Arc::new(meta),
            time_unit,
        }
    }

    /// Sets the metric to a date/time including the timezone offset.
    ///
    /// # Arguments
    ///
    /// * `dt` - the optinal datetime to set this to. If missing the current date is used.
    pub fn set(&self, dt: Option<Datetime>) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| {
            metric.set_sync(glean, dt);
        })
    }

    /// Sets the metric to a date/time which including the timezone offset synchronously.
    ///
    /// Use [`set`](Self::set) instead.
    #[doc(hidden)]
    pub fn set_sync(&self, glean: &Glean, value: Option<Datetime>) {
        if !self.should_record(glean) {
            return;
        }

        let value = match value {
            None => local_now_with_offset(),
            Some(dt) => {
                let timezone_offset = FixedOffset::east_opt(dt.offset_seconds);
                if timezone_offset.is_none() {
                    let msg = format!(
                        "Invalid timezone offset {}. Not recording.",
                        dt.offset_seconds
                    );
                    record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
                    return;
                };

                let datetime_obj = FixedOffset::east(dt.offset_seconds)
                    .ymd_opt(dt.year, dt.month, dt.day)
                    .and_hms_nano_opt(dt.hour, dt.minute, dt.second, dt.nanosecond);

                if let Some(dt) = datetime_obj.single() {
                    dt
                } else {
                    record_error(
                        glean,
                        &self.meta,
                        ErrorType::InvalidValue,
                        "Invalid input data. Not recording.",
                        None,
                    );
                    return;
                }
            }
        };

        self.set_sync_chrono(glean, value);
    }

    pub(crate) fn set_sync_chrono(&self, glean: &Glean, value: ChronoDatetime) {
        let value = Metric::Datetime(value, self.time_unit);
        glean.storage().record(glean, &self.meta, &value)
    }

    /// Gets the stored datetime value.
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<ChronoDatetime> {
        let (d, tu) = self.get_value_inner(glean, ping_name.into())?;

        // The string version of the test function truncates using string
        // parsing. Unfortunately `parse_from_str` errors with `NotEnough` if we
        // try to truncate with `get_iso_time_string` and then parse it back
        // in a `Datetime`. So we need to truncate manually.
        let time = d.time();
        match tu {
            TimeUnit::Nanosecond => d.date().and_hms_nano_opt(
                time.hour(),
                time.minute(),
                time.second(),
                time.nanosecond(),
            ),
            TimeUnit::Microsecond => {
                eprintln!(
                    "microseconds. nanoseconds={}, nanoseconds/1000={}",
                    time.nanosecond(),
                    time.nanosecond() / 1000
                );
                d.date().and_hms_nano_opt(
                    time.hour(),
                    time.minute(),
                    time.second(),
                    time.nanosecond() / 1000,
                )
            }
            TimeUnit::Millisecond => d.date().and_hms_nano_opt(
                time.hour(),
                time.minute(),
                time.second(),
                time.nanosecond() / 1000000,
            ),
            TimeUnit::Second => {
                d.date()
                    .and_hms_nano_opt(time.hour(), time.minute(), time.second(), 0)
            }
            TimeUnit::Minute => d.date().and_hms_nano_opt(time.hour(), time.minute(), 0, 0),
            TimeUnit::Hour => d.date().and_hms_nano_opt(time.hour(), 0, 0, 0),
            TimeUnit::Day => d.date().and_hms_nano_opt(0, 0, 0, 0),
        }
    }

    fn get_value_inner(
        &self,
        glean: &Glean,
        ping_name: Option<&str>,
    ) -> Option<(ChronoDatetime, TimeUnit)> {
        let queried_ping_name = ping_name.unwrap_or_else(|| &self.meta().send_in_pings[0]);

        match StorageManager.snapshot_metric(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Datetime(d, tu)) => Some((d, tu)),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the stored datetime value.
    ///
    /// The precision of this value is truncated to the `time_unit` precision.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `storage_name` - the storage name to look into.
    ///
    /// # Returns
    ///
    /// The stored value or `None` if nothing stored.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<Datetime> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| {
            let dt = self.get_value(glean, ping_name.as_deref());
            dt.map(Datetime::from)
        })
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the stored datetime value, formatted as an ISO8601 string.
    ///
    /// The precision of this value is truncated to the `time_unit` precision.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `storage_name` - the storage name to look into.
    ///
    /// # Returns
    ///
    /// The stored value or `None` if nothing stored.
    pub fn test_get_value_as_string(&self, ping_name: Option<String>) -> Option<String> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| self.get_value_as_string(glean, ping_name))
    }

    /// **Test-only API**
    ///
    /// Gets the stored datetime value, formatted as an ISO8601 string.
    #[doc(hidden)]
    pub fn get_value_as_string(&self, glean: &Glean, ping_name: Option<String>) -> Option<String> {
        let value = self.get_value_inner(glean, ping_name.as_deref());
        value.map(|(dt, tu)| get_iso_time_string(dt, tu))
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        crate::block_on_dispatcher();

        crate::core::with_glean(|glean| {
            test_get_num_recorded_errors(glean, self.meta(), error).unwrap_or(0)
        })
    }
}
