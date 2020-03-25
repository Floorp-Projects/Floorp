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
    BinaryReader, Result, SectionIteratorLimited, SectionReader, SectionWithLimitedItems, TableType,
};

pub struct TableSectionReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> TableSectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<TableSectionReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(TableSectionReader { reader, count })
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    /// Reads content of the table section.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x04, 0x05, 0x01, 0x70, 0x01, 0x01, 0x01,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::ModuleReader;
    /// let mut reader = ModuleReader::new(data).expect("module reader");
    /// let section = reader.read().expect("type section");
    /// let section = reader.read().expect("function section");
    /// let section = reader.read().expect("table section");
    /// let mut table_reader = section.get_table_section_reader().expect("table section reader");
    /// for _ in 0..table_reader.get_count() {
    ///     let table = table_reader.read().expect("table");
    ///     println!("Table: {:?}", table);
    /// }
    /// ```
    pub fn read(&mut self) -> Result<TableType> {
        self.reader.read_table_type()
    }
}

impl<'a> SectionReader for TableSectionReader<'a> {
    type Item = TableType;
    fn read(&mut self) -> Result<Self::Item> {
        TableSectionReader::read(self)
    }
    fn eof(&self) -> bool {
        self.reader.eof()
    }
    fn original_position(&self) -> usize {
        TableSectionReader::original_position(self)
    }
}

impl<'a> SectionWithLimitedItems for TableSectionReader<'a> {
    fn get_count(&self) -> u32 {
        TableSectionReader::get_count(self)
    }
}

impl<'a> IntoIterator for TableSectionReader<'a> {
    type Item = Result<TableType>;
    type IntoIter = SectionIteratorLimited<TableSectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIteratorLimited::new(self)
    }
}
