// This is a part of rust-chrono.
// Copyright (c) 2015, Kang Seonghoon.
// Portions copyright (c) 2015, John Nagle.
// See README.md and LICENSE.txt for details.

//! Date and time parsing routines.

use std::usize;

use Weekday;

use super::scan;
use super::{Parsed, ParseResult, Item};
use super::{OUT_OF_RANGE, INVALID, TOO_SHORT, TOO_LONG, BAD_FORMAT};

fn set_weekday_with_num_days_from_sunday(p: &mut Parsed, v: i64) -> ParseResult<()> {
    p.set_weekday(match v {
        0 => Weekday::Sun, 1 => Weekday::Mon, 2 => Weekday::Tue,
        3 => Weekday::Wed, 4 => Weekday::Thu, 5 => Weekday::Fri,
        6 => Weekday::Sat, _ => return Err(OUT_OF_RANGE)
    })
}

fn set_weekday_with_number_from_monday(p: &mut Parsed, v: i64) -> ParseResult<()> {
    p.set_weekday(match v {
        1 => Weekday::Mon, 2 => Weekday::Tue, 3 => Weekday::Wed,
        4 => Weekday::Thu, 5 => Weekday::Fri, 6 => Weekday::Sat,
        7 => Weekday::Sun, _ => return Err(OUT_OF_RANGE)
    })
}

fn parse_rfc2822<'a>(parsed: &mut Parsed, mut s: &'a str) -> ParseResult<(&'a str, ())> {
    macro_rules! try_consume {
        ($e:expr) => ({ let (s_, v) = try!($e); s = s_; v })
    }

    // an adapted RFC 2822 syntax from Section 3.3 and 4.3:
    //
    // date-time   = [ day-of-week "," ] date 1*S time *S
    // day-of-week = *S day-name *S
    // day-name    = "Mon" / "Tue" / "Wed" / "Thu" / "Fri" / "Sat" / "Sun"
    // date        = day month year
    // day         = *S 1*2DIGIT *S
    // month       = 1*S month-name 1*S
    // month-name  = "Jan" / "Feb" / "Mar" / "Apr" / "May" / "Jun" /
    //               "Jul" / "Aug" / "Sep" / "Oct" / "Nov" / "Dec"
    // year        = *S 2*DIGIT *S
    // time        = time-of-day 1*S zone
    // time-of-day = hour ":" minute [ ":" second ]
    // hour        = *S 2DIGIT *S
    // minute      = *S 2DIGIT *S
    // second      = *S 2DIGIT *S
    // zone        = ( "+" / "-" ) 4DIGIT /
    //               "UT" / "GMT" /                  ; same to +0000
    //               "EST" / "CST" / "MST" / "PST" / ; same to -0500 to -0800
    //               "EDT" / "CDT" / "MDT" / "PDT" / ; same to -0400 to -0700
    //               1*(%d65-90 / %d97-122)          ; same to -0000
    //
    // some notes:
    //
    // - quoted characters can be in any mixture of lower and upper cases.
    //
    // - we do not recognize a folding white space (FWS) or comment (CFWS).
    //   for our purposes, instead, we accept any sequence of Unicode
    //   white space characters (denoted here to `S`). any actual RFC 2822
    //   parser is expected to parse FWS and/or CFWS themselves and replace
    //   it with a single SP (`%x20`); this is legitimate.
    //
    // - two-digit year < 50 should be interpreted by adding 2000.
    //   two-digit year >= 50 or three-digit year should be interpreted
    //   by adding 1900. note that four-or-more-digit years less than 1000
    //   are *never* affected by this rule.
    //
    // - zone of `-0000` and any unrecognized legacy time zones (including
    //   *every* one-letter military time zones) are considered "missing",
    //   in such that we don't actually know what time zone is being used.
    //
    // - mismatching day-of-week is always an error, which is consistent to
    //   Chrono's own rules.
    //
    // - zones can range from `-9959` to `+9959`, but `FixedOffset` does not
    //   support offsets larger than 24 hours. this is not *that* problematic
    //   since we do not directly go to a `DateTime` so one can recover
    //   the offset information from `Parsed` anyway.

    s = s.trim_left();

    if let Ok((s_, weekday)) = scan::short_weekday(s) {
        if !s_.starts_with(",") { return Err(INVALID); }
        s = &s_[1..];
        try!(parsed.set_weekday(weekday));
    }

    s = s.trim_left();
    try!(parsed.set_day(try_consume!(scan::number(s, 1, 2))));
    s = try!(scan::space(s)); // mandatory
    try!(parsed.set_month(1 + try_consume!(scan::short_month0(s)) as i64));
    s = try!(scan::space(s)); // mandatory

    // distinguish two- and three-digit years from four-digit years
    let prevlen = s.len();
    let mut year = try_consume!(scan::number(s, 2, usize::MAX));
    let yearlen = prevlen - s.len();
    match (yearlen, year) {
        (2,  0...49) => { year += 2000; } //   47 -> 2047,   05 -> 2005
        (2, 50...99) => { year += 1900; } //   79 -> 1979
        (3,       _) => { year += 1900; } //  112 -> 2012,  009 -> 1909
        (_,       _) => {}                // 1987 -> 1987, 0654 -> 0654
    }
    try!(parsed.set_year(year));

    s = try!(scan::space(s)); // mandatory
    try!(parsed.set_hour(try_consume!(scan::number(s, 2, 2))));
    s = try!(scan::char(s.trim_left(), b':')).trim_left(); // *S ":" *S
    try!(parsed.set_minute(try_consume!(scan::number(s, 2, 2))));
    s = s.trim_left();
    if !s.is_empty() { // [ ":" *S 2DIGIT ]
        s = try!(scan::char(s, b':')).trim_left();
        try!(parsed.set_second(try_consume!(scan::number(s, 2, 2))));
    }

    s = try!(scan::space(s)); // mandatory
    if let Some(offset) = try_consume!(scan::timezone_offset_2822(s)) {
        // only set the offset when it is definitely known (i.e. not `-0000`)
        try!(parsed.set_offset(offset as i64));
    }

    Ok((s, ()))
}

