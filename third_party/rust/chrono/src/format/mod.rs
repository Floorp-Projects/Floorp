// This is a part of rust-chrono.
// Copyright (c) 2014-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! Formatting utilities for date and time.

use std::fmt;
use std::error::Error;

use {Datelike, Timelike};
use div::{div_floor, mod_floor};
use duration::Duration;
use offset::Offset;
use naive::date::NaiveDate;
use naive::time::NaiveTime;

pub use self::strftime::StrftimeItems;
pub use self::parsed::Parsed;
pub use self::parse::parse;

/// Padding characters for numeric items.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Pad {
    /// No padding.
    None,
    /// Zero (`0`) padding.
    Zero,
    /// Space padding.
    Space,
}

/// Numeric item types.
/// They have associated formatting width (FW) and parsing width (PW).
///
/// The **formatting width** is the minimal width to be formatted.
/// If the number is too short, and the padding is not [`Pad::None`](./enum.Pad.html#variant.None),
/// then it is left-padded.
/// If the number is too long or (in some cases) negative, it is printed as is.
///
/// The **parsing width** is the maximal width to be scanned.
/// The parser only tries to consume from one to given number of digits (greedily). 
/// It also trims the preceding whitespaces if any.
/// It cannot parse the negative number, so some date and time cannot be formatted then
/// parsed with the same formatting items.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Numeric {
    /// Full Gregorian year (FW=4, PW=∞).
    /// May accept years before 1 BCE or after 9999 CE, given an initial sign.
    Year,
    /// Gregorian year divided by 100 (century number; FW=PW=2). Implies the non-negative year.
    YearDiv100,
    /// Gregorian year modulo 100 (FW=PW=2). Cannot be negative.
    YearMod100,
    /// Year in the ISO week date (FW=4, PW=∞).
    /// May accept years before 1 BCE or after 9999 CE, given an initial sign.
    IsoYear,
    /// Year in the ISO week date, divided by 100 (FW=PW=2). Implies the non-negative year.
    IsoYearDiv100,
    /// Year in the ISO week date, modulo 100 (FW=PW=2). Cannot be negative.
    IsoYearMod100,
    /// Month (FW=PW=2).
    Month,
    /// Day of the month (FW=PW=2).
    Day,
    /// Week number, where the week 1 starts at the first Sunday of January (FW=PW=2).
    WeekFromSun,
    /// Week number, where the week 1 starts at the first Monday of January (FW=PW=2).
    WeekFromMon,
    /// Week number in the ISO week date (FW=PW=2).
    IsoWeek,
    /// Day of the week, where Sunday = 0 and Saturday = 6 (FW=PW=1).
    NumDaysFromSun,
    /// Day of the week, where Monday = 1 and Sunday = 7 (FW=PW=1).
    WeekdayFromMon,
    /// Day of the year (FW=PW=3).
    Ordinal,
    /// Hour number in the 24-hour clocks (FW=PW=2).
    Hour,
    /// Hour number in the 12-hour clocks (FW=PW=2).
    Hour12,
    /// The number of minutes since the last whole hour (FW=PW=2).
    Minute,
    /// The number of seconds since the last whole minute (FW=PW=2).
    Second,
    /// The number of nanoseconds since the last whole second (FW=PW=9).
    /// Note that this is *not* left-aligned;
    /// see also [`Fixed::Nanosecond`](./enum.Fixed.html#variant.Nanosecond).
    Nanosecond,
    /// The number of non-leap seconds since the midnight UTC on January 1, 1970 (FW=1, PW=∞).
    /// For formatting, it assumes UTC upon the absence of time zone offset.
    Timestamp,
}

