/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use std::time::{Duration, SystemTime, UNIX_EPOCH};

/// Typesafe way to manage server timestamps without accidentally mixing them up with
/// local ones.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Default)]
pub struct ServerTimestamp(pub i64);

impl ServerTimestamp {
    pub fn from_float_seconds(ts: f64) -> Self {
        let rf = (ts * 1000.0).round();
        if !rf.is_finite() || rf < 0.0 || rf >= i64::max_value() as f64 {
            error_support::report_error!("sync15-illegal-timestamp", "Illegal timestamp: {}", ts);
            ServerTimestamp(0)
        } else {
            ServerTimestamp(rf as i64)
        }
    }

    pub fn from_millis(ts: i64) -> Self {
        // Catch it in tests, but just complain and replace with 0 otherwise.
        debug_assert!(ts >= 0, "Bad timestamp: {}", ts);
        if ts >= 0 {
            Self(ts)
        } else {
            error_support::report_error!(
                "sync15-illegal-timestamp",
                "Illegal timestamp, substituting 0: {}",
                ts
            );
            Self(0)
        }
    }
}

// This lets us use these in hyper header! blocks.
impl std::str::FromStr for ServerTimestamp {
    type Err = std::num::ParseFloatError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let val = f64::from_str(s)?;
        Ok(Self::from_float_seconds(val))
    }
}

impl std::fmt::Display for ServerTimestamp {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0 as f64 / 1000.0)
    }
}

impl ServerTimestamp {
    pub const EPOCH: ServerTimestamp = ServerTimestamp(0);

    /// Returns None if `other` is later than `self` (Duration may not represent
    /// negative timespans in rust).
    #[inline]
    pub fn duration_since(self, other: ServerTimestamp) -> Option<Duration> {
        let delta = self.0 - other.0;
        if delta < 0 {
            None
        } else {
            Some(Duration::from_millis(delta as u64))
        }
    }

    /// Get the milliseconds for the timestamp.
    #[inline]
    pub fn as_millis(self) -> i64 {
        self.0
    }
}

impl serde::ser::Serialize for ServerTimestamp {
    fn serialize<S: serde::ser::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_f64(self.0 as f64 / 1000.0)
    }
}

impl<'de> serde::de::Deserialize<'de> for ServerTimestamp {
    fn deserialize<D: serde::de::Deserializer<'de>>(d: D) -> Result<Self, D::Error> {
        f64::deserialize(d).map(Self::from_float_seconds)
    }
}

/// Exposed only for tests that need to create a server timestamp from the system time
/// Please be cautious when constructing the timestamp directly, as constructing the server
/// timestamp from system time is almost certainly not what you'd want to do for non-test code
impl TryFrom<SystemTime> for ServerTimestamp {
    type Error = std::time::SystemTimeError;

    fn try_from(value: SystemTime) -> Result<Self, Self::Error> {
        Ok(Self(value.duration_since(UNIX_EPOCH)?.as_millis() as i64))
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_server_timestamp() {
        let t0 = ServerTimestamp(10_300_150);
        let t1 = ServerTimestamp(10_100_050);
        assert!(t1.duration_since(t0).is_none());
        assert!(t0.duration_since(t1).is_some());
        let dur = t0.duration_since(t1).unwrap();
        assert_eq!(dur.as_secs(), 200);
        assert_eq!(dur.subsec_nanos(), 100_000_000);
    }

    #[test]
    fn test_serde() {
        let ts = ServerTimestamp(123_456);

        // test serialize
        let ser = serde_json::to_string(&ts).unwrap();
        assert_eq!("123.456".to_string(), ser);

        // test deserialize of float
        let ts: ServerTimestamp = serde_json::from_str(&ser).unwrap();
        assert_eq!(ServerTimestamp(123_456), ts);

        // test deserialize of whole number
        let ts: ServerTimestamp = serde_json::from_str("123").unwrap();
        assert_eq!(ServerTimestamp(123_000), ts);

        // test deserialize of negative number
        let ts: ServerTimestamp = serde_json::from_str("-123").unwrap();
        assert_eq!(ServerTimestamp(0), ts);
    }
}
