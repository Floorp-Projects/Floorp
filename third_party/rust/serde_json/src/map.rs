//! A map of String to serde_json::Value.
//!
//! By default the map is backed by a [`BTreeMap`]. Enable the `preserve_order`
//! feature of serde_json to use [`IndexMap`] instead.
//!
//! [`BTreeMap`]: https://doc.rust-lang.org/std/collections/struct.BTreeMap.html
//! [`IndexMap`]: https://docs.rs/indexmap/*/indexmap/map/struct.IndexMap.html

use crate::lib::borrow::Borrow;
use crate::lib::iter::FromIterator;
use crate::lib::*;
use crate::value::Value;
use serde::de;

#[cfg(feature = "preserve_order")]
use indexmap::{self, IndexMap};

/// Represents a JSON key/value type.
pub struct Map<K, V> {
    map: MapImpl<K, V>,
}

#[cfg(not(feature = "preserve_order"))]
type MapImpl<K, V> = BTreeMap<K, V>;
#[cfg(feature = "preserve_order")]
type MapImpl<K, V> = IndexMap<K, V>;

impl Map<String, Value> {
    /// Makes a new empty Map.
    #[inline]
    pub fn new() -> Self {
        Map {
            map: MapImpl::new(),
        }
    }

    /// Makes a new empty Map with the given initial capacity.
    #[inline]
    pub fn with_capacity(capacity: usize) -> Self {
        Map {
            #[cfg(not(feature = "preserve_order"))]
            map: {
                // does not support with_capacity
                let _ = capacity;
                BTreeMap::new()
            },
            #[cfg(feature = "preserve_order")]
            map: IndexMap::with_capacity(capacity),
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
    pub fn get<Q>(&self, key: &Q) -> Option<&Value>
    where
        String: Borrow<Q>,
        Q: ?Sized + Ord + Eq + Hash,
    {
        self.map.get(key)
    }

    /// Returns true if the map contains a value for the specified key.
    ///
    /// The key may be any borrowed form of the map's key type, but the ordering
    /// on the borrowed form *must* match the ordering on the key type.
    #[inline]
    pub fn contains_key<Q>(&self, key: &Q) -> bool
    where
        String: Borrow<Q>,
        Q: ?Sized + Ord + Eq + Hash,
    {
        self.map.contains_key(key)
    }

    /// Returns a mutable reference to the value corresponding to the key.
    ///
    /// The key may be any borrowed form of the map's key type, but the ordering
    /// on the borrowed form *must* match the ordering on the key type.
    #[inline]
    pub fn get_mut<Q>(&mut self, key: &Q) -> Option<&mut Value>
    where
        String: Borrow<Q>,
        Q: ?Sized + Ord + Eq + Hash,
    {
        self.map.get_mut(key)
    }

    /// Inserts a key-value pair into the map.
    ///
    /// If the map did not have this key present, `None` is returned.
    ///
    /// If the map did have this key present, the value is updated, and the old
    /// value is returned.
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
    pub fn remove<Q>(&mut self, key: &Q) -> Option<Value>
    where
        String: Borrow<Q>,
        Q: ?Sized + Ord + Eq + Hash,
    {
        #[cfg(feature = "preserve_order")]
        return self.map.swap_remove(key);
        #[cfg(not(feature = "preserve_order"))]
        return self.map.remove(key);
    }

    /// Removes a key from the map, returning the stored key and value if the
    /// key was previously in the map.
    ///
    /// The key may be any borrowed form of the map's key type, but the ordering
    /// on the borrowed form *must* match the ordering on the key type.
    pub fn remove_entry<Q>(&mut self, key: &Q) -> Option<(String, Value)>
    where
        String: Borrow<Q>,
        Q: ?Sized + Ord + Eq + Hash,
    {
        #[cfg(any(feature = "preserve_order", not(no_btreemap_remove_entry)))]
        return self.map.remove_entry(key);
        #[cfg(all(
            not(feature = "preserve_order"),
            no_btreemap_remove_entry,
            not(no_btreemap_get_key_value),
        ))]
        {
            let (key, _value) = self.map.get_key_value(key)?;
            let key = key.clone();
            let value = self.map.remove::<String>(&key)?;
            Some((key, value))
        }
        #[cfg(all(
            not(feature = "preserve_order"),
            no_btreemap_remove_entry,
            no_btreemap_get_key_value,
        ))]
        {
            struct Key<'a, Q: ?Sized>(&'a Q);

            impl<'a, Q: ?Sized> RangeBounds<Q> for Key<'a, Q> {
                fn start_bound(&self) -> Bound<&Q> {
                    Bound::Included(self.0)
                }
                fn end_bound(&self) -> Bound<&Q> {
                    Bound::Included(self.0)
                }
            }

            let mut range = self.map.range(Key(key));
            let (key, _value) = range.next()?;
            let key = key.clone();
            let value = self.map.remove::<String>(&key)?;
            Some((key, value))
        }
    }

    /// Moves all elements from other into Self, leaving other empty.
    #[inline]
    pub fn append(&mut self, other: &mut Self) {
        #[cfg(feature = "preserve_order")]
        for (k, v) in mem::replace(&mut other.map, MapImpl::default()) {
            self.map.insert(k, v);
        }
        #[cfg(not(feature = "preserve_order"))]
        self.map.append(&mut other.map);
    }

    /// Gets the given key's corresponding entry in the map for in-place
    /// manipulation.
    pub fn entry<S>(&mut self, key: S) -> Entry
    where
        S: Into<String>,
    {
        #[cfg(not(feature = "preserve_order"))]
        use crate::lib::btree_map::Entry as EntryImpl;
        #[cfg(feature = "preserve_order")]
        use indexmap::map::Entry as EntryImpl;

        match self.map.entry(key.into()) {
            EntryImpl::Vacant(vacant) => Entry::Vacant(VacantEntry { vacant }),
            EntryImpl::Occupied(occupied) => Entry::Occupied(OccupiedEntry { occupied }),
        }
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
    pub fn iter(&self) -> Iter {
        Iter {
            iter: self.map.iter(),
        }
    }

    /// Gets a mutable iterator over the entries of the map.
    #[inline]
    pub fn iter_mut(&mut self) -> IterMut {
        IterMut {
            iter: self.map.iter_mut(),
        }
    }

    /// Gets an iterator over the keys of the map.
    #[inline]
    pub fn keys(&self) -> Keys {
        Keys {
            iter: self.map.keys(),
        }
    }

    /// Gets an iterator over the values of the map.
    #[inline]
    pub fn values(&self) -> Values {
        Values {
            iter: self.map.values(),
        }
    }

    /// Gets an iterator over mutable values of the map.
    #[inline]
    pub fn values_mut(&mut self) -> ValuesMut {
        ValuesMut {
            iter: self.map.values_mut(),
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

impl Eq for Map<String, Value> {}

/// Access an element of this map. Panics if the given key is not present in the
/// map.
///
/// ```
/// # use serde_json::Value;
/// #
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
impl<'a, Q> ops::Index<&'a Q> for Map<String, Value>
where
    String: Borrow<Q>,
    Q: ?Sized + Ord + Eq + Hash,
{
    type Output = Value;

    fn index(&self, index: &Q) -> &Value {
        self.map.index(index)
    }
}

/// Mutably access an element of this map. Panics if the given key is not
/// present in the map.
///
/// ```
/// # use serde_json::json;
/// #
/// # let mut map = serde_json::Map::new();
/// # map.insert("key".to_owned(), serde_json::Value::Null);
/// #
/// map["key"] = json!("value");
/// ```
impl<'a, Q> ops::IndexMut<&'a Q> for Map<String, Value>
where
    String: Borrow<Q>,
    Q: ?Sized + Ord + Eq + Hash,
{
    fn index_mut(&mut self, index: &Q) -> &mut Value {
        self.map.get_mut(index).expect("no entry found for key")
    }
}

impl Debug for Map<String, Value> {
    #[inline]
    fn fmt(&self, formatter: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        self.map.fmt(formatter)
    }
}

#[cfg(any(feature = "std", feature = "alloc"))]
impl serde::ser::Serialize for Map<String, Value> {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        use serde::ser::SerializeMap;
        let mut map = tri!(serializer.serialize_map(Some(self.len())));
        for (k, v) in self {
            tri!(map.serialize_entry(k, v));
        }
        map.end()
    }
}

impl<'de> de::Deserialize<'de> for Map<String, Value> {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        struct Visitor;

        impl<'de> de::Visitor<'de> for Visitor {
            type Value = Map<String, Value>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            #[inline]
            fn visit_unit<E>(self) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                Ok(Map::new())
            }

            #[cfg(any(feature = "std", feature = "alloc"))]
            #[inline]
            fn visit_map<V>(self, mut visitor: V) -> Result<Self::Value, V::Error>
            where
                V: de::MapAccess<'de>,
            {
                let mut values = Map::new();

                while let Some((key, value)) = tri!(visitor.next_entry()) {
                    values.insert(key, value);
                }

                Ok(values)
            }
        }

        deserializer.deserialize_map(Visitor)
    }
}

