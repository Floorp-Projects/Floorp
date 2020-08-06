
extern crate serde;

use self::serde::ser::{Serialize, Serializer, SerializeMap, SerializeSeq};
use self::serde::de::{Deserialize, Deserializer, Error, IntoDeserializer, MapAccess, SeqAccess, Visitor};
use self::serde::de::value::{MapDeserializer, SeqDeserializer};

use std::fmt::{self, Formatter};
use std::hash::{BuildHasher, Hash};
use std::marker::PhantomData;

use IndexMap;

/// Requires crate feature `"serde-1"`
impl<K, V, S> Serialize for IndexMap<K, V, S>
    where K: Serialize + Hash + Eq,
          V: Serialize,
          S: BuildHasher
{
    fn serialize<T>(&self, serializer: T) -> Result<T::Ok, T::Error>
        where T: Serializer
    {
        let mut map_serializer = try!(serializer.serialize_map(Some(self.len())));
        for (key, value) in self {
            try!(map_serializer.serialize_entry(key, value));
        }
        map_serializer.end()
    }
}

struct OrderMapVisitor<K, V, S>(PhantomData<(K, V, S)>);

impl<'de, K, V, S> Visitor<'de> for OrderMapVisitor<K, V, S>
    where K: Deserialize<'de> + Eq + Hash,
          V: Deserialize<'de>,
          S: Default + BuildHasher
{
    type Value = IndexMap<K, V, S>;

    fn expecting(&self, formatter: &mut Formatter) -> fmt::Result {
        write!(formatter, "a map")
    }

    fn visit_map<A>(self, mut map: A) -> Result<Self::Value, A::Error>
        where A: MapAccess<'de>
    {
        let mut values = IndexMap::with_capacity_and_hasher(map.size_hint().unwrap_or(0), S::default());

        while let Some((key, value)) = try!(map.next_entry()) {
            values.insert(key, value);
        }

        Ok(values)
    }
}

/// Requires crate feature `"serde-1"`
impl<'de, K, V, S> Deserialize<'de> for IndexMap<K, V, S>
    where K: Deserialize<'de> + Eq + Hash,
          V: Deserialize<'de>,
          S: Default + BuildHasher
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: Deserializer<'de>
    {
        deserializer.deserialize_map(OrderMapVisitor(PhantomData))
    }
}

impl<'de, K, V, S, E> IntoDeserializer<'de, E> for IndexMap<K, V, S>
    where K: IntoDeserializer<'de, E> + Eq + Hash,
          V: IntoDeserializer<'de, E>,
          S: BuildHasher,
          E: Error,
{
    type Deserializer = MapDeserializer<'de, <Self as IntoIterator>::IntoIter, E>;

    fn into_deserializer(self) -> Self::Deserializer {
        MapDeserializer::new(self.into_iter())
    }
}


use IndexSet;

/// Requires crate feature `"serde-1"`
impl<T, S> Serialize for IndexSet<T, S>
    where T: Serialize + Hash + Eq,
          S: BuildHasher
{
    fn serialize<Se>(&self, serializer: Se) -> Result<Se::Ok, Se::Error>
        where Se: Serializer
    {
        let mut set_serializer = try!(serializer.serialize_seq(Some(self.len())));
        for value in self {
            try!(set_serializer.serialize_element(value));
        }
        set_serializer.end()
    }
}

struct OrderSetVisitor<T, S>(PhantomData<(T, S)>);

impl<'de, T, S> Visitor<'de> for OrderSetVisitor<T, S>
    where T: Deserialize<'de> + Eq + Hash,
          S: Default + BuildHasher
{
    type Value = IndexSet<T, S>;

    fn expecting(&self, formatter: &mut Formatter) -> fmt::Result {
        write!(formatter, "a set")
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
        where A: SeqAccess<'de>
    {
        let mut values = IndexSet::with_capacity_and_hasher(seq.size_hint().unwrap_or(0), S::default());

        while let Some(value) = try!(seq.next_element()) {
            values.insert(value);
        }

        Ok(values)
    }
}

/// Requires crate feature `"serde-1"`
impl<'de, T, S> Deserialize<'de> for IndexSet<T, S>
    where T: Deserialize<'de> + Eq + Hash,
          S: Default + BuildHasher
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: Deserializer<'de>
    {
        deserializer.deserialize_seq(OrderSetVisitor(PhantomData))
    }
}

impl<'de, T, S, E> IntoDeserializer<'de, E> for IndexSet<T, S>
    where T: IntoDeserializer<'de, E> + Eq + Hash,
          S: BuildHasher,
          E: Error,
{
    type Deserializer = SeqDeserializer<<Self as IntoIterator>::IntoIter, E>;

    fn into_deserializer(self) -> Self::Deserializer {
        SeqDeserializer::new(self.into_iter())
    }
}
