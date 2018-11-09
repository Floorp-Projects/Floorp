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
    BinaryReader, ExternalKind, ImportSectionEntryType, Result, SectionIteratorLimited,
    SectionReader, SectionWithLimitedItems,
};

#[derive(Debug, Copy, Clone)]
pub struct Import<'a> {
    pub module: &'a [u8],
    pub field: &'a [u8],
    pub ty: ImportSectionEntryType,
}

pub struct ImportSectionReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> ImportSectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<ImportSectionReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(ImportSectionReader { reader, count })
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    /// Reads content of the import section.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00,
    /// #     0x02, 0x07, 0x01, 0x01, 0x41, 0x01, 0x66, 0x00, 0x00,
    /// #     0x03, 0x02, 0x01, 0x00, 0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::ModuleReader;
    /// let mut reader = ModuleReader::new(data).expect("module reader");
    /// let section = reader.read().expect("type section");
    /// let section = reader.read().expect("import section");
    /// let mut import_reader = section.get_import_section_reader().expect("import section reader");
    /// for _ in 0..import_reader.get_count() {
    ///     let import = import_reader.read().expect("import");
    ///     println!("Import: {:?}", import);
    /// }
    /// ```
    pub fn read<'b>(&mut self) -> Result<Import<'b>>
    where
        'a: 'b,
    {
        let module = self.reader.read_string()?;
        let field = self.reader.read_string()?;
        let kind = self.reader.read_external_kind()?;
        let ty = match kind {
            ExternalKind::Function => ImportSectionEntryType::Function(self.reader.read_var_u32()?),
            ExternalKind::Table => ImportSectionEntryType::Table(self.reader.read_table_type()?),
            ExternalKind::Memory => ImportSectionEntryType::Memory(self.reader.read_memory_type()?),
            ExternalKind::Global => ImportSectionEntryType::Global(self.reader.read_global_type()?),
        };
        Ok(Import { module, field, ty })
    }
}

impl<'a> SectionReader for ImportSectionReader<'a> {
    type Item = Import<'a>;
    fn read(&mut self) -> Result<Self::Item> {
        ImportSectionReader::read(self)
    }
    fn eof(&self) -> bool {
        self.reader.eof()
    }
    fn original_position(&self) -> usize {
        ImportSectionReader::original_position(self)
    }
}

impl<'a> SectionWithLimitedItems for ImportSectionReader<'a> {
    fn get_count(&self) -> u32 {
        ImportSectionReader::get_count(self)
    }
}

impl<'a> IntoIterator for ImportSectionReader<'a> {
    type Item = Result<Import<'a>>;
    type IntoIter = SectionIteratorLimited<ImportSectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIteratorLimited::new(self)
    }
}
