// This is a part of rust-chrono.
// Copyright (c) 2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

/*!
 * The UTC (Coordinated Universal Time) time zone.
 */

use std::fmt;
use stdtime;

use duration::Duration;
use naive::date::NaiveDate;
use naive::datetime::NaiveDateTime;
use date::Date;
use datetime::DateTime;
use super::{TimeZone, Offset, LocalResult};

/// The UTC time zone. This is the most efficient time zone when you don't need the local time.
/// It is also used as an offset (which is also a dummy type).
#[derive(Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "rustc-serialize", derive(RustcEncodable, RustcDecodable))]
pub struct UTC;

impl UTC {
    /// Returns a `Date` which corresponds to the current date.
    pub fn today() -> Date<UTC> { UTC::now().date() }

    /// Returns a `DateTime` which corresponds to the current date.
    pub fn now() -> DateTime<UTC> {
        let spec = stdtime::get_time();
        let naive = NaiveDateTime::from_timestamp(spec.sec, spec.nsec as u32);
        DateTime::from_utc(naive, UTC)
    }
}

impl TimeZone for UTC {
    type Offset = UTC;

    fn from_offset(_state: &UTC) -> UTC { UTC }

    fn offset_from_local_date(&self, _local: &NaiveDate) -> LocalResult<UTC> {
        LocalResult::Single(UTC)
    }
    fn offset_from_local_datetime(&self, _local: &NaiveDateTime) -> LocalResult<UTC> {
        LocalResult::Single(UTC)
    }

    fn offset_from_utc_date(&self, _utc: &NaiveDate) -> UTC { UTC }
    fn offset_from_utc_datetime(&self, _utc: &NaiveDateTime) -> UTC { UTC}
}

impl Offset for UTC {
    fn local_minus_utc(&self) -> Duration { Duration::zero() }
}

impl fmt::Debug for UTC {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { write!(f, "Z") }
}

impl fmt::Display for UTC {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { write!(f, "UTC") }
}

