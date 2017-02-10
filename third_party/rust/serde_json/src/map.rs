use serde::{ser, de};
use std::fmt::{self, Debug};
use value::Value;
use std::hash::Hash;
use std::iter::FromIterator;
use std::borrow::Borrow;
use std::ops;

#[cfg(not(feature = "preserve_order"))]
use std::collections::{BTreeMap, btree_map};

#[cfg(feature = "preserve_order")]
use linked_hash_map::{self, LinkedHashMap};

/// Represents a key/value type.
pub struct Map<K, V> {
    map: MapImpl<K, V>,
}

#[cfg(not(feature = "preserve_order"))]
type MapImpl<K, V> = BTreeMap<K, V>;
#[cfg(feature = "preserve_order")]
type MapImpl<K, V> = LinkedHashMap<K, V>;

impl Map<String, Value> {
    /// Makes a new empty Map.
    #[inline]
    pub fn new() -> Self {
        Map {
            map: MapImpl::new(),
        }
    }

    #[cfg(not(feature = "preserve_order"))]
    /// Makes a new empty Map with the given initial capacity.
    #[inline]
    pub fn with_capacity(capacity: usize) -> Self {
        // does not support with_capacity
        let _ = capacity;
        Map {
            map: BTreeMap::new(),
        }
    }

    #[cfg(feature = "preserve_order")]
    /// Makes a new empty Map with the given initial capacity.
    #[inline]
    pub fn with_capacity(capacity: usize) -> Self {
        Map {
            map: LinkedHashMap::with_capacity(capacity),
        }
    }

    /// Clears the map, removing all values.
    #[inline]
    pub fn clear(&mut self) {
        self.map.clear()
    }

    /// Returns a reference to the value corresponding to the key.
    ///
    /// The key may be any borrowed form of the map's key type, but the ordering
    /// on the borrowed form *must* match the ordering on the key type.
    #[inline]
    pub fn get<Q: ?Sized>(&self, key: &Q) -> Option<&Value>
        where String: Borrow<Q>,
              Q: Ord + Eq + Hash
    {
        self.map.get(key)
    }

    /// Returns true if the map contains a value for the specified key.
    ///
    /// The key may be any borrowed form of the map's key type, but the ordering
    /// on the borrowed form *must* match the ordering on the key type.
    #[inline]
    pub fn contains_key<Q: ?Sized>(&self, key: &Q) -> bool
        where String: Borrow<Q>,
              Q: Ord + Eq + Hash
    {
        self.map.contains_key(key)
    }

    /// Returns a mutable reference to the value corresponding to the key.
    ///
    /// The key may be any borrowed form of the map's key type, but the ordering
    /// on the borrowed form *must* match the ordering on the key type.
    #[inline]
    pub fn get_mut<Q: ?Sized>(&mut self, key: &Q) -> Option<&mut Value>
        where String: Borrow<Q>,
              Q: Ord + Eq + Hash
    {
        self.map.get_mut(key)
    }

    /// Inserts a key-value pair into the map.
    ///
    /// If the map did not have this key present, `None` is returned.
    ///
    /// If the map did have this key present, the value is updated, and the old
    /// value is returned. The key is not updated, though; this matters for
    /// types that can be `==` without being identical.
    #[inline]
    pub fn insert(&mut self, k: String, v: Value) -> Option<Value> {
        self.map.insert(k, v)
    }

    /// Removes a key from the map, returning the value at the key if the key
    /// was previously in the map.
    ///
    /// The key may be any borrowed form of the map's key type, but the ordering
    /// on the borrowed form *must* match the ordering on the key type.
    #[inline]
    pub fn remove<Q: ?Sized>(&mut self, key: &Q) -> Option<Value>
        where String: Borrow<Q>,
              Q: Ord + Eq + Hash
    {
        self.map.remove(key)
    }

    /// Returns the number of elements in the map.
    #[inline]
    pub fn len(&self) -> usize {
        self.map.len()
    }

    /// Returns true if the map contains no elements.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.map.is_empty()
    }

    /// Gets an iterator over the entries of the map.
    #[inline]
    pub fn iter(&self) -> MapIter {
        MapIter {
            iter: self.map.iter(),
        }
    }

    /// Gets a mutable iterator over the entries of the map.
    #[inline]
    pub fn iter_mut(&mut self) -> MapIterMut {
        MapIterMut {
            iter: self.map.iter_mut(),
        }
    }

    /// Gets an iterator over the keys of the map.
    #[inline]
    pub fn keys(&self) -> MapKeys {
        MapKeys {
            iter: self.map.keys(),
        }
    }

    /// Gets an iterator over the values of the map.
    #[inline]
    pub fn values(&self) -> MapValues {
        MapValues {
            iter: self.map.values(),
        }
    }
}

impl Default for Map<String, Value> {
    #[inline]
    fn default() -> Self {
        Map {
            map: MapImpl::new(),
        }
    }
}

impl Clone for Map<String, Value> {
    #[inline]
    fn clone(&self) -> Self {
        Map {
            map: self.map.clone(),
        }
    }
}

impl PartialEq for Map<String, Value> {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.map.eq(&other.map)
    }
}

