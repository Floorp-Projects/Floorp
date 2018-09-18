// This is a part of rust-chrono.
// Copyright (c) 2014-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

/*!
 * ISO 8601 date and time with time zone.
 */

use std::{str, fmt, hash};
use std::cmp::Ordering;
use std::ops::{Add, Sub};

use {Weekday, Timelike, Datelike};
use offset::{TimeZone, Offset};
use offset::utc::UTC;
use offset::local::Local;
use offset::fixed::FixedOffset;
use duration::Duration;
use naive::time::NaiveTime;
use naive::datetime::NaiveDateTime;
use date::Date;
use format::{Item, Numeric, Pad, Fixed};
use format::{parse, Parsed, ParseError, ParseResult, DelayedFormat, StrftimeItems};

/// ISO 8601 combined date and time with time zone.
#[derive(Clone)]
pub struct DateTime<Tz: TimeZone> {
    datetime: NaiveDateTime,
    offset: Tz::Offset,
}

impl<Tz: TimeZone> DateTime<Tz> {
    /// Makes a new `DateTime` with given *UTC* datetime and offset.
    /// The local datetime should be constructed via the `TimeZone` trait.
    //
    // note: this constructor is purposedly not named to `new` to discourage the direct usage.
    #[inline]
    pub fn from_utc(datetime: NaiveDateTime, offset: Tz::Offset) -> DateTime<Tz> {
        DateTime { datetime: datetime, offset: offset }
    }

    /// Retrieves a date component.
    #[inline]
    pub fn date(&self) -> Date<Tz> {
        Date::from_utc(self.naive_local().date(), self.offset.clone())
    }

    /// Retrieves a time component.
    /// Unlike `date`, this is not associated to the time zone.
    #[inline]
    pub fn time(&self) -> NaiveTime {
        self.datetime.time() + self.offset.local_minus_utc()
    }

    /// Returns the number of non-leap seconds since January 1, 1970 0:00:00 UTC
    /// (aka "UNIX timestamp").
    #[inline]
    pub fn timestamp(&self) -> i64 {
        self.datetime.timestamp()
    }

    /// Returns the number of milliseconds since the last second boundary
    ///
    /// warning: in event of a leap second, this may exceed 999
    ///
    /// note: this is not the number of milliseconds since January 1, 1970 0:00:00 UTC
    #[inline]
    pub fn timestamp_subsec_millis(&self) -> u32 {
        self.datetime.timestamp_subsec_millis()
    }

    /// Returns the number of microseconds since the last second boundary
    ///
    /// warning: in event of a leap second, this may exceed 999_999
    ///
    /// note: this is not the number of microseconds since January 1, 1970 0:00:00 UTC
    #[inline]
    pub fn timestamp_subsec_micros(&self) -> u32 {
        self.datetime.timestamp_subsec_micros()
    }

    /// Returns the number of nanoseconds since the last second boundary
    ///
    /// warning: in event of a leap second, this may exceed 999_999_999
    ///
    /// note: this is not the number of nanoseconds since January 1, 1970 0:00:00 UTC
    #[inline]
    pub fn timestamp_subsec_nanos(&self) -> u32 {
        self.datetime.timestamp_subsec_nanos()
    }

