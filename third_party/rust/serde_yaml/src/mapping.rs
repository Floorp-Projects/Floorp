//! A YAML mapping and its iterator types.

use crate::Value;
use linked_hash_map::LinkedHashMap;
use serde::{Deserialize, Deserializer, Serialize};
use std::fmt;
use std::iter::FromIterator;
use std::ops::{Index, IndexMut};

/// A YAML mapping in which the keys and values are both `serde_yaml::Value`.
#[derive(Clone, Debug, Default, Eq, Hash, PartialEq, PartialOrd)]
pub struct Mapping {
    map: LinkedHashMap<Value, Value>,
}

impl Mapping {
    /// Creates an empty YAML map.
    #[inline]
    pub fn new() -> Self {
        Self::default()
    }

    /// Creates an empty YAML map with the given initial capacity.
    #[inline]
    pub fn with_capacity(capacity: usize) -> Self {
        Mapping {
            map: LinkedHashMap::with_capacity(capacity),
        }
    }

    /// Reserves capacity for at least `additional` more elements to be inserted
    /// into the map. The map may reserve more space to avoid frequent
    /// allocations.
    ///
    /// # Panics
    ///
    /// Panics if the new allocation size overflows `usize`.
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        self.map.reserve(additional)
    }

    /// Shrinks the capacity of the map as much as possible. It will drop down
    /// as much as possible while maintaining the internal rules and possibly
    /// leaving some space in accordance with the resize policy.
    #[inline]
    pub fn shrink_to_fit(&mut self) {
        self.map.shrink_to_fit()
    }

    /// Inserts a key-value pair into the map. If the key already existed, the
    /// old value is returned.
    #[inline]
    pub fn insert(&mut self, k: Value, v: Value) -> Option<Value> {
        self.map.insert(k, v)
    }

    /// Checks if the map contains the given key.
    #[inline]
    pub fn contains_key(&self, k: &Value) -> bool {
        self.map.contains_key(k)
    }

    /// Returns the value corresponding to the key in the map.
    #[inline]
    pub fn get(&self, k: &Value) -> Option<&Value> {
        self.map.get(k)
    }

    /// Returns the mutable reference corresponding to the key in the map.
    #[inline]
    pub fn get_mut(&mut self, k: &Value) -> Option<&mut Value> {
        self.map.get_mut(k)
    }

    /// Removes and returns the value corresponding to the key from the map.
    #[inline]
    pub fn remove(&mut self, k: &Value) -> Option<Value> {
        self.map.remove(k)
    }

    /// Returns the maximum number of key-value pairs the map can hold without
    /// reallocating.
    #[inline]
    pub fn capacity(&self) -> usize {
        self.map.capacity()
    }

    /// Returns the number of key-value pairs in the map.
    #[inline]
    pub fn len(&self) -> usize {
        self.map.len()
    }

    /// Returns whether the map is currently empty.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.map.is_empty()
    }

    /// Clears the map of all key-value pairs.
    #[inline]
    pub fn clear(&mut self) {
        self.map.clear()
    }

    /// Returns a double-ended iterator visiting all key-value pairs in order of
    /// insertion. Iterator element type is `(&'a Value, &'a Value)`.
    #[inline]
    pub fn iter(&self) -> Iter {
        Iter {
            iter: self.map.iter(),
        }
    }

    /// Returns a double-ended iterator visiting all key-value pairs in order of
    /// insertion. Iterator element type is `(&'a Value, &'a mut ValuE)`.
    #[inline]
    pub fn iter_mut(&mut self) -> IterMut {
        IterMut {
            iter: self.map.iter_mut(),
        }
    }
}

impl<'a> Index<&'a Value> for Mapping {
    type Output = Value;
    #[inline]
    fn index(&self, index: &'a Value) -> &Value {
        self.map.index(index)
    }
}

impl<'a> IndexMut<&'a Value> for Mapping {
    #[inline]
    fn index_mut(&mut self, index: &'a Value) -> &mut Value {
        self.map.index_mut(index)
    }
}

impl Extend<(Value, Value)> for Mapping {
    #[inline]
    fn extend<I: IntoIterator<Item = (Value, Value)>>(&mut self, iter: I) {
        self.map.extend(iter);
    }
}

impl FromIterator<(Value, Value)> for Mapping {
    #[inline]
    fn from_iter<I: IntoIterator<Item = (Value, Value)>>(iter: I) -> Self {
        Mapping {
            map: LinkedHashMap::from_iter(iter),
        }
    }
}

macro_rules! delegate_iterator {
    (($name:ident $($generics:tt)*) => $item:ty) => {
        impl $($generics)* Iterator for $name $($generics)* {
            type Item = $item;
            #[inline]
            fn next(&mut self) -> Option<Self::Item> {
                self.iter.next()
            }
            #[inline]
            fn size_hint(&self) -> (usize, Option<usize>) {
                self.iter.size_hint()
            }
        }

        impl $($generics)* ExactSizeIterator for $name $($generics)* {
            #[inline]
            fn len(&self) -> usize {
                self.iter.len()
            }
        }
    }
}

/// Iterator over `&serde_yaml::Mapping`.
pub struct Iter<'a> {
    iter: linked_hash_map::Iter<'a, Value, Value>,
}

delegate_iterator!((Iter<'a>) => (&'a Value, &'a Value));

impl<'a> IntoIterator for &'a Mapping {
    type Item = (&'a Value, &'a Value);
    type IntoIter = Iter<'a>;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        Iter {
            iter: self.map.iter(),
        }
    }
}

/// Iterator over `&mut serde_yaml::Mapping`.
pub struct IterMut<'a> {
    iter: linked_hash_map::IterMut<'a, Value, Value>,
}

delegate_iterator!((IterMut<'a>) => (&'a Value, &'a mut Value));

impl<'a> IntoIterator for &'a mut Mapping {
    type Item = (&'a Value, &'a mut Value);
    type IntoIter = IterMut<'a>;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        IterMut {
            iter: self.map.iter_mut(),
        }
    }
}

/// Iterator over `serde_yaml::Mapping` by value.
pub struct IntoIter {
    iter: linked_hash_map::IntoIter<Value, Value>,
}

delegate_iterator!((IntoIter) => (Value, Value));

impl IntoIterator for Mapping {
    type Item = (Value, Value);
    type IntoIter = IntoIter;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        IntoIter {
            iter: self.map.into_iter(),
        }
    }
}

impl Serialize for Mapping {
    #[inline]
    fn serialize<S: serde::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        use serde::ser::SerializeMap;
        let mut map_serializer = serializer.serialize_map(Some(self.len()))?;
        for (k, v) in self {
            map_serializer.serialize_entry(k, v)?;
        }
        map_serializer.end()
    }
}

impl<'de> Deserialize<'de> for Mapping {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct Visitor;

        impl<'de> serde::de::Visitor<'de> for Visitor {
            type Value = Mapping;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a YAML mapping")
            }

            #[inline]
            fn visit_unit<E>(self) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(Mapping::new())
            }

            #[inline]
            fn visit_map<V>(self, mut visitor: V) -> Result<Self::Value, V::Error>
            where
                V: serde::de::MapAccess<'de>,
            {
                let mut values = Mapping::new();
                while let Some((k, v)) = visitor.next_entry()? {
                    values.insert(k, v);
                }
                Ok(values)
            }
        }

        deserializer.deserialize_map(Visitor)
    }
}
