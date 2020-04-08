// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::static_table::{StaticTableEntry, HEADER_STATIC_TABLE};
use crate::{Error, QPackSide, Res};
use neqo_common::qtrace;
use std::collections::VecDeque;
use std::convert::TryFrom;

pub struct LookupResult {
    pub index: u64,
    pub static_table: bool,
    pub value_matches: bool,
}

#[derive(Debug)]
pub struct DynamicTableEntry {
    base: u64,
    name: Vec<u8>,
    value: Vec<u8>,
    /// Number of streams that refer this entry.
    refs: u64,
}

impl DynamicTableEntry {
    pub fn can_reduce(&self, first_not_acked: u64) -> bool {
        self.refs == 0 && self.base < first_not_acked
    }

    pub fn size(&self) -> u64 {
        (self.name.len() + self.value.len() + 32) as u64
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
pub struct HeaderTable {
    qpack_side: QPackSide,
    dynamic: VecDeque<DynamicTableEntry>,
    // The total capacity (in QPACK bytes) of the table. This is set by
    // configuration.
    capacity: u64,
    // The amount of used capacity.
    used: u64,
    // The total number of inserts thus far.
    base: u64,
    // This is number of inserts that are acked. this correspond to index of the first not acked.
    acked_inserts_cnt: u64,
}

impl ::std::fmt::Display for HeaderTable {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(
            f,
            "HeaderTable for {} (base={} acked_inserts_cnt={} capacity={})",
            self.qpack_side, self.base, self.acked_inserts_cnt, self.capacity
        )
    }
}

impl HeaderTable {
    pub fn new(encoder: bool) -> Self {
        Self {
            qpack_side: if encoder {
                QPackSide::Encoder
            } else {
                QPackSide::Decoder
            },
            dynamic: VecDeque::new(),
            capacity: 0,
            used: 0,
            base: 0,
            acked_inserts_cnt: 0,
        }
    }

    pub fn base(&self) -> u64 {
        self.base
    }

    pub fn capacity(&self) -> u64 {
        self.capacity
    }

    pub fn set_capacity(&mut self, cap: u64) -> Res<()> {
        qtrace!([self], "set capacity to {}", cap);
        if !self.evict_to(cap) {
            return Err(Error::Internal);
        }
        self.capacity = cap;
        Ok(())
    }

    pub fn get_static(index: u64) -> Res<&'static StaticTableEntry> {
        let inx = usize::try_from(index).or(Err(Error::HeaderLookup))?;
        if inx > HEADER_STATIC_TABLE.len() {
            return Err(Error::HeaderLookup);
        }
        Ok(&HEADER_STATIC_TABLE[inx])
    }

    fn get_dynamic_with_abs_index(&mut self, index: u64) -> Res<&mut DynamicTableEntry> {
        if self.base <= index {
            debug_assert!(false, "This is an iternal error");
            return Err(Error::Internal);
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

    pub fn get_dynamic(&self, index: u64, base: u64, post: bool) -> Res<&DynamicTableEntry> {
        if self.base < base {
            return Err(Error::HeaderLookup);
        }
        let inx: u64;
        let base_rel = self.base - base;
        if post {
            if base_rel <= index {
                return Err(Error::HeaderLookup);
            }
            inx = base_rel - index - 1;
        } else {
            inx = base_rel + index;
        }

        self.get_dynamic_with_relative_index(inx)
    }

    pub fn remove_ref(&mut self, index: u64) {
        qtrace!([self], "remove reference to entry {}", index);
        self.get_dynamic_with_abs_index(index)
            .expect("we should have the entry")
            .remove_ref();
    }

    pub fn add_ref(&mut self, index: u64) {
        qtrace!([self], "add reference to entry {}", index);
        self.get_dynamic_with_abs_index(index)
            .expect("we should have the entry")
            .add_ref();
    }

    pub fn lookup(&mut self, name: &[u8], value: &[u8], can_block: bool) -> Option<LookupResult> {
        qtrace!(
            [self],
            "lookup name:{:?} value {:?} can_block={}",
            name,
            value,
            can_block
        );
        let mut name_match = None;
        for iter in HEADER_STATIC_TABLE.iter() {
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

    pub fn evict_to(&mut self, reduce: u64) -> bool {
        qtrace!(
            [self],
            "reduce table to {}, currently used:{}",
            reduce,
            self.used
        );
        while (!self.dynamic.is_empty()) && self.used > reduce {
            if let Some(e) = self.dynamic.back() {
                if let QPackSide::Encoder = self.qpack_side {
                    if !e.can_reduce(self.acked_inserts_cnt) {
                        return false;
                    }
                }
                self.used -= e.size();
                self.dynamic.pop_back();
            }
        }
        true
    }

    pub fn insert(&mut self, name: &[u8], value: &[u8]) -> Res<u64> {
        qtrace!([self], "insert name={:?} value={:?}", name, value);
        let entry = DynamicTableEntry {
            name: name.to_vec(),
            value: value.to_vec(),
            base: self.base,
            refs: 0,
        };
        if entry.size() > self.capacity || !self.evict_to(self.capacity - entry.size()) {
            match self.qpack_side {
                QPackSide::Encoder => return Err(Error::EncoderStream),
                QPackSide::Decoder => return Err(Error::DecoderStream),
            }
        }
        self.base += 1;
        self.used += entry.size();
        let index = entry.index();
        self.dynamic.push_front(entry);
        Ok(index)
    }

    pub fn insert_with_name_ref(
        &mut self,
        name_static_table: bool,
        name_index: u64,
        value: &[u8],
    ) -> Res<()> {
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
        self.insert(&name, value)?;
        Ok(())
    }

    pub fn duplicate(&mut self, index: u64) -> Res<()> {
        qtrace!([self], "dumplicate entry={}", index);
        // need to remember name and value because insert may delete the entry.
        let name: Vec<u8>;
        let value: Vec<u8>;
        {
            let entry = self.get_dynamic(index, self.base, false)?;
            name = entry.name().to_vec();
            value = entry.value().to_vec();
            qtrace!([self], "dumplicate name={:?} value={:?}", name, value);
        }
        self.insert(&name, &value)?;
        Ok(())
    }

    pub fn increment_acked(&mut self, increment: u64) -> Res<()> {
        qtrace!([self], "increment acked by {}", increment);
        self.acked_inserts_cnt += increment;
        if self.base < self.acked_inserts_cnt {
            return Err(Error::Internal);
        }
        Ok(())
    }

    pub fn get_acked_inserts_cnt(&self) -> u64 {
        self.acked_inserts_cnt
    }
}
