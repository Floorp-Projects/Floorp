// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::static_table::{StaticTableEntry, HEADER_STATIC_TABLE};
use crate::{Error, QPackSide, Res};
use std::collections::{HashMap, VecDeque};

#[derive(Debug)]
pub struct DynamicTableEntry {
    base: u64,
    name: Vec<u8>,
    value: Vec<u8>,
    refs: HashMap<u64, u8>, //TODO multiple header. value will be used for that: or of flags 0x1 for headers, ox2 for trailes.
}

impl DynamicTableEntry {
    pub fn can_reduce(&self, first_not_acked: u64) -> bool {
        self.refs.is_empty() && self.base < first_not_acked
    }

    pub fn size(&self) -> u64 {
        (self.name.len() + self.value.len() + 32) as u64
    }

    pub fn add_ref(&mut self, stream_id: u64, _block: u8) {
        self.refs.insert(stream_id, 1);
    }

    pub fn remove_ref(&mut self, stream_id: u64, _block: u8) {
        self.refs.remove(&stream_id);
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
    // The total capacity (in HPACK bytes) of the table. This is set by
    // configuration.
    capacity: u64,
    // The amount of used capacity.
    used: u64,
    // The total number of inserts thus far.
    base: u64,
    acked_inserts_cnt: u64,
}

impl HeaderTable {
    pub fn new(encoder: bool) -> HeaderTable {
        HeaderTable {
            qpack_side: if encoder {
                QPackSide::Encoder
            } else {
                QPackSide::Decoder
            },
            dynamic: VecDeque::new(),
            capacity: 0,
            used: 0,
            base: 0,
            acked_inserts_cnt: if encoder { 0 } else { std::u64::MAX },
        }
    }

    pub fn base(&self) -> u64 {
        self.base
    }

    pub fn capacity(&self) -> u64 {
        self.capacity
    }

    pub fn set_capacity(&mut self, c: u64) {
        self.evict_to(c);
        self.capacity = c;
    }

    pub fn get_static(&self, index: u64) -> Res<&StaticTableEntry> {
        if index > HEADER_STATIC_TABLE.len() as u64 {
            return Err(Error::HeaderLookupError);
        }
        let res = &HEADER_STATIC_TABLE[index as usize];
        Ok(res)
    }

    pub fn get_dynamic(&self, i: u64, base: u64, post: bool) -> Res<&DynamicTableEntry> {
        if self.base < base {
            return Err(Error::HeaderLookupError);
        }
        let inx: u64;
        let base_rel = self.base - base;
        if post {
            if base_rel <= i {
                return Err(Error::HeaderLookupError);
            }
            inx = base_rel - i - 1;
        } else {
            inx = base_rel + i;
        }
        if inx as usize >= self.dynamic.len() {
            return Err(Error::HeaderLookupError);
        }
        let res = &self.dynamic[inx as usize];
        Ok(res)
    }

    pub fn get_last_added_entry(&mut self) -> Option<&mut DynamicTableEntry> {
        self.dynamic.front_mut()
    }

    // separate lookups because static entries can not be return mut and we need dynamic entries mutable.
    pub fn lookup(
        &mut self,
        name: &[u8],
        value: &[u8],
    ) -> (
        Option<&StaticTableEntry>,
        Option<&mut DynamicTableEntry>,
        bool,
    ) {
        let mut name_match_s: Option<&StaticTableEntry> = None;
        for iter in HEADER_STATIC_TABLE.iter() {
            if iter.name() == name {
                if iter.value() == value {
                    return (Some(iter), None, true);
                }

                if name_match_s.is_none() {
                    name_match_s = Some(iter);
                }
            }
        }

        let mut name_match_d: Option<&mut DynamicTableEntry> = None;
        for iter in self.dynamic.iter_mut() {
            if iter.name == name {
                if iter.value == value {
                    return (None, Some(iter), true);
                }

                if name_match_s.is_none() && name_match_d.is_none() {
                    name_match_d = Some(iter);
                }
            }
        }

        (name_match_s, name_match_d, false)
    }

    pub fn evict_to(&mut self, reduce: u64) -> bool {
        while (!self.dynamic.is_empty()) && self.used > reduce {
            if let Some(e) = self.dynamic.back() {
                if !e.can_reduce(self.acked_inserts_cnt) {
                    return false;
                }
                self.used -= e.size();
                self.dynamic.pop_back();
            }
        }
        true
    }

    pub fn insert(&mut self, name: Vec<u8>, value: Vec<u8>) -> Res<()> {
        let entry = DynamicTableEntry {
            name,
            value,
            base: self.base,
            refs: HashMap::new(),
        };

        if entry.size() > self.capacity || !self.evict_to(self.capacity - entry.size()) {
            match self.qpack_side {
                QPackSide::Encoder => return Err(Error::EncoderStreamError),
                QPackSide::Decoder => return Err(Error::DecoderStreamError),
            }
        }
        self.base += 1;
        self.used += entry.size();
        self.dynamic.push_front(entry);
        Ok(())
    }

    pub fn insert_with_name_ref(
        &mut self,
        name_static_table: bool,
        name_index: u64,
        value: Vec<u8>,
    ) -> Res<()> {
        let name;
        if name_static_table {
            let entry = self.get_static(name_index)?;
            name = entry.name().to_vec()
        } else {
            let entry = self.get_dynamic(name_index, self.base, false)?;
            name = entry.name().to_vec();
        }
        self.insert(name, value)
    }

    pub fn duplicate(&mut self, index: u64) -> Res<()> {
        // need to remember name and value because inser may delete the entry.
        let name: Vec<u8>;
        let value: Vec<u8>;
        {
            let entry = self.get_dynamic(index, self.base, false)?;
            name = entry.name().to_vec();
            value = entry.value().to_vec();
        }
        self.insert(name, value)?;
        Ok(())
    }

    pub fn increment_acked(&mut self, increment: u64) {
        self.acked_inserts_cnt += increment;
    }

    pub fn get_acked_inserts_cnt(&self) -> u64 {
        self.acked_inserts_cnt
    }

    pub fn header_ack(&mut self, stream_id: u64) {
        for iter in self.dynamic.iter_mut() {
            iter.remove_ref(stream_id, 1);
        }
    }
}
