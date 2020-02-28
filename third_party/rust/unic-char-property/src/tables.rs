// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Character data tables used in UNIC.

use unic_char_range::CharRange;

/// A mapping from characters to some associated data.
///
/// For the set case, use `()` as the associated value.
#[derive(Copy, Clone, Debug)]
pub enum CharDataTable<V: 'static> {
    #[doc(hidden)]
    Direct(&'static [(char, V)]),
    #[doc(hidden)]
    Range(&'static [(CharRange, V)]),
}

impl<V> Default for CharDataTable<V> {
    fn default() -> Self {
        CharDataTable::Direct(&[])
    }
}

impl<V> CharDataTable<V> {
    /// Does this table contain a mapping for a character?
    pub fn contains(&self, needle: char) -> bool {
        match *self {
            CharDataTable::Direct(table) => {
                table.binary_search_by_key(&needle, |&(k, _)| k).is_ok()
            }
            CharDataTable::Range(table) => table
                .binary_search_by(|&(range, _)| range.cmp_char(needle))
                .is_ok(),
        }
    }
}

impl<V: Copy> CharDataTable<V> {
    /// Find the associated data for a character in this table.
    pub fn find(&self, needle: char) -> Option<V> {
        match *self {
            CharDataTable::Direct(table) => table
                .binary_search_by_key(&needle, |&(k, _)| k)
                .map(|idx| table[idx].1)
                .ok(),
            CharDataTable::Range(table) => table
                .binary_search_by(|&(range, _)| range.cmp_char(needle))
                .map(|idx| table[idx].1)
                .ok(),
        }
    }

    /// Find the range and the associated data for a character in the range table.
    pub fn find_with_range(&self, needle: char) -> Option<(CharRange, V)> {
        match *self {
            CharDataTable::Direct(_) => None,
            CharDataTable::Range(table) => table
                .binary_search_by(|&(range, _)| range.cmp_char(needle))
                .map(|idx| table[idx])
                .ok(),
        }
    }
}

impl<V: Copy + Default> CharDataTable<V> {
    /// Find the associated data for a character in this table, or the default value if not entered.
    pub fn find_or_default(&self, needle: char) -> V {
        self.find(needle).unwrap_or_else(Default::default)
    }
}

/// Iterator for `CharDataTable`. Iterates over pairs `(CharRange, V)`.
#[derive(Debug)]
pub struct CharDataTableIter<'a, V: 'static>(&'a CharDataTable<V>, usize);

impl<'a, V: Copy> Iterator for CharDataTableIter<'a, V> {
    type Item = (CharRange, V);

    fn next(&mut self) -> Option<Self::Item> {
        match *self.0 {
            CharDataTable::Direct(arr) => {
                if self.1 >= arr.len() {
                    None
                } else {
                    let idx = self.1;
                    self.1 += 1;
                    let (ch, v) = arr[idx];
                    Some((chars!(ch..=ch), v))
                }
            }
            CharDataTable::Range(arr) => {
                if self.1 >= arr.len() {
                    None
                } else {
                    let idx = self.1;
                    self.1 += 1;
                    Some(arr[idx])
                }
            }
        }
    }
}

impl<V> CharDataTable<V> {
    /// Iterate over the entries in this table. Yields pairs `(CharRange, V)`.
    pub fn iter(&self) -> CharDataTableIter<'_, V> {
        CharDataTableIter(self, 0)
    }
}
