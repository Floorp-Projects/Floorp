// This is a part of rust-chrono.
// Copyright (c) 2014-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! ISO 8601 date and time without timezone.

use std::{str, fmt, hash};
use std::ops::{Add, Sub};
use num::traits::ToPrimitive;

use {Weekday, Timelike, Datelike};
use div::div_mod_floor;
use duration::Duration;
use naive::time::NaiveTime;
use naive::date::NaiveDate;
use format::{Item, Numeric, Pad, Fixed};
use format::{parse, Parsed, ParseError, ParseResult, DelayedFormat, StrftimeItems};

/// ISO 8601 combined date and time without timezone.
#[derive(PartialEq, Eq, PartialOrd, Ord, Copy, Clone)]
pub struct NaiveDateTime {
    date: NaiveDate,
    time: NaiveTime,
}

impl NaiveDateTime {
    /// Makes a new `NaiveDateTime` from date and time components.
    /// Equivalent to `date.and_time(time)` and many other helper constructors on `NaiveDate`.
    #[inline]
    pub fn new(date: NaiveDate, time: NaiveTime) -> NaiveDateTime {
        NaiveDateTime { date: date, time: time }
    }

    /// Makes a new `NaiveDateTime` from the number of non-leap seconds
    /// since the midnight UTC on January 1, 1970 (aka "UNIX timestamp")
    /// and the number of nanoseconds since the last whole non-leap second.
    ///
    /// Panics on the out-of-range number of seconds and/or invalid nanosecond.
    #[inline]
    pub fn from_timestamp(secs: i64, nsecs: u32) -> NaiveDateTime {
        let datetime = NaiveDateTime::from_timestamp_opt(secs, nsecs);
        datetime.expect("invalid or out-of-range datetime")
    }

    /// Makes a new `NaiveDateTime` from the number of non-leap seconds
    /// since the midnight UTC on January 1, 1970 (aka "UNIX timestamp")
    /// and the number of nanoseconds since the last whole non-leap second.
    ///
    /// Returns `None` on the out-of-range number of seconds and/or invalid nanosecond.
    #[inline]
    pub fn from_timestamp_opt(secs: i64, nsecs: u32) -> Option<NaiveDateTime> {
        let (days, secs) = div_mod_floor(secs, 86400);
        let date = days.to_i32().and_then(|days| days.checked_add(719163))
                                .and_then(|days_ce| NaiveDate::from_num_days_from_ce_opt(days_ce));
        let time = NaiveTime::from_num_seconds_from_midnight_opt(secs as u32, nsecs);
        match (date, time) {
            (Some(date), Some(time)) => Some(NaiveDateTime { date: date, time: time }),
            (_, _) => None,
        }
    }

    /// *Deprecated:* Same to [`NaiveDateTime::from_timestamp`](#method.from_timestamp).
    #[inline]
    pub fn from_num_seconds_from_unix_epoch(secs: i64, nsecs: u32) -> NaiveDateTime {
        NaiveDateTime::from_timestamp(secs, nsecs)
    }

    /// *Deprecated:* Same to [`NaiveDateTime::from_timestamp_opt`](#method.from_timestamp_opt).
    #[inline]
    pub fn from_num_seconds_from_unix_epoch_opt(secs: i64, nsecs: u32) -> Option<NaiveDateTime> {
        NaiveDateTime::from_timestamp_opt(secs, nsecs)
    }

