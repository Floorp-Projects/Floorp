// This is a part of Chrono.
// See README.md and LICENSE.txt for details.

//! The UTC (Coordinated Universal Time) time zone.

use core::fmt;

use super::{FixedOffset, LocalResult, Offset, TimeZone};
use naive::{NaiveDate, NaiveDateTime};
#[cfg(all(
    feature = "clock",
    not(all(target_arch = "wasm32", not(target_os = "wasi"), feature = "wasmbind"))
))]
use std::time::{SystemTime, UNIX_EPOCH};
#[cfg(feature = "clock")]
use {Date, DateTime};

/// The UTC time zone. This is the most efficient time zone when you don't need the local time.
/// It is also used as an offset (which is also a dummy type).
///
/// Using the [`TimeZone`](./trait.TimeZone.html) methods
/// on the UTC struct is the preferred way to construct `DateTime<Utc>`
/// instances.
///
/// # Example
///
/// ~~~~
/// use chrono::{DateTime, TimeZone, NaiveDateTime, Utc};
///
/// let dt = DateTime::<Utc>::from_utc(NaiveDateTime::from_timestamp(61, 0), Utc);
///
/// assert_eq!(Utc.timestamp(61, 0), dt);
/// assert_eq!(Utc.ymd(1970, 1, 1).and_hms(0, 1, 1), dt);
/// ~~~~
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct Utc;

#[cfg(feature = "clock")]
impl Utc {
    /// Returns a `Date` which corresponds to the current date.
    pub fn today() -> Date<Utc> {
        Utc::now().date()
    }

    /// Returns a `DateTime` which corresponds to the current date.
    #[cfg(not(all(target_arch = "wasm32", not(target_os = "wasi"), feature = "wasmbind")))]
    pub fn now() -> DateTime<Utc> {
        let now =
            SystemTime::now().duration_since(UNIX_EPOCH).expect("system time before Unix epoch");
        let naive = NaiveDateTime::from_timestamp(now.as_secs() as i64, now.subsec_nanos() as u32);
        DateTime::from_utc(naive, Utc)
    }

    /// Returns a `DateTime` which corresponds to the current date.
    #[cfg(all(target_arch = "wasm32", not(target_os = "wasi"), feature = "wasmbind"))]
    pub fn now() -> DateTime<Utc> {
        let now = js_sys::Date::new_0();
        DateTime::<Utc>::from(now)
    }
}

impl TimeZone for Utc {
    type Offset = Utc;

    fn from_offset(_state: &Utc) -> Utc {
        Utc
    }

    fn offset_from_local_date(&self, _local: &NaiveDate) -> LocalResult<Utc> {
        LocalResult::Single(Utc)
    }
    fn offset_from_local_datetime(&self, _local: &NaiveDateTime) -> LocalResult<Utc> {
        LocalResult::Single(Utc)
    }

    fn offset_from_utc_date(&self, _utc: &NaiveDate) -> Utc {
        Utc
    }
    fn offset_from_utc_datetime(&self, _utc: &NaiveDateTime) -> Utc {
        Utc
    }
}

impl Offset for Utc {
    fn fix(&self) -> FixedOffset {
        FixedOffset::east(0)
    }
}

impl fmt::Debug for Utc {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Z")
    }
}

impl fmt::Display for Utc {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "UTC")
    }
}
