// This is a part of rust-chrono.
// Copyright (c) 2014-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

/*!
 * ISO 8601 calendar date with time zone.
 */

use std::{fmt, hash};
use std::cmp::Ordering;
use std::ops::{Add, Sub};

use {Weekday, Datelike};
use duration::Duration;
use offset::{TimeZone, Offset};
use offset::utc::UTC;
use naive;
use naive::date::NaiveDate;
use naive::time::NaiveTime;
use datetime::DateTime;
use format::{Item, DelayedFormat, StrftimeItems};

/// ISO 8601 calendar date with time zone.
///
/// This type should be considered ambiguous at best,
/// due to the inherent lack of precision required for the time zone resolution.
/// There are some guarantees on the usage of `Date<Tz>`:
///
/// - If properly constructed via `TimeZone::ymd` and others without an error,
///   the corresponding local date should exist for at least a moment.
///   (It may still have a gap from the offset changes.)
///
/// - The `TimeZone` is free to assign *any* `Offset` to the local date,
///   as long as that offset did occur in given day.
///   For example, if `2015-03-08T01:59-08:00` is followed by `2015-03-08T03:00-07:00`,
///   it may produce either `2015-03-08-08:00` or `2015-03-08-07:00`
///   but *not* `2015-03-08+00:00` and others.
///
/// - Once constructed as a full `DateTime`,
///   `DateTime::date` and other associated methods should return those for the original `Date`.
///   For example, if `dt = tz.ymd(y,m,d).hms(h,n,s)` were valid, `dt.date() == tz.ymd(y,m,d)`.
///
/// - The date is timezone-agnostic up to one day (i.e. practically always),
///   so the local date and UTC date should be equal for most cases
///   even though the raw calculation between `NaiveDate` and `Duration` may not.
#[derive(Clone)]
pub struct Date<Tz: TimeZone> {
    date: NaiveDate,
    offset: Tz::Offset,
}

/// The minimum possible `Date`.
pub const MIN: Date<UTC> = Date { date: naive::date::MIN, offset: UTC };
/// The maximum possible `Date`.
pub const MAX: Date<UTC> = Date { date: naive::date::MAX, offset: UTC };

impl<Tz: TimeZone> Date<Tz> {
    /// Makes a new `Date` with given *UTC* date and offset.
    /// The local date should be constructed via the `TimeZone` trait.
    //
    // note: this constructor is purposedly not named to `new` to discourage the direct usage.
    #[inline]
    pub fn from_utc(date: NaiveDate, offset: Tz::Offset) -> Date<Tz> {
        Date { date: date, offset: offset }
    }

    /// Makes a new `DateTime` from the current date and given `NaiveTime`.
    /// The offset in the current date is preserved.
    ///
    /// Panics on invalid datetime.
    #[inline]
    pub fn and_time(&self, time: NaiveTime) -> Option<DateTime<Tz>> {
        let localdt = self.naive_local().and_time(time);
        self.timezone().from_local_datetime(&localdt).single()
    }

    /// Makes a new `DateTime` from the current date, hour, minute and second.
    /// The offset in the current date is preserved.
    ///
    /// Panics on invalid hour, minute and/or second.
    #[inline]
    pub fn and_hms(&self, hour: u32, min: u32, sec: u32) -> DateTime<Tz> {
        self.and_hms_opt(hour, min, sec).expect("invalid time")
    }

