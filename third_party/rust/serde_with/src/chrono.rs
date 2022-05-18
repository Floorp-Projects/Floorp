//! De/Serialization of [chrono][] types
//!
//! This modules is only available if using the `chrono` feature of the crate.
//!
//! [chrono]: https://docs.rs/chrono/

use crate::{
    de::DeserializeAs,
    formats::{Flexible, Format, Strict, Strictness},
    ser::SerializeAs,
    utils, DurationSeconds, DurationSecondsWithFrac, TimestampSeconds, TimestampSecondsWithFrac,
};
use chrono_crate::{DateTime, Duration, Local, NaiveDateTime, Utc};
use serde::{de, Deserialize, Deserializer, Serialize, Serializer};
use utils::duration::{DurationSigned, Sign};

/// Create a [`DateTime`] for the Unix Epoch using the [`Utc`] timezone
fn unix_epoch_utc() -> DateTime<Utc> {
    DateTime::<Utc>::from_utc(NaiveDateTime::from_timestamp(0, 0), Utc)
}

/// Create a [`DateTime`] for the Unix Epoch using the [`Utc`] timezone
fn unix_epoch_local() -> DateTime<Local> {
    DateTime::<Utc>::from_utc(NaiveDateTime::from_timestamp(0, 0), Utc).with_timezone(&Local)
}

/// Deserialize a Unix timestamp with optional subsecond precision into a `DateTime<Utc>`.
///
/// The `DateTime<Utc>` can be serialized from an integer, a float, or a string representing a number.
///
/// # Examples
///
/// ```
/// # use chrono_crate::{DateTime, Utc};
/// # use serde_derive::Deserialize;
/// #
/// #[derive(Debug, Deserialize)]
/// struct S {
///     #[serde(with = "serde_with::chrono::datetime_utc_ts_seconds_from_any")]
///     date: DateTime<Utc>,
/// }
///
/// // Deserializes integers
/// assert!(serde_json::from_str::<S>(r#"{ "date": 1478563200 }"#).is_ok());
/// // floats
/// assert!(serde_json::from_str::<S>(r#"{ "date": 1478563200.123 }"#).is_ok());
/// // and strings with numbers, for high-precision values
/// assert!(serde_json::from_str::<S>(r#"{ "date": "1478563200.123" }"#).is_ok());
/// ```
///
pub mod datetime_utc_ts_seconds_from_any {
    use chrono_crate::{DateTime, NaiveDateTime, Utc};
    use serde::de::{Deserializer, Error, Unexpected, Visitor};

    /// Deserialize a Unix timestamp with optional subsecond precision into a `DateTime<Utc>`.
    pub fn deserialize<'de, D>(deserializer: D) -> Result<DateTime<Utc>, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct Helper;
        impl<'de> Visitor<'de> for Helper {
            type Value = DateTime<Utc>;

            fn expecting(&self, formatter: &mut ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
                formatter
                    .write_str("an integer, float, or string with optional subsecond precision.")
            }

            fn visit_i64<E>(self, value: i64) -> Result<Self::Value, E>
            where
                E: Error,
            {
                let ndt = NaiveDateTime::from_timestamp_opt(value, 0);
                if let Some(ndt) = ndt {
                    Ok(DateTime::<Utc>::from_utc(ndt, Utc))
                } else {
                    Err(Error::custom(format!(
                        "a timestamp which can be represented in a DateTime but received '{}'",
                        value
                    )))
                }
            }

            fn visit_u64<E>(self, value: u64) -> Result<Self::Value, E>
            where
                E: Error,
            {
                let ndt = NaiveDateTime::from_timestamp_opt(value as i64, 0);
                if let Some(ndt) = ndt {
                    Ok(DateTime::<Utc>::from_utc(ndt, Utc))
                } else {
                    Err(Error::custom(format!(
                        "a timestamp which can be represented in a DateTime but received '{}'",
                        value
                    )))
                }
            }

            fn visit_f64<E>(self, value: f64) -> Result<Self::Value, E>
            where
                E: Error,
            {
                let seconds = value.trunc() as i64;
                let nsecs = (value.fract() * 1_000_000_000_f64).abs() as u32;
                let ndt = NaiveDateTime::from_timestamp_opt(seconds, nsecs);
                if let Some(ndt) = ndt {
                    Ok(DateTime::<Utc>::from_utc(ndt, Utc))
                } else {
                    Err(Error::custom(format!(
                        "a timestamp which can be represented in a DateTime but received '{}'",
                        value
                    )))
                }
            }

            fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                let parts: Vec<_> = value.split('.').collect();