    /// *Deprecated*: Same to `DateTime::timestamp`.
    #[inline]
    pub fn num_seconds_from_unix_epoch(&self) -> i64 {
        self.timestamp()
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
    /// This does not change the actual `DateTime` (but will change the string representation).
    #[inline]
    pub fn with_timezone<Tz2: TimeZone>(&self, tz: &Tz2) -> DateTime<Tz2> {
        tz.from_utc_datetime(&self.datetime)
    }

    /// Adds given `Duration` to the current date and time.
    ///
    /// Returns `None` when it will result in overflow.
    #[inline]
    pub fn checked_add(self, rhs: Duration) -> Option<DateTime<Tz>> {
        let datetime = try_opt!(self.datetime.checked_add(rhs));
        Some(DateTime { datetime: datetime, offset: self.offset })
    }

    /// Subtracts given `Duration` from the current date and time.
    ///
    /// Returns `None` when it will result in overflow.
    #[inline]
    pub fn checked_sub(self, rhs: Duration) -> Option<DateTime<Tz>> {
        let datetime = try_opt!(self.datetime.checked_sub(rhs));
        Some(DateTime { datetime: datetime, offset: self.offset })
    }

    /// Returns a view to the naive UTC datetime.
    #[inline]
    pub fn naive_utc(&self) -> NaiveDateTime {
        self.datetime
    }

    /// Returns a view to the naive local datetime.
    #[inline]
    pub fn naive_local(&self) -> NaiveDateTime {
        self.datetime + self.offset.local_minus_utc()
    }
}

/// Maps the local datetime to other datetime with given conversion function.
fn map_local<Tz: TimeZone, F>(dt: &DateTime<Tz>, mut f: F) -> Option<DateTime<Tz>>
        where F: FnMut(NaiveDateTime) -> Option<NaiveDateTime> {
    f(dt.naive_local()).and_then(|datetime| dt.timezone().from_local_datetime(&datetime).single())
}

impl DateTime<FixedOffset> {
    /// Parses an RFC 2822 date and time string such as `Tue, 1 Jul 2003 10:52:37 +0200`,
    /// then returns a new `DateTime` with a parsed `FixedOffset`.
    pub fn parse_from_rfc2822(s: &str) -> ParseResult<DateTime<FixedOffset>> {
        const ITEMS: &'static [Item<'static>] = &[Item::Fixed(Fixed::RFC2822)];
        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, s, ITEMS.iter().cloned()));
        parsed.to_datetime()
    }

    /// Parses an RFC 3339 and ISO 8601 date and time string such as `1996-12-19T16:39:57-08:00`,
    /// then returns a new `DateTime` with a parsed `FixedOffset`.
    ///
    /// Why isn't this named `parse_from_iso8601`? That's because ISO 8601 allows some freedom
    /// over the syntax and RFC 3339 exercises that freedom to rigidly define a fixed format.
    pub fn parse_from_rfc3339(s: &str) -> ParseResult<DateTime<FixedOffset>> {
        const ITEMS: &'static [Item<'static>] = &[Item::Fixed(Fixed::RFC3339)];
        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, s, ITEMS.iter().cloned()));
        parsed.to_datetime()
    }

    /// Parses a string with the specified format string and
    /// returns a new `DateTime` with a parsed `FixedOffset`.
    /// See the [`format::strftime` module](../format/strftime/index.html)
    /// on the supported escape sequences.
    ///
    /// See also `Offset::datetime_from_str` which gives a local `DateTime` on specific time zone.
    pub fn parse_from_str(s: &str, fmt: &str) -> ParseResult<DateTime<FixedOffset>> {
        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, s, StrftimeItems::new(fmt)));
        parsed.to_datetime()
    }
}

impl<Tz: TimeZone> DateTime<Tz> where Tz::Offset: fmt::Display {
    /// Returns an RFC 2822 date and time string such as `Tue, 1 Jul 2003 10:52:37 +0200`.
    pub fn to_rfc2822(&self) -> String {
        const ITEMS: &'static [Item<'static>] = &[Item::Fixed(Fixed::RFC2822)];
        self.format_with_items(ITEMS.iter().cloned()).to_string()
    }

    /// Returns an RFC 3339 and ISO 8601 date and time string such as `1996-12-19T16:39:57-08:00`.
    pub fn to_rfc3339(&self) -> String {
        const ITEMS: &'static [Item<'static>] = &[Item::Fixed(Fixed::RFC3339)];
        self.format_with_items(ITEMS.iter().cloned()).to_string()
    }

    /// Formats the combined date and time with the specified formatting items.
    #[inline]
    pub fn format_with_items<'a, I>(&self, items: I) -> DelayedFormat<I>
            where I: Iterator<Item=Item<'a>> + Clone {
        let local = self.naive_local();
        DelayedFormat::new_with_offset(Some(local.date()), Some(local.time()), &self.offset, items)
    }

    /// Formats the combined date and time with the specified format string.
    /// See the [`format::strftime` module](../format/strftime/index.html)
    /// on the supported escape sequences.
    #[inline]
    pub fn format<'a>(&self, fmt: &'a str) -> DelayedFormat<StrftimeItems<'a>> {
        self.format_with_items(StrftimeItems::new(fmt))
    }
}