fn parse_rfc3339<'a>(parsed: &mut Parsed, mut s: &'a str) -> ParseResult<(&'a str, ())> {
    macro_rules! try_consume {
        ($e:expr) => ({ let (s_, v) = try!($e); s = s_; v })
    }

    // an adapted RFC 3339 syntax from Section 5.6:
    //
    // date-fullyear  = 4DIGIT
    // date-month     = 2DIGIT ; 01-12
    // date-mday      = 2DIGIT ; 01-28, 01-29, 01-30, 01-31 based on month/year
    // time-hour      = 2DIGIT ; 00-23
    // time-minute    = 2DIGIT ; 00-59
    // time-second    = 2DIGIT ; 00-58, 00-59, 00-60 based on leap second rules
    // time-secfrac   = "." 1*DIGIT
    // time-numoffset = ("+" / "-") time-hour ":" time-minute
    // time-offset    = "Z" / time-numoffset
    // partial-time   = time-hour ":" time-minute ":" time-second [time-secfrac]
    // full-date      = date-fullyear "-" date-month "-" date-mday
    // full-time      = partial-time time-offset
    // date-time      = full-date "T" full-time
    //
    // some notes:
    //
    // - quoted characters can be in any mixture of lower and upper cases.
    //
    // - it may accept any number of fractional digits for seconds.
    //   for Chrono, this means that we should skip digits past first 9 digits.
    //
    // - unlike RFC 2822, the valid offset ranges from -23:59 to +23:59.
    //   note that this restriction is unique to RFC 3339 and not ISO 8601.
    //   since this is not a typical Chrono behavior, we check it earlier.

    try!(parsed.set_year(try_consume!(scan::number(s, 4, 4))));
    s = try!(scan::char(s, b'-'));
    try!(parsed.set_month(try_consume!(scan::number(s, 2, 2))));
    s = try!(scan::char(s, b'-'));
    try!(parsed.set_day(try_consume!(scan::number(s, 2, 2))));

    s = match s.as_bytes().first() {
        Some(&b't') | Some(&b'T') => &s[1..],
        Some(_) => return Err(INVALID),
        None => return Err(TOO_SHORT),
    };

    try!(parsed.set_hour(try_consume!(scan::number(s, 2, 2))));
    s = try!(scan::char(s, b':'));
    try!(parsed.set_minute(try_consume!(scan::number(s, 2, 2))));
    s = try!(scan::char(s, b':'));
    try!(parsed.set_second(try_consume!(scan::number(s, 2, 2))));
    if s.starts_with(".") {
        let nanosecond = try_consume!(scan::nanosecond(&s[1..]));
        try!(parsed.set_nanosecond(nanosecond));
    }

    let offset = try_consume!(scan::timezone_offset_zulu(s, |s| scan::char(s, b':')));
    if offset <= -86400 || offset >= 86400 { return Err(OUT_OF_RANGE); }
    try!(parsed.set_offset(offset as i64));

    Ok((s, ()))
}

