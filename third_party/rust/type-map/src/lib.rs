use std::any::{Any, TypeId};

use fxhash::FxHashMap;

use std::collections::hash_map;
use std::marker::PhantomData;

/// A view into an occupied entry in a `TypeMap`.
#[derive(Debug)]
pub struct OccupiedEntry<'a, T> {
    data: hash_map::OccupiedEntry<'a, TypeId, Box<dyn Any>>,
    marker: PhantomData<fn(T)>,
}

impl<'a, T: 'static> OccupiedEntry<'a, T> {
    /// Gets a reference to the value in the entry.
    pub fn get(&self) -> &T {
        self.data.get().downcast_ref().unwrap()
    }

    ///Gets a mutable reference to the value in the entry.
    pub fn get_mut(&mut self) -> &mut T {
        self.data.get_mut().downcast_mut().unwrap()
    }

    /// Converts the `OccupiedEntry` into a mutable reference to the value in the entry
    /// with a lifetime bound to the map itself.
    pub fn into_mut(self) -> &'a mut T {
        self.data.into_mut().downcast_mut().unwrap()
    }

    /// Sets the value of the entry, and returns the entry's old value.
    pub fn insert(&mut self, value: T) -> T {
        self.data.insert(Box::new(value)).downcast().map(|boxed| *boxed).unwrap()
    }

    /// Takes the value out of the entry, and returns it.    
    pub fn remove(self) -> T {
        self.data.remove().downcast().map(|boxed| *boxed).unwrap()
    }
}

/// A view into a vacant entry in a `TypeMap`.
#[derive(Debug)]
pub struct VacantEntry<'a, T> {
    data: hash_map::VacantEntry<'a, TypeId, Box<dyn Any>>,
    marker: PhantomData<fn(T)>,
}

impl<'a, T: 'static> VacantEntry<'a, T> {
    /// Sets the value of the entry with the key of the `VacantEntry`, and returns a mutable reference to it.
    pub fn insert(self, value: T) -> &'a mut T {
        self.data.insert(Box::new(value)).downcast_mut().unwrap()
    }
}

/// A view into a single entry in a map, which may either be vacant or occupied.
#[derive(Debug)]
pub enum Entry<'a, T> {
    Occupied(OccupiedEntry<'a, T>),
    Vacant(VacantEntry<'a, T>),
}

impl<'a, T: 'static> Entry<'a, T> {
    /// Ensures a value is in the entry by inserting the default if empty, and returns
    /// a mutable reference to the value in the entry.
    pub fn or_insert(self, default: T) -> &'a mut T {
        match self {
            Entry::Occupied(inner) => inner.into_mut(),
            Entry::Vacant(inner) => inner.insert(default),
        }
    }

    /// Ensures a value is in the entry by inserting the result of the default function if empty, and returns
    /// a mutable reference to the value in the entry.
    pub fn or_insert_with<F: FnOnce() -> T>(self, default: F) -> &'a mut T {
        match self {
            Entry::Occupied(inner) => inner.into_mut(),
            Entry::Vacant(inner) => inner.insert(default()),
        }
    }
}

#[derive(Debug, Default)]
/// The typemap container
pub struct TypeMap {
    map: Option<FxHashMap<TypeId, Box<dyn Any>>>,
}

impl TypeMap {
    /// Create an empty `TypeMap`.
    #[inline]
    pub fn new() -> Self {
        Self { map: None }
    }