impl FromIterator<(String, Value)> for Map<String, Value> {
    fn from_iter<T>(iter: T) -> Self
    where
        T: IntoIterator<Item = (String, Value)>,
    {
        Map {
            map: FromIterator::from_iter(iter),
        }
    }
}

impl Extend<(String, Value)> for Map<String, Value> {
    fn extend<T>(&mut self, iter: T)
    where
        T: IntoIterator<Item = (String, Value)>,
    {
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

        impl $($generics)* FusedIterator for $name $($generics)* {}
    }
}

//////////////////////////////////////////////////////////////////////////////

/// A view into a single entry in a map, which may either be vacant or occupied.
/// This enum is constructed from the [`entry`] method on [`Map`].
///
/// [`entry`]: struct.Map.html#method.entry
/// [`Map`]: struct.Map.html
pub enum Entry<'a> {
    /// A vacant Entry.
    Vacant(VacantEntry<'a>),
    /// An occupied Entry.
    Occupied(OccupiedEntry<'a>),
}

/// A vacant Entry. It is part of the [`Entry`] enum.
///
/// [`Entry`]: enum.Entry.html
pub struct VacantEntry<'a> {
    vacant: VacantEntryImpl<'a>,
}

/// An occupied Entry. It is part of the [`Entry`] enum.
///
/// [`Entry`]: enum.Entry.html
pub struct OccupiedEntry<'a> {
    occupied: OccupiedEntryImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type VacantEntryImpl<'a> = btree_map::VacantEntry<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type VacantEntryImpl<'a> = indexmap::map::VacantEntry<'a, String, Value>;