/// Tries to parse given string into `parsed` with given formatting items.
/// Returns `Ok` when the entire string has been parsed (otherwise `parsed` should not be used).
/// There should be no trailing string after parsing;
/// use a stray [`Item::Space`](./enum.Item.html#variant.Space) to trim whitespaces.
///
/// This particular date and time parser is:
///
/// - Greedy. It will consume the longest possible prefix.
///   For example, `April` is always consumed entirely when the long month name is requested;
///   it equally accepts `Apr`, but prefers the longer prefix in this case.
///
/// - Padding-agnostic (for numeric items).
///   The [`Pad`](./enum.Pad.html) field is completely ignored,
///   so one can prepend any number of whitespace then any number of zeroes before numbers.
///
/// - (Still) obeying the intrinsic parsing width. This allows, for example, parsing `HHMMSS`.
pub fn parse<'a, I>(parsed: &mut Parsed, mut s: &str, items: I) -> ParseResult<()>
        where I: Iterator<Item=Item<'a>> {
    macro_rules! try_consume {
        ($e:expr) => ({ let (s_, v) = try!($e); s = s_; v })
    }

    for item in items {
        match item {
            Item::Literal(prefix) => {
                if s.len() < prefix.len() { return Err(TOO_SHORT); }
                if !s.starts_with(prefix) { return Err(INVALID); }
                s = &s[prefix.len()..];
            }

            Item::Space(_) => {
                s = s.trim_left();
            }

            Item::Numeric(spec, _pad) => {
                use super::Numeric::*;

                let (width, signed, set): (usize, bool,
                                           fn(&mut Parsed, i64) -> ParseResult<()>) = match spec {
                    Year           => (4, true, Parsed::set_year),
                    YearDiv100     => (2, false, Parsed::set_year_div_100),
                    YearMod100     => (2, false, Parsed::set_year_mod_100),
                    IsoYear        => (4, true, Parsed::set_isoyear),
                    IsoYearDiv100  => (2, false, Parsed::set_isoyear_div_100),
                    IsoYearMod100  => (2, false, Parsed::set_isoyear_mod_100),
                    Month          => (2, false, Parsed::set_month),
                    Day            => (2, false, Parsed::set_day),
                    WeekFromSun    => (2, false, Parsed::set_week_from_sun),
                    WeekFromMon    => (2, false, Parsed::set_week_from_mon),
                    IsoWeek        => (2, false, Parsed::set_isoweek),
                    NumDaysFromSun => (1, false, set_weekday_with_num_days_from_sunday),
                    WeekdayFromMon => (1, false, set_weekday_with_number_from_monday),
                    Ordinal        => (3, false, Parsed::set_ordinal),
                    Hour           => (2, false, Parsed::set_hour),
                    Hour12         => (2, false, Parsed::set_hour12),
                    Minute         => (2, false, Parsed::set_minute),
                    Second         => (2, false, Parsed::set_second),
                    Nanosecond     => (9, false, Parsed::set_nanosecond),
                    Timestamp      => (usize::MAX, false, Parsed::set_timestamp),
                };

                s = s.trim_left();
                let v = if signed {
                    if s.starts_with("-") {
                        let v = try_consume!(scan::number(&s[1..], 1, usize::MAX));
                        try!(0i64.checked_sub(v).ok_or(OUT_OF_RANGE))
                    } else if s.starts_with("+") {
                        try_consume!(scan::number(&s[1..], 1, usize::MAX))
                    } else {
                        // if there is no explicit sign, we respect the original `width`
                        try_consume!(scan::number(s, 1, width))
                    }
                } else {
                    try_consume!(scan::number(s, 1, width))
                };
                try!(set(parsed, v));
            }

            Item::Fixed(spec) => {
                use super::Fixed::*;

                match spec {
                    ShortMonthName => {
                        let month0 = try_consume!(scan::short_month0(s));
                        try!(parsed.set_month(month0 as i64 + 1));
                    }

                    LongMonthName => {
                        let month0 = try_consume!(scan::short_or_long_month0(s));
                        try!(parsed.set_month(month0 as i64 + 1));
                    }

                    ShortWeekdayName => {
                        let weekday = try_consume!(scan::short_weekday(s));
                        try!(parsed.set_weekday(weekday));
                    }

                    LongWeekdayName => {
                        let weekday = try_consume!(scan::short_or_long_weekday(s));
                        try!(parsed.set_weekday(weekday));
                    }

                    LowerAmPm | UpperAmPm => {
                        if s.len() < 2 { return Err(TOO_SHORT); }
                        let ampm = match (s.as_bytes()[0] | 32, s.as_bytes()[1] | 32) {
                            (b'a',b'm') => false,
                            (b'p',b'm') => true,
                            _ => return Err(INVALID)
                        };
                        try!(parsed.set_ampm(ampm));
                        s = &s[2..];
                    }

                    Nanosecond | Nanosecond3 | Nanosecond6 | Nanosecond9=> {
                        if s.starts_with(".") {
                            let nano = try_consume!(scan::nanosecond(&s[1..]));
                            try!(parsed.set_nanosecond(nano));
                        }
                    }

                    TimezoneName => return Err(BAD_FORMAT),

                    TimezoneOffsetColon | TimezoneOffset => {
                        let offset = try_consume!(scan::timezone_offset(s.trim_left(),
                                                                        scan::colon_or_space));
                        try!(parsed.set_offset(offset as i64));
                    }

                    TimezoneOffsetColonZ | TimezoneOffsetZ => {
                        let offset = try_consume!(scan::timezone_offset_zulu(s.trim_left(),
                                                                             scan::colon_or_space));
                        try!(parsed.set_offset(offset as i64));
                    }

                    RFC2822 => try_consume!(parse_rfc2822(parsed, s)),
                    RFC3339 => try_consume!(parse_rfc3339(parsed, s)),
                }
            }

            Item::Error => {
                return Err(BAD_FORMAT);
            }
        }
    }

    // if there are trailling chars, it is an error
    if !s.is_empty() {
        Err(TOO_LONG)
    } else {
        Ok(())
    }
}