    /// Makes a new `DateTime` from the current date, hour, minute and second.
    /// The offset in the current date is preserved.
    ///
    /// Returns `None` on invalid hour, minute and/or second.
    #[inline]
    pub fn and_hms_opt(&self, hour: u32, min: u32, sec: u32) -> Option<DateTime<Tz>> {
        NaiveTime::from_hms_opt(hour, min, sec).and_then(|time| self.and_time(time))
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and millisecond.
    /// The millisecond part can exceed 1,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Panics on invalid hour, minute, second and/or millisecond.
    #[inline]
    pub fn and_hms_milli(&self, hour: u32, min: u32, sec: u32, milli: u32) -> DateTime<Tz> {
        self.and_hms_milli_opt(hour, min, sec, milli).expect("invalid time")
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and millisecond.
    /// The millisecond part can exceed 1,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Returns `None` on invalid hour, minute, second and/or millisecond.
    #[inline]
    pub fn and_hms_milli_opt(&self, hour: u32, min: u32, sec: u32,
                             milli: u32) -> Option<DateTime<Tz>> {
        NaiveTime::from_hms_milli_opt(hour, min, sec, milli).and_then(|time| self.and_time(time))
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and microsecond.
    /// The microsecond part can exceed 1,000,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Panics on invalid hour, minute, second and/or microsecond.
    #[inline]
    pub fn and_hms_micro(&self, hour: u32, min: u32, sec: u32, micro: u32) -> DateTime<Tz> {
        self.and_hms_micro_opt(hour, min, sec, micro).expect("invalid time")
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and microsecond.
    /// The microsecond part can exceed 1,000,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Returns `None` on invalid hour, minute, second and/or microsecond.
    #[inline]
    pub fn and_hms_micro_opt(&self, hour: u32, min: u32, sec: u32,
                             micro: u32) -> Option<DateTime<Tz>> {
        NaiveTime::from_hms_micro_opt(hour, min, sec, micro).and_then(|time| self.and_time(time))
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and nanosecond.
    /// The nanosecond part can exceed 1,000,000,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Panics on invalid hour, minute, second and/or nanosecond.
    #[inline]
    pub fn and_hms_nano(&self, hour: u32, min: u32, sec: u32, nano: u32) -> DateTime<Tz> {
        self.and_hms_nano_opt(hour, min, sec, nano).expect("invalid time")
    }

    /// Makes a new `DateTime` from the current date, hour, minute, second and nanosecond.
    /// The nanosecond part can exceed 1,000,000,000 in order to represent the leap second.
    /// The offset in the current date is preserved.
    ///
    /// Returns `None` on invalid hour, minute, second and/or nanosecond.
    #[inline]
    pub fn and_hms_nano_opt(&self, hour: u32, min: u32, sec: u32,
                            nano: u32) -> Option<DateTime<Tz>> {
        NaiveTime::from_hms_nano_opt(hour, min, sec, nano).and_then(|time| self.and_time(time))
    }

    /// Makes a new `Date` for the next date.
    ///
    /// Panics when `self` is the last representable date.
    #[inline]
    pub fn succ(&self) -> Date<Tz> {
        self.succ_opt().expect("out of bound")
    }

    /// Makes a new `Date` for the next date.
    ///
    /// Returns `None` when `self` is the last representable date.
    #[inline]
    pub fn succ_opt(&self) -> Option<Date<Tz>> {
        self.date.succ_opt().map(|date| Date::from_utc(date, self.offset.clone()))
    }

    /// Makes a new `Date` for the prior date.
    ///
    /// Panics when `self` is the first representable date.
    #[inline]
    pub fn pred(&self) -> Date<Tz> {
        self.pred_opt().expect("out of bound")
    }

    /// Makes a new `Date` for the prior date.
    ///
    /// Returns `None` when `self` is the first representable date.
    #[inline]
    pub fn pred_opt(&self) -> Option<Date<Tz>> {
        self.date.pred_opt().map(|date| Date::from_utc(date, self.offset.clone()))
    }

    /// Retrieves an associated offset from UTC.
    #[inline]
    pub fn offset<'a>(&'a self) -> &'a Tz::Offset {
        &self.offset
    }

    /// Retrieves an associated time zone.
    #[inline]
    pub fn timezone(&self) -> Tz {
        TimeZone::from_offset(&self.offset)
    }

    /// Changes the associated time zone.
    /// This does not change the actual `Date` (but will change the string representation).
    #[inline]
    pub fn with_timezone<Tz2: TimeZone>(&self, tz: &Tz2) -> Date<Tz2> {
        tz.from_utc_date(&self.date)
    }

    /// Adds given `Duration` to the current date.
    ///
    /// Returns `None` when it will result in overflow.
    #[inline]
    pub fn checked_add(self, rhs: Duration) -> Option<Date<Tz>> {
        let date = try_opt!(self.date.checked_add(rhs));
        Some(Date { date: date, offset: self.offset })
    }

    /// Subtracts given `Duration` from the current date.
    ///
    /// Returns `None` when it will result in overflow.
    #[inline]
    pub fn checked_sub(self, rhs: Duration) -> Option<Date<Tz>> {
        let date = try_opt!(self.date.checked_sub(rhs));
        Some(Date { date: date, offset: self.offset })
    }

    /// Returns a view to the naive UTC date.
    #[inline]
    pub fn naive_utc(&self) -> NaiveDate {
        self.date
    }

    /// Returns a view to the naive local date.
    #[inline]
    pub fn naive_local(&self) -> NaiveDate {
        self.date + self.offset.local_minus_utc()
    }
}

/// Maps the local date to other date with given conversion function.
fn map_local<Tz: TimeZone, F>(d: &Date<Tz>, mut f: F) -> Option<Date<Tz>>
        where F: FnMut(NaiveDate) -> Option<NaiveDate> {
    f(d.naive_local()).and_then(|date| d.timezone().from_local_date(&date).single())
}

impl<Tz: TimeZone> Date<Tz> where Tz::Offset: fmt::Display {
    /// Formats the date with the specified formatting items.
    #[inline]
    pub fn format_with_items<'a, I>(&self, items: I) -> DelayedFormat<I>
            where I: Iterator<Item=Item<'a>> + Clone {
        DelayedFormat::new_with_offset(Some(self.naive_local()), None, &self.offset, items)
    }

    /// Formats the date with the specified format string.
    /// See the [`format::strftime` module](../format/strftime/index.html)
    /// on the supported escape sequences.
    #[inline]
    pub fn format<'a>(&self, fmt: &'a str) -> DelayedFormat<StrftimeItems<'a>> {
        self.format_with_items(StrftimeItems::new(fmt))
    }
}