impl<Tz: TimeZone> Datelike for DateTime<Tz> {
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
    fn with_year(&self, year: i32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_year(year))
    }

    #[inline]
    fn with_month(&self, month: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_month(month))
    }

    #[inline]
    fn with_month0(&self, month0: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_month0(month0))
    }

    #[inline]
    fn with_day(&self, day: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_day(day))
    }

    #[inline]
    fn with_day0(&self, day0: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_day0(day0))
    }

    #[inline]
    fn with_ordinal(&self, ordinal: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_ordinal(ordinal))
    }

    #[inline]
    fn with_ordinal0(&self, ordinal0: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_ordinal0(ordinal0))
    }
}

impl<Tz: TimeZone> Timelike for DateTime<Tz> {
    #[inline] fn hour(&self) -> u32 { self.naive_local().hour() }
    #[inline] fn minute(&self) -> u32 { self.naive_local().minute() }
    #[inline] fn second(&self) -> u32 { self.naive_local().second() }
    #[inline] fn nanosecond(&self) -> u32 { self.naive_local().nanosecond() }

    #[inline]
    fn with_hour(&self, hour: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_hour(hour))
    }

    #[inline]
    fn with_minute(&self, min: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_minute(min))
    }

    #[inline]
    fn with_second(&self, sec: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_second(sec))
    }

    #[inline]
    fn with_nanosecond(&self, nano: u32) -> Option<DateTime<Tz>> {
        map_local(self, |datetime| datetime.with_nanosecond(nano))
    }
}

// we need them as automatic impls cannot handle associated types
impl<Tz: TimeZone> Copy for DateTime<Tz> where <Tz as TimeZone>::Offset: Copy {}
unsafe impl<Tz: TimeZone> Send for DateTime<Tz> where <Tz as TimeZone>::Offset: Send {}

impl<Tz: TimeZone, Tz2: TimeZone> PartialEq<DateTime<Tz2>> for DateTime<Tz> {
    fn eq(&self, other: &DateTime<Tz2>) -> bool { self.datetime == other.datetime }
}

impl<Tz: TimeZone> Eq for DateTime<Tz> {
}

impl<Tz: TimeZone> PartialOrd for DateTime<Tz> {
    fn partial_cmp(&self, other: &DateTime<Tz>) -> Option<Ordering> {
        self.datetime.partial_cmp(&other.datetime)
    }
}

impl<Tz: TimeZone> Ord for DateTime<Tz> {
    fn cmp(&self, other: &DateTime<Tz>) -> Ordering { self.datetime.cmp(&other.datetime) }
}

impl<Tz: TimeZone> hash::Hash for DateTime<Tz> {
    fn hash<H: hash::Hasher>(&self, state: &mut H) { self.datetime.hash(state) }
}

impl<Tz: TimeZone> Add<Duration> for DateTime<Tz> {
    type Output = DateTime<Tz>;

    #[inline]
    fn add(self, rhs: Duration) -> DateTime<Tz> {
        self.checked_add(rhs).expect("`DateTime + Duration` overflowed")
    }
}

impl<Tz: TimeZone, Tz2: TimeZone> Sub<DateTime<Tz2>> for DateTime<Tz> {
    type Output = Duration;

    #[inline]
    fn sub(self, rhs: DateTime<Tz2>) -> Duration { self.datetime - rhs.datetime }
}

impl<Tz: TimeZone> Sub<Duration> for DateTime<Tz> {
    type Output = DateTime<Tz>;

    #[inline]
    fn sub(self, rhs: Duration) -> DateTime<Tz> {
        self.checked_sub(rhs).expect("`DateTime - Duration` overflowed")
    }
}

impl<Tz: TimeZone> fmt::Debug for DateTime<Tz> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}{:?}", self.naive_local(), self.offset)
    }
}

impl<Tz: TimeZone> fmt::Display for DateTime<Tz> where Tz::Offset: fmt::Display {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} {}", self.naive_local(), self.offset)
    }
}

impl str::FromStr for DateTime<FixedOffset> {
    type Err = ParseError;