#[cfg(not(feature = "preserve_order"))]
type OccupiedEntryImpl<'a> = btree_map::OccupiedEntry<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type OccupiedEntryImpl<'a> = indexmap::map::OccupiedEntry<'a, String, Value>;

impl<'a> Entry<'a> {
    /// Returns a reference to this entry's key.
    ///
    /// # Examples
    ///
    /// ```
    /// let mut map = serde_json::Map::new();
    /// assert_eq!(map.entry("serde").key(), &"serde");
    /// ```
    pub fn key(&self) -> &String {
        match *self {
            Entry::Vacant(ref e) => e.key(),
            Entry::Occupied(ref e) => e.key(),
        }
    }

    /// Ensures a value is in the entry by inserting the default if empty, and
    /// returns a mutable reference to the value in the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// let mut map = serde_json::Map::new();
    /// map.entry("serde").or_insert(json!(12));
    ///
    /// assert_eq!(map["serde"], 12);
    /// ```
    pub fn or_insert(self, default: Value) -> &'a mut Value {
        match self {
            Entry::Vacant(entry) => entry.insert(default),
            Entry::Occupied(entry) => entry.into_mut(),
        }
    }

    /// Ensures a value is in the entry by inserting the result of the default
    /// function if empty, and returns a mutable reference to the value in the
    /// entry.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// let mut map = serde_json::Map::new();
    /// map.entry("serde").or_insert_with(|| json!("hoho"));
    ///
    /// assert_eq!(map["serde"], "hoho".to_owned());
    /// ```
    pub fn or_insert_with<F>(self, default: F) -> &'a mut Value
    where
        F: FnOnce() -> Value,
    {
        match self {
            Entry::Vacant(entry) => entry.insert(default()),
            Entry::Occupied(entry) => entry.into_mut(),
        }
    }

    /// Provides in-place mutable access to an occupied entry before any
    /// potential inserts into the map.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// let mut map = serde_json::Map::new();
    /// map.entry("serde")
    ///     .and_modify(|e| *e = json!("rust"))
    ///     .or_insert(json!("cpp"));
    ///
    /// assert_eq!(map["serde"], "cpp");
    ///
    /// map.entry("serde")
    ///     .and_modify(|e| *e = json!("rust"))
    ///     .or_insert(json!("cpp"));
    ///
    /// assert_eq!(map["serde"], "rust");
    /// ```
    pub fn and_modify<F>(self, f: F) -> Self
    where
        F: FnOnce(&mut Value),
    {
        match self {
            Entry::Occupied(mut entry) => {
                f(entry.get_mut());
                Entry::Occupied(entry)
            }
            Entry::Vacant(entry) => Entry::Vacant(entry),
        }
    }
}