    /// Parses a string with the specified format string and returns a new `NaiveDateTime`.
    /// See the [`format::strftime` module](../../format/strftime/index.html)
    /// on the supported escape sequences.
    pub fn parse_from_str(s: &str, fmt: &str) -> ParseResult<NaiveDateTime> {
        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, s, StrftimeItems::new(fmt)));
        parsed.to_naive_datetime_with_offset(0) // no offset adjustment
    }

    /// Retrieves a date component.
    #[inline]
    pub fn date(&self) -> NaiveDate {
        self.date
    }

    /// Retrieves a time component.
    #[inline]
    pub fn time(&self) -> NaiveTime {
        self.time
    }

    /// Returns the number of non-leap seconds since the midnight on January 1, 1970.
    ///
    /// Note that this does *not* account for the timezone!
    /// The true "UNIX timestamp" would count seconds since the midnight *UTC* on the epoch.
    #[inline]
    pub fn timestamp(&self) -> i64 {
        let ndays = self.date.num_days_from_ce() as i64;
        let nseconds = self.time.num_seconds_from_midnight() as i64;
        (ndays - 719163) * 86400 + nseconds
    }

    /// Returns the number of milliseconds since the last whole non-leap second.
    ///
    /// The return value ranges from 0 to 999,
    /// or for [leap seconds](../time/index.html#leap-second-handling), to 1,999.
    #[inline]
    pub fn timestamp_subsec_millis(&self) -> u32 {
        self.timestamp_subsec_nanos() / 1_000_000
    }

    /// Returns the number of microseconds since the last whole non-leap second.
    ///
    /// The return value ranges from 0 to 999,999,
    /// or for [leap seconds](../time/index.html#leap-second-handling), to 1,999,999.
    #[inline]
    pub fn timestamp_subsec_micros(&self) -> u32 {
        self.timestamp_subsec_nanos() / 1_000
    }

    /// Returns the number of nanoseconds since the last whole non-leap second.
    ///
    /// The return value ranges from 0 to 999,999,999,
    /// or for [leap seconds](../time/index.html#leap-second-handling), to 1,999,999,999.
    #[inline]
    pub fn timestamp_subsec_nanos(&self) -> u32 {
        self.time.nanosecond()
    }

    /// *Deprecated:* Same to [`NaiveDateTime::timestamp`](#method.timestamp).
    #[inline]
    pub fn num_seconds_from_unix_epoch(&self) -> i64 {
        self.timestamp()
    }

    /// Adds given `Duration` to the current date and time.
    ///
    /// Returns `None` when it will result in overflow.
    pub fn checked_add(self, rhs: Duration) -> Option<NaiveDateTime> {
        // Duration does not directly give its parts, so we need some additional calculations.
        let days = rhs.num_days();
        let nanos = (rhs - Duration::days(days)).num_nanoseconds().unwrap();
        debug_assert!(Duration::days(days) + Duration::nanoseconds(nanos) == rhs);
        debug_assert!(-86400_000_000_000 < nanos && nanos < 86400_000_000_000);

        let mut date = try_opt!(self.date.checked_add(Duration::days(days)));
        let time = self.time + Duration::nanoseconds(nanos);

        // time always wraps around, but date needs to be adjusted for overflow.
        if nanos < 0 && time > self.time {
            date = try_opt!(date.pred_opt());
        } else if nanos > 0 && time < self.time {
            date = try_opt!(date.succ_opt());
        }
        Some(NaiveDateTime { date: date, time: time })
    }

    /// Subtracts given `Duration` from the current date and time.
    ///
    /// Returns `None` when it will result in overflow.
    pub fn checked_sub(self, rhs: Duration) -> Option<NaiveDateTime> {
        // Duration does not directly give its parts, so we need some additional calculations.
        let days = rhs.num_days();
        let nanos = (rhs - Duration::days(days)).num_nanoseconds().unwrap();
        debug_assert!(Duration::days(days) + Duration::nanoseconds(nanos) == rhs);
        debug_assert!(-86400_000_000_000 < nanos && nanos < 86400_000_000_000);

        let mut date = try_opt!(self.date.checked_sub(Duration::days(days)));
        let time = self.time - Duration::nanoseconds(nanos);

        // time always wraps around, but date needs to be adjusted for overflow.
        if nanos > 0 && time > self.time {
            date = try_opt!(date.pred_opt());
        } else if nanos < 0 && time < self.time {
            date = try_opt!(date.succ_opt());
        }
        Some(NaiveDateTime { date: date, time: time })
    }

    /// Formats the combined date and time with the specified formatting items.
    #[inline]
    pub fn format_with_items<'a, I>(&self, items: I) -> DelayedFormat<I>
            where I: Iterator<Item=Item<'a>> + Clone {
        DelayedFormat::new(Some(self.date.clone()), Some(self.time.clone()), items)
    }

    /// Formats the combined date and time with the specified format string.
    /// See the [`format::strftime` module](../../format/strftime/index.html)
    /// on the supported escape sequences.
    #[inline]
    pub fn format<'a>(&self, fmt: &'a str) -> DelayedFormat<StrftimeItems<'a>> {
        self.format_with_items(StrftimeItems::new(fmt))
    }
}

impl Datelike for NaiveDateTime {
    /// Returns the year number in the [calendar date](./index.html#calendar-date).
    ///
    /// See also the [`NaiveDate::year`](../date/struct.NaiveDate.html#method.year) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.year(), 2015);
    /// ~~~~
    #[inline]
    fn year(&self) -> i32 {
        self.date.year()
    }

    /// Returns the month number starting from 1.
    ///
    /// The return value ranges from 1 to 12.
    ///
    /// See also the [`NaiveDate::month`](../date/struct.NaiveDate.html#method.month) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.month(), 9);
    /// ~~~~
    #[inline]
    fn month(&self) -> u32 {
        self.date.month()
    }

    /// Returns the month number starting from 0.
    ///
    /// The return value ranges from 0 to 11.
    ///
    /// See also the [`NaiveDate::month0`](../date/struct.NaiveDate.html#method.month0) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.month0(), 8);
    /// ~~~~
    #[inline]
    fn month0(&self) -> u32 {
        self.date.month0()
    }

    /// Returns the day of month starting from 1.
    ///
    /// The return value ranges from 1 to 31. (The last day of month differs by months.)
    ///
    /// See also the [`NaiveDate::day`](../date/struct.NaiveDate.html#method.day) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.day(), 25);
    /// ~~~~
    #[inline]
    fn day(&self) -> u32 {
        self.date.day()
    }

    /// Returns the day of month starting from 0.
    ///
    /// The return value ranges from 0 to 30. (The last day of month differs by months.)
    ///
    /// See also the [`NaiveDate::day0`](../date/struct.NaiveDate.html#method.day0) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.day0(), 24);
    /// ~~~~
    #[inline]
    fn day0(&self) -> u32 {
        self.date.day0()
    }

    /// Returns the day of year starting from 1.
    ///
    /// The return value ranges from 1 to 366. (The last day of year differs by years.)
    ///
    /// See also the [`NaiveDate::ordinal`](../date/struct.NaiveDate.html#method.ordinal) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.ordinal(), 268);
    /// ~~~~
    #[inline]
    fn ordinal(&self) -> u32 {
        self.date.ordinal()
    }

    /// Returns the day of year starting from 0.
    ///
    /// The return value ranges from 0 to 365. (The last day of year differs by years.)
    ///
    /// See also the [`NaiveDate::ordinal0`](../date/struct.NaiveDate.html#method.ordinal0) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.ordinal0(), 267);
    /// ~~~~
    #[inline]
    fn ordinal0(&self) -> u32 {
        self.date.ordinal0()
    }

    /// Returns the day of week.
    ///
    /// See also the [`NaiveDate::weekday`](../date/struct.NaiveDate.html#method.weekday) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike, Weekday};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.weekday(), Weekday::Fri);
    /// ~~~~
    #[inline]
    fn weekday(&self) -> Weekday {
        self.date.weekday()
    }

