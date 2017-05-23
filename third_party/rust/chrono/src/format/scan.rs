// This is a part of rust-chrono.
// Copyright (c) 2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

/*!
 * Various scanning routines for the parser.
 */

use Weekday;
use super::{ParseResult, TOO_SHORT, INVALID, OUT_OF_RANGE};

/// Returns true when two slices are equal case-insensitively (in ASCII).
/// Assumes that the `pattern` is already converted to lower case.
fn equals(s: &str, pattern: &str) -> bool {
    let mut xs = s.as_bytes().iter().map(|&c| match c { b'A'...b'Z' => c + 32, _ => c });
    let mut ys = pattern.as_bytes().iter().cloned();
    loop {
        match (xs.next(), ys.next()) {
            (None, None) => return true,
            (None, _) | (_, None) => return false,
            (Some(x), Some(y)) if x != y => return false,
            _ => (),
        }
    }
}

/// Tries to parse the non-negative number from `min` to `max` digits.
///
/// The absence of digits at all is an unconditional error.
/// More than `max` digits are consumed up to the first `max` digits.
/// Any number that does not fit in `i64` is an error.
pub fn number(s: &str, min: usize, max: usize) -> ParseResult<(&str, i64)> {
    assert!(min <= max);

    // limit `s` to given number of digits
    let mut window = s.as_bytes();
    if window.len() > max { window = &window[..max]; }

    // scan digits
    let upto = window.iter().position(|&c| c < b'0' || b'9' < c).unwrap_or(window.len());
    if upto < min {
        return Err(if window.is_empty() {TOO_SHORT} else {INVALID});
    }

    // we can overflow here, which is the only possible cause of error from `parse`.
    let v: i64 = try!(s[..upto].parse().map_err(|_| OUT_OF_RANGE));
    Ok((&s[upto..], v))
}

/// Tries to consume at least one digits as a fractional second.
/// Returns the number of whole nanoseconds (0--999,999,999).
pub fn nanosecond(s: &str) -> ParseResult<(&str, i64)> {
    // record the number of digits consumed for later scaling.
    let origlen = s.len();
    let (s, v) = try!(number(s, 1, 9));
    let consumed = origlen - s.len();

    // scale the number accordingly.
    static SCALE: [i64; 10] = [0, 100_000_000, 10_000_000, 1_000_000, 100_000, 10_000,
                               1_000, 100, 10, 1];
    let v = try!(v.checked_mul(SCALE[consumed]).ok_or(OUT_OF_RANGE));

    // if there are more than 9 digits, skip next digits.
    let s = s.trim_left_matches(|c: char| '0' <= c && c <= '9');

    Ok((s, v))
}

/// Tries to parse the month index (0 through 11) with the first three ASCII letters.
pub fn short_month0(s: &str) -> ParseResult<(&str, u8)> {
    if s.len() < 3 { return Err(TOO_SHORT); }
    let buf = s.as_bytes();
    let month0 = match (buf[0] | 32, buf[1] | 32, buf[2] | 32) {
        (b'j',b'a',b'n') => 0,
        (b'f',b'e',b'b') => 1,
        (b'm',b'a',b'r') => 2,
        (b'a',b'p',b'r') => 3,
        (b'm',b'a',b'y') => 4,
        (b'j',b'u',b'n') => 5,
        (b'j',b'u',b'l') => 6,
        (b'a',b'u',b'g') => 7,
        (b's',b'e',b'p') => 8,
        (b'o',b'c',b't') => 9,
        (b'n',b'o',b'v') => 10,
        (b'd',b'e',b'c') => 11,
        _ => return Err(INVALID)
    };
    Ok((&s[3..], month0))
}

/// Tries to parse the weekday with the first three ASCII letters.
pub fn short_weekday(s: &str) -> ParseResult<(&str, Weekday)> {
    if s.len() < 3 { return Err(TOO_SHORT); }
    let buf = s.as_bytes();
    let weekday = match (buf[0] | 32, buf[1] | 32, buf[2] | 32) {
        (b'm',b'o',b'n') => Weekday::Mon,
        (b't',b'u',b'e') => Weekday::Tue,
        (b'w',b'e',b'd') => Weekday::Wed,
        (b't',b'h',b'u') => Weekday::Thu,
        (b'f',b'r',b'i') => Weekday::Fri,
        (b's',b'a',b't') => Weekday::Sat,
        (b's',b'u',b'n') => Weekday::Sun,
        _ => return Err(INVALID)
    };
    Ok((&s[3..], weekday))
}