                match *parts.as_slice() {
                    [seconds] => {
                        if let Ok(seconds) = i64::from_str_radix(seconds, 10) {
                            let ndt = NaiveDateTime::from_timestamp_opt(seconds, 0);
                            if let Some(ndt) = ndt {
                                Ok(DateTime::<Utc>::from_utc(ndt, Utc))
                            } else {
                                Err(Error::custom(format!(
                                    "a timestamp which can be represented in a DateTime but received '{}'",
                                    value
                                )))
                            }
                        } else {
                            Err(Error::invalid_value(Unexpected::Str(value), &self))
                        }
                    }
                    [seconds, subseconds] => {
                        if let Ok(seconds) = i64::from_str_radix(seconds, 10) {
                            let subseclen = subseconds.chars().count() as u32;
                            if subseclen > 9 {
                                return Err(Error::custom(format!(
                                    "DateTimes only support nanosecond precision but '{}' has more than 9 digits.",
                                    value
                                )));
                            }

                            if let Ok(mut subseconds) = u32::from_str_radix(subseconds, 10) {
                                // convert subseconds to nanoseconds (10^-9), require 9 places for nanoseconds
                                subseconds *= 10u32.pow(9 - subseclen);
                                let ndt = NaiveDateTime::from_timestamp_opt(seconds, subseconds);
                                if let Some(ndt) = ndt {
                                    Ok(DateTime::<Utc>::from_utc(ndt, Utc))
                                } else {
                                    Err(Error::custom(format!(
                                        "a timestamp which can be represented in a DateTime but received '{}'",
                                        value
                                    )))
                                }
                            } else {
                                Err(Error::invalid_value(Unexpected::Str(value), &self))
                            }
                        } else {
                            Err(Error::invalid_value(Unexpected::Str(value), &self))
                        }
                    }

                    _ => Err(Error::invalid_value(Unexpected::Str(value), &self)),
                }
            }
        }

        deserializer.deserialize_any(Helper)
    }
}

impl SerializeAs<NaiveDateTime> for DateTime<Utc> {
    fn serialize_as<S>(source: &NaiveDateTime, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let datetime = DateTime::<Utc>::from_utc(*source, Utc);
        datetime.serialize(serializer)
    }
}

impl<'de> DeserializeAs<'de, NaiveDateTime> for DateTime<Utc> {
    fn deserialize_as<D>(deserializer: D) -> Result<NaiveDateTime, D::Error>
    where
        D: Deserializer<'de>,
    {
        DateTime::<Utc>::deserialize(deserializer).map(|datetime| datetime.naive_utc())
    }
}

/// Convert a [`chrono::Duration`] into a [`DurationSigned`]
fn duration_into_duration_signed(dur: &Duration) -> DurationSigned {
    match dur.to_std() {
        Ok(dur) => DurationSigned::with_duration(Sign::Positive, dur),
        Err(_) => {
            if let Ok(dur) = (-*dur).to_std() {
                DurationSigned::with_duration(Sign::Negative, dur)
            } else {
                panic!("A chrono Duration should be convertible to a DurationSigned")
            }
        }
    }
}

/// Convert a [`DurationSigned`] into a [`chrono::Duration`]
fn duration_from_duration_signed<'de, D>(dur: DurationSigned) -> Result<Duration, D::Error>
where
    D: Deserializer<'de>,
{
    let mut chrono_dur = match Duration::from_std(dur.duration) {
        Ok(dur) => dur,
        Err(msg) => {
            return Err(de::Error::custom(format!(
                "Duration is outside of the representable range: {}",
                msg
            )))
        }
    };
    if dur.sign.is_negative() {
        chrono_dur = -chrono_dur;
    }
    Ok(chrono_dur)
}

macro_rules! use_duration_signed_ser {
    (
        $ty:ty =>
        $main_trait:ident $internal_trait:ident =>
        $source_to_dur:ident =>
        $({
            $format:ty, $strictness:ty =>
            $($tbound:ident: $bound:path $(,)?)*
        })*
    ) => {
        $(
            impl<$($tbound ,)*> SerializeAs<$ty> for $main_trait<$format, $strictness>
            where
                $($tbound: $bound,)*
            {
                fn serialize_as<S>(source: &$ty, serializer: S) -> Result<S::Ok, S::Error>
                where
                    S: Serializer,
                {
                    let dur: DurationSigned = $source_to_dur(source);
                    $internal_trait::<$format, $strictness>::serialize_as(
                        &dur,
                        serializer,
                    )
                }
            }
        )*
    };
}

fn datetime_to_duration<TZ>(source: &DateTime<TZ>) -> DurationSigned
where
    TZ: chrono_crate::TimeZone,
{
    duration_into_duration_signed(&source.clone().signed_duration_since(unix_epoch_utc()))
}