#[cfg(test)]
#[test]
fn test_parse() {
    use super::*;
    use super::IMPOSSIBLE;

    // workaround for Rust issue #22255
    fn parse_all(s: &str, items: &[Item]) -> ParseResult<Parsed> {
        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, s, items.iter().cloned()));
        Ok(parsed)
    }

    macro_rules! check {
        ($fmt:expr, $items:expr; $err:tt) => (
            assert_eq!(parse_all($fmt, &$items), Err($err))
        );
        ($fmt:expr, $items:expr; $($k:ident: $v:expr),*) => (
            assert_eq!(parse_all($fmt, &$items), Ok(Parsed { $($k: Some($v),)* ..Parsed::new() }))
        );
    }

    // empty string
    check!("",  []; );
    check!(" ", []; TOO_LONG);
    check!("a", []; TOO_LONG);

    // whitespaces
    check!("",          [sp!("")]; );
    check!(" ",         [sp!("")]; );
    check!("\t",        [sp!("")]; );
    check!(" \n\r  \n", [sp!("")]; );
    check!("a",         [sp!("")]; TOO_LONG);

    // literal
    check!("",    [lit!("a")]; TOO_SHORT);
    check!(" ",   [lit!("a")]; INVALID);
    check!("a",   [lit!("a")]; );
    check!("aa",  [lit!("a")]; TOO_LONG);
    check!("A",   [lit!("a")]; INVALID);
    check!("xy",  [lit!("xy")]; );
    check!("xy",  [lit!("x"), lit!("y")]; );
    check!("x y", [lit!("x"), lit!("y")]; INVALID);
    check!("xy",  [lit!("x"), sp!(""), lit!("y")]; );
    check!("x y", [lit!("x"), sp!(""), lit!("y")]; );

    // numeric
    check!("1987",        [num!(Year)]; year: 1987);
    check!("1987 ",       [num!(Year)]; TOO_LONG);
    check!("0x12",        [num!(Year)]; TOO_LONG); // `0` is parsed
    check!("x123",        [num!(Year)]; INVALID);
    check!("2015",        [num!(Year)]; year: 2015);
    check!("0000",        [num!(Year)]; year:    0);
    check!("9999",        [num!(Year)]; year: 9999);
    check!(" \t987",      [num!(Year)]; year:  987);
    check!("5",           [num!(Year)]; year:    5);
    check!("5\0",         [num!(Year)]; TOO_LONG);
    check!("\05",         [num!(Year)]; INVALID);
    check!("",            [num!(Year)]; TOO_SHORT);
    check!("12345",       [num!(Year), lit!("5")]; year: 1234);
    check!("12345",       [nums!(Year), lit!("5")]; year: 1234);
    check!("12345",       [num0!(Year), lit!("5")]; year: 1234);
    check!("12341234",    [num!(Year), num!(Year)]; year: 1234);
    check!("1234 1234",   [num!(Year), num!(Year)]; year: 1234);
    check!("1234 1235",   [num!(Year), num!(Year)]; IMPOSSIBLE);
    check!("1234 1234",   [num!(Year), lit!("x"), num!(Year)]; INVALID);
    check!("1234x1234",   [num!(Year), lit!("x"), num!(Year)]; year: 1234);
    check!("1234xx1234",  [num!(Year), lit!("x"), num!(Year)]; INVALID);
    check!("1234 x 1234", [num!(Year), lit!("x"), num!(Year)]; INVALID);

    // signed numeric
    check!("-42",         [num!(Year)]; year: -42);
    check!("+42",         [num!(Year)]; year: 42);
    check!("-0042",       [num!(Year)]; year: -42);
    check!("+0042",       [num!(Year)]; year: 42);
    check!("-42195",      [num!(Year)]; year: -42195);
    check!("+42195",      [num!(Year)]; year: 42195);
    check!("  -42195",    [num!(Year)]; year: -42195);
    check!("  +42195",    [num!(Year)]; year: 42195);
    check!("  -   42",    [num!(Year)]; INVALID);
    check!("  +   42",    [num!(Year)]; INVALID);
    check!("-",           [num!(Year)]; TOO_SHORT);
    check!("+",           [num!(Year)]; TOO_SHORT);

    // unsigned numeric
    check!("345",   [num!(Ordinal)]; ordinal: 345);
    check!("+345",  [num!(Ordinal)]; INVALID);
    check!("-345",  [num!(Ordinal)]; INVALID);
    check!(" 345",  [num!(Ordinal)]; ordinal: 345);
    check!(" +345", [num!(Ordinal)]; INVALID);
    check!(" -345", [num!(Ordinal)]; INVALID);

    // various numeric fields
    check!("1234 5678",
           [num!(Year), num!(IsoYear)];
           year: 1234, isoyear: 5678);
    check!("12 34 56 78",
           [num!(YearDiv100), num!(YearMod100), num!(IsoYearDiv100), num!(IsoYearMod100)];
           year_div_100: 12, year_mod_100: 34, isoyear_div_100: 56, isoyear_mod_100: 78);
    check!("1 2 3 4 5 6",
           [num!(Month), num!(Day), num!(WeekFromSun), num!(WeekFromMon), num!(IsoWeek),
            num!(NumDaysFromSun)];
           month: 1, day: 2, week_from_sun: 3, week_from_mon: 4, isoweek: 5, weekday: Weekday::Sat);
    check!("7 89 01",
           [num!(WeekdayFromMon), num!(Ordinal), num!(Hour12)];
           weekday: Weekday::Sun, ordinal: 89, hour_mod_12: 1);
    check!("23 45 6 78901234 567890123",
           [num!(Hour), num!(Minute), num!(Second), num!(Nanosecond), num!(Timestamp)];
           hour_div_12: 1, hour_mod_12: 11, minute: 45, second: 6, nanosecond: 78_901_234,
           timestamp: 567_890_123);

    // fixed: month and weekday names
    check!("apr",       [fix!(ShortMonthName)]; month: 4);
    check!("Apr",       [fix!(ShortMonthName)]; month: 4);
    check!("APR",       [fix!(ShortMonthName)]; month: 4);
    check!("ApR",       [fix!(ShortMonthName)]; month: 4);
    check!("April",     [fix!(ShortMonthName)]; TOO_LONG); // `Apr` is parsed
    check!("A",         [fix!(ShortMonthName)]; TOO_SHORT);
    check!("Sol",       [fix!(ShortMonthName)]; INVALID);
    check!("Apr",       [fix!(LongMonthName)]; month: 4);
    check!("Apri",      [fix!(LongMonthName)]; TOO_LONG); // `Apr` is parsed
    check!("April",     [fix!(LongMonthName)]; month: 4);
    check!("Aprill",    [fix!(LongMonthName)]; TOO_LONG);
    check!("Aprill",    [fix!(LongMonthName), lit!("l")]; month: 4);
    check!("Aprl",      [fix!(LongMonthName), lit!("l")]; month: 4);
    check!("April",     [fix!(LongMonthName), lit!("il")]; TOO_SHORT); // do not backtrack
    check!("thu",       [fix!(ShortWeekdayName)]; weekday: Weekday::Thu);
    check!("Thu",       [fix!(ShortWeekdayName)]; weekday: Weekday::Thu);
    check!("THU",       [fix!(ShortWeekdayName)]; weekday: Weekday::Thu);
    check!("tHu",       [fix!(ShortWeekdayName)]; weekday: Weekday::Thu);
    check!("Thursday",  [fix!(ShortWeekdayName)]; TOO_LONG); // `Thu` is parsed
    check!("T",         [fix!(ShortWeekdayName)]; TOO_SHORT);
    check!("The",       [fix!(ShortWeekdayName)]; INVALID);
    check!("Nop",       [fix!(ShortWeekdayName)]; INVALID);
    check!("Thu",       [fix!(LongWeekdayName)]; weekday: Weekday::Thu);
    check!("Thur",      [fix!(LongWeekdayName)]; TOO_LONG); // `Thu` is parsed
    check!("Thurs",     [fix!(LongWeekdayName)]; TOO_LONG); // ditto
    check!("Thursday",  [fix!(LongWeekdayName)]; weekday: Weekday::Thu);
    check!("Thursdays", [fix!(LongWeekdayName)]; TOO_LONG);
    check!("Thursdays", [fix!(LongWeekdayName), lit!("s")]; weekday: Weekday::Thu);
    check!("Thus",      [fix!(LongWeekdayName), lit!("s")]; weekday: Weekday::Thu);
    check!("Thursday",  [fix!(LongWeekdayName), lit!("rsday")]; TOO_SHORT); // do not backtrack

    // fixed: am/pm
    check!("am",  [fix!(LowerAmPm)]; hour_div_12: 0);
    check!("pm",  [fix!(LowerAmPm)]; hour_div_12: 1);
    check!("AM",  [fix!(LowerAmPm)]; hour_div_12: 0);
    check!("PM",  [fix!(LowerAmPm)]; hour_div_12: 1);
    check!("am",  [fix!(UpperAmPm)]; hour_div_12: 0);
    check!("pm",  [fix!(UpperAmPm)]; hour_div_12: 1);
    check!("AM",  [fix!(UpperAmPm)]; hour_div_12: 0);
    check!("PM",  [fix!(UpperAmPm)]; hour_div_12: 1);
    check!("Am",  [fix!(LowerAmPm)]; hour_div_12: 0);
    check!(" Am", [fix!(LowerAmPm)]; INVALID);
    check!("ame", [fix!(LowerAmPm)]; TOO_LONG); // `am` is parsed
    check!("a",   [fix!(LowerAmPm)]; TOO_SHORT);
    check!("p",   [fix!(LowerAmPm)]; TOO_SHORT);
    check!("x",   [fix!(LowerAmPm)]; TOO_SHORT);
    check!("xx",  [fix!(LowerAmPm)]; INVALID);
    check!("",    [fix!(LowerAmPm)]; TOO_SHORT);

    // fixed: dot plus nanoseconds
    check!("",              [fix!(Nanosecond)]; ); // no field set, but not an error
    check!("4",             [fix!(Nanosecond)]; TOO_LONG); // never consumes `4`
    check!("4",             [fix!(Nanosecond), num!(Second)]; second: 4);
    check!(".0",            [fix!(Nanosecond)]; nanosecond: 0);
    check!(".4",            [fix!(Nanosecond)]; nanosecond: 400_000_000);
    check!(".42",           [fix!(Nanosecond)]; nanosecond: 420_000_000);
    check!(".421",          [fix!(Nanosecond)]; nanosecond: 421_000_000);
    check!(".42195",        [fix!(Nanosecond)]; nanosecond: 421_950_000);
    check!(".421950803",    [fix!(Nanosecond)]; nanosecond: 421_950_803);
    check!(".421950803547", [fix!(Nanosecond)]; nanosecond: 421_950_803);
    check!(".000000003547", [fix!(Nanosecond)]; nanosecond: 3);
    check!(".000000000547", [fix!(Nanosecond)]; nanosecond: 0);
    check!(".",             [fix!(Nanosecond)]; TOO_SHORT);
    check!(".4x",           [fix!(Nanosecond)]; TOO_LONG);
    check!(".  4",          [fix!(Nanosecond)]; INVALID);
    check!("  .4",          [fix!(Nanosecond)]; TOO_LONG); // no automatic trimming

    // fixed: timezone offsets
    check!("+00:00",    [fix!(TimezoneOffset)]; offset: 0);
    check!("-00:00",    [fix!(TimezoneOffset)]; offset: 0);
    check!("+00:01",    [fix!(TimezoneOffset)]; offset: 60);
    check!("-00:01",    [fix!(TimezoneOffset)]; offset: -60);
    check!("+00:30",    [fix!(TimezoneOffset)]; offset: 30 * 60);
    check!("-00:30",    [fix!(TimezoneOffset)]; offset: -30 * 60);
    check!("+04:56",    [fix!(TimezoneOffset)]; offset: 296 * 60);
    check!("-04:56",    [fix!(TimezoneOffset)]; offset: -296 * 60);
    check!("+24:00",    [fix!(TimezoneOffset)]; offset: 24 * 60 * 60);
    check!("-24:00",    [fix!(TimezoneOffset)]; offset: -24 * 60 * 60);
    check!("+99:59",    [fix!(TimezoneOffset)]; offset: (100 * 60 - 1) * 60);
    check!("-99:59",    [fix!(TimezoneOffset)]; offset: -(100 * 60 - 1) * 60);
    check!("+00:59",    [fix!(TimezoneOffset)]; offset: 59 * 60);
    check!("+00:60",    [fix!(TimezoneOffset)]; OUT_OF_RANGE);
    check!("+00:99",    [fix!(TimezoneOffset)]; OUT_OF_RANGE);
    check!("#12:34",    [fix!(TimezoneOffset)]; INVALID);
    check!("12:34",     [fix!(TimezoneOffset)]; INVALID);
    check!("+12:34 ",   [fix!(TimezoneOffset)]; TOO_LONG);
    check!(" +12:34",   [fix!(TimezoneOffset)]; offset: 754 * 60);
    check!("\t -12:34", [fix!(TimezoneOffset)]; offset: -754 * 60);
    check!("",          [fix!(TimezoneOffset)]; TOO_SHORT);
    check!("+",         [fix!(TimezoneOffset)]; TOO_SHORT);
    check!("+1",        [fix!(TimezoneOffset)]; TOO_SHORT);
    check!("+12",       [fix!(TimezoneOffset)]; TOO_SHORT);
    check!("+123",      [fix!(TimezoneOffset)]; TOO_SHORT);
    check!("+1234",     [fix!(TimezoneOffset)]; offset: 754 * 60);
    check!("+12345",    [fix!(TimezoneOffset)]; TOO_LONG);
    check!("+12345",    [fix!(TimezoneOffset), num!(Day)]; offset: 754 * 60, day: 5);
    check!("Z",         [fix!(TimezoneOffset)]; INVALID);
    check!("z",         [fix!(TimezoneOffset)]; INVALID);
    check!("Z",         [fix!(TimezoneOffsetZ)]; offset: 0);
    check!("z",         [fix!(TimezoneOffsetZ)]; offset: 0);
    check!("Y",         [fix!(TimezoneOffsetZ)]; INVALID);
    check!("Zulu",      [fix!(TimezoneOffsetZ), lit!("ulu")]; offset: 0);
    check!("zulu",      [fix!(TimezoneOffsetZ), lit!("ulu")]; offset: 0);
    check!("+1234ulu",  [fix!(TimezoneOffsetZ), lit!("ulu")]; offset: 754 * 60);
    check!("+12:34ulu", [fix!(TimezoneOffsetZ), lit!("ulu")]; offset: 754 * 60);
    check!("???",       [fix!(TimezoneName)]; BAD_FORMAT); // not allowed

    // some practical examples
    check!("2015-02-04T14:37:05+09:00",
           [num!(Year), lit!("-"), num!(Month), lit!("-"), num!(Day), lit!("T"),
            num!(Hour), lit!(":"), num!(Minute), lit!(":"), num!(Second), fix!(TimezoneOffset)];
           year: 2015, month: 2, day: 4, hour_div_12: 1, hour_mod_12: 2,
           minute: 37, second: 5, offset: 32400);
    check!("Mon, 10 Jun 2013 09:32:37 GMT",
           [fix!(ShortWeekdayName), lit!(","), sp!(" "), num!(Day), sp!(" "),
            fix!(ShortMonthName), sp!(" "), num!(Year), sp!(" "), num!(Hour), lit!(":"),
            num!(Minute), lit!(":"), num!(Second), sp!(" "), lit!("GMT")];
           year: 2013, month: 6, day: 10, weekday: Weekday::Mon,
           hour_div_12: 0, hour_mod_12: 9, minute: 32, second: 37);
    check!("20060102150405",
           [num!(Year), num!(Month), num!(Day), num!(Hour), num!(Minute), num!(Second)];
           year: 2006, month: 1, day: 2, hour_div_12: 1, hour_mod_12: 3, minute: 4, second: 5);
    check!("3:14PM",
           [num!(Hour12), lit!(":"), num!(Minute), fix!(LowerAmPm)];
           hour_div_12: 1, hour_mod_12: 3, minute: 14);
    check!("12345678901234.56789",
           [num!(Timestamp), lit!("."), num!(Nanosecond)];
           nanosecond: 56_789, timestamp: 12_345_678_901_234);
    check!("12345678901234.56789",
           [num!(Timestamp), fix!(Nanosecond)];
           nanosecond: 567_890_000, timestamp: 12_345_678_901_234);
}

