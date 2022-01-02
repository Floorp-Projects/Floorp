/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;
use thin_vec::ThinVec;

#[repr(C)]
pub struct CompactSymbolTable {
    pub addr: ThinVec<u32>,
    pub index: ThinVec<u32>,
    pub buffer: ThinVec<u8>,
}

impl CompactSymbolTable {
    pub fn new() -> Self {
        Self {
            addr: ThinVec::new(),
            index: ThinVec::new(),
            buffer: ThinVec::new(),
        }
    }

    pub fn from_map(map: HashMap<u32, &str>) -> Self {
        let mut table = Self::new();
        let mut entries: Vec<_> = map.into_iter().collect();
        entries.sort_by_key(|&(addr, _)| addr);
        for (addr, name) in entries {
            table.addr.push(addr);
            table.index.push(table.buffer.len() as u32);
            table.add_name(name);
        }
        table.index.push(table.buffer.len() as u32);
        table
    }

    fn add_name(&mut self, name: &str) {
        self.buffer.extend_from_slice(name.as_bytes());
    }
}
