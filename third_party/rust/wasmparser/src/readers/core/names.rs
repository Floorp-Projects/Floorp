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

use crate::{BinaryReader, BinaryReaderError, Result, SectionIterator, SectionReader};
use std::ops::Range;

/// Represents a name for an index from the names section.
#[derive(Debug, Copy, Clone)]
pub struct Naming<'a> {
    /// The index being named.
    pub index: u32,
    /// The name for the index.
    pub name: &'a str,
}

/// Represents the type of name.
#[derive(Debug, Copy, Clone)]
pub enum NameType {
    /// The name is for a module.
    Module,
    /// The name is for a function.
    Function,
    /// The name is for a local.
    Local,
    /// The name is for a label.
    Label,
    /// The name is for a type.
    Type,
    /// The name is for a table.
    Table,
    /// The name is for a memory.
    Memory,
    /// The name is for a global.
    Global,
    /// The name is for an element segment.
    Element,
    /// The name is for a data segment.
    Data,
    /// The name is unknown.
    ///
    /// The value is the unknown section identifier.
    Unknown(u8),
}

/// Represents a single name in the names custom section.
#[derive(Debug, Copy, Clone)]
pub struct SingleName<'a> {
    data: &'a [u8],
    offset: usize,
}

impl<'a> SingleName<'a> {
    /// Gets the name as a string.
    pub fn get_name<'b>(&self) -> Result<&'b str>
    where
        'a: 'b,
    {
        let mut reader = BinaryReader::new_with_offset(self.data, self.offset);
        reader.read_string()
    }

    /// Gets the original position of the name.
    pub fn original_position(&self) -> usize {
        self.offset
    }
}

/// A reader for direct names in the names custom section.
pub struct NamingReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> NamingReader<'a> {
    fn new(data: &'a [u8], offset: usize) -> Result<NamingReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(NamingReader { reader, count })
    }

    fn skip(reader: &mut BinaryReader) -> Result<()> {
        let count = reader.read_var_u32()?;
        for _ in 0..count {
            reader.read_var_u32()?;
            reader.skip_string()?;
        }
        Ok(())
    }

    /// Gets the original position of the reader.
    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    /// Gets the count of items in the section.
    pub fn get_count(&self) -> u32 {
        self.count
    }

    /// Reads a name from the names custom section.
    pub fn read<'b>(&mut self) -> Result<Naming<'b>>
    where
        'a: 'b,
    {
        let index = self.reader.read_var_u32()?;
        let name = self.reader.read_string()?;
        Ok(Naming { index, name })
    }
}

/// Represents a name map from the names custom section.
#[derive(Debug, Copy, Clone)]
pub struct NameMap<'a> {
    data: &'a [u8],
    offset: usize,
}

impl<'a> NameMap<'a> {
    /// Gets a naming reader for the map.
    pub fn get_map<'b>(&self) -> Result<NamingReader<'b>>
    where
        'a: 'b,
    {
        NamingReader::new(self.data, self.offset)
    }

    /// Gets the original position of the map.
    pub fn original_position(&self) -> usize {
        self.offset
    }
}

/// Represents an indirect name in the names custom section.
#[derive(Debug, Copy, Clone)]
pub struct IndirectNaming<'a> {
    /// The indirect index of the name.
    pub indirect_index: u32,
    data: &'a [u8],
    offset: usize,
}

impl<'a> IndirectNaming<'a> {
    /// Gets the naming reader for the indirect name.
    pub fn get_map<'b>(&self) -> Result<NamingReader<'b>>
    where
        'a: 'b,
    {
        NamingReader::new(self.data, self.offset)
    }

    /// Gets the original position of the indirect name.
    pub fn original_position(&self) -> usize {
        self.offset
    }
}

/// Represents a reader for indirect names from the names custom section.
pub struct IndirectNamingReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> IndirectNamingReader<'a> {
    fn new(data: &'a [u8], offset: usize) -> Result<IndirectNamingReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(IndirectNamingReader { reader, count })
    }

    /// Gets the count of indirect names.
    pub fn get_indirect_count(&self) -> u32 {
        self.count
    }

    /// Gets the original position of the reader.
    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    /// Reads an indirect name from the reader.
    pub fn read<'b>(&mut self) -> Result<IndirectNaming<'b>>
    where
        'a: 'b,
    {
        let index = self.reader.read_var_u32()?;
        let start = self.reader.position;
        NamingReader::skip(&mut self.reader)?;
        let end = self.reader.position;
        Ok(IndirectNaming {
            indirect_index: index,
            data: &self.reader.buffer[start..end],
            offset: self.reader.original_offset + start,
        })
    }
}

