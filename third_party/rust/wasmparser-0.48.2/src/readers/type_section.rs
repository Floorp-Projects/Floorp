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
    BinaryReader, FuncType, Result, SectionIteratorLimited, SectionReader, SectionWithLimitedItems,
};

pub struct TypeSectionReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> TypeSectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<TypeSectionReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(TypeSectionReader { reader, count })
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    /// Reads content of the type section.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::ModuleReader;
    /// let mut reader = ModuleReader::new(data).expect("module reader");
    /// let section = reader.read().expect("section");
    /// let mut type_reader = section.get_type_section_reader().expect("type section reader");
    /// for _ in 0..type_reader.get_count() {
    ///     let ty = type_reader.read().expect("type");
    ///     println!("Type {:?}", ty);
    /// }
    /// ```
    pub fn read(&mut self) -> Result<FuncType> {
        self.reader.read_func_type()
    }
}

impl<'a> SectionReader for TypeSectionReader<'a> {
    type Item = FuncType;
    fn read(&mut self) -> Result<Self::Item> {
        TypeSectionReader::read(self)
    }
    fn eof(&self) -> bool {
        self.reader.eof()
    }
    fn original_position(&self) -> usize {
        TypeSectionReader::original_position(self)
    }
}

impl<'a> SectionWithLimitedItems for TypeSectionReader<'a> {
    fn get_count(&self) -> u32 {
        TypeSectionReader::get_count(self)
    }
}

impl<'a> IntoIterator for TypeSectionReader<'a> {
    type Item = Result<FuncType>;
    type IntoIter = SectionIteratorLimited<TypeSectionReader<'a>>;

    /// Implements iterator over the type section.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::ModuleReader;
    /// use wasmparser::{Result, FuncType};
    /// let mut reader = ModuleReader::new(data).expect("module reader");
    /// let section = reader.read().expect("section");
    /// let mut type_reader = section.get_type_section_reader().expect("type section reader");
    /// for ty in type_reader {
    ///     println!("Type {:?}", ty);
    /// }
    /// ```
    fn into_iter(self) -> Self::IntoIter {
        SectionIteratorLimited::new(self)
    }
}
