// This is a part of rust-chrono.
// Copyright (c) 2014-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

/*!
 * The time zone, which calculates offsets from the local time to UTC.
 *
 * There are three operations provided by the `TimeZone` trait:
 *
 * 1. Converting the local `NaiveDateTime` to `DateTime<Tz>`
 * 2. Converting the UTC `NaiveDateTime` to `DateTime<Tz>`
 * 3. Converting `DateTime<Tz>` to the local `NaiveDateTime`
 *
 * 1 is used for constructors. 2 is used for the `with_timezone` method of date and time types.
 * 3 is used for other methods, e.g. `year()` or `format()`, and provided by an associated type
 * which implements `Offset` (which then passed to `TimeZone` for actual implementations).
 * Technically speaking `TimeZone` has a total knowledge about given timescale,
 * but `Offset` is used as a cache to avoid the repeated conversion
 * and provides implementations for 1 and 3.
 * An `TimeZone` instance can be reconstructed from the corresponding `Offset` instance.
 */

use std::fmt;

use Weekday;
use duration::Duration;
use naive::date::NaiveDate;
use naive::time::NaiveTime;
use naive::datetime::NaiveDateTime;
use date::Date;
use datetime::DateTime;
use format::{parse, Parsed, ParseResult, StrftimeItems};

/// The conversion result from the local time to the timezone-aware datetime types.
#[derive(Clone, PartialEq, Debug)]
pub enum LocalResult<T> {
    /// Given local time representation is invalid.
    /// This can occur when, for example, the positive timezone transition.
    None,
    /// Given local time representation has a single unique result.
    Single(T),
    /// Given local time representation has multiple results and thus ambiguous.
    /// This can occur when, for example, the negative timezone transition.
    Ambiguous(T /*min*/, T /*max*/),
}

impl<T> LocalResult<T> {
    /// Returns `Some` only when the conversion result is unique, or `None` otherwise.
    pub fn single(self) -> Option<T> {
        match self { LocalResult::Single(t) => Some(t), _ => None }
    }

    /// Returns `Some` for the earliest possible conversion result, or `None` if none.
    pub fn earliest(self) -> Option<T> {
        match self { LocalResult::Single(t) | LocalResult::Ambiguous(t,_) => Some(t), _ => None }
    }

    /// Returns `Some` for the latest possible conversion result, or `None` if none.
    pub fn latest(self) -> Option<T> {
        match self { LocalResult::Single(t) | LocalResult::Ambiguous(_,t) => Some(t), _ => None }
    }

    /// Maps a `LocalResult<T>` into `LocalResult<U>` with given function.
    pub fn map<U, F: FnMut(T) -> U>(self, mut f: F) -> LocalResult<U> {
        match self {
            LocalResult::None => LocalResult::None,
            LocalResult::Single(v) => LocalResult::Single(f(v)),
            LocalResult::Ambiguous(min, max) => LocalResult::Ambiguous(f(min), f(max)),
        }
    }
}

impl<Tz: TimeZone> LocalResult<Date<Tz>> {
    /// Makes a new `DateTime` from the current date and given `NaiveTime`.
    /// The offset in the current date is preserved.
    ///
    /// Propagates any error. Ambiguous result would be discarded.
    #[inline]
    pub fn and_time(self, time: NaiveTime) -> LocalResult<DateTime<Tz>> {
        match self {
            LocalResult::Single(d) => d.and_time(time)
                                       .map_or(LocalResult::None, LocalResult::Single),
            _ => LocalResult::None,
        }
    }

