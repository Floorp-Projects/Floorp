// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![allow(clippy::too_many_arguments)]

use crate::error_recording::{record_error, ErrorType};
use crate::metrics::time_unit::TimeUnit;
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::util::{get_iso_time_string, local_now_with_offset};
use crate::CommonMetricData;
use crate::Glean;

use chrono::{DateTime, FixedOffset, TimeZone};

/// A datetime type.
///
/// Used to feed data to the `DatetimeMetric`.
pub type Datetime = DateTime<FixedOffset>;

/// A datetime metric.
///
/// Used to record an absolute date and time, such as the time the user first ran
/// the application.
#[derive(Debug)]
pub struct DatetimeMetric {
    meta: CommonMetricData,
    time_unit: TimeUnit,
}

impl MetricType for DatetimeMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn meta_mut(&mut self) -> &mut CommonMetricData {
        &mut self.meta
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl DatetimeMetric {
    /// Creates a new datetime metric.
    pub fn new(meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        Self { meta, time_unit }
    }

    /// Sets the metric to a date/time including the timezone offset.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `year` - the year to set the metric to.
    /// * `month` - the month to set the metric to (1-12).
    /// * `day` - the day to set the metric to (1-based).
    /// * `hour` - the hour to set the metric to.
    /// * `minute` - the minute to set the metric to.
    /// * `second` - the second to set the metric to.
    /// * `nano` - the nanosecond fraction to the last whole second.
    /// * `offset_seconds` - the timezone difference, in seconds, for the Eastern
    ///   Hemisphere. Negative seconds mean Western Hemisphere.
    pub fn set_with_details(
        &self,
        glean: &Glean,
        year: i32,
        month: u32,
        day: u32,
        hour: u32,
        minute: u32,
        second: u32,
        nano: u32,
        offset_seconds: i32,
    ) {
        if !self.should_record(glean) {
            return;
        }

        let timezone_offset = FixedOffset::east_opt(offset_seconds);
        if timezone_offset.is_none() {
            let msg = format!("Invalid timezone offset {}. Not recording.", offset_seconds);
            record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
            return;
        };

        let datetime_obj = FixedOffset::east(offset_seconds)
            .ymd_opt(year, month, day)
            .and_hms_nano_opt(hour, minute, second, nano);

        match datetime_obj.single() {
            Some(d) => self.set(glean, Some(d)),
            _ => {
                record_error(
                    glean,
                    &self.meta,
                    ErrorType::InvalidValue,
                    "Invalid input data. Not recording.",
                    None,
                );
            }
        }
    }

    /// Sets the metric to a date/time which including the timezone offset.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `value` - Some date/time value, with offset, to set the metric to.
    ///             If none, the current local time is used.
    pub fn set(&self, glean: &Glean, value: Option<Datetime>) {
        if !self.should_record(glean) {
            return;
        }

        let value = value.unwrap_or_else(local_now_with_offset);
        let value = Metric::Datetime(value, self.time_unit);
        glean.storage().record(glean, &self.meta, &value)
    }

    /// Gets the stored datetime value.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `storage_name` - the storage name to look into.
    ///
    /// # Returns
    ///
    /// The stored value or `None` if nothing stored.
    pub(crate) fn get_value(&self, glean: &Glean, storage_name: &str) -> Option<Datetime> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta().identifier(glean),
        ) {
            Some(Metric::Datetime(dt, _)) => Some(dt),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as a String.
    ///
    /// The precision of this value is truncated to the `time_unit` precision.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value_as_string(&self, glean: &Glean, storage_name: &str) -> Option<String> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
        ) {
            Some(Metric::Datetime(d, tu)) => Some(get_iso_time_string(d, tu)),
            _ => None,
        }
    }
}