/// Fixed-format item types.
///
/// They have their own rules of formatting and parsing.
/// Otherwise noted, they print in the specified cases but parse case-insensitively.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Fixed {
    /// Abbreviated month names.
    ///
    /// Prints a three-letter-long name in the title case, reads the same name in any case.
    ShortMonthName,
    /// Full month names.
    ///
    /// Prints a full name in the title case, reads either a short or full name in any case.
    LongMonthName,
    /// Abbreviated day of the week names.
    ///
    /// Prints a three-letter-long name in the title case, reads the same name in any case.
    ShortWeekdayName,
    /// Full day of the week names.
    ///
    /// Prints a full name in the title case, reads either a short or full name in any case.
    LongWeekdayName,
    /// AM/PM.
    ///
    /// Prints in lower case, reads in any case.
    LowerAmPm,
    /// AM/PM.
    ///
    /// Prints in upper case, reads in any case.
    UpperAmPm,
    /// An optional dot plus one or more digits for left-aligned nanoseconds.
    /// May print nothing, 3, 6 or 9 digits according to the available accuracy.
    /// See also [`Numeric::Nanosecond`](./enum.Numeric.html#variant.Nanosecond).
    Nanosecond,
    /// Same to [`Nanosecond`](#variant.Nanosecond) but the accuracy is fixed to 3.
    Nanosecond3,
    /// Same to [`Nanosecond`](#variant.Nanosecond) but the accuracy is fixed to 6.
    Nanosecond6,
    /// Same to [`Nanosecond`](#variant.Nanosecond) but the accuracy is fixed to 9.
    Nanosecond9,
    /// Timezone name.
    ///
    /// It does not support parsing, its use in the parser is an immediate failure.
    TimezoneName,
    /// Offset from the local time to UTC (`+09:00` or `-04:00` or `+00:00`).
    ///
    /// In the parser, the colon can be omitted and/or surrounded with any amount of whitespaces.
    /// The offset is limited from `-24:00` to `+24:00`,
    /// which is same to [`FixedOffset`](../offset/fixed/struct.FixedOffset.html)'s range.
    TimezoneOffsetColon,
    /// Offset from the local time to UTC (`+09:00` or `-04:00` or `Z`).
    ///
    /// In the parser, the colon can be omitted and/or surrounded with any amount of whitespaces,
    /// and `Z` can be either in upper case or in lower case.
    /// The offset is limited from `-24:00` to `+24:00`,
    /// which is same to [`FixedOffset`](../offset/fixed/struct.FixedOffset.html)'s range.
    TimezoneOffsetColonZ,
    /// Same to [`TimezoneOffsetColon`](#variant.TimezoneOffsetColon) but prints no colon.
    /// Parsing allows an optional colon.
    TimezoneOffset,
    /// Same to [`TimezoneOffsetColonZ`](#variant.TimezoneOffsetColonZ) but prints no colon.
    /// Parsing allows an optional colon.
    TimezoneOffsetZ,
    /// RFC 2822 date and time syntax. Commonly used for email and MIME date and time.
    RFC2822,
    /// RFC 3339 & ISO 8601 date and time syntax.
    RFC3339,
}

/// A single formatting item. This is used for both formatting and parsing.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Item<'a> {
    /// A literally printed and parsed text.
    Literal(&'a str),
    /// Whitespace. Prints literally but reads zero or more whitespace.
    Space(&'a str),
    /// Numeric item. Can be optionally padded to the maximal length (if any) when formatting;
    /// the parser simply ignores any padded whitespace and zeroes.
    Numeric(Numeric, Pad),
    /// Fixed-format item.
    Fixed(Fixed),
    /// Issues a formatting error. Used to signal an invalid format string.
    Error,
}

macro_rules! lit  { ($x:expr) => (Item::Literal($x)) }
macro_rules! sp   { ($x:expr) => (Item::Space($x)) }
macro_rules! num  { ($x:ident) => (Item::Numeric(Numeric::$x, Pad::None)) }
macro_rules! num0 { ($x:ident) => (Item::Numeric(Numeric::$x, Pad::Zero)) }
macro_rules! nums { ($x:ident) => (Item::Numeric(Numeric::$x, Pad::Space)) }
macro_rules! fix  { ($x:ident) => (Item::Fixed(Fixed::$x)) }

/// An error from the `parse` function.
#[derive(Debug, Clone, PartialEq, Copy)]
pub struct ParseError(ParseErrorKind);

#[derive(Debug, Clone, PartialEq, Copy)]
enum ParseErrorKind {
    /// Given field is out of permitted range.
    OutOfRange,

    /// There is no possible date and time value with given set of fields.
    ///
    /// This does not include the out-of-range conditions, which are trivially invalid.
    /// It includes the case that there are one or more fields that are inconsistent to each other.
    Impossible,

    /// Given set of fields is not enough to make a requested date and time value.
    ///
    /// Note that there *may* be a case that given fields constrain the possible values so much
    /// that there is a unique possible value. Chrono only tries to be correct for
    /// most useful sets of fields however, as such constraint solving can be expensive.
    NotEnough,

    /// The input string has some invalid character sequence for given formatting items.
    Invalid,

    /// The input string has been prematurely ended.
    TooShort,

    /// All formatting items have been read but there is a remaining input.
    TooLong,

    /// There was an error on the formatting string, or there were non-supported formating items.
    BadFormat,
}

/// Same to `Result<T, ParseError>`.
pub type ParseResult<T> = Result<T, ParseError>;

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.description().fmt(f)
    }
}