impl<Tz: TimeZone> Datelike for Date<Tz> {
    #[inline] fn year(&self) -> i32 { self.naive_local().year() }
    #[inline] fn month(&self) -> u32 { self.naive_local().month() }
    #[inline] fn month0(&self) -> u32 { self.naive_local().month0() }
    #[inline] fn day(&self) -> u32 { self.naive_local().day() }
    #[inline] fn day0(&self) -> u32 { self.naive_local().day0() }
    #[inline] fn ordinal(&self) -> u32 { self.naive_local().ordinal() }
    #[inline] fn ordinal0(&self) -> u32 { self.naive_local().ordinal0() }
    #[inline] fn weekday(&self) -> Weekday { self.naive_local().weekday() }
    #[inline] fn isoweekdate(&self) -> (i32, u32, Weekday) { self.naive_local().isoweekdate() }

    #[inline]
    fn with_year(&self, year: i32) -> Option<Date<Tz>> {
        map_local(self, |date| date.with_year(year))
    }

    #[inline]
    fn with_month(&self, month: u32) -> Option<Date<Tz>> {
        map_local(self, |date| date.with_month(month))
    }

    #[inline]
    fn with_month0(&self, month0: u32) -> Option<Date<Tz>> {
        map_local(self, |date| date.with_month0(month0))
    }

    #[inline]
    fn with_day(&self, day: u32) -> Option<Date<Tz>> {
        map_local(self, |date| date.with_day(day))
    }

    #[inline]
    fn with_day0(&self, day0: u32) -> Option<Date<Tz>> {
        map_local(self, |date| date.with_day0(day0))
    }

    #[inline]
    fn with_ordinal(&self, ordinal: u32) -> Option<Date<Tz>> {
        map_local(self, |date| date.with_ordinal(ordinal))
    }

    #[inline]
    fn with_ordinal0(&self, ordinal0: u32) -> Option<Date<Tz>> {
        map_local(self, |date| date.with_ordinal0(ordinal0))
    }
}

// we need them as automatic impls cannot handle associated types
impl<Tz: TimeZone> Copy for Date<Tz> where <Tz as TimeZone>::Offset: Copy {}
unsafe impl<Tz: TimeZone> Send for Date<Tz> where <Tz as TimeZone>::Offset: Send {}

impl<Tz: TimeZone, Tz2: TimeZone> PartialEq<Date<Tz2>> for Date<Tz> {
    fn eq(&self, other: &Date<Tz2>) -> bool { self.date == other.date }
}

impl<Tz: TimeZone> Eq for Date<Tz> {
}

impl<Tz: TimeZone> PartialOrd for Date<Tz> {
    fn partial_cmp(&self, other: &Date<Tz>) -> Option<Ordering> {
        self.date.partial_cmp(&other.date)
    }
}

impl<Tz: TimeZone> Ord for Date<Tz> {
    fn cmp(&self, other: &Date<Tz>) -> Ordering { self.date.cmp(&other.date) }
}

impl<Tz: TimeZone> hash::Hash for Date<Tz> {
    fn hash<H: hash::Hasher>(&self, state: &mut H) { self.date.hash(state) }
}

impl<Tz: TimeZone> Add<Duration> for Date<Tz> {
    type Output = Date<Tz>;

    #[inline]
    fn add(self, rhs: Duration) -> Date<Tz> {
        self.checked_add(rhs).expect("`Date + Duration` overflowed")
    }
}

impl<Tz: TimeZone, Tz2: TimeZone> Sub<Date<Tz2>> for Date<Tz> {
    type Output = Duration;

    #[inline]
    fn sub(self, rhs: Date<Tz2>) -> Duration { self.date - rhs.date }
}

impl<Tz: TimeZone> Sub<Duration> for Date<Tz> {
    type Output = Date<Tz>;