#[cfg(test)]
#[test]
fn test_rfc2822() {
    use datetime::DateTime;
    use offset::fixed::FixedOffset;
    use super::*;
    use super::NOT_ENOUGH;

    // Test data - (input, Ok(expected result after parse and format) or Err(error code))
    let testdates = [
        ("Tue, 20 Jan 2015 17:35:20 -0800", Ok("Tue, 20 Jan 2015 17:35:20 -0800")), // normal case
        ("20 Jan 2015 17:35:20 -0800", Ok("Tue, 20 Jan 2015 17:35:20 -0800")),  // no day of week
        ("20 JAN 2015 17:35:20 -0800", Ok("Tue, 20 Jan 2015 17:35:20 -0800")),  // upper case month
        ("11 Sep 2001 09:45:00 EST", Ok("Tue, 11 Sep 2001 09:45:00 -0500")),
        ("30 Feb 2015 17:35:20 -0800", Err(OUT_OF_RANGE)),      // bad day of month
        ("Tue, 20 Jan 2015", Err(TOO_SHORT)),                   // omitted fields
        ("Tue, 20 Avr 2015 17:35:20 -0800", Err(INVALID)),      // bad month name
        ("Tue, 20 Jan 2015 25:35:20 -0800", Err(OUT_OF_RANGE)), // bad hour
        ("Tue, 20 Jan 2015 7:35:20 -0800", Err(INVALID)),       // bad # of digits in hour
        ("Tue, 20 Jan 2015 17:65:20 -0800", Err(OUT_OF_RANGE)), // bad minute
        ("Tue, 20 Jan 2015 17:35:90 -0800", Err(OUT_OF_RANGE)), // bad second
        ("Tue, 20 Jan 2015 17:35:20 -0890", Err(OUT_OF_RANGE)), // bad offset
        ("6 Jun 1944 04:00:00Z", Err(INVALID)),                 // bad offset (zulu not allowed)
        ("Tue, 20 Jan 2015 17:35:20 HAS", Err(NOT_ENOUGH))      // bad named time zone
    ];

    fn rfc2822_to_datetime(date: &str) -> ParseResult<DateTime<FixedOffset>> {
        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, date, [Item::Fixed(Fixed::RFC2822)].iter().cloned()));
        parsed.to_datetime()
    }

    fn fmt_rfc2822_datetime(dt: DateTime<FixedOffset>) -> String {
        dt.format_with_items([Item::Fixed(Fixed::RFC2822)].iter().cloned()).to_string()
    }

    // Test against test data above
    for &(date, checkdate) in testdates.iter() {
        let d = rfc2822_to_datetime(date);          // parse a date
        let dt = match d {                          // did we get a value?
            Ok(dt) => Ok(fmt_rfc2822_datetime(dt)), // yes, go on
            Err(e) => Err(e),                       // otherwise keep an error for the comparison
        };
        if dt != checkdate.map(|s| s.to_string()) { // check for expected result
            panic!("Date conversion failed for {}\nReceived: {:?}\nExpected: {:?}",
                   date, dt, checkdate);
        }
    };
}