/// Tries to parse the month index (0 through 11) with short or long month names.
/// It prefers long month names to short month names when both are possible.
pub fn short_or_long_month0(s: &str) -> ParseResult<(&str, u8)> {
    // lowercased month names, minus first three chars
    static LONG_MONTH_SUFFIXES: [&'static str; 12] =
        ["uary", "ruary", "ch", "il", "", "e", "y", "ust", "tember", "ober", "ember", "ember"];

    let (mut s, month0) = try!(short_month0(s));

    // tries to consume the suffix if possible
    let suffix = LONG_MONTH_SUFFIXES[month0 as usize];
    if s.len() >= suffix.len() && equals(&s[..suffix.len()], suffix) {
        s = &s[suffix.len()..];
    }

    Ok((s, month0))
}

/// Tries to parse the weekday with short or long weekday names.
/// It prefers long weekday names to short weekday names when both are possible.
pub fn short_or_long_weekday(s: &str) -> ParseResult<(&str, Weekday)> {
    // lowercased weekday names, minus first three chars
    static LONG_WEEKDAY_SUFFIXES: [&'static str; 7] =
        ["day", "sday", "nesday", "rsday", "day", "urday", "day"];

    let (mut s, weekday) = try!(short_weekday(s));

    // tries to consume the suffix if possible
    let suffix = LONG_WEEKDAY_SUFFIXES[weekday.num_days_from_monday() as usize];
    if s.len() >= suffix.len() && equals(&s[..suffix.len()], suffix) {
        s = &s[suffix.len()..];
    }

    Ok((s, weekday))
}

/// Tries to consume exactly one given character.
pub fn char(s: &str, c1: u8) -> ParseResult<&str> {
    match s.as_bytes().first() {
        Some(&c) if c == c1 => Ok(&s[1..]),
        Some(_) => Err(INVALID),
        None => Err(TOO_SHORT),
    }
}

/// Tries to consume one or more whitespace.
pub fn space(s: &str) -> ParseResult<&str> {
    let s_ = s.trim_left();
    if s_.len() < s.len() {
        Ok(s_)
    } else if s.is_empty() {
        Err(TOO_SHORT)
    } else {
        Err(INVALID)
    }
}

/// Consumes any number (including zero) of colon or spaces.
pub fn colon_or_space(s: &str) -> ParseResult<&str> {
    Ok(s.trim_left_matches(|c: char| c == ':' || c.is_whitespace()))
}

/// Tries to parse `[-+]\d\d` continued by `\d\d`. Return an offset in seconds if possible.
///
/// The additional `colon` may be used to parse a mandatory or optional `:`
/// between hours and minutes, and should return either a new suffix or `Err` when parsing fails.
pub fn timezone_offset<F>(mut s: &str, mut colon: F) -> ParseResult<(&str, i32)>
        where F: FnMut(&str) -> ParseResult<&str> {
    fn digits(s: &str) -> ParseResult<(u8, u8)> {
        let b = s.as_bytes();
        if b.len() < 2 {
            Err(TOO_SHORT)
        } else {
            Ok((b[0], b[1]))
        }
    }
    let negative = match s.as_bytes().first() {
        Some(&b'+') => false,
        Some(&b'-') => true,
        Some(_) => return Err(INVALID),
        None => return Err(TOO_SHORT),
    };
    s = &s[1..];

    // hours (00--99)
    let hours = match try!(digits(s)) {
        (h1 @ b'0'...b'9', h2 @ b'0'...b'9') => ((h1 - b'0') * 10 + (h2 - b'0')) as i32,
        _ => return Err(INVALID),
    };
    s = &s[2..];

    // colons (and possibly other separators)
    s = try!(colon(s));

    // minutes (00--59)
    let minutes = match try!(digits(s)) {
        (m1 @ b'0'...b'5', m2 @ b'0'...b'9') => ((m1 - b'0') * 10 + (m2 - b'0')) as i32,
        (b'6'...b'9', b'0'...b'9') => return Err(OUT_OF_RANGE),
        _ => return Err(INVALID),
    };
    s = &s[2..];

    let seconds = hours * 3600 + minutes * 60;
    Ok((s, if negative {-seconds} else {seconds}))
}

/// Same to `timezone_offset` but also allows for `z`/`Z` which is same to `+00:00`.
pub fn timezone_offset_zulu<F>(s: &str, colon: F) -> ParseResult<(&str, i32)>
        where F: FnMut(&str) -> ParseResult<&str> {
    match s.as_bytes().first() {
        Some(&b'z') | Some(&b'Z') => Ok((&s[1..], 0)),
        _ => timezone_offset(s, colon),
    }
}

/// Same to `timezone_offset` but also allows for RFC 2822 legacy timezones.
/// May return `None` which indicates an insufficient offset data (i.e. `-0000`).
pub fn timezone_offset_2822(s: &str) -> ParseResult<(&str, Option<i32>)> {
    // tries to parse legacy time zone names
    let upto = s.as_bytes().iter().position(|&c| match c { b'a'...b'z' | b'A'...b'Z' => false,
                                                           _ => true }).unwrap_or(s.len());
    if upto > 0 {
        let name = &s[..upto];
        let s = &s[upto..];
        if equals(name, "gmt") || equals(name, "ut") {
            Ok((s, Some(0)))
        } else if equals(name, "est") {
            Ok((s, Some(-5 * 3600)))
        } else if equals(name, "edt") {
            Ok((s, Some(-4 * 3600)))
        } else if equals(name, "cst") {
            Ok((s, Some(-6 * 3600)))
        } else if equals(name, "cdt") {
            Ok((s, Some(-5 * 3600)))
        } else if equals(name, "mst") {
            Ok((s, Some(-7 * 3600)))
        } else if equals(name, "mdt") {
            Ok((s, Some(-6 * 3600)))
        } else if equals(name, "pst") {
            Ok((s, Some(-8 * 3600)))
        } else if equals(name, "pdt") {
            Ok((s, Some(-7 * 3600)))
        } else {
            Ok((s, None)) // recommended by RFC 2822: consume but treat it as -0000
        }
    } else {
        let (s_, offset) = try!(timezone_offset(s, |s| Ok(s)));
        if offset == 0 && s.starts_with("-") { // -0000 is not same to +0000
            Ok((s_, None))
        } else {
            Ok((s_, Some(offset)))
        }
    }
}