    /// Insert a value into this `TypeMap`.
    ///
    /// If a value of this type already exists, it will be returned.
    pub fn insert<T: 'static>(&mut self, val: T) -> Option<T> {
        self.map
            .get_or_insert_with(|| FxHashMap::default())
            .insert(TypeId::of::<T>(), Box::new(val))
            .and_then(|boxed| boxed.downcast().ok().map(|boxed| *boxed))
    }

    /// Check if container contains value for type
    pub fn contains<T: 'static>(&self) -> bool {
        self.map.as_ref().and_then(|m| m.get(&TypeId::of::<T>())).is_some()
    }

    /// Get a reference to a value previously inserted on this `TypeMap`.
    pub fn get<T: 'static>(&self) -> Option<&T> {
        self.map
            .as_ref()
            .and_then(|m| m.get(&TypeId::of::<T>()))
            .and_then(|boxed| boxed.downcast_ref())
    }

    /// Get a mutable reference to a value previously inserted on this `TypeMap`.
    pub fn get_mut<T: 'static>(&mut self) -> Option<&mut T> {
        self.map
            .as_mut()
            .and_then(|m| m.get_mut(&TypeId::of::<T>()))
            .and_then(|boxed| boxed.downcast_mut())
    }

    /// Remove a value from this `TypeMap`.
    ///
    /// If a value of this type exists, it will be returned.
    pub fn remove<T: 'static>(&mut self) -> Option<T> {
        self.map
            .as_mut()
            .and_then(|m| m.remove(&TypeId::of::<T>()))
            .and_then(|boxed| boxed.downcast().ok().map(|boxed| *boxed))
    }

    /// Clear the `TypeMap` of all inserted values.
    #[inline]
    pub fn clear(&mut self) {
        self.map = None;
    }

    /// Get an entry in the `TypeMap` for in-place manipulation.
    pub fn entry<T: 'static>(&mut self) -> Entry<T> {
        match self.map.get_or_insert_with(|| FxHashMap::default()).entry(TypeId::of::<T>()) {
            hash_map::Entry::Occupied(e) => {
                Entry::Occupied(OccupiedEntry { data: e, marker: PhantomData })
            }
            hash_map::Entry::Vacant(e) => {
                Entry::Vacant(VacantEntry { data: e, marker: PhantomData })
            }
        }
    }
}

/// Provides the same `TypeMap` container, but with `Send` + `Sync` bounds on values
pub mod concurrent {

    use std::any::{Any, TypeId};

    use fxhash::FxHashMap;

    use std::collections::hash_map;
    use std::marker::PhantomData;

