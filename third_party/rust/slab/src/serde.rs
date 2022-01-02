extern crate serde;

use core::fmt;
use core::marker::PhantomData;

use self::serde::de::{Deserialize, Deserializer, MapAccess, Visitor};
use self::serde::ser::{Serialize, SerializeMap, Serializer};

use super::{Entry, Slab};

impl<T> Serialize for Slab<T>
where
    T: Serialize,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map_serializer = serializer.serialize_map(Some(self.len()))?;
        for (key, value) in self {
            map_serializer.serialize_key(&key)?;
            map_serializer.serialize_value(value)?;
        }
        map_serializer.end()
    }
}

struct SlabVisitor<T>(PhantomData<T>);

impl<'de, T> Visitor<'de> for SlabVisitor<T>
where
    T: Deserialize<'de>,
{
    type Value = Slab<T>;

    fn expecting(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "a map")
    }

    fn visit_map<A>(self, mut map: A) -> Result<Self::Value, A::Error>
    where
        A: MapAccess<'de>,
    {
        let mut slab = Slab::with_capacity(map.size_hint().unwrap_or(0));

        // same as FromIterator impl
        let mut vacant_list_broken = false;
        while let Some((key, value)) = map.next_entry()? {
            if key < slab.entries.len() {
                // iterator is not sorted, might need to recreate vacant list
                if let Entry::Vacant(_) = slab.entries[key] {
                    vacant_list_broken = true;
                    slab.len += 1;
                }
                // if an element with this key already exists, replace it.
                // This is consisent with HashMap and BtreeMap
                slab.entries[key] = Entry::Occupied(value);
            } else {
                // insert holes as necessary
                while slab.entries.len() < key {
                    // add the entry to the start of the vacant list
                    let next = slab.next;
                    slab.next = slab.entries.len();
                    slab.entries.push(Entry::Vacant(next));
                }
                slab.entries.push(Entry::Occupied(value));
                slab.len += 1;
            }
        }
        if slab.len == slab.entries.len() {
            // no vacant enries, so next might not have been updated
            slab.next = slab.entries.len();
        } else if vacant_list_broken {
            slab.recreate_vacant_list();
        }

        Ok(slab)
    }
}

impl<'de, T> Deserialize<'de> for Slab<T>
where
    T: Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_map(SlabVisitor(PhantomData))
    }
}
