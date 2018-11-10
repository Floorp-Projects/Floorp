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
    BinaryReader, BinaryReaderError, NameType, Naming, Result, SectionIterator, SectionReader,
};

#[derive(Debug, Copy, Clone)]
pub struct ModuleName<'a> {
    data: &'a [u8],
    offset: usize,
}

impl<'a> ModuleName<'a> {
    pub fn get_name<'b>(&self) -> Result<&'b [u8]>
    where
        'a: 'b,
    {
        let mut reader = BinaryReader::new_with_offset(self.data, self.offset);
        reader.read_string()
    }
}

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
            reader.skip_var_32()?;
            reader.skip_string()?;
        }
        Ok(())
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    pub fn read<'b>(&mut self) -> Result<Naming<'b>>
    where
        'a: 'b,
    {
        let index = self.reader.read_var_u32()?;
        let name = self.reader.read_string()?;
        Ok(Naming { index, name })
    }
}

#[derive(Debug, Copy, Clone)]
pub struct FunctionName<'a> {
    data: &'a [u8],
    offset: usize,
}

impl<'a> FunctionName<'a> {
    pub fn get_map<'b>(&self) -> Result<NamingReader<'b>>
    where
        'a: 'b,
    {
        NamingReader::new(self.data, self.offset)
    }
}

#[derive(Debug, Copy, Clone)]
pub struct FunctionLocalName<'a> {
    pub func_index: u32,
    data: &'a [u8],
    offset: usize,
}

impl<'a> FunctionLocalName<'a> {
    pub fn get_map<'b>(&self) -> Result<NamingReader<'b>>
    where
        'a: 'b,
    {
        NamingReader::new(self.data, self.offset)
    }
}

pub struct FunctionLocalReader<'a> {
    reader: BinaryReader<'a>,
    count: u32,
}

impl<'a> FunctionLocalReader<'a> {
    fn new(data: &'a [u8], offset: usize) -> Result<FunctionLocalReader<'a>> {
        let mut reader = BinaryReader::new_with_offset(data, offset);
        let count = reader.read_var_u32()?;
        Ok(FunctionLocalReader { reader, count })
    }

    pub fn get_count(&self) -> u32 {
        self.count
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

    pub fn read<'b>(&mut self) -> Result<FunctionLocalName<'b>>
    where
        'a: 'b,
    {
        let func_index = self.reader.read_var_u32()?;
        let start = self.reader.position;
        NamingReader::skip(&mut self.reader)?;
        let end = self.reader.position;
        Ok(FunctionLocalName {
            func_index,
            data: &self.reader.buffer[start..end],
            offset: self.reader.original_offset + start,
        })
    }
}

#[derive(Debug, Copy, Clone)]
pub struct LocalName<'a> {
    data: &'a [u8],
    offset: usize,
}

impl<'a> LocalName<'a> {
    pub fn get_function_local_reader<'b>(&self) -> Result<FunctionLocalReader<'b>>
    where
        'a: 'b,
    {
        FunctionLocalReader::new(self.data, self.offset)
    }
}

#[derive(Debug, Copy, Clone)]
pub enum Name<'a> {
    Module(ModuleName<'a>),
    Function(FunctionName<'a>),
    Local(LocalName<'a>),
}

pub struct NameSectionReader<'a> {
    reader: BinaryReader<'a>,
}

impl<'a> NameSectionReader<'a> {
    pub fn new(data: &'a [u8], offset: usize) -> Result<NameSectionReader<'a>> {
        Ok(NameSectionReader {
            reader: BinaryReader::new_with_offset(data, offset),
        })
    }

    fn verify_section_end(&self, end: usize) -> Result<()> {
        if self.reader.buffer.len() < end {
            return Err(BinaryReaderError {
                message: "Name entry extends past end of the code section",
                offset: self.reader.original_offset + self.reader.buffer.len(),
            });
        }
        Ok(())
    }

    pub fn eof(&self) -> bool {
        self.reader.eof()
    }

    pub fn original_position(&self) -> usize {
        self.reader.original_position()
    }

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
            NameType::Module => Name::Module(ModuleName { data, offset }),
            NameType::Function => Name::Function(FunctionName { data, offset }),
            NameType::Local => Name::Local(LocalName { data, offset }),
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
}

impl<'a> IntoIterator for NameSectionReader<'a> {
    type Item = Result<Name<'a>>;
    type IntoIter = SectionIterator<NameSectionReader<'a>>;

    fn into_iter(self) -> Self::IntoIter {
        SectionIterator::new(self)
    }
}