    /// Makes a new `DateTime` from the current date, hour, minute and second.
    /// The offset in the current date is preserved.
    ///
    /// Propagates any error. Ambiguous result would be discarded.
    #[inline]
    pub fn and_hms_opt(self, hour: u32, min: u32, sec: u32) -> LocalResult<DateTime<Tz>> {
        match self {
            LocalResult::Single(d) => d.and_hms_opt(hour, min, sec)
                                       .map_or(LocalResult::None, LocalResult::Single),
            _ => LocalResult::None,
        }
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and millisecond.
    /// The millisecond part can exceed 1,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Propagates any error. Ambiguous result would be discarded.
    #[inline]
    pub fn and_hms_milli_opt(self, hour: u32, min: u32, sec: u32,
                             milli: u32) -> LocalResult<DateTime<Tz>> {
        match self {
            LocalResult::Single(d) => d.and_hms_milli_opt(hour, min, sec, milli)
                                       .map_or(LocalResult::None, LocalResult::Single),
            _ => LocalResult::None,
        }
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and microsecond.
    /// The microsecond part can exceed 1,000,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Propagates any error. Ambiguous result would be discarded.
    #[inline]
    pub fn and_hms_micro_opt(self, hour: u32, min: u32, sec: u32,
                             micro: u32) -> LocalResult<DateTime<Tz>> {
        match self {
            LocalResult::Single(d) => d.and_hms_micro_opt(hour, min, sec, micro)
                                       .map_or(LocalResult::None, LocalResult::Single),
            _ => LocalResult::None,
        }
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and nanosecond.
    /// The nanosecond part can exceed 1,000,000,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Propagates any error. Ambiguous result would be discarded.
    #[inline]
    pub fn and_hms_nano_opt(self, hour: u32, min: u32, sec: u32,
                            nano: u32) -> LocalResult<DateTime<Tz>> {
        match self {
            LocalResult::Single(d) => d.and_hms_nano_opt(hour, min, sec, nano)
                                       .map_or(LocalResult::None, LocalResult::Single),
            _ => LocalResult::None,
        }
    }

}

impl<T: fmt::Debug> LocalResult<T> {
    /// Returns the single unique conversion result, or panics accordingly.
    pub fn unwrap(self) -> T {
        match self {
            LocalResult::None => panic!("No such local time"),
            LocalResult::Single(t) => t,
            LocalResult::Ambiguous(t1,t2) => {
                panic!("Ambiguous local time, ranging from {:?} to {:?}", t1, t2)
            }
        }
    }
}

/// The offset from the local time to UTC.
pub trait Offset: Sized + Clone + fmt::Debug {
    /// Returns the offset from UTC to the local time stored.
    fn local_minus_utc(&self) -> Duration;
}

/// The time zone.
pub trait TimeZone: Sized + Clone {
    /// An associated offset type.
    /// This type is used to store the actual offset in date and time types.
    /// The original `TimeZone` value can be recovered via `TimeZone::from_offset`.
    type Offset: Offset;

    /// Makes a new `Date` from year, month, day and the current time zone.
    /// This assumes the proleptic Gregorian calendar, with the year 0 being 1 BCE.
    ///
    /// The time zone normally does not affect the date (unless it is between UTC-24 and UTC+24),
    /// but it will propagate to the `DateTime` values constructed via this date.
    ///
    /// Panics on the out-of-range date, invalid month and/or day.
    fn ymd(&self, year: i32, month: u32, day: u32) -> Date<Self> {
        self.ymd_opt(year, month, day).unwrap()
    }

    /// Makes a new `Date` from year, month, day and the current time zone.
    /// This assumes the proleptic Gregorian calendar, with the year 0 being 1 BCE.
    ///
    /// The time zone normally does not affect the date (unless it is between UTC-24 and UTC+24),
    /// but it will propagate to the `DateTime` values constructed via this date.
    ///
    /// Returns `None` on the out-of-range date, invalid month and/or day.
    fn ymd_opt(&self, year: i32, month: u32, day: u32) -> LocalResult<Date<Self>> {
        match NaiveDate::from_ymd_opt(year, month, day) {
            Some(d) => self.from_local_date(&d),
            None => LocalResult::None,
        }
    }

    /// Makes a new `Date` from year, day of year (DOY or "ordinal") and the current time zone.
    /// This assumes the proleptic Gregorian calendar, with the year 0 being 1 BCE.
    ///
    /// The time zone normally does not affect the date (unless it is between UTC-24 and UTC+24),
    /// but it will propagate to the `DateTime` values constructed via this date.
    ///
    /// Panics on the out-of-range date and/or invalid DOY.
    fn yo(&self, year: i32, ordinal: u32) -> Date<Self> {
        self.yo_opt(year, ordinal).unwrap()
    }

    /// Makes a new `Date` from year, day of year (DOY or "ordinal") and the current time zone.
    /// This assumes the proleptic Gregorian calendar, with the year 0 being 1 BCE.
    ///
    /// The time zone normally does not affect the date (unless it is between UTC-24 and UTC+24),
    /// but it will propagate to the `DateTime` values constructed via this date.
    ///
    /// Returns `None` on the out-of-range date and/or invalid DOY.
    fn yo_opt(&self, year: i32, ordinal: u32) -> LocalResult<Date<Self>> {
        match NaiveDate::from_yo_opt(year, ordinal) {
            Some(d) => self.from_local_date(&d),
            None => LocalResult::None,
        }
    }

    /// Makes a new `Date` from ISO week date (year and week number), day of the week (DOW) and
    /// the current time zone.
    /// This assumes the proleptic Gregorian calendar, with the year 0 being 1 BCE.
    /// The resulting `Date` may have a different year from the input year.
    ///
    /// The time zone normally does not affect the date (unless it is between UTC-24 and UTC+24),
    /// but it will propagate to the `DateTime` values constructed via this date.
    ///
    /// Panics on the out-of-range date and/or invalid week number.
    fn isoywd(&self, year: i32, week: u32, weekday: Weekday) -> Date<Self> {
        self.isoywd_opt(year, week, weekday).unwrap()
    }

    /// Makes a new `Date` from ISO week date (year and week number), day of the week (DOW) and
    /// the current time zone.
    /// This assumes the proleptic Gregorian calendar, with the year 0 being 1 BCE.
    /// The resulting `Date` may have a different year from the input year.
    ///
    /// The time zone normally does not affect the date (unless it is between UTC-24 and UTC+24),
    /// but it will propagate to the `DateTime` values constructed via this date.
    ///
    /// Returns `None` on the out-of-range date and/or invalid week number.
    fn isoywd_opt(&self, year: i32, week: u32, weekday: Weekday) -> LocalResult<Date<Self>> {
        match NaiveDate::from_isoywd_opt(year, week, weekday) {
            Some(d) => self.from_local_date(&d),
            None => LocalResult::None,
        }
    }

    /// Makes a new `DateTime` from the number of non-leap seconds
    /// since January 1, 1970 0:00:00 UTC (aka "UNIX timestamp")
    /// and the number of nanoseconds since the last whole non-leap second.
    ///
    /// Panics on the out-of-range number of seconds and/or invalid nanosecond.
    fn timestamp(&self, secs: i64, nsecs: u32) -> DateTime<Self> {
        self.timestamp_opt(secs, nsecs).unwrap()
    }

    /// Makes a new `DateTime` from the number of non-leap seconds
    /// since January 1, 1970 0:00:00 UTC (aka "UNIX timestamp")
    /// and the number of nanoseconds since the last whole non-leap second.
    ///
    /// Returns `None` on the out-of-range number of seconds and/or invalid nanosecond.
    fn timestamp_opt(&self, secs: i64, nsecs: u32) -> LocalResult<DateTime<Self>> {
        match NaiveDateTime::from_timestamp_opt(secs, nsecs) {
            Some(dt) => LocalResult::Single(self.from_utc_datetime(&dt)),
            None => LocalResult::None,
        }
    }

    /// Parses a string with the specified format string and
    /// returns a `DateTime` with the current offset.
    /// See the [`format::strftime` module](../../format/strftime/index.html)
    /// on the supported escape sequences.
    ///
    /// If the format does not include offsets, the current offset is assumed;
    /// otherwise the input should have a matching UTC offset.
    ///
    /// See also `DateTime::parse_from_str` which gives a local `DateTime`
    /// with parsed `FixedOffset`.
    fn datetime_from_str(&self, s: &str, fmt: &str) -> ParseResult<DateTime<Self>> {
        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, s, StrftimeItems::new(fmt)));
        parsed.to_datetime_with_timezone(self)
    }

    /// Reconstructs the time zone from the offset.
    fn from_offset(offset: &Self::Offset) -> Self;

    /// Creates the offset(s) for given local `NaiveDate` if possible.
    fn offset_from_local_date(&self, local: &NaiveDate) -> LocalResult<Self::Offset>;

    /// Creates the offset(s) for given local `NaiveDateTime` if possible.
    fn offset_from_local_datetime(&self, local: &NaiveDateTime) -> LocalResult<Self::Offset>;

    /// Converts the local `NaiveDate` to the timezone-aware `Date` if possible.
    fn from_local_date(&self, local: &NaiveDate) -> LocalResult<Date<Self>> {
        self.offset_from_local_date(local).map(|offset| {
            Date::from_utc(*local - offset.local_minus_utc(), offset)
        })
    }

    /// Converts the local `NaiveDateTime` to the timezone-aware `DateTime` if possible.
    fn from_local_datetime(&self, local: &NaiveDateTime) -> LocalResult<DateTime<Self>> {
        self.offset_from_local_datetime(local).map(|offset| {
            DateTime::from_utc(*local - offset.local_minus_utc(), offset)
        })
    }

    /// Creates the offset for given UTC `NaiveDate`. This cannot fail.
    fn offset_from_utc_date(&self, utc: &NaiveDate) -> Self::Offset;

    /// Creates the offset for given UTC `NaiveDateTime`. This cannot fail.
    fn offset_from_utc_datetime(&self, utc: &NaiveDateTime) -> Self::Offset;

    /// Converts the UTC `NaiveDate` to the local time.
    /// The UTC is continuous and thus this cannot fail (but can give the duplicate local time).
    fn from_utc_date(&self, utc: &NaiveDate) -> Date<Self> {
        Date::from_utc(utc.clone(), self.offset_from_utc_date(utc))
    }

    /// Converts the UTC `NaiveDateTime` to the local time.
    /// The UTC is continuous and thus this cannot fail (but can give the duplicate local time).
    fn from_utc_datetime(&self, utc: &NaiveDateTime) -> DateTime<Self> {
        DateTime::from_utc(utc.clone(), self.offset_from_utc_datetime(utc))
    }
}

pub mod utc;
pub mod fixed;
pub mod local;

