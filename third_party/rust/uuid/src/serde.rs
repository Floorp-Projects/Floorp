extern crate serde;
extern crate std;

use self::std::prelude::v1::*;
use self::serde::{de, Deserialize, Deserializer, Serialize, Serializer};

use Uuid;

impl Serialize for Uuid {
    fn serialize<S: Serializer>(&self, serializer: &mut S) -> Result<(), S::Error> {
        serializer.serialize_str(&self.hyphenated().to_string())
    }
}

impl Deserialize for Uuid {
    fn deserialize<D: Deserializer>(deserializer: &mut D) -> Result<Self, D::Error> {
        struct UuidVisitor;

        impl de::Visitor for UuidVisitor {
            type Value = Uuid;

            fn visit_str<E: de::Error>(&mut self, value: &str) -> Result<Uuid, E> {
                value.parse::<Uuid>().map_err(|e| E::custom(e.to_string()))
            }

            fn visit_bytes<E: de::Error>(&mut self, value: &[u8]) -> Result<Uuid, E> {
                Uuid::from_bytes(value).map_err(|e| E::custom(e.to_string()))
            }
        }

        deserializer.deserialize(UuidVisitor)
                    .or_else(|_| deserializer.deserialize_string(UuidVisitor))
                    .or_else(|_| deserializer.deserialize_bytes(UuidVisitor))
    }
}