#[cfg(test)]
#[test]
fn parse_rfc850() {
    use ::{UTC, TimeZone};

    static RFC850_FMT: &'static str =  "%A, %d-%b-%y %T GMT";

    let dt_str = "Sunday, 06-Nov-94 08:49:37 GMT";
    let dt = UTC.ymd(1994, 11, 6).and_hms(8, 49, 37);

    // Check that the format is what we expect
    assert_eq!(dt.format(RFC850_FMT).to_string(), dt_str);

    // Check that it parses correctly
    assert_eq!(Ok(dt), UTC.datetime_from_str("Sunday, 06-Nov-94 08:49:37 GMT", RFC850_FMT));

    // Check that the rest of the weekdays parse correctly (this test originally failed because
    // Sunday parsed incorrectly).
    let testdates = [
        (UTC.ymd(1994, 11, 7).and_hms(8, 49, 37),  "Monday, 07-Nov-94 08:49:37 GMT"),
        (UTC.ymd(1994, 11, 8).and_hms(8, 49, 37),  "Tuesday, 08-Nov-94 08:49:37 GMT"),
        (UTC.ymd(1994, 11, 9).and_hms(8, 49, 37),  "Wednesday, 09-Nov-94 08:49:37 GMT"),
        (UTC.ymd(1994, 11, 10).and_hms(8, 49, 37), "Thursday, 10-Nov-94 08:49:37 GMT"),
        (UTC.ymd(1994, 11, 11).and_hms(8, 49, 37), "Friday, 11-Nov-94 08:49:37 GMT"),
        (UTC.ymd(1994, 11, 12).and_hms(8, 49, 37), "Saturday, 12-Nov-94 08:49:37 GMT"),
    ];

    for val in &testdates {
        assert_eq!(Ok(val.0), UTC.datetime_from_str(val.1, RFC850_FMT));
    }
}