impl Error for ParseError {
    fn description(&self) -> &str {
        match self.0 {
            ParseErrorKind::OutOfRange => "input is out of range",
            ParseErrorKind::Impossible => "no possible date and time matching input",
            ParseErrorKind::NotEnough => "input is not enough for unique date and time",
            ParseErrorKind::Invalid => "input contains invalid characters",
            ParseErrorKind::TooShort => "premature end of input",
            ParseErrorKind::TooLong => "trailing input",
            ParseErrorKind::BadFormat => "bad or unsupported format string",
        }
    }
}

// to be used in this module and submodules
const OUT_OF_RANGE: ParseError = ParseError(ParseErrorKind::OutOfRange);
const IMPOSSIBLE:   ParseError = ParseError(ParseErrorKind::Impossible);
const NOT_ENOUGH:   ParseError = ParseError(ParseErrorKind::NotEnough);
const INVALID:      ParseError = ParseError(ParseErrorKind::Invalid);
const TOO_SHORT:    ParseError = ParseError(ParseErrorKind::TooShort);
const TOO_LONG:     ParseError = ParseError(ParseErrorKind::TooLong);
const BAD_FORMAT:   ParseError = ParseError(ParseErrorKind::BadFormat);

/// Tries to format given arguments with given formatting items.
/// Internally used by `DelayedFormat`.
pub fn format<'a, I>(w: &mut fmt::Formatter, date: Option<&NaiveDate>, time: Option<&NaiveTime>,
                     off: Option<&(String, Duration)>, items: I) -> fmt::Result
        where I: Iterator<Item=Item<'a>> {
    // full and abbreviated month and weekday names
    static SHORT_MONTHS: [&'static str; 12] =
        ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    static LONG_MONTHS: [&'static str; 12] =
        ["January", "February", "March", "April", "May", "June",
         "July", "August", "September", "October", "November", "December"];
    static SHORT_WEEKDAYS: [&'static str; 7] =
        ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
    static LONG_WEEKDAYS: [&'static str; 7] =
        ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"];

    for item in items {
        match item {
            Item::Literal(s) | Item::Space(s) => try!(write!(w, "{}", s)),

            Item::Numeric(spec, pad) => {
                use self::Numeric::*;

                let week_from_sun = |d: &NaiveDate|
                    (d.ordinal() as i32 - d.weekday().num_days_from_sunday() as i32 + 7) / 7;
                let week_from_mon = |d: &NaiveDate|
                    (d.ordinal() as i32 - d.weekday().num_days_from_monday() as i32 + 7) / 7;

                let (width, v) = match spec {
                    Year           => (4, date.map(|d| d.year() as i64)),
                    YearDiv100     => (2, date.map(|d| div_floor(d.year() as i64, 100))),
                    YearMod100     => (2, date.map(|d| mod_floor(d.year() as i64, 100))),
                    IsoYear        => (4, date.map(|d| d.isoweekdate().0 as i64)),
                    IsoYearDiv100  => (2, date.map(|d| div_floor(d.isoweekdate().0 as i64, 100))),
                    IsoYearMod100  => (2, date.map(|d| mod_floor(d.isoweekdate().0 as i64, 100))),
                    Month          => (2, date.map(|d| d.month() as i64)),
                    Day            => (2, date.map(|d| d.day() as i64)),
                    WeekFromSun    => (2, date.map(|d| week_from_sun(d) as i64)),
                    WeekFromMon    => (2, date.map(|d| week_from_mon(d) as i64)),
                    IsoWeek        => (2, date.map(|d| d.isoweekdate().1 as i64)),
                    NumDaysFromSun => (1, date.map(|d| d.weekday().num_days_from_sunday() as i64)),
                    WeekdayFromMon => (1, date.map(|d| d.weekday().number_from_monday() as i64)),
                    Ordinal        => (3, date.map(|d| d.ordinal() as i64)),
                    Hour           => (2, time.map(|t| t.hour() as i64)),
                    Hour12         => (2, time.map(|t| t.hour12().1 as i64)),
                    Minute         => (2, time.map(|t| t.minute() as i64)),
                    Second         => (2, time.map(|t| (t.second() +
                                                        t.nanosecond() / 1_000_000_000) as i64)),
                    Nanosecond     => (9, time.map(|t| (t.nanosecond() % 1_000_000_000) as i64)),
                    Timestamp      => (1, match (date, time, off) {
                        (Some(d), Some(t), None) =>
                            Some(d.and_time(*t).timestamp()),
                        (Some(d), Some(t), Some(&(_, off))) =>
                            Some((d.and_time(*t) - off).timestamp()),
                        (_, _, _) => None
                    }),
                };

                if let Some(v) = v {
                    if (spec == Year || spec == IsoYear) && !(0 <= v && v < 10000) {
                        // non-four-digit years require an explicit sign as per ISO 8601
                        match pad {
                            Pad::None => try!(write!(w, "{:+}", v)),
                            Pad::Zero => try!(write!(w, "{:+01$}", v, width + 1)),
                            Pad::Space => try!(write!(w, "{:+1$}", v, width + 1)),
                        }
                    } else {
                        match pad {
                            Pad::None => try!(write!(w, "{}", v)),
                            Pad::Zero => try!(write!(w, "{:01$}", v, width)),
                            Pad::Space => try!(write!(w, "{:1$}", v, width)),
                        }
                    }
                } else {
                    return Err(fmt::Error); // insufficient arguments for given format
                }
            },

            Item::Fixed(spec) => {
                use self::Fixed::*;

                /// Prints an offset from UTC in the format of `+HHMM` or `+HH:MM`.
                /// `Z` instead of `+00[:]00` is allowed when `allow_zulu` is true.
                fn write_local_minus_utc(w: &mut fmt::Formatter, off: Duration,
                                         allow_zulu: bool, use_colon: bool) -> fmt::Result {
                    let off = off.num_minutes();
                    if !allow_zulu || off != 0 {
                        let (sign, off) = if off < 0 {('-', -off)} else {('+', off)};
                        if use_colon {
                            write!(w, "{}{:02}:{:02}", sign, off / 60, off % 60)
                        } else {
                            write!(w, "{}{:02}{:02}", sign, off / 60, off % 60)
                        }
                    } else {
                        write!(w, "Z")
                    }
                }

                let ret = match spec {
                    ShortMonthName =>
                        date.map(|d| write!(w, "{}", SHORT_MONTHS[d.month0() as usize])),
                    LongMonthName =>
                        date.map(|d| write!(w, "{}", LONG_MONTHS[d.month0() as usize])),
                    ShortWeekdayName =>
                        date.map(|d| write!(w, "{}",
                            SHORT_WEEKDAYS[d.weekday().num_days_from_monday() as usize])),
                    LongWeekdayName =>
                        date.map(|d| write!(w, "{}",
                            LONG_WEEKDAYS[d.weekday().num_days_from_monday() as usize])),
                    LowerAmPm =>
                        time.map(|t| write!(w, "{}", if t.hour12().0 {"pm"} else {"am"})),
                    UpperAmPm =>
                        time.map(|t| write!(w, "{}", if t.hour12().0 {"PM"} else {"AM"})),
                    Nanosecond =>
                        time.map(|t| {
                            let nano = t.nanosecond() % 1_000_000_000;
                            if nano == 0 {
                                Ok(())
                            } else if nano % 1_000_000 == 0 {
                                write!(w, ".{:03}", nano / 1_000_000)
                            } else if nano % 1_000 == 0 {
                                write!(w, ".{:06}", nano / 1_000)
                            } else {
                                write!(w, ".{:09}", nano)
                            }
                        }),
                    Nanosecond3 =>
                        time.map(|t| {
                            let nano = t.nanosecond() % 1_000_000_000;
                            write!(w, ".{:03}", nano / 1_000_000)
                        }),
                    Nanosecond6 =>
                        time.map(|t| {
                            let nano = t.nanosecond() % 1_000_000_000;
                            write!(w, ".{:06}", nano / 1_000)
                        }),
                    Nanosecond9 =>
                        time.map(|t| {
                            let nano = t.nanosecond() % 1_000_000_000;
                            write!(w, ".{:09}", nano)
                        }),
                    TimezoneName =>
                        off.map(|&(ref name, _)| write!(w, "{}", *name)),
                    TimezoneOffsetColon =>
                        off.map(|&(_, off)| write_local_minus_utc(w, off, false, true)),
                    TimezoneOffsetColonZ =>
                        off.map(|&(_, off)| write_local_minus_utc(w, off, true, true)),
                    TimezoneOffset =>
                        off.map(|&(_, off)| write_local_minus_utc(w, off, false, false)),
                    TimezoneOffsetZ =>
                        off.map(|&(_, off)| write_local_minus_utc(w, off, true, false)),
                    RFC2822 => // same to `%a, %e %b %Y %H:%M:%S %z`
                        if let (Some(d), Some(t), Some(&(_, off))) = (date, time, off) {
                            let sec = t.second() + t.nanosecond() / 1_000_000_000;
                            try!(write!(w, "{}, {:2} {} {:04} {:02}:{:02}:{:02} ",
                                        SHORT_WEEKDAYS[d.weekday().num_days_from_monday() as usize],
                                        d.day(), SHORT_MONTHS[d.month0() as usize], d.year(),
                                        t.hour(), t.minute(), sec));
                            Some(write_local_minus_utc(w, off, false, false))
                        } else {
                            None
                        },
                    RFC3339 => // same to `%Y-%m-%dT%H:%M:%S%.f%:z`
                        if let (Some(d), Some(t), Some(&(_, off))) = (date, time, off) {
                            // reuse `Debug` impls which already print ISO 8601 format.
                            // this is faster in this way.
                            try!(write!(w, "{:?}T{:?}", d, t));
                            Some(write_local_minus_utc(w, off, false, true))
                        } else {
                            None
                        },
                };

                match ret {
                    Some(ret) => try!(ret),
                    None => return Err(fmt::Error), // insufficient arguments for given format
                }
            },

            Item::Error => return Err(fmt::Error),
        }
    }

    Ok(())
}

pub mod parsed;

// due to the size of parsing routines, they are in separate modules.
mod scan;
mod parse;

pub mod strftime;

/// A *temporary* object which can be used as an argument to `format!` or others.
/// This is normally constructed via `format` methods of each date and time type.
#[derive(Debug)]
pub struct DelayedFormat<I> {
    /// The date view, if any.
    date: Option<NaiveDate>,
    /// The time view, if any.
    time: Option<NaiveTime>,
    /// The name and local-to-UTC difference for the offset (timezone), if any.
    off: Option<(String, Duration)>,
    /// An iterator returning formatting items.
    items: I,
}

impl<'a, I: Iterator<Item=Item<'a>> + Clone> DelayedFormat<I> {
    /// Makes a new `DelayedFormat` value out of local date and time.
    pub fn new(date: Option<NaiveDate>, time: Option<NaiveTime>, items: I) -> DelayedFormat<I> {
        DelayedFormat { date: date, time: time, off: None, items: items }
    }

    /// Makes a new `DelayedFormat` value out of local date and time and UTC offset.
    pub fn new_with_offset<Off>(date: Option<NaiveDate>, time: Option<NaiveTime>,
                                offset: &Off, items: I) -> DelayedFormat<I>
            where Off: Offset + fmt::Display {
        let name_and_diff = (offset.to_string(), offset.local_minus_utc());
        DelayedFormat { date: date, time: time, off: Some(name_and_diff), items: items }
    }
}

impl<'a, I: Iterator<Item=Item<'a>> + Clone> fmt::Display for DelayedFormat<I> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        format(f, self.date.as_ref(), self.time.as_ref(), self.off.as_ref(), self.items.clone())
    }
}