    #[inline]
    fn isoweekdate(&self) -> (i32, u32, Weekday) {
        self.date.isoweekdate()
    }

    /// Makes a new `NaiveDateTime` with the year number changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveDate::with_year`](../date/struct.NaiveDate.html#method.with_year) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 25).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_year(2016), Some(NaiveDate::from_ymd(2016, 9, 25).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_year(-308), Some(NaiveDate::from_ymd(-308, 9, 25).and_hms(12, 34, 56)));
    /// ~~~~
    #[inline]
    fn with_year(&self, year: i32) -> Option<NaiveDateTime> {
        self.date.with_year(year).map(|d| NaiveDateTime { date: d, ..*self })
    }

    /// Makes a new `NaiveDateTime` with the month number (starting from 1) changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveDate::with_month`](../date/struct.NaiveDate.html#method.with_month) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 30).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_month(10), Some(NaiveDate::from_ymd(2015, 10, 30).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_month(13), None); // no month 13
    /// assert_eq!(dt.with_month(2), None); // no February 30
    /// ~~~~
    #[inline]
    fn with_month(&self, month: u32) -> Option<NaiveDateTime> {
        self.date.with_month(month).map(|d| NaiveDateTime { date: d, ..*self })
    }

    /// Makes a new `NaiveDateTime` with the month number (starting from 0) changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveDate::with_month0`](../date/struct.NaiveDate.html#method.with_month0) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 30).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_month0(9), Some(NaiveDate::from_ymd(2015, 10, 30).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_month0(12), None); // no month 13
    /// assert_eq!(dt.with_month0(1), None); // no February 30
    /// ~~~~
    #[inline]
    fn with_month0(&self, month0: u32) -> Option<NaiveDateTime> {
        self.date.with_month0(month0).map(|d| NaiveDateTime { date: d, ..*self })
    }

    /// Makes a new `NaiveDateTime` with the day of month (starting from 1) changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveDate::with_day`](../date/struct.NaiveDate.html#method.with_day) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_day(30), Some(NaiveDate::from_ymd(2015, 9, 30).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_day(31), None); // no September 31
    /// ~~~~
    #[inline]
    fn with_day(&self, day: u32) -> Option<NaiveDateTime> {
        self.date.with_day(day).map(|d| NaiveDateTime { date: d, ..*self })
    }

    /// Makes a new `NaiveDateTime` with the day of month (starting from 0) changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveDate::with_day0`](../date/struct.NaiveDate.html#method.with_day0) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_day0(29), Some(NaiveDate::from_ymd(2015, 9, 30).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_day0(30), None); // no September 31
    /// ~~~~
    #[inline]
    fn with_day0(&self, day0: u32) -> Option<NaiveDateTime> {
        self.date.with_day0(day0).map(|d| NaiveDateTime { date: d, ..*self })
    }

    /// Makes a new `NaiveDateTime` with the day of year (starting from 1) changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveDate::with_ordinal`](../date/struct.NaiveDate.html#method.with_ordinal) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_ordinal(60),
    ///            Some(NaiveDate::from_ymd(2015, 3, 1).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_ordinal(366), None); // 2015 had only 365 days
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2016, 9, 8).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_ordinal(60),
    ///            Some(NaiveDate::from_ymd(2016, 2, 29).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_ordinal(366),
    ///            Some(NaiveDate::from_ymd(2016, 12, 31).and_hms(12, 34, 56)));
    /// ~~~~
    #[inline]
    fn with_ordinal(&self, ordinal: u32) -> Option<NaiveDateTime> {
        self.date.with_ordinal(ordinal).map(|d| NaiveDateTime { date: d, ..*self })
    }

    /// Makes a new `NaiveDateTime` with the day of year (starting from 0) changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveDate::with_ordinal0`](../date/struct.NaiveDate.html#method.with_ordinal0) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Datelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_ordinal0(59),
    ///            Some(NaiveDate::from_ymd(2015, 3, 1).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_ordinal0(365), None); // 2015 had only 365 days
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2016, 9, 8).and_hms(12, 34, 56);
    /// assert_eq!(dt.with_ordinal0(59),
    ///            Some(NaiveDate::from_ymd(2016, 2, 29).and_hms(12, 34, 56)));
    /// assert_eq!(dt.with_ordinal0(365),
    ///            Some(NaiveDate::from_ymd(2016, 12, 31).and_hms(12, 34, 56)));
    /// ~~~~
    #[inline]
    fn with_ordinal0(&self, ordinal0: u32) -> Option<NaiveDateTime> {
        self.date.with_ordinal0(ordinal0).map(|d| NaiveDateTime { date: d, ..*self })
    }
}

impl Timelike for NaiveDateTime {
    /// Returns the hour number from 0 to 23.
    ///
    /// See also the [`NaiveTime::hour`](../time/struct.NaiveTime.html#method.hour) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Timelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 56, 789);
    /// assert_eq!(dt.hour(), 12);
    /// ~~~~
    #[inline]
    fn hour(&self) -> u32 {
        self.time.hour()
    }

    /// Returns the minute number from 0 to 59.
    ///
    /// See also the [`NaiveTime::minute`](../time/struct.NaiveTime.html#method.minute) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Timelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 56, 789);
    /// assert_eq!(dt.minute(), 34);
    /// ~~~~
    #[inline]
    fn minute(&self) -> u32 {
        self.time.minute()
    }

    /// Returns the second number from 0 to 59.
    ///
    /// See also the [`NaiveTime::second`](../time/struct.NaiveTime.html#method.second) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Timelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 56, 789);
    /// assert_eq!(dt.second(), 56);
    /// ~~~~
    #[inline]
    fn second(&self) -> u32 {
        self.time.second()
    }

