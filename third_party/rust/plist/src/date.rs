use humantime;
use std::{
    fmt,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

/// A UTC timestamp used for serialization to and from the plist date type.
///
/// Note that while this type implements `Serialize` and `Deserialize` it will behave strangely if
/// used with serializers from outside this crate.
#[derive(Clone, Copy, Eq, Hash, PartialEq)]
pub struct Date {
    inner: SystemTime,
}

pub(crate) struct InfiniteOrNanDate;

impl Date {
    /// The unix timestamp of the plist epoch.
    const PLIST_EPOCH_UNIX_TIMESTAMP: Duration = Duration::from_secs(978_307_200);

    pub(crate) fn from_rfc3339(date: &str) -> Result<Self, ()> {
        Ok(Date {
            inner: humantime::parse_rfc3339(date).map_err(|_| ())?,
        })
    }

    pub(crate) fn to_rfc3339(&self) -> String {
        format!("{}", humantime::format_rfc3339(self.inner))
    }

    pub(crate) fn from_seconds_since_plist_epoch(
        timestamp: f64,
    ) -> Result<Date, InfiniteOrNanDate> {
        // `timestamp` is the number of seconds since the plist epoch of 1/1/2001 00:00:00.
        let plist_epoch = UNIX_EPOCH + Date::PLIST_EPOCH_UNIX_TIMESTAMP;

        if !timestamp.is_finite() {
            return Err(InfiniteOrNanDate);
        }

        let is_negative = timestamp < 0.0;
        let timestamp = timestamp.abs();
        let seconds = timestamp.floor() as u64;
        let subsec_nanos = (timestamp.fract() * 1e9) as u32;

        let dur_since_plist_epoch = Duration::new(seconds, subsec_nanos);

        let inner = if is_negative {
            plist_epoch - dur_since_plist_epoch
        } else {
            plist_epoch + dur_since_plist_epoch
        };

        Ok(Date { inner })
    }

    pub(crate) fn to_seconds_since_plist_epoch(&self) -> f64 {
        // needed until #![feature(duration_float)] is stabilized
        fn as_secs_f64(d: Duration) -> f64 {
            const NANOS_PER_SEC: f64 = 1_000_000_000.00;
            (d.as_secs() as f64) + f64::from(d.subsec_nanos()) / NANOS_PER_SEC
        }

        let plist_epoch = UNIX_EPOCH + Date::PLIST_EPOCH_UNIX_TIMESTAMP;
        match self.inner.duration_since(plist_epoch) {
            Ok(dur_since_plist_epoch) => as_secs_f64(dur_since_plist_epoch),
            Err(err) => -as_secs_f64(err.duration()),
        }
    }
}

impl fmt::Debug for Date {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        let rfc3339 = humantime::format_rfc3339(self.inner);
        <humantime::Rfc3339Timestamp as fmt::Display>::fmt(&rfc3339, f)
    }
}

impl From<SystemTime> for Date {
    fn from(date: SystemTime) -> Self {
        Date { inner: date }
    }
}

impl Into<SystemTime> for Date {
    fn into(self) -> SystemTime {
        self.inner
    }
}

#[cfg(feature = "serde")]
pub mod serde_impls {
    use serde::{
        de::{Deserialize, Deserializer, Error, Unexpected, Visitor},
        ser::{Serialize, Serializer},
    };
    use std::fmt;

    use crate::Date;

    pub const DATE_NEWTYPE_STRUCT_NAME: &str = "PLIST-DATE";

    impl Serialize for Date {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: Serializer,
        {
            let date_str = self.to_rfc3339();
            serializer.serialize_newtype_struct(DATE_NEWTYPE_STRUCT_NAME, &date_str)
        }
    }

    struct DateNewtypeVisitor;

    impl<'de> Visitor<'de> for DateNewtypeVisitor {
        type Value = Date;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("a plist date newtype")
        }

        fn visit_newtype_struct<D>(self, deserializer: D) -> Result<Self::Value, D::Error>
        where
            D: Deserializer<'de>,
        {
            deserializer.deserialize_str(DateStrVisitor)
        }
    }

    struct DateStrVisitor;

    impl<'de> Visitor<'de> for DateStrVisitor {
        type Value = Date;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("a plist date string")
        }

        fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
        where
            E: Error,
        {
            Date::from_rfc3339(v).map_err(|()| E::invalid_value(Unexpected::Str(v), &self))
        }
    }

    impl<'de> Deserialize<'de> for Date {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            deserializer.deserialize_newtype_struct(DATE_NEWTYPE_STRUCT_NAME, DateNewtypeVisitor)
        }
    }
}
