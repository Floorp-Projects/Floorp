/* Copyright 2018 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use super::{
    BinaryReader, MemoryType, Result, SectionIteratorLimited, SectionReader,
    SectionWithLimitedItems,
};

pub struct MemorySectionReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> MemorySectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<MemorySectionReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(MemorySectionReader { reader, count })
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    /// Reads content of the memory section.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x05, 0x03, 0x01, 0x00, 0x02,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::ModuleReader;
    /// let mut reader = ModuleReader::new(data).expect("module reader");
    /// let section = reader.read().expect("type section");
    /// let section = reader.read().expect("function section");
    /// let section = reader.read().expect("memory section");
    /// let mut memory_reader = section.get_memory_section_reader().expect("memory section reader");
    /// for _ in 0..memory_reader.get_count() {
    ///     let memory = memory_reader.read().expect("memory");
    ///     println!("Memory: {:?}", memory);
    /// }
    /// ```
    pub fn read(&mut self) -> Result<MemoryType> {
        self.reader.read_memory_type()
    }
}

impl<'a> SectionReader for MemorySectionReader<'a> {
    type Item = MemoryType;
    fn read(&mut self) -> Result<Self::Item> {
        MemorySectionReader::read(self)
    }
    fn eof(&self) -> bool {
        self.reader.eof()
    }
    fn original_position(&self) -> usize {
        MemorySectionReader::original_position(self)
    }
}

impl<'a> SectionWithLimitedItems for MemorySectionReader<'a> {
    fn get_count(&self) -> u32 {
        MemorySectionReader::get_count(self)
    }
}

impl<'a> IntoIterator for MemorySectionReader<'a> {
    type Item = Result<MemoryType>;
    type IntoIter = SectionIteratorLimited<MemorySectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIteratorLimited::new(self)
    }
}
