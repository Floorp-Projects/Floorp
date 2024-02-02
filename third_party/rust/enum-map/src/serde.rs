// SPDX-FileCopyrightText: 2017 - 2023 Kamila Borowska <kamila@borowska.pw>
// SPDX-FileCopyrightText: 2021 Bruno Corrêa Zimmermann <brunoczim@gmail.com>
//
// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{enum_map, EnumArray, EnumMap};
use core::fmt;
use core::marker::PhantomData;
use serde::de::{self, Deserialize, Deserializer, Error, MapAccess, SeqAccess};
use serde::ser::{Serialize, SerializeTuple, Serializer};

/// Requires crate feature `"serde"`
impl<K: EnumArray<V> + Serialize, V: Serialize> Serialize for EnumMap<K, V> {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        if serializer.is_human_readable() {
            serializer.collect_map(self)
        } else {
            let mut tup = serializer.serialize_tuple(self.len())?;
            for value in self.values() {
                tup.serialize_element(value)?;
            }
            tup.end()
        }
    }
}

/// Requires crate feature `"serde"`
impl<'de, K, V> Deserialize<'de> for EnumMap<K, V>
where
    K: EnumArray<V> + EnumArray<Option<V>> + Deserialize<'de>,
    V: Deserialize<'de>,
{
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        if deserializer.is_human_readable() {
            deserializer.deserialize_map(HumanReadableVisitor(PhantomData))
        } else {
            deserializer.deserialize_tuple(K::LENGTH, CompactVisitor(PhantomData))
        }
    }
}

struct HumanReadableVisitor<K, V>(PhantomData<(K, V)>);

impl<'de, K, V> de::Visitor<'de> for HumanReadableVisitor<K, V>
where
    K: EnumArray<V> + EnumArray<Option<V>> + Deserialize<'de>,
    V: Deserialize<'de>,
{
    type Value = EnumMap<K, V>;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "a map")
    }

    fn visit_map<M: MapAccess<'de>>(self, mut access: M) -> Result<Self::Value, M::Error> {
        let mut entries = EnumMap::default();
        while let Some((key, value)) = access.next_entry()? {
            entries[key] = Some(value);
        }
        for value in entries.values() {
            value
                .as_ref()
                .ok_or_else(|| M::Error::custom("key not specified"))?;
        }
        Ok(enum_map! { key => entries[key].take().unwrap() })
    }
}

struct CompactVisitor<K, V>(PhantomData<(K, V)>);

impl<'de, K, V> de::Visitor<'de> for CompactVisitor<K, V>
where
    K: EnumArray<V> + EnumArray<Option<V>> + Deserialize<'de>,
    V: Deserialize<'de>,
{
    type Value = EnumMap<K, V>;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "a sequence")
    }

    fn visit_seq<M: SeqAccess<'de>>(self, mut access: M) -> Result<Self::Value, M::Error> {
        let mut entries = EnumMap::default();
        let len = entries.len();
        {
            let mut iter = entries.values_mut();
            while let Some(place) = iter.next() {
                *place = Some(access.next_element()?.ok_or_else(|| {
                    M::Error::invalid_length(
                        len - iter.len() - 1,
                        &"a sequence with as many elements as there are variants",
                    )
                })?);
            }
        }
        Ok(enum_map! { key => entries[key].take().unwrap() })
    }
}