    fn from_str(s: &str) -> ParseResult<DateTime<FixedOffset>> {
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
                             Item::Fixed(Fixed::Nanosecond),
            Item::Space(""), Item::Fixed(Fixed::TimezoneOffsetZ),
            Item::Space(""),
        ];

        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, s, ITEMS.iter().cloned()));
        parsed.to_datetime()
    }
}

impl str::FromStr for DateTime<UTC> {
    type Err = ParseError;

    fn from_str(s: &str) -> ParseResult<DateTime<UTC>> {
        s.parse::<DateTime<FixedOffset>>().map(|dt| dt.with_timezone(&UTC))
    }
}

impl str::FromStr for DateTime<Local> {
    type Err = ParseError;

    fn from_str(s: &str) -> ParseResult<DateTime<Local>> {
        s.parse::<DateTime<FixedOffset>>().map(|dt| dt.with_timezone(&Local))
    }
}

#[cfg(feature = "rustc-serialize")]
mod rustc_serialize {
    use super::DateTime;
    use offset::TimeZone;
    use rustc_serialize::{Encodable, Encoder, Decodable, Decoder};

    // TODO the current serialization format is NEVER intentionally defined.
    // in the future it is likely to be redefined to more sane and reasonable format.

    impl<Tz: TimeZone> Encodable for DateTime<Tz> where Tz::Offset: Encodable {
        fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
            s.emit_struct("DateTime", 2, |s| {
                try!(s.emit_struct_field("datetime", 0, |s| self.datetime.encode(s)));
                try!(s.emit_struct_field("offset", 1, |s| self.offset.encode(s)));
                Ok(())
            })
        }
    }

    impl<Tz: TimeZone> Decodable for DateTime<Tz> where Tz::Offset: Decodable {
        fn decode<D: Decoder>(d: &mut D) -> Result<DateTime<Tz>, D::Error> {
            d.read_struct("DateTime", 2, |d| {
                let datetime = try!(d.read_struct_field("datetime", 0, Decodable::decode));
                let offset = try!(d.read_struct_field("offset", 1, Decodable::decode));
                Ok(DateTime::from_utc(datetime, offset))
            })
        }
    }

    #[test]
    fn test_encodable() {
        use offset::utc::UTC;
        use rustc_serialize::json::encode;

        assert_eq!(
            encode(&UTC.ymd(2014, 7, 24).and_hms(12, 34, 6)).ok(),
            Some(concat!(r#"{"datetime":{"date":{"ymdf":16501977},"#,
                                      r#""time":{"secs":45246,"frac":0}},"#,
                          r#""offset":{}}"#).into()));
    }

    #[test]
    fn test_decodable() {
        use offset::utc::UTC;
        use rustc_serialize::json;

        let decode = |s: &str| json::decode::<DateTime<UTC>>(s);

        assert_eq!(
            decode(r#"{"datetime":{"date":{"ymdf":16501977},
                                   "time":{"secs":45246,"frac":0}},
                       "offset":{}}"#).ok(),
            Some(UTC.ymd(2014, 7, 24).and_hms(12, 34, 6)));

        assert_eq!(
            decode(r#"{"datetime":{"date":{"ymdf":0},
                                   "time":{"secs":0,"frac":0}},
                       "offset":{}}"#).ok(),
            None);
    }
}

#[cfg(feature = "serde")]
mod serde {
    use super::DateTime;
    use offset::TimeZone;
    use offset::utc::UTC;
    use offset::local::Local;
    use offset::fixed::FixedOffset;
    use std::fmt::Display;
    use serde::{ser, de};

    // TODO not very optimized for space (binary formats would want something better)

    impl<Tz: TimeZone> ser::Serialize for DateTime<Tz>
        where Tz::Offset: Display
    {
        fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
            where S: ser::Serializer
        {
            // Debug formatting is correct RFC3339, and it allows Zulu.
            serializer.serialize_str(&format!("{:?}", self))
        }
    }

    struct DateTimeVisitor;

    impl de::Visitor for DateTimeVisitor {
        type Value = DateTime<FixedOffset>;

        fn visit_str<E>(&mut self, value: &str) -> Result<DateTime<FixedOffset>, E>
            where E: de::Error
        {
            value.parse().map_err(|err| E::custom(format!("{}", err)))
        }
    }

