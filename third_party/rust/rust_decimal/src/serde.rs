use crate::Decimal;
use alloc::string::ToString;
use core::{fmt, str::FromStr};
use num_traits::FromPrimitive;
use serde::{self, de::Unexpected};

#[cfg(not(feature = "serde-str"))]
impl<'de> serde::Deserialize<'de> for Decimal {
    fn deserialize<D>(deserializer: D) -> Result<Decimal, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_any(DecimalVisitor)
    }
}

#[cfg(all(feature = "serde-str", not(feature = "serde-float")))]
impl<'de> serde::Deserialize<'de> for Decimal {
    fn deserialize<D>(deserializer: D) -> Result<Decimal, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_str(DecimalVisitor)
    }
}

#[cfg(all(feature = "serde-str", feature = "serde-float"))]
impl<'de> serde::Deserialize<'de> for Decimal {
    fn deserialize<D>(deserializer: D) -> Result<Decimal, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_f64(DecimalVisitor)
    }
}

// It's a shame this needs to be redefined for this feature and not able to be referenced directly
#[cfg(feature = "serde-arbitrary-precision")]
const DECIMAL_KEY_TOKEN: &str = "$serde_json::private::Number";

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

    #[cfg(feature = "serde-arbitrary-precision")]
    fn visit_map<A>(self, map: A) -> Result<Decimal, A::Error>
    where
        A: serde::de::MapAccess<'de>,
    {
        let mut map = map;
        let value = map.next_key::<DecimalKey>()?;
        if value.is_none() {
            return Err(serde::de::Error::invalid_type(Unexpected::Map, &self));
        }
        let v: DecimalFromString = map.next_value()?;
        Ok(v.value)
    }
}

#[cfg(feature = "serde-arbitrary-precision")]
struct DecimalKey;

#[cfg(feature = "serde-arbitrary-precision")]
impl<'de> serde::de::Deserialize<'de> for DecimalKey {
    fn deserialize<D>(deserializer: D) -> Result<DecimalKey, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        struct FieldVisitor;

        impl<'de> serde::de::Visitor<'de> for FieldVisitor {
            type Value = ();

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a valid decimal field")
            }

            fn visit_str<E>(self, s: &str) -> Result<(), E>
            where
                E: serde::de::Error,
            {
                if s == DECIMAL_KEY_TOKEN {
                    Ok(())
                } else {
                    Err(serde::de::Error::custom("expected field with custom name"))
                }
            }
        }

        deserializer.deserialize_identifier(FieldVisitor)?;
        Ok(DecimalKey)
    }
}

#[cfg(feature = "serde-arbitrary-precision")]
pub struct DecimalFromString {
    pub value: Decimal,
}

#[cfg(feature = "serde-arbitrary-precision")]
impl<'de> serde::de::Deserialize<'de> for DecimalFromString {
    fn deserialize<D>(deserializer: D) -> Result<DecimalFromString, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        struct Visitor;

        impl<'de> serde::de::Visitor<'de> for Visitor {
            type Value = DecimalFromString;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("string containing a decimal")
            }

            fn visit_str<E>(self, value: &str) -> Result<DecimalFromString, E>
            where
                E: serde::de::Error,
            {
                let d = Decimal::from_str(value)
                    .or_else(|_| Decimal::from_scientific(value))
                    .map_err(serde::de::Error::custom)?;
                Ok(DecimalFromString { value: d })
            }
        }

        deserializer.deserialize_str(Visitor)
    }
}

#[cfg(not(feature = "serde-float"))]
impl serde::Serialize for Decimal {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(crate::str::to_str_internal(self, true, None).as_ref())
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
    #[cfg(not(feature = "serde-str"))]
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
                "expected successful deserialization for {}. Error: {:?}",
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
    #[cfg(feature = "serde-arbitrary-precision")]
    fn deserialize_basic_decimal() {
        let d: Decimal = serde_json::from_str("1.1234127836128763").unwrap();
        // Typically, this would not work without this feature enabled due to rounding
        assert_eq!(d.to_string(), "1.1234127836128763");
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
    #[cfg(all(feature = "serde-str", not(feature = "serde-float")))]
    fn bincode_serialization() {
        use bincode::{deserialize, serialize};

        let data = [
            "0",
            "0.00",
            "3.14159",
            "-3.14159",
            "1234567890123.4567890",
            "-1234567890123.4567890",
            "5233.9008808150288439427720175",
            "-5233.9008808150288439427720175",
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
    #[cfg(all(feature = "serde-str", feature = "serde-float"))]
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

    #[test]
    #[cfg(all(feature = "serde-str", not(feature = "serde-float")))]
    fn bincode_nested_serialization() {
        // Issue #361
        #[derive(Deserialize, Serialize, Debug)]
        pub struct Foo {
            value: Decimal,
        }

        let s = Foo {
            value: Decimal::new(-1, 3).round_dp(0),
        };
        let ser = bincode::serialize(&s).unwrap();
        let des: Foo = bincode::deserialize(&ser).unwrap();
        assert_eq!(des.value, s.value);
    }
}
