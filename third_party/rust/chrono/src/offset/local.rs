// This is a part of rust-chrono.
// Copyright (c) 2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

/*!
 * The local (system) time zone.
 */

use stdtime;

use {Datelike, Timelike};
use duration::Duration;
use naive::date::NaiveDate;
use naive::time::NaiveTime;
use naive::datetime::NaiveDateTime;
use date::Date;
use datetime::DateTime;
use super::{TimeZone, LocalResult};
use super::fixed::FixedOffset;

/// Converts a `time::Tm` struct into the timezone-aware `DateTime`.
/// This assumes that `time` is working correctly, i.e. any error is fatal.
fn tm_to_datetime(mut tm: stdtime::Tm) -> DateTime<Local> {
    if tm.tm_sec >= 60 {
        tm.tm_nsec += (tm.tm_sec - 59) * 1_000_000_000;
        tm.tm_sec = 59;
    }

    #[cfg(not(windows))]
    fn tm_to_naive_date(tm: &stdtime::Tm) -> NaiveDate {
        // from_yo is more efficient than from_ymd (since it's the internal representation).
        NaiveDate::from_yo(tm.tm_year + 1900, tm.tm_yday as u32 + 1)
    }

    #[cfg(windows)]
    fn tm_to_naive_date(tm: &stdtime::Tm) -> NaiveDate {
        // ...but tm_yday is broken in Windows (issue #85)
        NaiveDate::from_ymd(tm.tm_year + 1900, tm.tm_mon as u32 + 1, tm.tm_mday as u32)
    }

    let date = tm_to_naive_date(&tm);
    let time = NaiveTime::from_hms_nano(tm.tm_hour as u32, tm.tm_min as u32,
                                        tm.tm_sec as u32, tm.tm_nsec as u32);
    let offset = FixedOffset::east(tm.tm_utcoff);
    DateTime::from_utc(date.and_time(time) + Duration::seconds(-tm.tm_utcoff as i64), offset)
}

/// Converts a local `NaiveDateTime` to the `time::Timespec`.
fn datetime_to_timespec(d: &NaiveDateTime, local: bool) -> stdtime::Timespec {
    // well, this exploits an undocumented `Tm::to_timespec` behavior
    // to get the exact function we want (either `timegm` or `mktime`).
    // the number 1 is arbitrary but should be non-zero to trigger `mktime`.
    let tm_utcoff = if local {1} else {0};

    let tm = stdtime::Tm {
        tm_sec: d.second() as i32,
        tm_min: d.minute() as i32,
        tm_hour: d.hour() as i32,
        tm_mday: d.day() as i32,
        tm_mon: d.month0() as i32, // yes, C is that strange...
        tm_year: d.year() - 1900, // this doesn't underflow, we know that d is `NaiveDateTime`.
        tm_wday: 0, // to_local ignores this
        tm_yday: 0, // and this
        tm_isdst: -1,
        tm_utcoff: tm_utcoff,
        tm_nsec: d.nanosecond() as i32,
    };
    tm.to_timespec()
}

/// The local timescale. This is implemented via the standard `time` crate.
#[derive(Copy, Clone)]
#[cfg_attr(feature = "rustc-serialize", derive(RustcEncodable, RustcDecodable))]
pub struct Local;

impl Local {
    /// Returns a `Date` which corresponds to the current date.
    pub fn today() -> Date<Local> {
        Local::now().date()
    }

    /// Returns a `DateTime` which corresponds to the current date.
    pub fn now() -> DateTime<Local> {
        tm_to_datetime(stdtime::now())
    }
}

impl TimeZone for Local {
    type Offset = FixedOffset;

    fn from_offset(_offset: &FixedOffset) -> Local { Local }

    // they are easier to define in terms of the finished date and time unlike other offsets
    fn offset_from_local_date(&self, local: &NaiveDate) -> LocalResult<FixedOffset> {
        self.from_local_date(local).map(|date| *date.offset())
    }
    fn offset_from_local_datetime(&self, local: &NaiveDateTime) -> LocalResult<FixedOffset> {
        self.from_local_datetime(local).map(|datetime| *datetime.offset())
    }

    fn offset_from_utc_date(&self, utc: &NaiveDate) -> FixedOffset {
        *self.from_utc_date(utc).offset()
    }
    fn offset_from_utc_datetime(&self, utc: &NaiveDateTime) -> FixedOffset {
        *self.from_utc_datetime(utc).offset()
    }

    // override them for avoiding redundant works
    fn from_local_date(&self, local: &NaiveDate) -> LocalResult<Date<Local>> {
        // this sounds very strange, but required for keeping `TimeZone::ymd` sane.
        // in the other words, we use the offset at the local midnight
        // but keep the actual date unaltered (much like `FixedOffset`).
        let midnight = self.from_local_datetime(&local.and_hms(0, 0, 0));
        midnight.map(|datetime| Date::from_utc(*local, datetime.offset().clone()))
    }
    fn from_local_datetime(&self, local: &NaiveDateTime) -> LocalResult<DateTime<Local>> {
        let timespec = datetime_to_timespec(local, true);
        LocalResult::Single(tm_to_datetime(stdtime::at(timespec)))
    }

    fn from_utc_date(&self, utc: &NaiveDate) -> Date<Local> {
        let midnight = self.from_utc_datetime(&utc.and_hms(0, 0, 0));
        Date::from_utc(*utc, midnight.offset().clone())
    }
    fn from_utc_datetime(&self, utc: &NaiveDateTime) -> DateTime<Local> {
        let timespec = datetime_to_timespec(utc, false);
        tm_to_datetime(stdtime::at(timespec))
    }
}