    /// Returns the number of nanoseconds since the whole non-leap second.
    /// The range from 1,000,000,000 to 1,999,999,999 represents
    /// the [leap second](./naive/time/index.html#leap-second-handling).
    ///
    /// See also the
    /// [`NaiveTime::nanosecond`](../time/struct.NaiveTime.html#method.nanosecond) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Timelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 56, 789);
    /// assert_eq!(dt.nanosecond(), 789_000_000);
    /// ~~~~
    #[inline]
    fn nanosecond(&self) -> u32 {
        self.time.nanosecond()
    }

    /// Makes a new `NaiveDateTime` with the hour number changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveTime::with_hour`](../time/struct.NaiveTime.html#method.with_hour) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Timelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 56, 789);
    /// assert_eq!(dt.with_hour(7),
    ///            Some(NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(7, 34, 56, 789)));
    /// assert_eq!(dt.with_hour(24), None);
    /// ~~~~
    #[inline]
    fn with_hour(&self, hour: u32) -> Option<NaiveDateTime> {
        self.time.with_hour(hour).map(|t| NaiveDateTime { time: t, ..*self })
    }

    /// Makes a new `NaiveDateTime` with the minute number changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    ///
    /// See also the
    /// [`NaiveTime::with_minute`](../time/struct.NaiveTime.html#method.with_minute) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Timelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 56, 789);
    /// assert_eq!(dt.with_minute(45),
    ///            Some(NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 45, 56, 789)));
    /// assert_eq!(dt.with_minute(60), None);
    /// ~~~~
    #[inline]
    fn with_minute(&self, min: u32) -> Option<NaiveDateTime> {
        self.time.with_minute(min).map(|t| NaiveDateTime { time: t, ..*self })
    }

    /// Makes a new `NaiveDateTime` with the second number changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    /// As with the [`second`](#method.second) method,
    /// the input range is restricted to 0 through 59.
    ///
    /// See also the
    /// [`NaiveTime::with_second`](../time/struct.NaiveTime.html#method.with_second) method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Timelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 56, 789);
    /// assert_eq!(dt.with_second(17),
    ///            Some(NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 17, 789)));
    /// assert_eq!(dt.with_second(60), None);
    /// ~~~~
    #[inline]
    fn with_second(&self, sec: u32) -> Option<NaiveDateTime> {
        self.time.with_second(sec).map(|t| NaiveDateTime { time: t, ..*self })
    }

    /// Makes a new `NaiveDateTime` with nanoseconds since the whole non-leap second changed.
    ///
    /// Returns `None` when the resulting `NaiveDateTime` would be invalid.
    /// As with the [`nanosecond`](#method.nanosecond) method,
    /// the input range can exceed 1,000,000,000 for leap seconds.
    ///
    /// See also the
    /// [`NaiveTime::with_nanosecond`](../time/struct.NaiveTime.html#method.with_nanosecond)
    /// method.
    ///
    /// # Example
    ///
    /// ~~~~
    /// use chrono::{NaiveDate, NaiveDateTime, Timelike};
    ///
    /// let dt: NaiveDateTime = NaiveDate::from_ymd(2015, 9, 8).and_hms_milli(12, 34, 56, 789);
    /// assert_eq!(dt.with_nanosecond(333_333_333),
    ///            Some(NaiveDate::from_ymd(2015, 9, 8).and_hms_nano(12, 34, 56, 333_333_333)));
    /// assert_eq!(dt.with_nanosecond(1_333_333_333), // leap second
    ///            Some(NaiveDate::from_ymd(2015, 9, 8).and_hms_nano(12, 34, 56, 1_333_333_333)));
    /// assert_eq!(dt.with_nanosecond(2_000_000_000), None);
    /// ~~~~
    #[inline]
    fn with_nanosecond(&self, nano: u32) -> Option<NaiveDateTime> {
        self.time.with_nanosecond(nano).map(|t| NaiveDateTime { time: t, ..*self })
    }
}

/// `NaiveDateTime` can be used as a key to the hash maps (in principle).
///
/// Practically this also takes account of fractional seconds, so it is not recommended.
/// (For the obvious reason this also distinguishes leap seconds from non-leap seconds.)
impl hash::Hash for NaiveDateTime {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.date.hash(state);
        self.time.hash(state);
    }
}

impl Add<Duration> for NaiveDateTime {
    type Output = NaiveDateTime;

    #[inline]
    fn add(self, rhs: Duration) -> NaiveDateTime {
        self.checked_add(rhs).expect("`NaiveDateTime + Duration` overflowed")
    }
}

impl Sub<NaiveDateTime> for NaiveDateTime {
    type Output = Duration;

    fn sub(self, rhs: NaiveDateTime) -> Duration {
        (self.date - rhs.date) + (self.time - rhs.time)
    }
}

impl Sub<Duration> for NaiveDateTime {
    type Output = NaiveDateTime;

    #[inline]
    fn sub(self, rhs: Duration) -> NaiveDateTime {
        self.checked_sub(rhs).expect("`NaiveDateTime - Duration` overflowed")
    }
}

impl fmt::Debug for NaiveDateTime {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}T{:?}", self.date, self.time)
    }
}

impl fmt::Display for NaiveDateTime {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} {}", self.date, self.time)
    }
}

impl str::FromStr for NaiveDateTime {
    type Err = ParseError;