    impl de::Deserialize for DateTime<FixedOffset> {
        fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
            where D: de::Deserializer
        {
            deserializer.deserialize(DateTimeVisitor)
        }
    }

    impl de::Deserialize for DateTime<UTC> {
        fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
            where D: de::Deserializer
        {
            deserializer.deserialize(DateTimeVisitor).map(|dt| dt.with_timezone(&UTC))
        }
    }

    impl de::Deserialize for DateTime<Local> {
        fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
            where D: de::Deserializer
        {
            deserializer.deserialize(DateTimeVisitor).map(|dt| dt.with_timezone(&Local))
        }
    }

    #[cfg(test)] extern crate serde_json;

    #[test]
    fn test_serde_serialize() {
        use self::serde_json::to_string;

        assert_eq!(to_string(&UTC.ymd(2014, 7, 24).and_hms(12, 34, 6)).ok(),
                   Some(r#""2014-07-24T12:34:06Z""#.into()));
    }

    #[test]
    fn test_serde_deserialize() {
        use self::serde_json;

        let from_str = |s: &str| serde_json::from_str::<DateTime<UTC>>(s);

        assert_eq!(from_str(r#""2014-07-24T12:34:06Z""#).ok(),
                   Some(UTC.ymd(2014, 7, 24).and_hms(12, 34, 6)));

        assert!(from_str(r#""2014-07-32T12:34:06Z""#).is_err());
    }
}

#[cfg(test)]
mod tests {
    use super::DateTime;
    use Datelike;
    use naive::time::NaiveTime;
    use naive::date::NaiveDate;
    use duration::Duration;
    use offset::TimeZone;
    use offset::utc::UTC;
    use offset::local::Local;
    use offset::fixed::FixedOffset;

    #[test]
    #[allow(non_snake_case)]
    fn test_datetime_offset() {
        let EST = FixedOffset::west(5*60*60);
        let EDT = FixedOffset::west(4*60*60);
        let KST = FixedOffset::east(9*60*60);

        assert_eq!(format!("{}", UTC.ymd(2014, 5, 6).and_hms(7, 8, 9)),
                   "2014-05-06 07:08:09 UTC");
        assert_eq!(format!("{}", EDT.ymd(2014, 5, 6).and_hms(7, 8, 9)),
                   "2014-05-06 07:08:09 -04:00");
        assert_eq!(format!("{}", KST.ymd(2014, 5, 6).and_hms(7, 8, 9)),
                   "2014-05-06 07:08:09 +09:00");
        assert_eq!(format!("{:?}", UTC.ymd(2014, 5, 6).and_hms(7, 8, 9)),
                   "2014-05-06T07:08:09Z");
        assert_eq!(format!("{:?}", EDT.ymd(2014, 5, 6).and_hms(7, 8, 9)),
                   "2014-05-06T07:08:09-04:00");
        assert_eq!(format!("{:?}", KST.ymd(2014, 5, 6).and_hms(7, 8, 9)),
                   "2014-05-06T07:08:09+09:00");

        // edge cases
        assert_eq!(format!("{:?}", UTC.ymd(2014, 5, 6).and_hms(0, 0, 0)),
                   "2014-05-06T00:00:00Z");
        assert_eq!(format!("{:?}", EDT.ymd(2014, 5, 6).and_hms(0, 0, 0)),
                   "2014-05-06T00:00:00-04:00");
        assert_eq!(format!("{:?}", KST.ymd(2014, 5, 6).and_hms(0, 0, 0)),
                   "2014-05-06T00:00:00+09:00");
        assert_eq!(format!("{:?}", UTC.ymd(2014, 5, 6).and_hms(23, 59, 59)),
                   "2014-05-06T23:59:59Z");
        assert_eq!(format!("{:?}", EDT.ymd(2014, 5, 6).and_hms(23, 59, 59)),
                   "2014-05-06T23:59:59-04:00");
        assert_eq!(format!("{:?}", KST.ymd(2014, 5, 6).and_hms(23, 59, 59)),
                   "2014-05-06T23:59:59+09:00");

        assert_eq!(UTC.ymd(2014, 5, 6).and_hms(7, 8, 9), EDT.ymd(2014, 5, 6).and_hms(3, 8, 9));
        assert_eq!(UTC.ymd(2014, 5, 6).and_hms(7, 8, 9) + Duration::seconds(3600 + 60 + 1),
                   UTC.ymd(2014, 5, 6).and_hms(8, 9, 10));
        assert_eq!(UTC.ymd(2014, 5, 6).and_hms(7, 8, 9) - EDT.ymd(2014, 5, 6).and_hms(10, 11, 12),
                   Duration::seconds(-7*3600 - 3*60 - 3));

        assert_eq!(*UTC.ymd(2014, 5, 6).and_hms(7, 8, 9).offset(), UTC);
        assert_eq!(*EDT.ymd(2014, 5, 6).and_hms(7, 8, 9).offset(), EDT);
        assert!(*EDT.ymd(2014, 5, 6).and_hms(7, 8, 9).offset() != EST);
    }

    #[test]
    fn test_datetime_date_and_time() {
        let tz = FixedOffset::east(5*60*60);
        let d = tz.ymd(2014, 5, 6).and_hms(7, 8, 9);
        assert_eq!(d.time(), NaiveTime::from_hms(7, 8, 9));
        assert_eq!(d.date(), tz.ymd(2014, 5, 6));
        assert_eq!(d.date().naive_local(), NaiveDate::from_ymd(2014, 5, 6));
        assert_eq!(d.date().and_time(d.time()), Some(d));

        let tz = FixedOffset::east(4*60*60);
        let d = tz.ymd(2016, 5, 4).and_hms(3, 2, 1);
        assert_eq!(d.time(), NaiveTime::from_hms(3, 2, 1));
        assert_eq!(d.date(), tz.ymd(2016, 5, 4));
        assert_eq!(d.date().naive_local(), NaiveDate::from_ymd(2016, 5, 4));
        assert_eq!(d.date().and_time(d.time()), Some(d));

        let tz = FixedOffset::west(13*60*60);
        let d = tz.ymd(2017, 8, 9).and_hms(12, 34, 56);
        assert_eq!(d.time(), NaiveTime::from_hms(12, 34, 56));
        assert_eq!(d.date(), tz.ymd(2017, 8, 9));
        assert_eq!(d.date().naive_local(), NaiveDate::from_ymd(2017, 8, 9));
        assert_eq!(d.date().and_time(d.time()), Some(d));
    }

    #[test]
    fn test_datetime_with_timezone() {
        let local_now = Local::now();
        let utc_now = local_now.with_timezone(&UTC);
        let local_now2 = utc_now.with_timezone(&Local);
        assert_eq!(local_now, local_now2);
    }

    #[test]
    #[allow(non_snake_case)]
    fn test_datetime_rfc2822_and_rfc3339() {
        let EDT = FixedOffset::east(5*60*60);
        assert_eq!(UTC.ymd(2015, 2, 18).and_hms(23, 16, 9).to_rfc2822(),
                   "Wed, 18 Feb 2015 23:16:09 +0000");
        assert_eq!(UTC.ymd(2015, 2, 18).and_hms(23, 16, 9).to_rfc3339(),
                   "2015-02-18T23:16:09+00:00");
        assert_eq!(EDT.ymd(2015, 2, 18).and_hms_milli(23, 16, 9, 150).to_rfc2822(),
                   "Wed, 18 Feb 2015 23:16:09 +0500");
        assert_eq!(EDT.ymd(2015, 2, 18).and_hms_milli(23, 16, 9, 150).to_rfc3339(),
                   "2015-02-18T23:16:09.150+05:00");
        assert_eq!(EDT.ymd(2015, 2, 18).and_hms_micro(23, 59, 59, 1_234_567).to_rfc2822(),
                   "Wed, 18 Feb 2015 23:59:60 +0500");
        assert_eq!(EDT.ymd(2015, 2, 18).and_hms_micro(23, 59, 59, 1_234_567).to_rfc3339(),
                   "2015-02-18T23:59:60.234567+05:00");

        assert_eq!(DateTime::parse_from_rfc2822("Wed, 18 Feb 2015 23:16:09 +0000"),
                   Ok(FixedOffset::east(0).ymd(2015, 2, 18).and_hms(23, 16, 9)));
        assert_eq!(DateTime::parse_from_rfc3339("2015-02-18T23:16:09Z"),
                   Ok(FixedOffset::east(0).ymd(2015, 2, 18).and_hms(23, 16, 9)));
        assert_eq!(DateTime::parse_from_rfc2822("Wed, 18 Feb 2015 23:59:60 +0500"),
                   Ok(EDT.ymd(2015, 2, 18).and_hms_milli(23, 59, 59, 1_000)));
        assert_eq!(DateTime::parse_from_rfc3339("2015-02-18T23:59:60.234567+05:00"),
                   Ok(EDT.ymd(2015, 2, 18).and_hms_micro(23, 59, 59, 1_234_567)));
    }

    #[test]
    fn test_datetime_from_str() {
        assert_eq!("2015-2-18T23:16:9.15Z".parse::<DateTime<FixedOffset>>(),
                   Ok(FixedOffset::east(0).ymd(2015, 2, 18).and_hms_milli(23, 16, 9, 150)));
        assert_eq!("2015-2-18T13:16:9.15-10:00".parse::<DateTime<FixedOffset>>(),
                   Ok(FixedOffset::west(10 * 3600).ymd(2015, 2, 18).and_hms_milli(13, 16, 9, 150)));
        assert!("2015-2-18T23:16:9.15".parse::<DateTime<FixedOffset>>().is_err());

        assert_eq!("2015-2-18T23:16:9.15Z".parse::<DateTime<UTC>>(),
                   Ok(UTC.ymd(2015, 2, 18).and_hms_milli(23, 16, 9, 150)));
        assert_eq!("2015-2-18T13:16:9.15-10:00".parse::<DateTime<UTC>>(),
                   Ok(UTC.ymd(2015, 2, 18).and_hms_milli(23, 16, 9, 150)));
        assert!("2015-2-18T23:16:9.15".parse::<DateTime<UTC>>().is_err());

        // no test for `DateTime<Local>`, we cannot verify that much.
    }

    #[test]
    fn test_datetime_parse_from_str() {
        let ymdhms = |y,m,d,h,n,s,off| FixedOffset::east(off).ymd(y,m,d).and_hms(h,n,s);
        assert_eq!(DateTime::parse_from_str("2014-5-7T12:34:56+09:30", "%Y-%m-%dT%H:%M:%S%z"),
                   Ok(ymdhms(2014, 5, 7, 12, 34, 56, 570*60))); // ignore offset
        assert!(DateTime::parse_from_str("20140507000000", "%Y%m%d%H%M%S").is_err()); // no offset
        assert!(DateTime::parse_from_str("Fri, 09 Aug 2013 23:54:35 GMT",
                                         "%a, %d %b %Y %H:%M:%S GMT").is_err());
        assert_eq!(UTC.datetime_from_str("Fri, 09 Aug 2013 23:54:35 GMT",
                                         "%a, %d %b %Y %H:%M:%S GMT"),
                   Ok(UTC.ymd(2013, 8, 9).and_hms(23, 54, 35)));
    }

    #[test]
    fn test_datetime_format_with_local() {
        // if we are not around the year boundary, local and UTC date should have the same year
        let dt = Local::now().with_month(5).unwrap();
        assert_eq!(dt.format("%Y").to_string(), dt.with_timezone(&UTC).format("%Y").to_string());
    }

    #[test]
    fn test_datetime_is_copy() {
        // UTC is known to be `Copy`.
        let a = UTC::now();
        let b = a;
        assert_eq!(a, b);
    }

    #[test]
    fn test_datetime_is_send() {
        use std::thread;

        // UTC is known to be `Send`.
        let a = UTC::now();
        thread::spawn(move || {
            let _ = a;
        }).join().unwrap();
    }

    #[test]
    fn test_subsecond_part() {
        let datetime = UTC.ymd(2014, 7, 8).and_hms_nano(9, 10, 11, 1234567);

        assert_eq!(1,       datetime.timestamp_subsec_millis());
        assert_eq!(1234,    datetime.timestamp_subsec_micros());
        assert_eq!(1234567, datetime.timestamp_subsec_nanos());
    }
}

