/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use rusqlite::types::{FromSql, FromSqlResult, ToSql, ToSqlOutput, ValueRef};
use rusqlite::Result as RusqliteResult;
use serde_derive::*;
use std::fmt;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

// Typesafe way to manage timestamps.
// We should probably work out how to share this too?
#[derive(
    Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Deserialize, Serialize, Default,
)]
pub struct Timestamp(pub u64);

impl Timestamp {
    pub fn now() -> Self {
        SystemTime::now().into()
    }

    /// Returns None if `other` is later than `self` (Duration may not represent
    /// negative timespans in rust).
    #[inline]
    pub fn duration_since(self, other: Timestamp) -> Option<Duration> {
        // just do this via SystemTime.
        SystemTime::from(self).duration_since(other.into()).ok()
    }

    #[inline]
    pub fn checked_sub(self, d: Duration) -> Option<Timestamp> {
        SystemTime::from(self).checked_sub(d).map(Timestamp::from)
    }

    #[inline]
    pub fn checked_add(self, d: Duration) -> Option<Timestamp> {
        SystemTime::from(self).checked_add(d).map(Timestamp::from)
    }

    pub fn as_millis(self) -> u64 {
        self.0
    }

    pub fn as_millis_i64(self) -> i64 {
        self.0 as i64
    }
    /// In desktop sync, bookmarks are clamped to Jan 23, 1993 (which is 727747200000)
    /// There's no good reason history records could be older than that, so we do
    /// the same here (even though desktop's history currently doesn't)
    /// XXX - there's probably a case to be made for this being, say, 5 years ago -
    /// then all requests earlier than that are collapsed into a single visit at
    /// this timestamp.
    pub const EARLIEST: Timestamp = Timestamp(727_747_200_000);
}

impl From<Timestamp> for u64 {
    #[inline]
    fn from(ts: Timestamp) -> Self {
        ts.0
    }
}

impl From<SystemTime> for Timestamp {
    #[inline]
    fn from(st: SystemTime) -> Self {
        let d = st.duration_since(UNIX_EPOCH).unwrap(); // hrmph - unwrap doesn't seem ideal
        Timestamp((d.as_secs()) * 1000 + (u64::from(d.subsec_nanos()) / 1_000_000))
    }
}

impl From<Timestamp> for SystemTime {
    #[inline]
    fn from(ts: Timestamp) -> Self {
        UNIX_EPOCH + Duration::from_millis(ts.into())
    }
}

impl From<u64> for Timestamp {
    #[inline]
    fn from(ts: u64) -> Self {
        assert!(ts != 0);
        Timestamp(ts)
    }
}

impl fmt::Display for Timestamp {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl ToSql for Timestamp {
    fn to_sql(&self) -> RusqliteResult<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(self.0 as i64)) // hrm - no u64 in rusqlite
    }
}

impl FromSql for Timestamp {
    fn column_result(value: ValueRef<'_>) -> FromSqlResult<Self> {
        value.as_i64().map(|v| Timestamp(v as u64)) // hrm - no u64
    }
}
