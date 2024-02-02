// SPDX-FileCopyrightText: 2017 - 2021 Kamila Borowska <kamila@borowska.pw>
// SPDX-FileCopyrightText: 2021 Bruno Corrêa Zimmermann <brunoczim@gmail.com>
// SPDX-FileCopyrightText: 2021 micycle
// SPDX-FileCopyrightText: 2023 Nicolas Carranza <nicarran@yandex.com>
//
// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{enum_map, EnumArray, EnumMap};
use core::fmt::{self, Debug, Formatter};
use core::hash::{Hash, Hasher};
use core::iter::{Extend, FromIterator};
use core::ops::{Index, IndexMut};

impl<K: EnumArray<V> + Debug, V: Debug> Debug for EnumMap<K, V> {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.debug_map().entries(self).finish()
    }
}

impl<K: EnumArray<V>, V> Extend<(K, V)> for EnumMap<K, V> {
    fn extend<I: IntoIterator<Item = (K, V)>>(&mut self, iter: I) {
        for (key, value) in iter {
            self[key] = value;
        }
    }
}

impl<'a, K, V> Extend<(&'a K, &'a V)> for EnumMap<K, V>
where
    K: EnumArray<V> + Copy,
    V: Copy,
{
    fn extend<I: IntoIterator<Item = (&'a K, &'a V)>>(&mut self, iter: I) {
        self.extend(iter.into_iter().map(|(&key, &value)| (key, value)));
    }
}

impl<K, V> FromIterator<(K, V)> for EnumMap<K, V>
where
    Self: Default,
    K: EnumArray<V>,
{
    fn from_iter<I: IntoIterator<Item = (K, V)>>(iter: I) -> Self {
        let mut map = EnumMap::default();
        map.extend(iter);
        map
    }
}

impl<K: EnumArray<V>, V> Index<K> for EnumMap<K, V> {
    type Output = V;

    #[inline]
    fn index(&self, key: K) -> &V {
        &self.as_slice()[key.into_usize()]
    }
}

impl<K: EnumArray<V>, V> IndexMut<K> for EnumMap<K, V> {
    #[inline]
    fn index_mut(&mut self, key: K) -> &mut V {
        &mut self.as_mut_slice()[key.into_usize()]
    }
}

// Implementations provided by derive attribute are too specific, and put requirements on K.
// This is caused by rust-lang/rust#26925.
impl<K: EnumArray<V>, V> Clone for EnumMap<K, V>
where
    K::Array: Clone,
{
    #[inline]
    fn clone(&self) -> Self {
        EnumMap {
            array: self.array.clone(),
        }
    }
}

impl<K: EnumArray<V>, V> Copy for EnumMap<K, V> where K::Array: Copy {}

impl<K: EnumArray<V>, V: PartialEq> PartialEq for EnumMap<K, V> {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.as_slice() == other.as_slice()
    }
}

impl<K: EnumArray<V>, V: Eq> Eq for EnumMap<K, V> {}

impl<K: EnumArray<V>, V: Hash> Hash for EnumMap<K, V> {
    #[inline]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.as_slice().hash(state);
    }
}

impl<K: EnumArray<V>, V: Default> Default for EnumMap<K, V> {
    #[inline]
    fn default() -> Self {
        enum_map! { _ => V::default() }
    }
}

impl<K: EnumArray<V>, V: PartialOrd> PartialOrd for EnumMap<K, V> {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        self.as_slice().partial_cmp(other.as_slice())
    }
}

impl<K: EnumArray<V>, V: Ord> Ord for EnumMap<K, V> {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.as_slice().cmp(other.as_slice())
    }
}