/// Represents an indirect name map.
#[derive(Debug, Copy, Clone)]
pub struct IndirectNameMap<'a> {
    data: &'a [u8],
    offset: usize,
}

impl<'a> IndirectNameMap<'a> {
    /// Gets an indirect naming reader for the map.
    pub fn get_indirect_map<'b>(&self) -> Result<IndirectNamingReader<'b>>
    where
        'a: 'b,
    {
        IndirectNamingReader::new(self.data, self.offset)
    }

    /// Gets an original position of the map.
    pub fn original_position(&self) -> usize {
        self.offset
    }
}

/// Represents a name read from the names custom section.
#[derive(Debug, Clone)]
pub enum Name<'a> {
    /// The name is for the module.
    Module(SingleName<'a>),
    /// The name is for the functions.
    Function(NameMap<'a>),
    /// The name is for the function locals.
    Local(IndirectNameMap<'a>),
    /// The name is for the function labels.
    Label(IndirectNameMap<'a>),
    /// The name is for the types.
    Type(NameMap<'a>),
    /// The name is for the tables.
    Table(NameMap<'a>),
    /// The name is for the memories.
    Memory(NameMap<'a>),
    /// The name is for the globals.
    Global(NameMap<'a>),
    /// The name is for the element segments.
    Element(NameMap<'a>),
    /// The name is for the data segments.
    Data(NameMap<'a>),
    /// An unknown [name subsection](https://webassembly.github.io/spec/core/appendix/custom.html#subsections).
    Unknown {
        /// The identifier for this subsection.
        ty: u8,
        /// The contents of this subsection.
        data: &'a [u8],
        /// The range of bytes, relative to the start of the original data
        /// stream, that the contents of this subsection reside in.
        range: Range<usize>,
    },
}

/// A reader for the name custom section of a WebAssembly module.
pub struct NameSectionReader<'a> {
    reader: BinaryReader<'a>,
}

impl<'a> NameSectionReader<'a> {
    /// Constructs a new `NameSectionReader` from the given data and offset.
    pub fn new(data: &'a [u8], offset: usize) -> Result<NameSectionReader<'a>> {
        Ok(NameSectionReader {
            reader: BinaryReader::new_with_offset(data, offset),
        })
    }

    fn verify_section_end(&self, end: usize) -> Result<()> {
        if self.reader.buffer.len() < end {
            return Err(BinaryReaderError::new(
                "name entry extends past end of the code section",
                self.reader.original_offset + self.reader.buffer.len(),
            ));
        }
        Ok(())
    }

    /// Determines if the reader is at the end of the section.
    pub fn eof(&self) -> bool {
        self.reader.eof()
    }

    /// Gets the original position of the section reader.
    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    /// Reads a name from the section.
    pub fn read<'b>(&mut self) -> Result<Name<'b>>
    where
        'a: 'b,
    {
        let ty = self.reader.read_name_type()?;
        let payload_len = self.reader.read_var_u32()? as usize;
        let payload_start = self.reader.position;
        let payload_end = payload_start + payload_len;
        self.verify_section_end(payload_end)?;
        let offset = self.reader.original_offset + payload_start;
        let data = &self.reader.buffer[payload_start..payload_end];
        self.reader.skip_to(payload_end);
        Ok(match ty {
            NameType::Module => Name::Module(SingleName { data, offset }),
            NameType::Function => Name::Function(NameMap { data, offset }),
            NameType::Local => Name::Local(IndirectNameMap { data, offset }),
            NameType::Label => Name::Label(IndirectNameMap { data, offset }),
            NameType::Type => Name::Type(NameMap { data, offset }),
            NameType::Table => Name::Table(NameMap { data, offset }),
            NameType::Memory => Name::Memory(NameMap { data, offset }),
            NameType::Global => Name::Global(NameMap { data, offset }),
            NameType::Element => Name::Element(NameMap { data, offset }),
            NameType::Data => Name::Data(NameMap { data, offset }),
            NameType::Unknown(ty) => Name::Unknown {
                ty,
                data,
                range: offset..offset + payload_len,
            },
        })
    }
}

impl<'a> SectionReader for NameSectionReader<'a> {
    type Item = Name<'a>;
    fn read(&mut self) -> Result<Self::Item> {
        NameSectionReader::read(self)
    }
    fn eof(&self) -> bool {
        NameSectionReader::eof(self)
    }
    fn original_position(&self) -> usize {
        NameSectionReader::original_position(self)
    }
    fn range(&self) -> Range<usize> {
        self.reader.range()
    }
}

impl<'a> IntoIterator for NameSectionReader<'a> {
    type Item = Result<Name<'a>>;
    type IntoIter = SectionIterator<NameSectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIterator::new(self)
    }
}