impl<'a> VacantEntry<'a> {
    /// Gets a reference to the key that would be used when inserting a value
    /// through the VacantEntry.
    ///
    /// # Examples
    ///
    /// ```
    /// use serde_json::map::Entry;
    ///
    /// let mut map = serde_json::Map::new();
    ///
    /// match map.entry("serde") {
    ///     Entry::Vacant(vacant) => {
    ///         assert_eq!(vacant.key(), &"serde");
    ///     }
    ///     Entry::Occupied(_) => unimplemented!(),
    /// }
    /// ```
    #[inline]
    pub fn key(&self) -> &String {
        self.vacant.key()
    }

    /// Sets the value of the entry with the VacantEntry's key, and returns a
    /// mutable reference to it.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// use serde_json::map::Entry;
    ///
    /// let mut map = serde_json::Map::new();
    ///
    /// match map.entry("serde") {
    ///     Entry::Vacant(vacant) => {
    ///         vacant.insert(json!("hoho"));
    ///     }
    ///     Entry::Occupied(_) => unimplemented!(),
    /// }
    /// ```
    #[inline]
    pub fn insert(self, value: Value) -> &'a mut Value {
        self.vacant.insert(value)
    }
}

impl<'a> OccupiedEntry<'a> {
    /// Gets a reference to the key in the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// use serde_json::map::Entry;
    ///
    /// let mut map = serde_json::Map::new();
    /// map.insert("serde".to_owned(), json!(12));
    ///
    /// match map.entry("serde") {
    ///     Entry::Occupied(occupied) => {
    ///         assert_eq!(occupied.key(), &"serde");
    ///     }
    ///     Entry::Vacant(_) => unimplemented!(),
    /// }
    /// ```
    #[inline]
    pub fn key(&self) -> &String {
        self.occupied.key()
    }

    /// Gets a reference to the value in the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// use serde_json::map::Entry;
    ///
    /// let mut map = serde_json::Map::new();
    /// map.insert("serde".to_owned(), json!(12));
    ///
    /// match map.entry("serde") {
    ///     Entry::Occupied(occupied) => {
    ///         assert_eq!(occupied.get(), 12);
    ///     }
    ///     Entry::Vacant(_) => unimplemented!(),
    /// }
    /// ```
    #[inline]
    pub fn get(&self) -> &Value {
        self.occupied.get()
    }

    /// Gets a mutable reference to the value in the entry.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// use serde_json::map::Entry;
    ///
    /// let mut map = serde_json::Map::new();
    /// map.insert("serde".to_owned(), json!([1, 2, 3]));
    ///
    /// match map.entry("serde") {
    ///     Entry::Occupied(mut occupied) => {
    ///         occupied.get_mut().as_array_mut().unwrap().push(json!(4));
    ///     }
    ///     Entry::Vacant(_) => unimplemented!(),
    /// }
    ///
    /// assert_eq!(map["serde"].as_array().unwrap().len(), 4);
    /// ```
    #[inline]
    pub fn get_mut(&mut self) -> &mut Value {
        self.occupied.get_mut()
    }