    #[inline]
    fn sub(self, rhs: Duration) -> Date<Tz> {
        self.checked_sub(rhs).expect("`Date - Duration` overflowed")
    }
}

impl<Tz: TimeZone> fmt::Debug for Date<Tz> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}{:?}", self.naive_local(), self.offset)
    }
}

impl<Tz: TimeZone> fmt::Display for Date<Tz> where Tz::Offset: fmt::Display {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}{}", self.naive_local(), self.offset)
    }
}

#[cfg(feature = "rustc-serialize")]
mod rustc_serialize {
    use super::Date;
    use offset::TimeZone;
    use rustc_serialize::{Encodable, Encoder, Decodable, Decoder};

    // TODO the current serialization format is NEVER intentionally defined.
    // in the future it is likely to be redefined to more sane and reasonable format.

    impl<Tz: TimeZone> Encodable for Date<Tz> where Tz::Offset: Encodable {
        fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
            s.emit_struct("Date", 2, |s| {
                try!(s.emit_struct_field("date", 0, |s| self.date.encode(s)));
                try!(s.emit_struct_field("offset", 1, |s| self.offset.encode(s)));
                Ok(())
            })
        }
    }

    impl<Tz: TimeZone> Decodable for Date<Tz> where Tz::Offset: Decodable {
        fn decode<D: Decoder>(d: &mut D) -> Result<Date<Tz>, D::Error> {
            d.read_struct("Date", 2, |d| {
                let date = try!(d.read_struct_field("date", 0, Decodable::decode));
                let offset = try!(d.read_struct_field("offset", 1, Decodable::decode));
                Ok(Date::from_utc(date, offset))
            })
        }
    }

    #[test]
    fn test_encodable() {
        use offset::utc::UTC;
        use rustc_serialize::json::encode;

        assert_eq!(encode(&UTC.ymd(2014, 7, 24)).ok(),
                   Some(r#"{"date":{"ymdf":16501977},"offset":{}}"#.into()));
    }

    #[test]
    fn test_decodable() {
        use offset::utc::UTC;
        use rustc_serialize::json;

        let decode = |s: &str| json::decode::<Date<UTC>>(s);

        assert_eq!(decode(r#"{"date":{"ymdf":16501977},"offset":{}}"#).ok(),
                   Some(UTC.ymd(2014, 7, 24)));

        assert!(decode(r#"{"date":{"ymdf":0},"offset":{}}"#).is_err());
    }
}

#[cfg(test)]
mod tests {
    use std::fmt;

    use Datelike;
    use duration::Duration;
    use naive::date::NaiveDate;
    use naive::datetime::NaiveDateTime;
    use offset::{TimeZone, Offset, LocalResult};
    use offset::local::Local;

    #[derive(Copy, Clone, PartialEq, Eq)]
    struct UTC1y; // same to UTC but with an offset of 365 days

    #[derive(Copy, Clone, PartialEq, Eq)]
    struct OneYear;

    impl TimeZone for UTC1y {
        type Offset = OneYear;

        fn from_offset(_offset: &OneYear) -> UTC1y { UTC1y }

        fn offset_from_local_date(&self, _local: &NaiveDate) -> LocalResult<OneYear> {
            LocalResult::Single(OneYear)
        }
        fn offset_from_local_datetime(&self, _local: &NaiveDateTime) -> LocalResult<OneYear> {
            LocalResult::Single(OneYear)
        }

        fn offset_from_utc_date(&self, _utc: &NaiveDate) -> OneYear { OneYear }
        fn offset_from_utc_datetime(&self, _utc: &NaiveDateTime) -> OneYear { OneYear }
    }

    impl Offset for OneYear {
        fn local_minus_utc(&self) -> Duration { Duration::days(365) }
    }

    impl fmt::Debug for OneYear {
        fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { write!(f, "+8760:00") }
    }

    #[test]
    fn test_date_weird_offset() {
        assert_eq!(format!("{:?}", UTC1y.ymd(2012, 2, 29)),
                   "2012-02-29+8760:00".to_string());
        assert_eq!(format!("{:?}", UTC1y.ymd(2012, 2, 29).and_hms(5, 6, 7)),
                   "2012-02-29T05:06:07+8760:00".to_string());
        assert_eq!(format!("{:?}", UTC1y.ymd(2012, 3, 4)),
                   "2012-03-04+8760:00".to_string());
        assert_eq!(format!("{:?}", UTC1y.ymd(2012, 3, 4).and_hms(5, 6, 7)),
                   "2012-03-04T05:06:07+8760:00".to_string());
    }

    #[test]
    fn test_local_date_sanity_check() { // issue #27
        assert_eq!(Local.ymd(2999, 12, 28).day(), 28);
    }
}

