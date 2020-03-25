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
    BinaryReader, BinaryReaderError, CustomSectionKind, Range, Result, SectionCode, SectionHeader,
};

use super::{
    read_data_count_section_content, read_sourcemappingurl_section_content,
    read_start_section_content, CodeSectionReader, DataSectionReader, ElementSectionReader,
    ExportSectionReader, FunctionSectionReader, GlobalSectionReader, ImportSectionReader,
    LinkingSectionReader, MemorySectionReader, NameSectionReader, ProducersSectionReader,
    RelocSectionReader, TableSectionReader, TypeSectionReader,
};

#[derive(Debug)]
pub struct Section<'a> {
    pub code: SectionCode<'a>,
    offset: usize,
    data: &'a [u8],
}

impl<'a> Section<'a> {
    /// Creates reader for the type section. Available when the reader just read
    /// the type section.
    pub fn get_type_section_reader<'b>(&self) -> Result<TypeSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Type => TypeSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_type_section_reader"),
        }
    }

    /// Creates reader for the function section. Available when the reader just read
    /// the function section.
    pub fn get_function_section_reader<'b>(&self) -> Result<FunctionSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Function => FunctionSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_function_section_reader"),
        }
    }

    /// Creates reader for the code section. Available when the reader just read
    /// the code section.
    pub fn get_code_section_reader<'b>(&self) -> Result<CodeSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Code => CodeSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_function_section_reader"),
        }
    }

    /// Creates reader for the export section. Available when the reader just read
    /// the export section.
    pub fn get_export_section_reader<'b>(&self) -> Result<ExportSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Export => ExportSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_export_section_reader"),
        }
    }

    /// Creates reader for the import section. Available when the reader just read
    /// the import section.
    pub fn get_import_section_reader<'b>(&self) -> Result<ImportSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Import => ImportSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_import_section_reader"),
        }
    }

    /// Creates reader for the global section. Available when the reader just read
    /// the global section.
    pub fn get_global_section_reader<'b>(&self) -> Result<GlobalSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Global => GlobalSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_global_section_reader"),
        }
    }

    /// Creates reader for the memory section. Available when the reader just read
    /// the memory section.
    pub fn get_memory_section_reader<'b>(&self) -> Result<MemorySectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Memory => MemorySectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_memory_section_reader"),
        }
    }

    /// Creates reader for the data section. Available when the reader just read
    /// the data section.
    pub fn get_data_section_reader<'b>(&self) -> Result<DataSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Data => DataSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_data_section_reader"),
        }
    }

    /// Creates reader for the table section. Available when the reader just read
    /// the table section.
    pub fn get_table_section_reader<'b>(&self) -> Result<TableSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Table => TableSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_table_section_reader"),
        }
    }

    /// Creates reader for the element section. Available when the reader just read
    /// the element section.
    pub fn get_element_section_reader<'b>(&self) -> Result<ElementSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Element => ElementSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_element_section_reader"),
        }
    }

    pub fn get_name_section_reader<'b>(&self) -> Result<NameSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Custom {
                kind: CustomSectionKind::Name,
                ..
            } => NameSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_name_section_reader"),
        }
    }

    pub fn get_producers_section_reader<'b>(&self) -> Result<ProducersSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Custom {
                kind: CustomSectionKind::Producers,
                ..
            } => ProducersSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_producers_section_reader"),
        }
    }

    pub fn get_linking_section_reader<'b>(&self) -> Result<LinkingSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Custom {
                kind: CustomSectionKind::Linking,
                ..
            } => LinkingSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_linking_section_reader"),
        }
    }

    pub fn get_reloc_section_reader<'b>(&self) -> Result<RelocSectionReader<'b>>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Custom {
                kind: CustomSectionKind::Reloc,
                ..
            } => RelocSectionReader::new(self.data, self.offset),
            _ => panic!("Invalid state for get_reloc_section_reader"),
        }
    }

    pub fn get_start_section_content(&self) -> Result<u32> {
        match self.code {
            SectionCode::Start => read_start_section_content(self.data, self.offset),
            _ => panic!("Invalid state for get_start_section_content"),
        }
    }

    pub fn get_data_count_section_content(&self) -> Result<u32> {
        match self.code {
            SectionCode::DataCount => read_data_count_section_content(self.data, self.offset),
            _ => panic!("Invalid state for get_data_count_section_content"),
        }
    }

    pub fn get_sourcemappingurl_section_content<'b>(&self) -> Result<&'b str>
    where
        'a: 'b,
    {
        match self.code {
            SectionCode::Custom {
                kind: CustomSectionKind::SourceMappingURL,
                ..
            } => read_sourcemappingurl_section_content(self.data, self.offset),
            _ => panic!("Invalid state for get_start_section_content"),
        }
    }

    pub fn get_binary_reader<'b>(&self) -> BinaryReader<'b>
    where
        'a: 'b,
    {
        BinaryReader::new_with_offset(self.data, self.offset)
    }

    pub fn range(&self) -> Range {
        Range {
            start: self.offset,
            end: self.offset + self.data.len(),
        }
    }

    pub fn content<'b>(&self) -> Result<SectionContent<'b>>
    where
        'a: 'b,
    {
        let c = match self.code {
            SectionCode::Type => SectionContent::Type(self.get_type_section_reader()?),
            SectionCode::Function => SectionContent::Function(self.get_function_section_reader()?),
            SectionCode::Code => SectionContent::Code(self.get_code_section_reader()?),
            SectionCode::Export => SectionContent::Export(self.get_export_section_reader()?),
            SectionCode::Import => SectionContent::Import(self.get_import_section_reader()?),
            SectionCode::Global => SectionContent::Global(self.get_global_section_reader()?),
            SectionCode::Memory => SectionContent::Memory(self.get_memory_section_reader()?),
            SectionCode::Data => SectionContent::Data(self.get_data_section_reader()?),
            SectionCode::Table => SectionContent::Table(self.get_table_section_reader()?),
            SectionCode::Element => SectionContent::Element(self.get_element_section_reader()?),
            SectionCode::Start => SectionContent::Start(self.get_start_section_content()?),
            SectionCode::DataCount => {
                SectionContent::DataCount(self.get_data_count_section_content()?)
            }
            SectionCode::Custom { kind, name } => {
                // The invalid custom section may cause trouble during
                // content() call. The spec recommends to ignore erroneous
                // content in custom section.
                // Return None in the content field if invalid.
                let binary = self.get_binary_reader();
                let content = match kind {
                    CustomSectionKind::Name => self
                        .get_name_section_reader()
                        .ok()
                        .map(CustomSectionContent::Name),
                    CustomSectionKind::Producers => self
                        .get_producers_section_reader()
                        .ok()
                        .map(CustomSectionContent::Producers),
                    CustomSectionKind::Linking => self
                        .get_linking_section_reader()
                        .ok()
                        .map(CustomSectionContent::Linking),
                    CustomSectionKind::Reloc => self
                        .get_reloc_section_reader()
                        .ok()
                        .map(CustomSectionContent::Reloc),
                    CustomSectionKind::SourceMappingURL => self
                        .get_sourcemappingurl_section_content()
                        .ok()
                        .map(CustomSectionContent::SourceMappingURL),
                    _ => None,
                };
                SectionContent::Custom {
                    name,
                    binary,
                    content,
                }
            }
        };
        Ok(c)
    }
}