    fn from_str(s: &str) -> ParseResult<NaiveDateTime> {
        const ITEMS: &'static [Item<'static>] = &[
            Item::Space(""), Item::Numeric(Numeric::Year, Pad::Zero),
            Item::Space(""), Item::Literal("-"),
            Item::Space(""), Item::Numeric(Numeric::Month, Pad::Zero),
            Item::Space(""), Item::Literal("-"),
            Item::Space(""), Item::Numeric(Numeric::Day, Pad::Zero),
            Item::Space(""), Item::Literal("T"), // XXX shouldn't this be case-insensitive?
            Item::Space(""), Item::Numeric(Numeric::Hour, Pad::Zero),
            Item::Space(""), Item::Literal(":"),
            Item::Space(""), Item::Numeric(Numeric::Minute, Pad::Zero),
            Item::Space(""), Item::Literal(":"),
            Item::Space(""), Item::Numeric(Numeric::Second, Pad::Zero),
            Item::Fixed(Fixed::Nanosecond), Item::Space(""),
        ];

        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, s, ITEMS.iter().cloned()));
        parsed.to_naive_datetime_with_offset(0)
    }
}

#[cfg(feature = "rustc-serialize")]
mod rustc_serialize {
    use super::NaiveDateTime;
    use rustc_serialize::{Encodable, Encoder, Decodable, Decoder};

    // TODO the current serialization format is NEVER intentionally defined.
    // in the future it is likely to be redefined to more sane and reasonable format.

