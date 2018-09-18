// This is a part of rust-chrono.
// Copyright (c) 2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

/*!
 * The time zone which has a fixed offset from UTC.
 */

use std::fmt;

use div::div_mod_floor;
use duration::Duration;
use naive::date::NaiveDate;
use naive::datetime::NaiveDateTime;
use super::{TimeZone, Offset, LocalResult};

/// The time zone with fixed offset, from UTC-23:59:59 to UTC+23:59:59.
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct FixedOffset {
    local_minus_utc: i32,
}

impl FixedOffset {
    /// Makes a new `FixedOffset` from the serialized representation.
    /// Used for serialization formats.
    #[cfg(feature = "rustc-serialize")]
    fn from_serialized(secs: i32) -> Option<FixedOffset> {
        // check if the values are in the range
        if secs <= -86400 || 86400 <= secs { return None; }

        let offset = FixedOffset { local_minus_utc: secs };
        Some(offset)
    }

    /// Returns a serialized representation of this `FixedOffset`.
    #[cfg(feature = "rustc-serialize")]
    fn to_serialized(&self) -> i32 {
        self.local_minus_utc
    }

    /// Makes a new `FixedOffset` for the Eastern Hemisphere with given timezone difference.
    /// The negative `secs` means the Western Hemisphere.
    ///
    /// Panics on the out-of-bound `secs`.
    pub fn east(secs: i32) -> FixedOffset {
        FixedOffset::east_opt(secs).expect("FixedOffset::east out of bounds")
    }

    /// Makes a new `FixedOffset` for the Eastern Hemisphere with given timezone difference.
    /// The negative `secs` means the Western Hemisphere.
    ///
    /// Returns `None` on the out-of-bound `secs`.
    pub fn east_opt(secs: i32) -> Option<FixedOffset> {
        if -86400 < secs && secs < 86400 {
            Some(FixedOffset { local_minus_utc: secs })
        } else {
            None
        }
    }

    /// Makes a new `FixedOffset` for the Western Hemisphere with given timezone difference.
    /// The negative `secs` means the Eastern Hemisphere.
    ///
    /// Panics on the out-of-bound `secs`.
    pub fn west(secs: i32) -> FixedOffset {
        FixedOffset::west_opt(secs).expect("FixedOffset::west out of bounds")
    }

    /// Makes a new `FixedOffset` for the Western Hemisphere with given timezone difference.
    /// The negative `secs` means the Eastern Hemisphere.
    ///
    /// Returns `None` on the out-of-bound `secs`.
    pub fn west_opt(secs: i32) -> Option<FixedOffset> {
        if -86400 < secs && secs < 86400 {
            Some(FixedOffset { local_minus_utc: -secs })
        } else {
            None
        }
    }
}

impl TimeZone for FixedOffset {
    type Offset = FixedOffset;

    fn from_offset(offset: &FixedOffset) -> FixedOffset { offset.clone() }

    fn offset_from_local_date(&self, _local: &NaiveDate) -> LocalResult<FixedOffset> {
        LocalResult::Single(self.clone())
    }
    fn offset_from_local_datetime(&self, _local: &NaiveDateTime) -> LocalResult<FixedOffset> {
        LocalResult::Single(self.clone())
    }

    fn offset_from_utc_date(&self, _utc: &NaiveDate) -> FixedOffset { self.clone() }
    fn offset_from_utc_datetime(&self, _utc: &NaiveDateTime) -> FixedOffset { self.clone() }
}

impl Offset for FixedOffset {
    fn local_minus_utc(&self) -> Duration { Duration::seconds(self.local_minus_utc as i64) }
}

impl fmt::Debug for FixedOffset {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let offset = self.local_minus_utc;
        let (sign, offset) = if offset < 0 {('-', -offset)} else {('+', offset)};
        let (mins, sec) = div_mod_floor(offset, 60);
        let (hour, min) = div_mod_floor(mins, 60);
        if sec == 0 {
            write!(f, "{}{:02}:{:02}", sign, hour, min)
        } else {
            write!(f, "{}{:02}:{:02}:{:02}", sign, hour, min, sec)
        }
    }
}

impl fmt::Display for FixedOffset {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { fmt::Debug::fmt(self, f) }
}

#[cfg(feature = "rustc-serialize")]
mod rustc_serialize {
    use super::FixedOffset;
    use rustc_serialize::{Encodable, Encoder, Decodable, Decoder};

    // TODO the current serialization format is NEVER intentionally defined.
    // this basically follows the automatically generated implementation for those traits,
    // plus manual verification steps for avoiding security problem.
    // in the future it is likely to be redefined to more sane and reasonable format.

    impl Encodable for FixedOffset {
        fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
            let secs = self.to_serialized();
            s.emit_struct("FixedOffset", 1, |s| {
                try!(s.emit_struct_field("local_minus_utc", 0, |s| secs.encode(s)));
                Ok(())
            })
        }
    }

    impl Decodable for FixedOffset {
        fn decode<D: Decoder>(d: &mut D) -> Result<FixedOffset, D::Error> {
            d.read_struct("FixedOffset", 1, |d| {
                let secs = try!(d.read_struct_field("local_minus_utc", 0, Decodable::decode));
                FixedOffset::from_serialized(secs).ok_or_else(|| d.error("invalid offset"))
            })
        }
    }

    #[test]
    fn test_encodable() {
        use rustc_serialize::json::encode;

        assert_eq!(encode(&FixedOffset::east(0)).ok(),
                   Some(r#"{"local_minus_utc":0}"#.into()));
        assert_eq!(encode(&FixedOffset::east(1234)).ok(),
                   Some(r#"{"local_minus_utc":1234}"#.into()));
        assert_eq!(encode(&FixedOffset::east(86399)).ok(),
                   Some(r#"{"local_minus_utc":86399}"#.into()));
        assert_eq!(encode(&FixedOffset::west(1234)).ok(),
                   Some(r#"{"local_minus_utc":-1234}"#.into()));
        assert_eq!(encode(&FixedOffset::west(86399)).ok(),
                   Some(r#"{"local_minus_utc":-86399}"#.into()));
    }

    #[test]
    fn test_decodable() {
        use rustc_serialize::json;

        let decode = |s: &str| json::decode::<FixedOffset>(s);

        assert_eq!(decode(r#"{"local_minus_utc":0}"#).ok(), Some(FixedOffset::east(0)));
        assert_eq!(decode(r#"{"local_minus_utc": 1234}"#).ok(), Some(FixedOffset::east(1234)));
        assert_eq!(decode(r#"{"local_minus_utc":86399}"#).ok(), Some(FixedOffset::east(86399)));
        assert_eq!(decode(r#"{"local_minus_utc":-1234}"#).ok(), Some(FixedOffset::west(1234)));
        assert_eq!(decode(r#"{"local_minus_utc":-86399}"#).ok(), Some(FixedOffset::west(86399)));

        assert!(decode(r#"{"local_minus_utc":86400}"#).is_err());
        assert!(decode(r#"{"local_minus_utc":-86400}"#).is_err());
        assert!(decode(r#"{"local_minus_utc":0.1}"#).is_err());
        assert!(decode(r#"{"local_minus_utc":null}"#).is_err());
        assert!(decode(r#"{}"#).is_err());
        assert!(decode(r#"0"#).is_err());
        assert!(decode(r#"1234"#).is_err());
        assert!(decode(r#""string""#).is_err());
        assert!(decode(r#"null"#).is_err());
    }
}

