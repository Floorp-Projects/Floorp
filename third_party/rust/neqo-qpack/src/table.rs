// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::static_table::{StaticTableEntry, HEADER_STATIC_TABLE};
use crate::{Error, Res};
use neqo_common::qtrace;
use std::collections::VecDeque;
use std::convert::TryFrom;

pub const ADDITIONAL_TABLE_ENTRY_SIZE: usize = 32;

pub struct LookupResult {
    pub index: u64,
    pub static_table: bool,
    pub value_matches: bool,
}

#[derive(Debug)]
pub(crate) struct DynamicTableEntry {
    base: u64,
    name: Vec<u8>,
    value: Vec<u8>,
    /// Number of header blocks that refer this entry.
    /// This is only used by the encoder.
    refs: u64,
}

impl DynamicTableEntry {
    pub fn can_reduce(&self, first_not_acked: u64) -> bool {
        self.refs == 0 && self.base < first_not_acked
    }

    pub fn size(&self) -> usize {
        self.name.len() + self.value.len() + ADDITIONAL_TABLE_ENTRY_SIZE
    }

    pub fn add_ref(&mut self) {
        self.refs += 1;
    }

    pub fn remove_ref(&mut self) {
        assert!(self.refs > 0);
        self.refs -= 1;
    }

    pub fn name(&self) -> &[u8] {
        &self.name
    }

    pub fn value(&self) -> &[u8] {
        &self.value
    }

    pub fn index(&self) -> u64 {
        self.base
    }
}

#[derive(Debug)]
pub(crate) struct HeaderTable {
    dynamic: VecDeque<DynamicTableEntry>,
    /// The total capacity (in QPACK bytes) of the table. This is set by
    /// configuration.
    capacity: u64,
    /// The amount of used capacity.
    used: u64,
    /// The total number of inserts thus far.
    base: u64,
    /// This is number of inserts that are acked. this correspond to index of the first not acked.
    /// This is only used by thee encoder.
    acked_inserts_cnt: u64,
}

impl ::std::fmt::Display for HeaderTable {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(
            f,
            "HeaderTable for (base={} acked_inserts_cnt={} capacity={})",
            self.base, self.acked_inserts_cnt, self.capacity
        )
    }
}

impl HeaderTable {
    pub fn new(encoder: bool) -> Self {
        Self {
            dynamic: VecDeque::new(),
            capacity: 0,
            used: 0,
            base: 0,
            acked_inserts_cnt: if encoder { 0 } else { u64::max_value() },
        }
    }

    /// Returns number of inserts.
    pub fn base(&self) -> u64 {
        self.base
    }

    /// Returns capacity of the dynamic table
    pub fn capacity(&self) -> u64 {
        self.capacity
    }

    /// Change the dynamic table capacity.
    /// ### Errors
    /// `ChangeCapacity` if table capacity cannot be reduced.
    /// The table cannot be reduce if there are entries that are referred at the moment or their inserts are unacked.
    pub fn set_capacity(&mut self, cap: u64) -> Res<()> {
        qtrace!([self], "set capacity to {}", cap);
        if !self.evict_to(cap) {
            return Err(Error::ChangeCapacity);
        }
        self.capacity = cap;
        Ok(())
    }