    impl Encodable for NaiveDateTime {
        fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
            s.emit_struct("NaiveDateTime", 2, |s| {
                try!(s.emit_struct_field("date", 0, |s| self.date.encode(s)));
                try!(s.emit_struct_field("time", 1, |s| self.time.encode(s)));
                Ok(())
            })
        }
    }

    impl Decodable for NaiveDateTime {
        fn decode<D: Decoder>(d: &mut D) -> Result<NaiveDateTime, D::Error> {
            d.read_struct("NaiveDateTime", 2, |d| {
                let date = try!(d.read_struct_field("date", 0, Decodable::decode));
                let time = try!(d.read_struct_field("time", 1, Decodable::decode));
                Ok(NaiveDateTime::new(date, time))
            })
        }
    }

    #[test]
    fn test_encodable() {
        use naive::date::{self, NaiveDate};
        use rustc_serialize::json::encode;

        assert_eq!(
            encode(&NaiveDate::from_ymd(2016, 7, 8).and_hms_milli(9, 10, 48, 90)).ok(),
            Some(r#"{"date":{"ymdf":16518115},"time":{"secs":33048,"frac":90000000}}"#.into()));
        assert_eq!(
            encode(&NaiveDate::from_ymd(2014, 7, 24).and_hms(12, 34, 6)).ok(),
            Some(r#"{"date":{"ymdf":16501977},"time":{"secs":45246,"frac":0}}"#.into()));
        assert_eq!(
            encode(&NaiveDate::from_ymd(0, 1, 1).and_hms_milli(0, 0, 59, 1_000)).ok(),
            Some(r#"{"date":{"ymdf":20},"time":{"secs":59,"frac":1000000000}}"#.into()));
        assert_eq!(
            encode(&NaiveDate::from_ymd(-1, 12, 31).and_hms_nano(23, 59, 59, 7)).ok(),
            Some(r#"{"date":{"ymdf":-2341},"time":{"secs":86399,"frac":7}}"#.into()));
        assert_eq!(
            encode(&date::MIN.and_hms(0, 0, 0)).ok(),
            Some(r#"{"date":{"ymdf":-2147483625},"time":{"secs":0,"frac":0}}"#.into()));
        assert_eq!(
            encode(&date::MAX.and_hms_nano(23, 59, 59, 1_999_999_999)).ok(),
            Some(r#"{"date":{"ymdf":2147481311},"time":{"secs":86399,"frac":1999999999}}"#.into()));
    }

    #[test]
    fn test_decodable() {
        use naive::date::{self, NaiveDate};
        use rustc_serialize::json;

        let decode = |s: &str| json::decode::<NaiveDateTime>(s);

        assert_eq!(
            decode(r#"{"date":{"ymdf":16518115},"time":{"secs":33048,"frac":90000000}}"#).ok(),
            Some(NaiveDate::from_ymd(2016, 7, 8).and_hms_milli(9, 10, 48, 90)));
        assert_eq!(
            decode(r#"{"time":{"frac":0,"secs":45246},"date":{"ymdf":16501977}}"#).ok(),
            Some(NaiveDate::from_ymd(2014, 7, 24).and_hms(12, 34, 6)));
        assert_eq!(
            decode(r#"{"date": {"ymdf": 20},
                       "time": {"secs": 59,
                                "frac": 1000000000}}"#).ok(),
            Some(NaiveDate::from_ymd(0, 1, 1).and_hms_milli(0, 0, 59, 1_000)));
        assert_eq!(
            decode(r#"{"date":{"ymdf":-2341},"time":{"secs":86399,"frac":7}}"#).ok(),
            Some(NaiveDate::from_ymd(-1, 12, 31).and_hms_nano(23, 59, 59, 7)));
        assert_eq!(
            decode(r#"{"date":{"ymdf":-2147483625},"time":{"secs":0,"frac":0}}"#).ok(),
            Some(date::MIN.and_hms(0, 0, 0)));
        assert_eq!(
            decode(r#"{"date":{"ymdf":2147481311},"time":{"secs":86399,"frac":1999999999}}"#).ok(),
            Some(date::MAX.and_hms_nano(23, 59, 59, 1_999_999_999)));

        // bad formats
        assert!(decode(r#"{"date":{},"time":{}}"#).is_err());
        assert!(decode(r#"{"date":{"ymdf":0},"time":{"secs":0,"frac":0}}"#).is_err());
        assert!(decode(r#"{"date":{"ymdf":20},"time":{"secs":86400,"frac":0}}"#).is_err());
        assert!(decode(r#"{"date":{"ymdf":20},"time":{"secs":0,"frac":-1}}"#).is_err());
        assert!(decode(r#"{"date":20,"time":{"secs":0,"frac":0}}"#).is_err());
        assert!(decode(r#"{"date":"2016-08-04","time":"01:02:03.456"}"#).is_err());
        assert!(decode(r#"{"date":{"ymdf":20}}"#).is_err());
        assert!(decode(r#"{"time":{"secs":0,"frac":0}}"#).is_err());
        assert!(decode(r#"{"ymdf":20}"#).is_err());
        assert!(decode(r#"{"secs":0,"frac":0}"#).is_err());
        assert!(decode(r#"{}"#).is_err());
        assert!(decode(r#"0"#).is_err());
        assert!(decode(r#"-1"#).is_err());
        assert!(decode(r#""string""#).is_err());
        assert!(decode(r#""2016-08-04T12:34:56""#).is_err()); // :(
        assert!(decode(r#""2016-08-04T12:34:56.789""#).is_err()); // :(
        assert!(decode(r#"null"#).is_err());
    }
}

#[cfg(feature = "serde")]
mod serde {
    use super::NaiveDateTime;
    use serde::{ser, de};

    // TODO not very optimized for space (binary formats would want something better)

    impl ser::Serialize for NaiveDateTime {
        fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
            where S: ser::Serializer
        {
            serializer.serialize_str(&format!("{:?}", self))
        }
    }

    struct NaiveDateTimeVisitor;

    impl de::Visitor for NaiveDateTimeVisitor {
        type Value = NaiveDateTime;

        fn visit_str<E>(&mut self, value: &str) -> Result<NaiveDateTime, E>
            where E: de::Error
        {
            value.parse().map_err(|err| E::custom(format!("{}", err)))
        }
    }

    impl de::Deserialize for NaiveDateTime {
        fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
            where D: de::Deserializer
        {
            deserializer.deserialize(NaiveDateTimeVisitor)
        }
    }

    #[cfg(test)] extern crate serde_json;

    #[test]
    fn test_serde_serialize() {
        use naive::date::{self, NaiveDate};
        use self::serde_json::to_string;

        assert_eq!(
            to_string(&NaiveDate::from_ymd(2016, 7, 8).and_hms_milli(9, 10, 48, 90)).ok(),
            Some(r#""2016-07-08T09:10:48.090""#.into()));
        assert_eq!(
            to_string(&NaiveDate::from_ymd(2014, 7, 24).and_hms(12, 34, 6)).ok(),
            Some(r#""2014-07-24T12:34:06""#.into()));
        assert_eq!(
            to_string(&NaiveDate::from_ymd(0, 1, 1).and_hms_milli(0, 0, 59, 1_000)).ok(),
            Some(r#""0000-01-01T00:00:60""#.into()));
        assert_eq!(
            to_string(&NaiveDate::from_ymd(-1, 12, 31).and_hms_nano(23, 59, 59, 7)).ok(),
            Some(r#""-0001-12-31T23:59:59.000000007""#.into()));
        assert_eq!(
            to_string(&date::MIN.and_hms(0, 0, 0)).ok(),
            Some(r#""-262144-01-01T00:00:00""#.into()));
        assert_eq!(
            to_string(&date::MAX.and_hms_nano(23, 59, 59, 1_999_999_999)).ok(),
            Some(r#""+262143-12-31T23:59:60.999999999""#.into()));
    }

    #[test]
    fn test_serde_deserialize() {
        use naive::date::{self, NaiveDate};
        use self::serde_json::from_str;

        let from_str = |s: &str| serde_json::from_str::<NaiveDateTime>(s);

        assert_eq!(
            from_str(r#""2016-07-08T09:10:48.090""#).ok(),
            Some(NaiveDate::from_ymd(2016, 7, 8).and_hms_milli(9, 10, 48, 90)));
        assert_eq!(
            from_str(r#""2016-7-8T9:10:48.09""#).ok(),
            Some(NaiveDate::from_ymd(2016, 7, 8).and_hms_milli(9, 10, 48, 90)));
        assert_eq!(
            from_str(r#""2014-07-24T12:34:06""#).ok(),
            Some(NaiveDate::from_ymd(2014, 7, 24).and_hms(12, 34, 6)));
        assert_eq!(
            from_str(r#""0000-01-01T00:00:60""#).ok(),
            Some(NaiveDate::from_ymd(0, 1, 1).and_hms_milli(0, 0, 59, 1_000)));
        assert_eq!(
            from_str(r#""0-1-1T0:0:60""#).ok(),
            Some(NaiveDate::from_ymd(0, 1, 1).and_hms_milli(0, 0, 59, 1_000)));
        assert_eq!(
            from_str(r#""-0001-12-31T23:59:59.000000007""#).ok(),
            Some(NaiveDate::from_ymd(-1, 12, 31).and_hms_nano(23, 59, 59, 7)));
        assert_eq!(
            from_str(r#""-262144-01-01T00:00:00""#).ok(),
            Some(date::MIN.and_hms(0, 0, 0)));
        assert_eq!(
            from_str(r#""+262143-12-31T23:59:60.999999999""#).ok(),
            Some(date::MAX.and_hms_nano(23, 59, 59, 1_999_999_999)));
        assert_eq!(
            from_str(r#""+262143-12-31T23:59:60.9999999999997""#).ok(), // excess digits are ignored
            Some(date::MAX.and_hms_nano(23, 59, 59, 1_999_999_999)));

        // bad formats
        assert!(from_str(r#""""#).is_err());
        assert!(from_str(r#""2016-07-08""#).is_err());
        assert!(from_str(r#""09:10:48.090""#).is_err());
        assert!(from_str(r#""20160708T091048.090""#).is_err());
        assert!(from_str(r#""2000-00-00T00:00:00""#).is_err());
        assert!(from_str(r#""2000-02-30T00:00:00""#).is_err());
        assert!(from_str(r#""2001-02-29T00:00:00""#).is_err());
        assert!(from_str(r#""2002-02-28T24:00:00""#).is_err());
        assert!(from_str(r#""2002-02-28T23:60:00""#).is_err());
        assert!(from_str(r#""2002-02-28T23:59:61""#).is_err());
        assert!(from_str(r#""2016-07-08T09:10:48,090""#).is_err());
        assert!(from_str(r#""2016-07-08 09:10:48.090""#).is_err());
        assert!(from_str(r#""2016-007-08T09:10:48.090""#).is_err());
        assert!(from_str(r#""yyyy-mm-ddThh:mm:ss.fffffffff""#).is_err());
        assert!(from_str(r#"0"#).is_err());
        assert!(from_str(r#"20160708000000"#).is_err());
        assert!(from_str(r#"{}"#).is_err());
        assert!(from_str(r#"{"date":{"ymdf":20},"time":{"secs":0,"frac":0}}"#).is_err()); // :(
        assert!(from_str(r#"null"#).is_err());
    }
}

#[cfg(test)]
mod tests {
    use super::NaiveDateTime;
    use Datelike;
    use duration::Duration;
    use naive::date as naive_date;
    use naive::date::NaiveDate;
    use std::i64;

    #[test]
    fn test_datetime_from_timestamp() {
        let from_timestamp = |secs| NaiveDateTime::from_timestamp_opt(secs, 0);
        let ymdhms = |y,m,d,h,n,s| NaiveDate::from_ymd(y,m,d).and_hms(h,n,s);
        assert_eq!(from_timestamp(-1), Some(ymdhms(1969, 12, 31, 23, 59, 59)));
        assert_eq!(from_timestamp(0), Some(ymdhms(1970, 1, 1, 0, 0, 0)));
        assert_eq!(from_timestamp(1), Some(ymdhms(1970, 1, 1, 0, 0, 1)));
        assert_eq!(from_timestamp(1_000_000_000), Some(ymdhms(2001, 9, 9, 1, 46, 40)));
        assert_eq!(from_timestamp(0x7fffffff), Some(ymdhms(2038, 1, 19, 3, 14, 7)));
        assert_eq!(from_timestamp(i64::MIN), None);
        assert_eq!(from_timestamp(i64::MAX), None);
    }

    #[test]
    fn test_datetime_add() {
        fn check((y,m,d,h,n,s): (i32,u32,u32,u32,u32,u32), rhs: Duration,
                 result: Option<(i32,u32,u32,u32,u32,u32)>) {
            let lhs = NaiveDate::from_ymd(y, m, d).and_hms(h, n, s);
            let sum = result.map(|(y,m,d,h,n,s)| NaiveDate::from_ymd(y, m, d).and_hms(h, n, s));
            assert_eq!(lhs.checked_add(rhs), sum);
            assert_eq!(lhs.checked_sub(-rhs), sum);
        };

        check((2014,5,6, 7,8,9), Duration::seconds(3600 + 60 + 1), Some((2014,5,6, 8,9,10)));
        check((2014,5,6, 7,8,9), Duration::seconds(-(3600 + 60 + 1)), Some((2014,5,6, 6,7,8)));
        check((2014,5,6, 7,8,9), Duration::seconds(86399), Some((2014,5,7, 7,8,8)));
        check((2014,5,6, 7,8,9), Duration::seconds(86400 * 10), Some((2014,5,16, 7,8,9)));
        check((2014,5,6, 7,8,9), Duration::seconds(-86400 * 10), Some((2014,4,26, 7,8,9)));
        check((2014,5,6, 7,8,9), Duration::seconds(86400 * 10), Some((2014,5,16, 7,8,9)));

        // overflow check
        // assumes that we have correct values for MAX/MIN_DAYS_FROM_YEAR_0 from `naive::date`.
        // (they are private constants, but the equivalence is tested in that module.)
        let max_days_from_year_0 = naive_date::MAX - NaiveDate::from_ymd(0,1,1);
        check((0,1,1, 0,0,0), max_days_from_year_0, Some((naive_date::MAX.year(),12,31, 0,0,0)));
        check((0,1,1, 0,0,0), max_days_from_year_0 + Duration::seconds(86399),
              Some((naive_date::MAX.year(),12,31, 23,59,59)));
        check((0,1,1, 0,0,0), max_days_from_year_0 + Duration::seconds(86400), None);
        check((0,1,1, 0,0,0), Duration::max_value(), None);

        let min_days_from_year_0 = naive_date::MIN - NaiveDate::from_ymd(0,1,1);
        check((0,1,1, 0,0,0), min_days_from_year_0, Some((naive_date::MIN.year(),1,1, 0,0,0)));
        check((0,1,1, 0,0,0), min_days_from_year_0 - Duration::seconds(1), None);
        check((0,1,1, 0,0,0), Duration::min_value(), None);
    }

    #[test]
    fn test_datetime_sub() {
        let ymdhms = |y,m,d,h,n,s| NaiveDate::from_ymd(y,m,d).and_hms(h,n,s);
        assert_eq!(ymdhms(2014, 5, 6, 7, 8, 9) - ymdhms(2014, 5, 6, 7, 8, 9), Duration::zero());
        assert_eq!(ymdhms(2014, 5, 6, 7, 8, 10) - ymdhms(2014, 5, 6, 7, 8, 9),
                   Duration::seconds(1));
        assert_eq!(ymdhms(2014, 5, 6, 7, 8, 9) - ymdhms(2014, 5, 6, 7, 8, 10),
                   Duration::seconds(-1));
        assert_eq!(ymdhms(2014, 5, 7, 7, 8, 9) - ymdhms(2014, 5, 6, 7, 8, 10),
                   Duration::seconds(86399));
        assert_eq!(ymdhms(2001, 9, 9, 1, 46, 39) - ymdhms(1970, 1, 1, 0, 0, 0),
                   Duration::seconds(999_999_999));
    }

    #[test]
    fn test_datetime_timestamp() {
        let to_timestamp = |y,m,d,h,n,s| NaiveDate::from_ymd(y,m,d).and_hms(h,n,s).timestamp();
        assert_eq!(to_timestamp(1969, 12, 31, 23, 59, 59), -1);
        assert_eq!(to_timestamp(1970, 1, 1, 0, 0, 0), 0);
        assert_eq!(to_timestamp(1970, 1, 1, 0, 0, 1), 1);
        assert_eq!(to_timestamp(2001, 9, 9, 1, 46, 40), 1_000_000_000);
        assert_eq!(to_timestamp(2038, 1, 19, 3, 14, 7), 0x7fffffff);
    }

    #[test]
    fn test_datetime_from_str() {
        // valid cases
        let valid = [
            "2015-2-18T23:16:9.15",
            "-77-02-18T23:16:09",
            "  +82701  -  05  -  6  T  15  :  9  : 60.898989898989   ",
        ];
        for &s in &valid {
            let d = match s.parse::<NaiveDateTime>() {
                Ok(d) => d,
                Err(e) => panic!("parsing `{}` has failed: {}", s, e)
            };
            let s_ = format!("{:?}", d);
            // `s` and `s_` may differ, but `s.parse()` and `s_.parse()` must be same
            let d_ = match s_.parse::<NaiveDateTime>() {
                Ok(d) => d,
                Err(e) => panic!("`{}` is parsed into `{:?}`, but reparsing that has failed: {}",
                                 s, d, e)
            };
            assert!(d == d_, "`{}` is parsed into `{:?}`, but reparsed result \
                              `{:?}` does not match", s, d, d_);
        }

        // some invalid cases
        // since `ParseErrorKind` is private, all we can do is to check if there was an error
        assert!("".parse::<NaiveDateTime>().is_err());
        assert!("x".parse::<NaiveDateTime>().is_err());
        assert!("15".parse::<NaiveDateTime>().is_err());
        assert!("15:8:9".parse::<NaiveDateTime>().is_err());
        assert!("15-8-9".parse::<NaiveDateTime>().is_err());
        assert!("2015-15-15T15:15:15".parse::<NaiveDateTime>().is_err());
        assert!("2012-12-12T12:12:12x".parse::<NaiveDateTime>().is_err());
        assert!("2012-123-12T12:12:12".parse::<NaiveDateTime>().is_err());
        assert!("+ 82701-123-12T12:12:12".parse::<NaiveDateTime>().is_err());
        assert!("+802701-123-12T12:12:12".parse::<NaiveDateTime>().is_err()); // out-of-bound
    }

    #[test]
    fn test_datetime_parse_from_str() {
        let ymdhms = |y,m,d,h,n,s| NaiveDate::from_ymd(y,m,d).and_hms(h,n,s);
        assert_eq!(NaiveDateTime::parse_from_str("2014-5-7T12:34:56+09:30", "%Y-%m-%dT%H:%M:%S%z"),
                   Ok(ymdhms(2014, 5, 7, 12, 34, 56))); // ignore offset
        assert_eq!(NaiveDateTime::parse_from_str("2015-W06-1 000000", "%G-W%V-%u%H%M%S"),
                   Ok(ymdhms(2015, 2, 2, 0, 0, 0)));
        assert_eq!(NaiveDateTime::parse_from_str("Fri, 09 Aug 2013 23:54:35 GMT",
                                                 "%a, %d %b %Y %H:%M:%S GMT"),
                   Ok(ymdhms(2013, 8, 9, 23, 54, 35)));
        assert!(NaiveDateTime::parse_from_str("Sat, 09 Aug 2013 23:54:35 GMT",
                                              "%a, %d %b %Y %H:%M:%S GMT").is_err());
        assert!(NaiveDateTime::parse_from_str("2014-5-7 12:3456", "%Y-%m-%d %H:%M:%S").is_err());
        assert!(NaiveDateTime::parse_from_str("12:34:56", "%H:%M:%S").is_err()); // insufficient
    }

    #[test]
    fn test_datetime_format() {
        let dt = NaiveDate::from_ymd(2010, 9, 8).and_hms_milli(7, 6, 54, 321);
        assert_eq!(dt.format("%c").to_string(), "Wed Sep  8 07:06:54 2010");
        assert_eq!(dt.format("%s").to_string(), "1283929614");
        assert_eq!(dt.format("%t%n%%%n%t").to_string(), "\t\n%\n\t");

        // a horror of leap second: coming near to you.
        let dt = NaiveDate::from_ymd(2012, 6, 30).and_hms_milli(23, 59, 59, 1_000);
        assert_eq!(dt.format("%c").to_string(), "Sat Jun 30 23:59:60 2012");
        assert_eq!(dt.format("%s").to_string(), "1341100799"); // not 1341100800, it's intentional.
    }

    #[test]
    fn test_datetime_add_sub_invariant() { // issue #37
        let base = NaiveDate::from_ymd(2000, 1, 1).and_hms(0, 0, 0);
        let t = -946684799990000;
        let time = base + Duration::microseconds(t);
        assert_eq!(t, (time - base).num_microseconds().unwrap());
    }
}
