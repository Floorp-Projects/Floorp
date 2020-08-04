use crate::Decimal;

use num_traits::FromPrimitive;

use serde::{self, de::Unexpected};

use std::{fmt, str::FromStr};

#[cfg(not(feature = "serde-bincode"))]
impl<'de> serde::Deserialize<'de> for Decimal {
    fn deserialize<D>(deserializer: D) -> Result<Decimal, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_any(DecimalVisitor)
    }
}

#[cfg(all(feature = "serde-bincode", not(feature = "serde-float")))]
impl<'de> serde::Deserialize<'de> for Decimal {
    fn deserialize<D>(deserializer: D) -> Result<Decimal, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_str(DecimalVisitor)
    }
}

#[cfg(all(feature = "serde-bincode", feature = "serde-float"))]
impl<'de> serde::Deserialize<'de> for Decimal {
    fn deserialize<D>(deserializer: D) -> Result<Decimal, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_f64(DecimalVisitor)
    }
}

struct DecimalVisitor;

impl<'de> serde::de::Visitor<'de> for DecimalVisitor {
    type Value = Decimal;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "a Decimal type representing a fixed-point number")
    }

    fn visit_i64<E>(self, value: i64) -> Result<Decimal, E>
    where
        E: serde::de::Error,
    {
        match Decimal::from_i64(value) {
            Some(s) => Ok(s),
            None => Err(E::invalid_value(Unexpected::Signed(value), &self)),
        }
    }

    fn visit_u64<E>(self, value: u64) -> Result<Decimal, E>
    where
        E: serde::de::Error,
    {
        match Decimal::from_u64(value) {
            Some(s) => Ok(s),
            None => Err(E::invalid_value(Unexpected::Unsigned(value), &self)),
        }
    }

    fn visit_f64<E>(self, value: f64) -> Result<Decimal, E>
    where
        E: serde::de::Error,
    {
        Decimal::from_str(&value.to_string()).map_err(|_| E::invalid_value(Unexpected::Float(value), &self))
    }

    fn visit_str<E>(self, value: &str) -> Result<Decimal, E>
    where
        E: serde::de::Error,
    {
        Decimal::from_str(value)
            .or_else(|_| Decimal::from_scientific(value))
            .map_err(|_| E::invalid_value(Unexpected::Str(value), &self))
    }
}

#[cfg(not(feature = "serde-float"))]
impl serde::Serialize for Decimal {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(&self.to_string())
    }
}

#[cfg(feature = "serde-float")]
impl serde::Serialize for Decimal {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        use num_traits::ToPrimitive;
        serializer.serialize_f64(self.to_f64().unwrap())
    }
}

#[cfg(test)]
mod test {

    use super::*;

    use serde_derive::{Deserialize, Serialize};

    #[derive(Serialize, Deserialize, Debug)]
    struct Record {
        amount: Decimal,
    }

    #[test]
    #[cfg(not(feature = "serde-bincode"))]
    fn deserialize_valid_decimal() {
        let data = [
            ("{\"amount\":\"1.234\"}", "1.234"),
            ("{\"amount\":1234}", "1234"),
            ("{\"amount\":1234.56}", "1234.56"),
            ("{\"amount\":\"1.23456e3\"}", "1234.56"),
        ];
        for &(serialized, value) in data.iter() {
            let result = serde_json::from_str(serialized);
            assert_eq!(
                true,
                result.is_ok(),
                "expected successful deseralization for {}. Error: {:?}",
                serialized,
                result.err().unwrap()
            );
            let record: Record = result.unwrap();
            assert_eq!(
                value,
                record.amount.to_string(),
                "expected: {}, actual: {}",
                value,
                record.amount.to_string()
            );
        }
    }

    #[test]
    #[should_panic]
    fn deserialize_invalid_decimal() {
        let serialized = "{\"amount\":\"foo\"}";
        let _: Record = serde_json::from_str(serialized).unwrap();
    }

    #[test]
    #[cfg(not(feature = "serde-float"))]
    fn serialize_decimal() {
        let record = Record {
            amount: Decimal::new(1234, 3),
        };
        let serialized = serde_json::to_string(&record).unwrap();
        assert_eq!("{\"amount\":\"1.234\"}", serialized);
    }

    #[test]
    #[cfg(feature = "serde-float")]
    fn serialize_decimal() {
        let record = Record {
            amount: Decimal::new(1234, 3),
        };
        let serialized = serde_json::to_string(&record).unwrap();
        assert_eq!("{\"amount\":1.234}", serialized);
    }

    #[test]
    #[cfg(all(feature = "serde-bincode", not(feature = "serde-float")))]
    fn bincode_serialization() {
        use bincode::{deserialize, serialize};

        let data = [
            "0",
            "0.00",
            "3.14159",
            "-3.14159",
            "1234567890123.4567890",
            "-1234567890123.4567890",
        ];
        for &raw in data.iter() {
            let value = Decimal::from_str(raw).unwrap();
            let encoded = serialize(&value).unwrap();
            let decoded: Decimal = deserialize(&encoded[..]).unwrap();
            assert_eq!(value, decoded);
            assert_eq!(8usize + raw.len(), encoded.len());
        }
    }

    #[test]
    #[cfg(all(feature = "serde-bincode", feature = "serde-float"))]
    fn bincode_serialization() {
        use bincode::{deserialize, serialize};

        let data = [
            ("0", "0"),
            ("0.00", "0.00"),
            ("3.14159", "3.14159"),
            ("-3.14159", "-3.14159"),
            ("1234567890123.4567890", "1234567890123.4568"),
            ("-1234567890123.4567890", "-1234567890123.4568"),
        ];
        for &(value, expected) in data.iter() {
            let value = Decimal::from_str(value).unwrap();
            let expected = Decimal::from_str(expected).unwrap();
            let encoded = serialize(&value).unwrap();
            let decoded: Decimal = deserialize(&encoded[..]).unwrap();
            assert_eq!(expected, decoded);
            assert_eq!(8usize, encoded.len());
        }
    }
}