pub enum SectionContent<'a> {
    Type(TypeSectionReader<'a>),
    Function(FunctionSectionReader<'a>),
    Code(CodeSectionReader<'a>),
    Export(ExportSectionReader<'a>),
    Import(ImportSectionReader<'a>),
    Global(GlobalSectionReader<'a>),
    Memory(MemorySectionReader<'a>),
    Data(DataSectionReader<'a>),
    Table(TableSectionReader<'a>),
    Element(ElementSectionReader<'a>),
    Start(u32),
    DataCount(u32),
    Custom {
        name: &'a str,
        binary: BinaryReader<'a>,
        content: Option<CustomSectionContent<'a>>,
    },
}

pub enum CustomSectionContent<'a> {
    Name(NameSectionReader<'a>),
    Producers(ProducersSectionReader<'a>),
    Linking(LinkingSectionReader<'a>),
    Reloc(RelocSectionReader<'a>),
    SourceMappingURL(&'a str),
}

/// Reads top-level WebAssembly file structure: header and sections.
pub struct ModuleReader<'a> {
    reader: BinaryReader<'a>,
    version: u32,
    read_ahead: Option<(usize, SectionHeader<'a>)>,
}

impl<'a> ModuleReader<'a> {
    pub fn new(data: &[u8]) -> Result<ModuleReader> {
        let mut reader = BinaryReader::new(data);
        let version = reader.read_file_header()?;
        Ok(ModuleReader {
            reader,
            version,
            read_ahead: None,
        })
    }

    pub fn get_version(&self) -> u32 {
        self.version
    }

    pub fn current_position(&self) -> usize {
        match self.read_ahead {
            Some((position, _)) => position,
            _ => self.reader.current_position(),
        }
    }

    pub fn eof(&self) -> bool {
        self.read_ahead.is_none() && self.reader.eof()
    }

    fn verify_section_end(&self, end: usize) -> Result<()> {
        if self.reader.buffer.len() < end {
            return Err(BinaryReaderError::new(
                "Section body extends past end of file",
                self.reader.buffer.len(),
            ));
        }
        if self.reader.position > end {
            return Err(BinaryReaderError::new(
                "Section header is too big to fit into section body",
                end,
            ));
        }
        Ok(())
    }

    /// Reads next top-level record from the WebAssembly binary data.
    /// The methods returns reference to current state of the reader.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::ModuleReader;
    /// let mut reader = ModuleReader::new(data).expect("reader");
    /// let section = reader.read().expect("section #1");
    /// println!("First section {:?}", section);
    /// let section = reader.read().expect("section #2");
    /// println!("Second section {:?}", section);
    /// assert!(!reader.eof(), "there are more sections");
    /// ```
    pub fn read<'b>(&mut self) -> Result<Section<'b>>
    where
        'a: 'b,
    {
        let SectionHeader {
            code,
            payload_start,
            payload_len,
        } = match self.read_ahead.take() {
            Some((_, section_header)) => section_header,
            None => self.reader.read_section_header()?,
        };
        let payload_end = payload_start + payload_len;
        self.verify_section_end(payload_end)?;
        let body_start = self.reader.position;
        self.reader.skip_to(payload_end);
        Ok(Section {
            code,
            offset: body_start,
            data: &self.reader.buffer[body_start..payload_end],
        })
    }

    fn ensure_read_ahead(&mut self) -> Result<()> {
        if self.read_ahead.is_none() && !self.eof() {
            let position = self.reader.current_position();
            self.read_ahead = Some((position, self.reader.read_section_header()?));
        }
        Ok(())
    }

    /// Skips custom sections.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x00, 0x8, 0x03, 0x63, 0x61, 0x74, 0x01, 0x02, 0x03, 0x04,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::ModuleReader;
    /// use wasmparser::SectionCode;
    /// let mut reader = ModuleReader::new(data).expect("reader");
    /// while { reader.skip_custom_sections(); !reader.eof() } {
    ///     let section = reader.read().expect("section");
    ///     if let SectionCode::Custom {..} = section.code { panic!("no custom"); }
    ///     println!("Section {:?}", section);
    /// }
    /// ```
    pub fn skip_custom_sections(&mut self) -> Result<()> {
        loop {
            self.ensure_read_ahead()?;
            match self.read_ahead {
                Some((
                    _,
                    SectionHeader {
                        code: SectionCode::Custom { .. },
                        payload_start,
                        payload_len,
                    },
                )) => {
                    self.verify_section_end(payload_start + payload_len)?;
                    // Skip section
                    self.read_ahead = None;
                    self.reader.skip_to(payload_start + payload_len);
                }
                _ => break,
            };
        }
        Ok(())
    }
}

impl<'a> IntoIterator for ModuleReader<'a> {
    type Item = Result<Section<'a>>;
    type IntoIter = ModuleIterator<'a>;
    fn into_iter(self) -> Self::IntoIter {
        ModuleIterator {
            reader: self,
            err: false,
        }
    }
}

pub struct ModuleIterator<'a> {
    reader: ModuleReader<'a>,
    err: bool,
}

impl<'a> Iterator for ModuleIterator<'a> {
    type Item = Result<Section<'a>>;

    /// Iterates sections from the WebAssembly binary data. Stops at first error.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::ModuleReader;
    /// for section in ModuleReader::new(data).expect("reader") {
    ///     println!("Section {:?}", section);
    /// }
    /// ```
    fn next(&mut self) -> Option<Self::Item> {
        if self.err || self.reader.eof() {
            return None;
        }
        let result = self.reader.read();
        self.err = result.is_err();
        Some(result)
    }
}
