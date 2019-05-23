// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core::fmt;
use prelude::*;
use serde::{de, Deserialize, Deserializer, Serialize, Serializer};

#[cfg(feature = "serde")]
impl Serialize for Uuid {
    fn serialize<S: Serializer>(
        &self,
        serializer: S,
    ) -> Result<S::Ok, S::Error> {
        if serializer.is_human_readable() {
            serializer
                .serialize_str(&self.to_hyphenated().encode_lower(&mut [0; 36]))
        } else {
            serializer.serialize_bytes(self.as_bytes())
        }
    }
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for Uuid {
    fn deserialize<D: Deserializer<'de>>(
        deserializer: D,
    ) -> Result<Self, D::Error> {
        if deserializer.is_human_readable() {
            struct UuidStringVisitor;

            impl<'vi> de::Visitor<'vi> for UuidStringVisitor {
                type Value = Uuid;

                fn expecting(
                    &self,
                    formatter: &mut fmt::Formatter,
                ) -> fmt::Result {
                    write!(formatter, "a UUID string")
                }

                fn visit_str<E: de::Error>(
                    self,
                    value: &str,
                ) -> Result<Uuid, E> {
                    value.parse::<Uuid>().map_err(E::custom)
                }

                fn visit_bytes<E: de::Error>(
                    self,
                    value: &[u8],
                ) -> Result<Uuid, E> {
                    Uuid::from_slice(value).map_err(E::custom)
                }
            }

            deserializer.deserialize_str(UuidStringVisitor)
        } else {
            struct UuidBytesVisitor;

            impl<'vi> de::Visitor<'vi> for UuidBytesVisitor {
                type Value = Uuid;

                fn expecting(
                    &self,
                    formatter: &mut fmt::Formatter,
                ) -> fmt::Result {
                    write!(formatter, "bytes")
                }

                fn visit_bytes<E: de::Error>(
                    self,
                    value: &[u8],
                ) -> Result<Uuid, E> {
                    Uuid::from_slice(value).map_err(E::custom)
                }
            }

            deserializer.deserialize_bytes(UuidBytesVisitor)
        }
    }
}

#[cfg(all(test, feature = "serde"))]
mod serde_tests {
    use serde_test;

    use prelude::*;

    #[test]
    fn test_serialize_readable() {
        use serde_test::Configure;

        let uuid_str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_tokens(
            &u.readable(),
            &[serde_test::Token::Str(uuid_str)],
        );
    }

    #[test]
    fn test_serialize_compact() {
        use serde_test::Configure;

        let uuid_bytes = b"F9168C5E-CEB2-4F";
        let u = Uuid::from_slice(uuid_bytes).unwrap();
        serde_test::assert_tokens(
            &u.compact(),
            &[serde_test::Token::Bytes(uuid_bytes)],
        );
    }
}