    /// Get a static entry with `index`.
    /// ### Errors
    /// `HeaderLookup` if the index does not exist in the static table.
    pub fn get_static(index: u64) -> Res<&'static StaticTableEntry> {
        let inx = usize::try_from(index).or(Err(Error::HeaderLookup))?;
        if inx > HEADER_STATIC_TABLE.len() {
            return Err(Error::HeaderLookup);
        }
        Ok(&HEADER_STATIC_TABLE[inx])
    }

    fn get_dynamic_with_abs_index(&mut self, index: u64) -> Res<&mut DynamicTableEntry> {
        if self.base <= index {
            debug_assert!(false, "This is an internal error");
            return Err(Error::HeaderLookup);
        }
        let inx = self.base - index - 1;
        let inx = usize::try_from(inx).or(Err(Error::HeaderLookup))?;
        if inx >= self.dynamic.len() {
            return Err(Error::HeaderLookup);
        }
        Ok(&mut self.dynamic[inx])
    }

    fn get_dynamic_with_relative_index(&self, index: u64) -> Res<&DynamicTableEntry> {
        let inx = usize::try_from(index).or(Err(Error::HeaderLookup))?;
        if inx >= self.dynamic.len() {
            return Err(Error::HeaderLookup);
        }
        Ok(&self.dynamic[inx])
    }

    /// Get a entry in the  dynamic table.
    /// ### Errors
    /// `HeaderLookup` if entry does not exist.
    pub fn get_dynamic(&self, index: u64, base: u64, post: bool) -> Res<&DynamicTableEntry> {
        let inx = if post {
            if self.base < (base + index + 1) {
                return Err(Error::HeaderLookup);
            }
            self.base - (base + index + 1)
        } else {
            if (self.base + index) < base {
                return Err(Error::HeaderLookup);
            }
            (self.base + index) - base
        };

        self.get_dynamic_with_relative_index(inx)
    }

    /// Remove a reference to a dynamic table entry.
    pub fn remove_ref(&mut self, index: u64) {
        qtrace!([self], "remove reference to entry {}", index);
        self.get_dynamic_with_abs_index(index)
            .expect("we should have the entry")
            .remove_ref();
    }

    /// Add a reference to a dynamic table entry.
    pub fn add_ref(&mut self, index: u64) {
        qtrace!([self], "add reference to entry {}", index);
        self.get_dynamic_with_abs_index(index)
            .expect("we should have the entry")
            .add_ref();
    }

    /// Look for a header pair.
    /// The function returns `LookupResult`: `index`, `static_table` (if it is a static table entry) and `value_matches`
    /// (if the header value matches as well not only header name)
    pub fn lookup(&mut self, name: &[u8], value: &[u8], can_block: bool) -> Option<LookupResult> {
        qtrace!(
            [self],
            "lookup name:{:?} value {:?} can_block={}",
            name,
            value,
            can_block
        );
        let mut name_match = None;
        for iter in HEADER_STATIC_TABLE {
            if iter.name() == name {
                if iter.value() == value {
                    return Some(LookupResult {
                        index: iter.index(),
                        static_table: true,
                        value_matches: true,
                    });
                }

                if name_match.is_none() {
                    name_match = Some(LookupResult {
                        index: iter.index(),
                        static_table: true,
                        value_matches: false,
                    });
                }
            }
        }

        for iter in &mut self.dynamic {
            if !can_block && iter.index() >= self.acked_inserts_cnt {
                continue;
            }
            if iter.name == name {
                if iter.value == value {
                    return Some(LookupResult {
                        index: iter.index(),
                        static_table: false,
                        value_matches: true,
                    });
                }

                if name_match.is_none() {
                    name_match = Some(LookupResult {
                        index: iter.index(),
                        static_table: false,
                        value_matches: false,
                    });
                }
            }
        }
        name_match
    }

    fn evict_to(&mut self, reduce: u64) -> bool {
        self.evict_to_internal(reduce, false)
    }

    pub fn can_evict_to(&mut self, reduce: u64) -> bool {
        self.evict_to_internal(reduce, true)
    }

    pub fn evict_to_internal(&mut self, reduce: u64, only_check: bool) -> bool {
        qtrace!(
            [self],
            "reduce table to {}, currently used:{} only_check:{}",
            reduce,
            self.used,
            only_check
        );
        let mut used = self.used;
        while (!self.dynamic.is_empty()) && used > reduce {
            if let Some(e) = self.dynamic.back() {
                if !e.can_reduce(self.acked_inserts_cnt) {
                    return false;
                }
                used -= u64::try_from(e.size()).unwrap();
                if !only_check {
                    self.used -= u64::try_from(e.size()).unwrap();
                    self.dynamic.pop_back();
                }
            }
        }
        true
    }

    pub fn insert_possible(&mut self, size: usize) -> bool {
        u64::try_from(size).unwrap() <= self.capacity
            && self.can_evict_to(self.capacity - u64::try_from(size).unwrap())
    }

    /// Insert a new entry.
    /// ### Errors
    /// `DynamicTableFull` if an entry cannot be added to the table because there is not enough space and/or
    /// other entry cannot be evicted.
    pub fn insert(&mut self, name: &[u8], value: &[u8]) -> Res<u64> {
        qtrace!([self], "insert name={:?} value={:?}", name, value);
        let entry = DynamicTableEntry {
            name: name.to_vec(),
            value: value.to_vec(),
            base: self.base,
            refs: 0,
        };
        if u64::try_from(entry.size()).unwrap() > self.capacity
            || !self.evict_to(self.capacity - u64::try_from(entry.size()).unwrap())
        {
            return Err(Error::DynamicTableFull);
        }
        self.base += 1;
        self.used += u64::try_from(entry.size()).unwrap();
        let index = entry.index();
        self.dynamic.push_front(entry);
        Ok(index)
    }

    /// Insert a new entry with the name refer to by a index to static or dynamic table.
    /// ### Errors
    /// `DynamicTableFull` if an entry cannot be added to the table because there is not enough space and/or
    /// other entry cannot be evicted.
    /// `HeaderLookup` if the index dos not exits in the static/dynamic table.
    pub fn insert_with_name_ref(
        &mut self,
        name_static_table: bool,
        name_index: u64,
        value: &[u8],
    ) -> Res<u64> {
        qtrace!(
            [self],
            "insert with ref to index={} in {} value={:?}",
            name_index,
            if name_static_table {
                "static"
            } else {
                "dynamic"
            },
            value
        );
        let name = if name_static_table {
            HeaderTable::get_static(name_index)?.name().to_vec()
        } else {
            self.get_dynamic(name_index, self.base, false)?
                .name()
                .to_vec()
        };
        self.insert(&name, value)
    }

    /// Duplicate an entry.
    /// ### Errors
    /// `DynamicTableFull` if an entry cannot be added to the table because there is not enough space and/or
    /// other entry cannot be evicted.
    /// `HeaderLookup` if the index dos not exits in the static/dynamic table.
    pub fn duplicate(&mut self, index: u64) -> Res<u64> {
        qtrace!([self], "duplicate entry={}", index);
        // need to remember name and value because insert may delete the entry.
        let name: Vec<u8>;
        let value: Vec<u8>;
        {
            let entry = self.get_dynamic(index, self.base, false)?;
            name = entry.name().to_vec();
            value = entry.value().to_vec();
            qtrace!([self], "dumplicate name={:?} value={:?}", name, value);
        }
        self.insert(&name, &value)
    }

    /// Increment number of acknowledge entries.
    /// ### Errors
    /// `IncrementAck` if ack is greater than actual number of inserts.
    pub fn increment_acked(&mut self, increment: u64) -> Res<()> {
        qtrace!([self], "increment acked by {}", increment);
        self.acked_inserts_cnt += increment;
        if self.base < self.acked_inserts_cnt {
            return Err(Error::IncrementAck);
        }
        Ok(())
    }

    /// Return number of acknowledge inserts.
    pub fn get_acked_inserts_cnt(&self) -> u64 {
        self.acked_inserts_cnt
    }
}