    /// Converts the entry into a mutable reference to its value.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// use serde_json::map::Entry;
    ///
    /// let mut map = serde_json::Map::new();
    /// map.insert("serde".to_owned(), json!([1, 2, 3]));
    ///
    /// match map.entry("serde") {
    ///     Entry::Occupied(mut occupied) => {
    ///         occupied.into_mut().as_array_mut().unwrap().push(json!(4));
    ///     }
    ///     Entry::Vacant(_) => unimplemented!(),
    /// }
    ///
    /// assert_eq!(map["serde"].as_array().unwrap().len(), 4);
    /// ```
    #[inline]
    pub fn into_mut(self) -> &'a mut Value {
        self.occupied.into_mut()
    }

    /// Sets the value of the entry with the `OccupiedEntry`'s key, and returns
    /// the entry's old value.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// use serde_json::map::Entry;
    ///
    /// let mut map = serde_json::Map::new();
    /// map.insert("serde".to_owned(), json!(12));
    ///
    /// match map.entry("serde") {
    ///     Entry::Occupied(mut occupied) => {
    ///         assert_eq!(occupied.insert(json!(13)), 12);
    ///         assert_eq!(occupied.get(), 13);
    ///     }
    ///     Entry::Vacant(_) => unimplemented!(),
    /// }
    /// ```
    #[inline]
    pub fn insert(&mut self, value: Value) -> Value {
        self.occupied.insert(value)
    }

    /// Takes the value of the entry out of the map, and returns it.
    ///
    /// # Examples
    ///
    /// ```
    /// # use serde_json::json;
    /// #
    /// use serde_json::map::Entry;
    ///
    /// let mut map = serde_json::Map::new();
    /// map.insert("serde".to_owned(), json!(12));
    ///
    /// match map.entry("serde") {
    ///     Entry::Occupied(occupied) => {
    ///         assert_eq!(occupied.remove(), 12);
    ///     }
    ///     Entry::Vacant(_) => unimplemented!(),
    /// }
    /// ```
    #[inline]
    pub fn remove(self) -> Value {
        #[cfg(feature = "preserve_order")]
        return self.occupied.swap_remove();
        #[cfg(not(feature = "preserve_order"))]
        return self.occupied.remove();
    }
}

//////////////////////////////////////////////////////////////////////////////

impl<'a> IntoIterator for &'a Map<String, Value> {
    type Item = (&'a String, &'a Value);
    type IntoIter = Iter<'a>;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        Iter {
            iter: self.map.iter(),
        }
    }
}

/// An iterator over a serde_json::Map's entries.
pub struct Iter<'a> {
    iter: IterImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type IterImpl<'a> = btree_map::Iter<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type IterImpl<'a> = indexmap::map::Iter<'a, String, Value>;

delegate_iterator!((Iter<'a>) => (&'a String, &'a Value));

//////////////////////////////////////////////////////////////////////////////

impl<'a> IntoIterator for &'a mut Map<String, Value> {
    type Item = (&'a String, &'a mut Value);
    type IntoIter = IterMut<'a>;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        IterMut {
            iter: self.map.iter_mut(),
        }
    }
}

/// A mutable iterator over a serde_json::Map's entries.
pub struct IterMut<'a> {
    iter: IterMutImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type IterMutImpl<'a> = btree_map::IterMut<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type IterMutImpl<'a> = indexmap::map::IterMut<'a, String, Value>;

delegate_iterator!((IterMut<'a>) => (&'a String, &'a mut Value));

//////////////////////////////////////////////////////////////////////////////

impl IntoIterator for Map<String, Value> {
    type Item = (String, Value);
    type IntoIter = IntoIter;
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        IntoIter {
            iter: self.map.into_iter(),
        }
    }
}

/// An owning iterator over a serde_json::Map's entries.
pub struct IntoIter {
    iter: IntoIterImpl,
}

#[cfg(not(feature = "preserve_order"))]
type IntoIterImpl = btree_map::IntoIter<String, Value>;
#[cfg(feature = "preserve_order")]
type IntoIterImpl = indexmap::map::IntoIter<String, Value>;

delegate_iterator!((IntoIter) => (String, Value));

//////////////////////////////////////////////////////////////////////////////

/// An iterator over a serde_json::Map's keys.
pub struct Keys<'a> {
    iter: KeysImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type KeysImpl<'a> = btree_map::Keys<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type KeysImpl<'a> = indexmap::map::Keys<'a, String, Value>;

delegate_iterator!((Keys<'a>) => &'a String);

//////////////////////////////////////////////////////////////////////////////

/// An iterator over a serde_json::Map's values.
pub struct Values<'a> {
    iter: ValuesImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type ValuesImpl<'a> = btree_map::Values<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type ValuesImpl<'a> = indexmap::map::Values<'a, String, Value>;

delegate_iterator!((Values<'a>) => &'a Value);

//////////////////////////////////////////////////////////////////////////////

/// A mutable iterator over a serde_json::Map's values.
pub struct ValuesMut<'a> {
    iter: ValuesMutImpl<'a>,
}

#[cfg(not(feature = "preserve_order"))]
type ValuesMutImpl<'a> = btree_map::ValuesMut<'a, String, Value>;
#[cfg(feature = "preserve_order")]
type ValuesMutImpl<'a> = indexmap::map::ValuesMut<'a, String, Value>;

delegate_iterator!((ValuesMut<'a>) => &'a mut Value);