use_duration_signed_ser!(
    Duration =>
    DurationSeconds DurationSeconds =>
    duration_into_duration_signed =>
    {i64, STRICTNESS => STRICTNESS: Strictness}
    {f64, STRICTNESS => STRICTNESS: Strictness}
    {String, STRICTNESS => STRICTNESS: Strictness}
);
use_duration_signed_ser!(
    DateTime<TZ> =>
    TimestampSeconds DurationSeconds =>
    datetime_to_duration =>
    {i64, STRICTNESS => TZ: chrono_crate::offset::TimeZone, STRICTNESS: Strictness}
    {f64, STRICTNESS => TZ: chrono_crate::offset::TimeZone, STRICTNESS: Strictness}
    {String, STRICTNESS => TZ: chrono_crate::offset::TimeZone, STRICTNESS: Strictness}
);
use_duration_signed_ser!(
    Duration =>
    DurationSecondsWithFrac DurationSecondsWithFrac =>
    duration_into_duration_signed =>
    {f64, STRICTNESS => STRICTNESS: Strictness}
    {String, STRICTNESS => STRICTNESS: Strictness}
);
use_duration_signed_ser!(
    DateTime<TZ> =>
    TimestampSecondsWithFrac DurationSecondsWithFrac =>
    datetime_to_duration =>
    {f64, STRICTNESS => TZ: chrono_crate::offset::TimeZone, STRICTNESS: Strictness}
    {String, STRICTNESS => TZ: chrono_crate::offset::TimeZone, STRICTNESS: Strictness}
);

macro_rules! use_duration_signed_de {
    (
        $ty:ty =>
        $main_trait:ident $internal_trait:ident =>
        $dur_to_result:ident =>
        $({
            $format:ty, $strictness:ty =>
            $($tbound:ident: $bound:ident)*
        })*
    ) => {
        $(
            impl<'de, $($tbound,)*> DeserializeAs<'de, $ty> for $main_trait<$format, $strictness>
            where
                $($tbound: $bound,)*
            {
                fn deserialize_as<D>(deserializer: D) -> Result<$ty, D::Error>
                where
                    D: Deserializer<'de>,
                {
                    let dur: DurationSigned = $internal_trait::<$format, $strictness>::deserialize_as(deserializer)?;
                    $dur_to_result::<D>(dur)
                }
            }
        )*
    };
}

fn duration_to_datetime_utc<'de, D>(dur: DurationSigned) -> Result<DateTime<Utc>, D::Error>
where
    D: Deserializer<'de>,
{
    Ok(unix_epoch_utc() + duration_from_duration_signed::<D>(dur)?)
}

fn duration_to_datetime_local<'de, D>(dur: DurationSigned) -> Result<DateTime<Local>, D::Error>
where
    D: Deserializer<'de>,
{
    Ok(unix_epoch_local() + duration_from_duration_signed::<D>(dur)?)
}

// No subsecond precision
use_duration_signed_de!(
    Duration =>
    DurationSeconds DurationSeconds =>
    duration_from_duration_signed =>
    {i64, Strict =>}
    {f64, Strict =>}
    {String, Strict =>}
    {FORMAT, Flexible => FORMAT: Format}
);
use_duration_signed_de!(
    DateTime<Utc> =>
    TimestampSeconds DurationSeconds =>
    duration_to_datetime_utc =>
    {i64, Strict =>}
    {f64, Strict =>}
    {String, Strict =>}
    {FORMAT, Flexible => FORMAT: Format}
);
use_duration_signed_de!(
    DateTime<Local> =>
    TimestampSeconds DurationSeconds =>
    duration_to_datetime_local =>
    {i64, Strict =>}
    {f64, Strict =>}
    {String, Strict =>}
    {FORMAT, Flexible => FORMAT: Format}
);

// Duration/Timestamp WITH FRACTIONS
use_duration_signed_de!(
    Duration =>
    DurationSecondsWithFrac DurationSecondsWithFrac =>
    duration_from_duration_signed =>
    {f64, Strict =>}
    {String, Strict =>}
    {FORMAT, Flexible => FORMAT: Format}
);
use_duration_signed_de!(
    DateTime<Utc> =>
    TimestampSecondsWithFrac DurationSecondsWithFrac =>
    duration_to_datetime_utc =>
    {f64, Strict =>}
    {String, Strict =>}
    {FORMAT, Flexible => FORMAT: Format}
);
use_duration_signed_de!(
    DateTime<Local> =>
    TimestampSecondsWithFrac DurationSecondsWithFrac =>
    duration_to_datetime_local =>
    {f64, Strict =>}
    {String, Strict =>}
    {FORMAT, Flexible => FORMAT: Format}
);