    /// A view into an occupied entry in a `TypeMap`.
    #[derive(Debug)]
    pub struct OccupiedEntry<'a, T> {
        data: hash_map::OccupiedEntry<'a, TypeId, Box<dyn Any + Send + Sync>>,
        marker: PhantomData<fn(T)>,
    }

    impl<'a, T: 'static + Send + Sync> OccupiedEntry<'a, T> {
        /// Gets a reference to the value in the entry.
        pub fn get(&self) -> &T {
            self.data.get().downcast_ref().unwrap()
        }

        ///Gets a mutable reference to the value in the entry.
        pub fn get_mut(&mut self) -> &mut T {
            self.data.get_mut().downcast_mut().unwrap()
        }

        /// Converts the `OccupiedEntry` into a mutable reference to the value in the entry
        /// with a lifetime bound to the map itself.
        pub fn into_mut(self) -> &'a mut T {
            self.data.into_mut().downcast_mut().unwrap()
        }

        /// Sets the value of the entry, and returns the entry's old value.
        pub fn insert(&mut self, value: T) -> T {
            (self.data.insert(Box::new(value)) as Box<dyn Any>)
                .downcast()
                .map(|boxed| *boxed)
                .unwrap()
        }

        /// Takes the value out of the entry, and returns it.    
        pub fn remove(self) -> T {
            (self.data.remove() as Box<dyn Any>).downcast().map(|boxed| *boxed).unwrap()
        }
    }

    /// A view into a vacant entry in a `TypeMap`.
    #[derive(Debug)]
    pub struct VacantEntry<'a, T> {
        data: hash_map::VacantEntry<'a, TypeId, Box<dyn Any + Send + Sync>>,
        marker: PhantomData<fn(T)>,
    }

    impl<'a, T: 'static + Send + Sync> VacantEntry<'a, T> {
        /// Sets the value of the entry with the key of the `VacantEntry`, and returns a mutable reference to it.
        pub fn insert(self, value: T) -> &'a mut T {
            self.data.insert(Box::new(value)).downcast_mut().unwrap()
        }
    }

    /// A view into a single entry in a map, which may either be vacant or occupied.
    #[derive(Debug)]
    pub enum Entry<'a, T> {
        Occupied(OccupiedEntry<'a, T>),
        Vacant(VacantEntry<'a, T>),
    }

    impl<'a, T: 'static + Send + Sync> Entry<'a, T> {
        /// Ensures a value is in the entry by inserting the default if empty, and returns
        /// a mutable reference to the value in the entry.
        pub fn or_insert(self, default: T) -> &'a mut T {
            match self {
                Entry::Occupied(inner) => inner.into_mut(),
                Entry::Vacant(inner) => inner.insert(default),
            }
        }

        /// Ensures a value is in the entry by inserting the result of the default function if empty, and returns
        /// a mutable reference to the value in the entry.
        pub fn or_insert_with<F: FnOnce() -> T>(self, default: F) -> &'a mut T {
            match self {
                Entry::Occupied(inner) => inner.into_mut(),
                Entry::Vacant(inner) => inner.insert(default()),
            }
        }
    }

    #[derive(Debug, Default)]
    /// The typemap container
    pub struct TypeMap {
        map: Option<FxHashMap<TypeId, Box<dyn Any + Send + Sync>>>,
    }

    impl TypeMap {
        /// Create an empty `TypeMap`.
        #[inline]
        pub fn new() -> Self {
            Self { map: None }
        }

        /// Insert a value into this `TypeMap`.
        ///
        /// If a value of this type already exists, it will be returned.
        pub fn insert<T: Send + Sync + 'static>(&mut self, val: T) -> Option<T> {
            self.map
                .get_or_insert_with(|| FxHashMap::default())
                .insert(TypeId::of::<T>(), Box::new(val))
                .and_then(|boxed| (boxed as Box<dyn Any>).downcast().ok().map(|boxed| *boxed))
        }

        /// Check if container contains value for type
        pub fn contains<T: 'static>(&self) -> bool {
            self.map.as_ref().and_then(|m| m.get(&TypeId::of::<T>())).is_some()
        }

        /// Get a reference to a value previously inserted on this `TypeMap`.
        pub fn get<T: 'static>(&self) -> Option<&T> {
            self.map
                .as_ref()
                .and_then(|m| m.get(&TypeId::of::<T>()))
                .and_then(|boxed| boxed.downcast_ref())
        }

        /// Get a mutable reference to a value previously inserted on this `TypeMap`.
        pub fn get_mut<T: 'static>(&mut self) -> Option<&mut T> {
            self.map
                .as_mut()
                .and_then(|m| m.get_mut(&TypeId::of::<T>()))
                .and_then(|boxed| boxed.downcast_mut())
        }

        /// Remove a value from this `TypeMap`.
        ///
        /// If a value of this type exists, it will be returned.
        pub fn remove<T: 'static>(&mut self) -> Option<T> {
            self.map
                .as_mut()
                .and_then(|m| m.remove(&TypeId::of::<T>()))
                .and_then(|boxed| (boxed as Box<dyn Any>).downcast().ok().map(|boxed| *boxed))
        }

        /// Clear the `TypeMap` of all inserted values.
        #[inline]
        pub fn clear(&mut self) {
            self.map = None;
        }

        /// Get an entry in the `TypeMap` for in-place manipulation.
        pub fn entry<T: 'static + Send + Sync>(&mut self) -> Entry<T> {
            match self.map.get_or_insert_with(|| FxHashMap::default()).entry(TypeId::of::<T>()) {
                hash_map::Entry::Occupied(e) => {
                    Entry::Occupied(OccupiedEntry { data: e, marker: PhantomData })
                }
                hash_map::Entry::Vacant(e) => {
                    Entry::Vacant(VacantEntry { data: e, marker: PhantomData })
                }
            }
        }
    }
}

#[test]
fn test_type_map() {
    #[derive(Debug, PartialEq)]
    struct MyType(i32);

    #[derive(Debug, PartialEq, Default)]
    struct MyType2(String);

    let mut map = TypeMap::new();

    map.insert(5i32);
    map.insert(MyType(10));

    assert_eq!(map.get(), Some(&5i32));
    assert_eq!(map.get_mut(), Some(&mut 5i32));

    assert_eq!(map.remove::<i32>(), Some(5i32));
    assert!(map.get::<i32>().is_none());

    assert_eq!(map.get::<bool>(), None);
    assert_eq!(map.get(), Some(&MyType(10)));

    let entry = map.entry::<MyType2>();

    let mut v = entry.or_insert_with(MyType2::default);

    v.0 = "Hello".into();

    assert_eq!(map.get(), Some(&MyType2("Hello".into())));
}