#[cfg(test)]
#[test]
fn test_rfc3339() {
    use datetime::DateTime;
    use offset::fixed::FixedOffset;
    use super::*;

    // Test data - (input, Ok(expected result after parse and format) or Err(error code))
    let testdates = [
        ("2015-01-20T17:35:20-08:00", Ok("2015-01-20T17:35:20-08:00")), // normal case
        ("1944-06-06T04:04:00Z", Ok("1944-06-06T04:04:00+00:00")),      // D-day
        ("2001-09-11T09:45:00-08:00", Ok("2001-09-11T09:45:00-08:00")),
        ("2015-01-20T17:35:20.001-08:00", Ok("2015-01-20T17:35:20.001-08:00")),
        ("2015-01-20T17:35:20.000031-08:00", Ok("2015-01-20T17:35:20.000031-08:00")),
        ("2015-01-20T17:35:20.000000004-08:00", Ok("2015-01-20T17:35:20.000000004-08:00")),
        ("2015-01-20T17:35:20.000000000452-08:00", Ok("2015-01-20T17:35:20-08:00")), // too small
        ("2015-02-30T17:35:20-08:00", Err(OUT_OF_RANGE)),               // bad day of month
        ("2015-01-20T25:35:20-08:00", Err(OUT_OF_RANGE)),               // bad hour
        ("2015-01-20T17:65:20-08:00", Err(OUT_OF_RANGE)),               // bad minute
        ("2015-01-20T17:35:90-08:00", Err(OUT_OF_RANGE)),               // bad second
        ("2015-01-20T17:35:20-24:00", Err(OUT_OF_RANGE)),               // bad offset
    ];

    fn rfc3339_to_datetime(date: &str) -> ParseResult<DateTime<FixedOffset>> {
        let mut parsed = Parsed::new();
        try!(parse(&mut parsed, date, [Item::Fixed(Fixed::RFC3339)].iter().cloned()));
        parsed.to_datetime()
    }

    fn fmt_rfc3339_datetime(dt: DateTime<FixedOffset>) -> String {
        dt.format_with_items([Item::Fixed(Fixed::RFC3339)].iter().cloned()).to_string()
    }

    // Test against test data above
    for &(date, checkdate) in testdates.iter() {
        let d = rfc3339_to_datetime(date);          // parse a date
        let dt = match d {                          // did we get a value?
            Ok(dt) => Ok(fmt_rfc3339_datetime(dt)), // yes, go on
            Err(e) => Err(e),                       // otherwise keep an error for the comparison
        };
        if dt != checkdate.map(|s| s.to_string()) { // check for expected result
            panic!("Date conversion failed for {}\nReceived: {:?}\nExpected: {:?}",
                   date, dt, checkdate);
        }
    };
}

