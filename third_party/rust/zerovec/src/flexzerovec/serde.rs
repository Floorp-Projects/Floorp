// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{FlexZeroSlice, FlexZeroVec};
use alloc::vec::Vec;
use core::fmt;
use serde::de::{self, Deserialize, Deserializer, SeqAccess, Visitor};
#[cfg(feature = "serde")]
use serde::ser::{Serialize, SerializeSeq, Serializer};

#[derive(Default)]
struct FlexZeroVecVisitor {}

impl<'de> Visitor<'de> for FlexZeroVecVisitor {
    type Value = FlexZeroVec<'de>;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a sequence or borrowed buffer of bytes")
    }

    fn visit_borrowed_bytes<E>(self, bytes: &'de [u8]) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        FlexZeroVec::parse_byte_slice(bytes).map_err(de::Error::custom)
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
    where
        A: SeqAccess<'de>,
    {
        let mut vec: Vec<usize> = if let Some(capacity) = seq.size_hint() {
            Vec::with_capacity(capacity)
        } else {
            Vec::new()
        };
        while let Some(value) = seq.next_element::<usize>()? {
            vec.push(value);
        }
        Ok(vec.into_iter().collect())
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
impl<'de, 'a> Deserialize<'de> for FlexZeroVec<'a>
where
    'de: 'a,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let visitor = FlexZeroVecVisitor::default();
        if deserializer.is_human_readable() {
            deserializer.deserialize_seq(visitor)
        } else {
            deserializer.deserialize_bytes(visitor)
        }
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
impl<'de, 'a> Deserialize<'de> for &'a FlexZeroSlice
where
    'de: 'a,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            Err(de::Error::custom(
                "&FlexZeroSlice cannot be deserialized from human-readable formats",
            ))
        } else {
            let deserialized: FlexZeroVec<'a> = FlexZeroVec::deserialize(deserializer)?;
            let borrowed = if let FlexZeroVec::Borrowed(b) = deserialized {
                b
            } else {
                return Err(de::Error::custom(
                    "&FlexZeroSlice can only deserialize in zero-copy ways",
                ));
            };
            Ok(borrowed)
        }
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
impl Serialize for FlexZeroVec<'_> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            let mut seq = serializer.serialize_seq(Some(self.len()))?;
            for value in self.iter() {
                seq.serialize_element(&value)?;
            }
            seq.end()
        } else {
            serializer.serialize_bytes(self.as_bytes())
        }
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
impl Serialize for FlexZeroSlice {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        self.as_flexzerovec().serialize(serializer)
    }
}

#[cfg(test)]
#[allow(non_camel_case_types)]
mod test {
    use super::{FlexZeroSlice, FlexZeroVec};

    #[derive(serde::Serialize, serde::Deserialize)]
    struct DeriveTest_FlexZeroVec<'data> {
        #[serde(borrow)]
        _data: FlexZeroVec<'data>,
    }

    #[derive(serde::Serialize, serde::Deserialize)]
    struct DeriveTest_FlexZeroSlice<'data> {
        #[serde(borrow)]
        _data: &'data FlexZeroSlice,
    }

    // [1, 22, 333, 4444];
    const BYTES: &[u8] = &[2, 0x01, 0x00, 0x16, 0x00, 0x4D, 0x01, 0x5C, 0x11];
    const JSON_STR: &str = "[1,22,333,4444]";
    const BINCODE_BUF: &[u8] = &[9, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 22, 0, 77, 1, 92, 17];

    #[test]
    fn test_serde_json() {
        let zerovec_orig: FlexZeroVec = FlexZeroVec::parse_byte_slice(BYTES).expect("parse");
        let json_str = serde_json::to_string(&zerovec_orig).expect("serialize");
        assert_eq!(JSON_STR, json_str);
        // FlexZeroVec should deserialize from JSON to either Vec or FlexZeroVec
        let vec_new: Vec<usize> =
            serde_json::from_str(&json_str).expect("deserialize from buffer to Vec");
        assert_eq!(zerovec_orig.to_vec(), vec_new);
        let zerovec_new: FlexZeroVec =
            serde_json::from_str(&json_str).expect("deserialize from buffer to FlexZeroVec");
        assert_eq!(zerovec_orig.to_vec(), zerovec_new.to_vec());
        assert!(matches!(zerovec_new, FlexZeroVec::Owned(_)));
    }

    #[test]
    fn test_serde_bincode() {
        let zerovec_orig: FlexZeroVec = FlexZeroVec::parse_byte_slice(BYTES).expect("parse");
        let bincode_buf = bincode::serialize(&zerovec_orig).expect("serialize");
        assert_eq!(BINCODE_BUF, bincode_buf);
        let zerovec_new: FlexZeroVec =
            bincode::deserialize(&bincode_buf).expect("deserialize from buffer to FlexZeroVec");
        assert_eq!(zerovec_orig.to_vec(), zerovec_new.to_vec());
        assert!(matches!(zerovec_new, FlexZeroVec::Borrowed(_)));
    }

    #[test]
    fn test_vzv_borrowed() {
        let zerovec_orig: &FlexZeroSlice = FlexZeroSlice::parse_byte_slice(BYTES).expect("parse");
        let bincode_buf = bincode::serialize(&zerovec_orig).expect("serialize");
        assert_eq!(BINCODE_BUF, bincode_buf);
        let zerovec_new: &FlexZeroSlice =
            bincode::deserialize(&bincode_buf).expect("deserialize from buffer to FlexZeroSlice");
        assert_eq!(zerovec_orig.to_vec(), zerovec_new.to_vec());
    }
}