/// Access an element of this map. Panics if the given key is not present in the
/// map.
///
/// ```rust
/// # use serde_json::Value;
/// # let val = &Value::String("".to_owned());
/// # let _ =
/// match *val {
///     Value::String(ref s) => Some(s.as_str()),
///     Value::Array(ref arr) => arr[0].as_str(),
///     Value::Object(ref map) => map["type"].as_str(),
///     _ => None,
/// }
/// # ;
/// ```
impl<'a, Q: ?Sized> ops::Index<&'a Q> for Map<String, Value>
    where String: Borrow<Q>,
          Q: Ord + Eq + Hash
{
    type Output = Value;

    fn index(&self, index: &Q) -> &Value {
        self.map.index(index)
    }
}

impl Debug for Map<String, Value> {
    #[inline]
    fn fmt(&self, formatter: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        self.map.fmt(formatter)
    }
}

impl ser::Serialize for Map<String, Value> {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: ser::Serializer
    {
        use serde::ser::SerializeMap;
        let mut map = try!(serializer.serialize_map(Some(self.len())));
        for (k, v) in self {
            try!(map.serialize_key(k));
            try!(map.serialize_value(v));
        }
        map.end()
    }
}

impl de::Deserialize for Map<String, Value> {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: de::Deserializer
    {
        struct Visitor;

        impl de::Visitor for Visitor {
            type Value = Map<String, Value>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            #[inline]
            fn visit_unit<E>(self) -> Result<Self::Value, E>
                where E: de::Error
            {
                Ok(Map::new())
            }

            #[inline]
            fn visit_map<V>(self, mut visitor: V) -> Result<Self::Value, V::Error>
                where V: de::MapVisitor
            {
                let mut values = Map::with_capacity(visitor.size_hint().0);

                while let Some((key, value)) = try!(visitor.visit()) {
                    values.insert(key, value);
                }

                Ok(values)
            }
        }

        deserializer.deserialize_map(Visitor)
    }
}

impl FromIterator<(String, Value)> for Map<String, Value> {
    fn from_iter<T>(iter: T) -> Self where T: IntoIterator<Item=(String, Value)> {
        Map {
            map: FromIterator::from_iter(iter)
        }
    }
}

impl Extend<(String, Value)> for Map<String, Value> {
    fn extend<T>(&mut self, iter: T) where T: IntoIterator<Item=(String, Value)> {
        self.map.extend(iter);
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

        impl $($generics)* DoubleEndedIterator for $name $($generics)* {
            #[inline]
            fn next_back(&mut self) -> Option<Self::Item> {
                self.iter.next_back()
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

//////////////////////////////////////////////////////////////////////////////

impl<'a> IntoIterator for &'a Map<String, Value> {
    type Item = (&'a String, &'a Value);
    type IntoIter = MapIter<'a>;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        MapIter {
            iter: self.map.iter(),
        }
    }
}

pub struct MapIter<'a> {
    iter: MapIterImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type MapIterImpl<'a> = btree_map::Iter<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type MapIterImpl<'a> = linked_hash_map::Iter<'a, String, Value>;

delegate_iterator!((MapIter<'a>) => (&'a String, &'a Value));

//////////////////////////////////////////////////////////////////////////////

impl<'a> IntoIterator for &'a mut Map<String, Value> {
    type Item = (&'a String, &'a mut Value);
    type IntoIter = MapIterMut<'a>;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        MapIterMut {
            iter: self.map.iter_mut(),
        }
    }
}

pub struct MapIterMut<'a> {
    iter: MapIterMutImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type MapIterMutImpl<'a> = btree_map::IterMut<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type MapIterMutImpl<'a> = linked_hash_map::IterMut<'a, String, Value>;

delegate_iterator!((MapIterMut<'a>) => (&'a String, &'a mut Value));

//////////////////////////////////////////////////////////////////////////////

impl IntoIterator for Map<String, Value> {
    type Item = (String, Value);
    type IntoIter = MapIntoIter;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        MapIntoIter {
            iter: self.map.into_iter(),
        }
    }
}

pub struct MapIntoIter {
    iter: MapIntoIterImpl,
}

#[cfg(not(feature = "preserve_order"))]
type MapIntoIterImpl = btree_map::IntoIter<String, Value>;
#[cfg(feature = "preserve_order")]
type MapIntoIterImpl = linked_hash_map::IntoIter<String, Value>;

delegate_iterator!((MapIntoIter) => (String, Value));

//////////////////////////////////////////////////////////////////////////////

pub struct MapKeys<'a> {
    iter: MapKeysImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type MapKeysImpl<'a> = btree_map::Keys<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type MapKeysImpl<'a> = linked_hash_map::Keys<'a, String, Value>;

delegate_iterator!((MapKeys<'a>) => &'a String);

//////////////////////////////////////////////////////////////////////////////

pub struct MapValues<'a> {
    iter: MapValuesImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type MapValuesImpl<'a> = btree_map::Values<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type MapValuesImpl<'a> = linked_hash_map::Values<'a, String, Value>;

delegate_iterator!((MapValues<'a>) => &'a Value);
