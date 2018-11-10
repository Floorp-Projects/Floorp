/* Copyright 2017 Mozilla Foundation
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
// See https://github.com/WebAssembly/design/blob/master/BinaryEncoding.md

use std::boxed::Box;
use std::vec::Vec;

use limits::{
    MAX_WASM_FUNCTIONS, MAX_WASM_FUNCTION_LOCALS, MAX_WASM_STRING_SIZE, MAX_WASM_TABLE_ENTRIES,
};

use primitives::{
    BinaryReaderError, CustomSectionKind, ExternalKind, FuncType, GlobalType,
    ImportSectionEntryType, LinkingType, MemoryType, NameType, Naming, Operator, RelocType, Result,
    SectionCode, TableType, Type,
};

use binary_reader::{BinaryReader, SectionHeader};

const MAX_DATA_CHUNK_SIZE: usize = MAX_WASM_STRING_SIZE;

#[derive(Debug)]
pub struct LocalName<'a> {
    pub index: u32,
    pub locals: Box<[Naming<'a>]>,
}

#[derive(Debug)]
pub enum NameEntry<'a> {
    Module(&'a [u8]),
    Function(Box<[Naming<'a>]>),
    Local(Box<[LocalName<'a>]>),
}

#[derive(Debug)]
pub struct RelocEntry {
    pub ty: RelocType,
    pub offset: u32,
    pub index: u32,
    pub addend: Option<u32>,
}

enum InitExpressionContinuation {
    GlobalSection,
    ElementSection,
    DataSection,
}

/// Bytecode range in the WebAssembly module.
#[derive(Debug, Copy, Clone)]
pub struct Range {
    pub start: usize,
    pub end: usize,
}

impl Range {
    pub fn new(start: usize, end: usize) -> Range {
        assert!(start <= end);
        Range { start, end }
    }

    pub fn slice<'a>(&self, data: &'a [u8]) -> &'a [u8] {
        &data[self.start..self.end]
    }
}

#[derive(Debug)]
pub enum ParserState<'a> {
    Error(BinaryReaderError),
    Initial,
    BeginWasm {
        version: u32,
    },
    EndWasm,
    BeginSection {
        code: SectionCode<'a>,
        range: Range,
    },
    EndSection,
    SkippingSection,
    ReadingCustomSection(CustomSectionKind),
    ReadingSectionRawData,
    SectionRawData(&'a [u8]),

    TypeSectionEntry(FuncType),
    ImportSectionEntry {
        module: &'a [u8],
        field: &'a [u8],
        ty: ImportSectionEntryType,
    },
    FunctionSectionEntry(u32),
    TableSectionEntry(TableType),
    MemorySectionEntry(MemoryType),
    ExportSectionEntry {
        field: &'a [u8],
        kind: ExternalKind,
        index: u32,
    },
    NameSectionEntry(NameEntry<'a>),
    StartSectionEntry(u32),

    BeginInitExpressionBody,
    InitExpressionOperator(Operator<'a>),
    EndInitExpressionBody,

    BeginFunctionBody {
        range: Range,
    },
    FunctionBodyLocals {
        locals: Box<[(u32, Type)]>,
    },
    CodeOperator(Operator<'a>),
    EndFunctionBody,
    SkippingFunctionBody,

    BeginElementSectionEntry(u32),
    ElementSectionEntryBody(Box<[u32]>),
    EndElementSectionEntry,

    BeginDataSectionEntry(u32),
    EndDataSectionEntry,
    BeginDataSectionEntryBody(u32),
    DataSectionEntryBodyChunk(&'a [u8]),
    EndDataSectionEntryBody,

    BeginGlobalSectionEntry(GlobalType),
    EndGlobalSectionEntry,

    RelocSectionHeader(SectionCode<'a>),
    RelocSectionEntry(RelocEntry),
    LinkingSectionEntry(LinkingType),

    SourceMappingURL(&'a [u8]),
}

#[derive(Debug, Copy, Clone)]
pub enum ParserInput {
    Default,
    SkipSection,
    SkipFunctionBody,
    ReadCustomSection,
    ReadSectionRawData,
}

pub trait WasmDecoder<'a> {
    fn read(&mut self) -> &ParserState<'a>;
    fn push_input(&mut self, input: ParserInput);
    fn read_with_input(&mut self, input: ParserInput) -> &ParserState<'a>;
    fn create_binary_reader<'b>(&mut self) -> BinaryReader<'b>
    where
        'a: 'b;
    fn last_state(&self) -> &ParserState<'a>;
}

/// The `Parser` type. A simple event-driven parser of WebAssembly binary
/// format. The `read(&mut self)` is used to iterate through WebAssembly records.
pub struct Parser<'a> {
    reader: BinaryReader<'a>,
    state: ParserState<'a>,
    section_range: Option<Range>,
    function_range: Option<Range>,
    init_expr_continuation: Option<InitExpressionContinuation>,
    read_data_bytes: Option<u32>,
    section_entries_left: u32,
}

impl<'a> Parser<'a> {
    /// Constructs `Parser` type.
    ///
    /// # Examples
    /// ```
    /// let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ///     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    ///     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// let mut parser = wasmparser::Parser::new(data);
    /// ```
    pub fn new(data: &[u8]) -> Parser {
        Parser {
            reader: BinaryReader::new(data),
            state: ParserState::Initial,
            section_range: None,
            function_range: None,
            init_expr_continuation: None,
            read_data_bytes: None,
            section_entries_left: 0,
        }
    }

    pub fn eof(&self) -> bool {
        self.reader.eof()
    }

    pub fn current_position(&self) -> usize {
        self.reader.current_position()
    }

    fn read_header(&mut self) -> Result<()> {
        let version = self.reader.read_file_header()?;
        self.state = ParserState::BeginWasm { version };
        Ok(())
    }

    fn read_section_header(&mut self) -> Result<()> {
        let SectionHeader {
            code,
            payload_start,
            payload_len,
        } = self.reader.read_section_header()?;
        let payload_end = payload_start + payload_len;
        if self.reader.buffer.len() < payload_end {
            return Err(BinaryReaderError {
                message: "Section body extends past end of file",
                offset: self.reader.buffer.len(),
            });
        }
        if self.reader.position > payload_end {
            return Err(BinaryReaderError {
                message: "Section header is too big to fit into section body",
                offset: payload_end,
            });
        }
        let range = Range {
            start: self.reader.position,
            end: payload_end,
        };
        self.state = ParserState::BeginSection { code, range };
        self.section_range = Some(range);
        Ok(())
    }

    fn read_type_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::TypeSectionEntry(self.reader.read_func_type()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_import_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let module = self.reader.read_string()?;
        let field = self.reader.read_string()?;
        let kind = self.reader.read_external_kind()?;
        let ty: ImportSectionEntryType;
        match kind {
            ExternalKind::Function => {
                ty = ImportSectionEntryType::Function(self.reader.read_var_u32()?)
            }
            ExternalKind::Table => {
                ty = ImportSectionEntryType::Table(self.reader.read_table_type()?)
            }
            ExternalKind::Memory => {
                ty = ImportSectionEntryType::Memory(self.reader.read_memory_type()?)
            }
            ExternalKind::Global => {
                ty = ImportSectionEntryType::Global(self.reader.read_global_type()?)
            }
        }

        self.state = ParserState::ImportSectionEntry { module, field, ty };
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_function_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::FunctionSectionEntry(self.reader.read_var_u32()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_memory_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::MemorySectionEntry(self.reader.read_memory_type()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_global_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::BeginGlobalSectionEntry(self.reader.read_global_type()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_init_expression_body(&mut self, cont: InitExpressionContinuation) {
        self.state = ParserState::BeginInitExpressionBody;
        self.init_expr_continuation = Some(cont);
    }

    fn read_init_expression_operator(&mut self) -> Result<()> {
        let op = self.reader.read_operator()?;
        if let Operator::End = op {
            self.state = ParserState::EndInitExpressionBody;
            return Ok(());
        }
        self.state = ParserState::InitExpressionOperator(op);
        Ok(())
    }

    fn read_export_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let field = self.reader.read_string()?;
        let kind = self.reader.read_external_kind()?;
        let index = self.reader.read_var_u32()?;
        self.state = ParserState::ExportSectionEntry { field, kind, index };
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_element_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::BeginElementSectionEntry(self.reader.read_var_u32()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_element_entry_body(&mut self) -> Result<()> {
        let num_elements = self.reader.read_var_u32()? as usize;
        if num_elements > MAX_WASM_TABLE_ENTRIES {
            return Err(BinaryReaderError {
                message: "num_elements is out of bounds",
                offset: self.reader.position - 1,
            });
        }
        let mut elements: Vec<u32> = Vec::with_capacity(num_elements);
        for _ in 0..num_elements {
            elements.push(self.reader.read_var_u32()?);
        }
        self.state = ParserState::ElementSectionEntryBody(elements.into_boxed_slice());
        Ok(())
    }

    fn read_function_body(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let size = self.reader.read_var_u32()? as usize;
        let body_end = self.reader.position + size;
        let range = Range {
            start: self.reader.position,
            end: body_end,
        };
        self.state = ParserState::BeginFunctionBody { range };
        self.function_range = Some(range);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_function_body_locals(&mut self) -> Result<()> {
        let local_count = self.reader.read_local_count()?;
        let mut locals: Vec<(u32, Type)> = Vec::with_capacity(local_count);
        let mut locals_total = 0;
        for _ in 0..local_count {
            let (count, ty) = self.reader.read_local_decl(&mut locals_total)?;
            locals.push((count, ty));
        }
        self.state = ParserState::FunctionBodyLocals {
            locals: locals.into_boxed_slice(),
        };
        Ok(())
    }

    fn read_code_operator(&mut self) -> Result<()> {
        if self.reader.position >= self.function_range.unwrap().end {
            if let ParserState::CodeOperator(Operator::End) = self.state {
                self.state = ParserState::EndFunctionBody;
                self.function_range = None;
                return Ok(());
            }
            return Err(BinaryReaderError {
                message: "Expected end of function marker",
                offset: self.function_range.unwrap().end,
            });
        }
        let op = self.reader.read_operator()?;
        self.state = ParserState::CodeOperator(op);
        Ok(())
    }

    fn read_table_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        self.state = ParserState::TableSectionEntry(self.reader.read_table_type()?);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_data_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let index = self.reader.read_var_u32()?;
        self.state = ParserState::BeginDataSectionEntry(index);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_data_entry_body(&mut self) -> Result<()> {
        let size = self.reader.read_var_u32()?;
        self.state = ParserState::BeginDataSectionEntryBody(size);
        self.read_data_bytes = Some(size);
        Ok(())
    }

    fn read_name_entry(&mut self) -> Result<()> {
        if self.reader.position >= self.section_range.unwrap().end {
            return self.position_to_section_end();
        }
        let ty = self.reader.read_name_type()?;
        self.reader.read_var_u32()?; // payload_len
        let entry = match ty {
            NameType::Module => NameEntry::Module(self.reader.read_string()?),
            NameType::Function => {
                NameEntry::Function(self.reader.read_name_map(MAX_WASM_FUNCTIONS)?)
            }
            NameType::Local => {
                let funcs_len = self.reader.read_var_u32()? as usize;
                if funcs_len > MAX_WASM_FUNCTIONS {
                    return Err(BinaryReaderError {
                        message: "function count is out of bounds",
                        offset: self.reader.position - 1,
                    });
                }
                let mut funcs: Vec<LocalName<'a>> = Vec::with_capacity(funcs_len);
                for _ in 0..funcs_len {
                    funcs.push(LocalName {
                        index: self.reader.read_var_u32()?,
                        locals: self.reader.read_name_map(MAX_WASM_FUNCTION_LOCALS)?,
                    });
                }
                NameEntry::Local(funcs.into_boxed_slice())
            }
        };
        self.state = ParserState::NameSectionEntry(entry);
        Ok(())
    }

    fn read_source_mapping(&mut self) -> Result<()> {
        self.state = ParserState::SourceMappingURL(self.reader.read_string()?);
        Ok(())
    }

    // See https://github.com/WebAssembly/tool-conventions/blob/master/Linking.md
    fn read_reloc_header(&mut self) -> Result<()> {
        let section_id_position = self.reader.position;
        let section_id = self.reader.read_var_u7()?;
        let section_code = self
            .reader
            .read_section_code(section_id, section_id_position)?;
        self.state = ParserState::RelocSectionHeader(section_code);
        Ok(())
    }

    fn read_reloc_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let ty = self.reader.read_reloc_type()?;
        let offset = self.reader.read_var_u32()?;
        let index = self.reader.read_var_u32()?;
        let addend = match ty {
            RelocType::FunctionIndexLEB
            | RelocType::TableIndexSLEB
            | RelocType::TableIndexI32
            | RelocType::TypeIndexLEB
            | RelocType::GlobalIndexLEB => None,
            RelocType::GlobalAddrLEB | RelocType::GlobalAddrSLEB | RelocType::GlobalAddrI32 => {
                Some(self.reader.read_var_u32()?)
            }
        };
        self.state = ParserState::RelocSectionEntry(RelocEntry {
            ty,
            offset,
            index,
            addend,
        });
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_linking_entry(&mut self) -> Result<()> {
        if self.section_entries_left == 0 {
            return self.position_to_section_end();
        }
        let entry = self.reader.read_linking_type()?;
        self.state = ParserState::LinkingSectionEntry(entry);
        self.section_entries_left -= 1;
        Ok(())
    }

    fn read_section_body(&mut self) -> Result<()> {
        match self.state {
            ParserState::BeginSection {
                code: SectionCode::Type,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_type_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Import,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_import_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Function,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_function_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Memory,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_memory_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Global,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_global_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Export,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_export_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Element,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_element_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Code,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_function_body()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Table,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_table_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Data,
                ..
            } => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_data_entry()?;
            }
            ParserState::BeginSection {
                code: SectionCode::Start,
                ..
            } => {
                self.state = ParserState::StartSectionEntry(self.reader.read_var_u32()?);
            }
            ParserState::BeginSection {
                code: SectionCode::Custom { .. },
                ..
            } => {
                self.read_section_body_bytes()?;
            }
            _ => unreachable!(),
        }
        Ok(())
    }

    fn read_custom_section_body(&mut self) -> Result<()> {
        match self.state {
            ParserState::ReadingCustomSection(CustomSectionKind::Name) => {
                self.read_name_entry()?;
            }
            ParserState::ReadingCustomSection(CustomSectionKind::SourceMappingURL) => {
                self.read_source_mapping()?;
            }
            ParserState::ReadingCustomSection(CustomSectionKind::Reloc) => {
                self.read_reloc_header()?;
            }
            ParserState::ReadingCustomSection(CustomSectionKind::Linking) => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_linking_entry()?;
            }
            ParserState::ReadingCustomSection(CustomSectionKind::Unknown) => {
                self.read_section_body_bytes()?;
            }
            _ => unreachable!(),
        }
        Ok(())
    }

    fn ensure_reader_position_in_section_range(&self) -> Result<()> {
        if self.section_range.unwrap().end < self.reader.position {
            return Err(BinaryReaderError {
                message: "Position past the section end",
                offset: self.section_range.unwrap().end,
            });
        }
        Ok(())
    }

    fn position_to_section_end(&mut self) -> Result<()> {
        self.ensure_reader_position_in_section_range()?;
        self.reader.skip_to(self.section_range.unwrap().end);
        self.section_range = None;
        self.state = ParserState::EndSection;
        Ok(())
    }

    fn read_section_body_bytes(&mut self) -> Result<()> {
        self.ensure_reader_position_in_section_range()?;
        if self.section_range.unwrap().end == self.reader.position {
            self.state = ParserState::EndSection;
            self.section_range = None;
            return Ok(());
        }
        let to_read =
            if self.section_range.unwrap().end - self.reader.position < MAX_DATA_CHUNK_SIZE {
                self.section_range.unwrap().end - self.reader.position
            } else {
                MAX_DATA_CHUNK_SIZE
            };
        let bytes = self.reader.read_bytes(to_read)?;
        self.state = ParserState::SectionRawData(bytes);
        Ok(())
    }

    fn read_data_chunk(&mut self) -> Result<()> {
        if self.read_data_bytes.unwrap() == 0 {
            self.state = ParserState::EndDataSectionEntryBody;
            self.read_data_bytes = None;
            return Ok(());
        }
        let to_read = if self.read_data_bytes.unwrap() as usize > MAX_DATA_CHUNK_SIZE {
            MAX_DATA_CHUNK_SIZE
        } else {
            self.read_data_bytes.unwrap() as usize
        };
        let chunk = self.reader.read_bytes(to_read)?;
        *self.read_data_bytes.as_mut().unwrap() -= to_read as u32;
        self.state = ParserState::DataSectionEntryBodyChunk(chunk);
        Ok(())
    }

    fn read_next_section(&mut self) -> Result<()> {
        if self.reader.eof() {
            self.state = ParserState::EndWasm;
        } else {
            self.read_section_header()?;
        }
        Ok(())
    }

    fn read_wrapped(&mut self) -> Result<()> {
        match self.state {
            ParserState::EndWasm => panic!("Parser in end state"),
            ParserState::Error(_) => panic!("Parser in error state"),
            ParserState::Initial => self.read_header()?,
            ParserState::BeginWasm { .. } | ParserState::EndSection => self.read_next_section()?,
            ParserState::BeginSection { .. } => self.read_section_body()?,
            ParserState::SkippingSection => {
                self.position_to_section_end()?;
                self.read_next_section()?;
            }
            ParserState::TypeSectionEntry(_) => self.read_type_entry()?,
            ParserState::ImportSectionEntry { .. } => self.read_import_entry()?,
            ParserState::FunctionSectionEntry(_) => self.read_function_entry()?,
            ParserState::MemorySectionEntry(_) => self.read_memory_entry()?,
            ParserState::TableSectionEntry(_) => self.read_table_entry()?,
            ParserState::ExportSectionEntry { .. } => self.read_export_entry()?,
            ParserState::BeginGlobalSectionEntry(_) => {
                self.read_init_expression_body(InitExpressionContinuation::GlobalSection)
            }
            ParserState::EndGlobalSectionEntry => self.read_global_entry()?,
            ParserState::BeginElementSectionEntry(_) => {
                self.read_init_expression_body(InitExpressionContinuation::ElementSection)
            }
            ParserState::BeginInitExpressionBody | ParserState::InitExpressionOperator(_) => {
                self.read_init_expression_operator()?
            }
            ParserState::BeginDataSectionEntry(_) => {
                self.read_init_expression_body(InitExpressionContinuation::DataSection)
            }
            ParserState::EndInitExpressionBody => {
                match self.init_expr_continuation {
                    Some(InitExpressionContinuation::GlobalSection) => {
                        self.state = ParserState::EndGlobalSectionEntry
                    }
                    Some(InitExpressionContinuation::ElementSection) => {
                        self.read_element_entry_body()?
                    }
                    Some(InitExpressionContinuation::DataSection) => self.read_data_entry_body()?,
                    None => unreachable!(),
                }
                self.init_expr_continuation = None;
            }
            ParserState::BeginFunctionBody { .. } => self.read_function_body_locals()?,
            ParserState::FunctionBodyLocals { .. } | ParserState::CodeOperator(_) => {
                self.read_code_operator()?
            }
            ParserState::EndFunctionBody => self.read_function_body()?,
            ParserState::SkippingFunctionBody => {
                assert!(self.reader.position <= self.function_range.unwrap().end);
                self.reader.skip_to(self.function_range.unwrap().end);
                self.function_range = None;
                self.read_function_body()?;
            }
            ParserState::EndDataSectionEntry => self.read_data_entry()?,
            ParserState::BeginDataSectionEntryBody(_)
            | ParserState::DataSectionEntryBodyChunk(_) => self.read_data_chunk()?,
            ParserState::EndDataSectionEntryBody => {
                self.state = ParserState::EndDataSectionEntry;
            }
            ParserState::ElementSectionEntryBody(_) => {
                self.state = ParserState::EndElementSectionEntry;
            }
            ParserState::EndElementSectionEntry => self.read_element_entry()?,
            ParserState::StartSectionEntry(_) => self.position_to_section_end()?,
            ParserState::NameSectionEntry(_) => self.read_name_entry()?,
            ParserState::SourceMappingURL(_) => self.position_to_section_end()?,
            ParserState::RelocSectionHeader(_) => {
                self.section_entries_left = self.reader.read_var_u32()?;
                self.read_reloc_entry()?;
            }
            ParserState::RelocSectionEntry(_) => self.read_reloc_entry()?,
            ParserState::LinkingSectionEntry(_) => self.read_linking_entry()?,
            ParserState::ReadingCustomSection(_) => self.read_custom_section_body()?,
            ParserState::ReadingSectionRawData | ParserState::SectionRawData(_) => {
                self.read_section_body_bytes()?
            }
        }
        Ok(())
    }

    fn skip_section(&mut self) {
        match self.state {
            ParserState::Initial
            | ParserState::EndWasm
            | ParserState::Error(_)
            | ParserState::BeginWasm { .. }
            | ParserState::EndSection => panic!("Invalid reader state during skip section"),
            _ => self.state = ParserState::SkippingSection,
        }
    }

    fn skip_function_body(&mut self) {
        match self.state {
            ParserState::BeginFunctionBody { .. }
            | ParserState::FunctionBodyLocals { .. }
            | ParserState::CodeOperator(_) => self.state = ParserState::SkippingFunctionBody,
            _ => panic!("Invalid reader state during skip function body"),
        }
    }

    fn read_custom_section(&mut self) {
        match self.state {
            ParserState::BeginSection {
                code: SectionCode::Custom { kind, .. },
                ..
            } => {
                self.state = ParserState::ReadingCustomSection(kind);
            }
            _ => panic!("Invalid reader state during reading custom section"),
        }
    }

    fn read_raw_section_data(&mut self) {
        match self.state {
            ParserState::BeginSection { .. } => self.state = ParserState::ReadingSectionRawData,
            _ => panic!("Invalid reader state during reading raw section data"),
        }
    }
}

impl<'a> WasmDecoder<'a> for Parser<'a> {
    /// Reads next record from the WebAssembly binary data. The methods returns
    /// reference to current state of the parser. See `ParserState` num.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::WasmDecoder;
    /// let mut parser = wasmparser::Parser::new(data);
    /// {
    ///     let state = parser.read();
    ///     println!("First state {:?}", state);
    /// }
    /// {
    ///     let state = parser.read();
    ///     println!("Second state {:?}", state);
    /// }
    /// ```
    fn read(&mut self) -> &ParserState<'a> {
        let result = self.read_wrapped();
        if result.is_err() {
            self.state = ParserState::Error(result.err().unwrap());
        }
        &self.state
    }

    fn push_input(&mut self, input: ParserInput) {
        match input {
            ParserInput::Default => (),
            ParserInput::SkipSection => self.skip_section(),
            ParserInput::SkipFunctionBody => self.skip_function_body(),
            ParserInput::ReadCustomSection => self.read_custom_section(),
            ParserInput::ReadSectionRawData => self.read_raw_section_data(),
        }
    }

    /// Creates a BinaryReader when current state is ParserState::BeginSection
    /// or ParserState::BeginFunctionBody.
    ///
    /// # Examples
    /// ```
    /// # let data = &[0x0, 0x61, 0x73, 0x6d, 0x1, 0x0, 0x0, 0x0, 0x1, 0x84,
    /// #              0x80, 0x80, 0x80, 0x0, 0x1, 0x60, 0x0, 0x0, 0x3, 0x83,
    /// #              0x80, 0x80, 0x80, 0x0, 0x2, 0x0, 0x0, 0x6, 0x81, 0x80,
    /// #              0x80, 0x80, 0x0, 0x0, 0xa, 0x91, 0x80, 0x80, 0x80, 0x0,
    /// #              0x2, 0x83, 0x80, 0x80, 0x80, 0x0, 0x0, 0x1, 0xb, 0x83,
    /// #              0x80, 0x80, 0x80, 0x0, 0x0, 0x0, 0xb];
    /// use wasmparser::{WasmDecoder, Parser, ParserState};
    /// let mut parser = Parser::new(data);
    /// let mut function_readers = Vec::new();
    /// loop {
    ///     match *parser.read() {
    ///         ParserState::Error(_) |
    ///         ParserState::EndWasm => break,
    ///         ParserState::BeginFunctionBody {..} => {
    ///             let reader = parser.create_binary_reader();
    ///             function_readers.push(reader);
    ///         }
    ///         _ => continue
    ///     }
    /// }
    /// for (i, reader) in function_readers.iter_mut().enumerate() {
    ///     println!("Function {}", i);
    ///     while let Ok(ref op) = reader.read_operator() {
    ///       println!("  {:?}", op);
    ///     }
    /// }
    /// ```
    fn create_binary_reader<'b>(&mut self) -> BinaryReader<'b>
    where
        'a: 'b,
    {
        let range;
        match self.state {
            ParserState::BeginSection { .. } => {
                range = self.section_range.unwrap();
                self.skip_section();
            }
            ParserState::BeginFunctionBody { .. } | ParserState::FunctionBodyLocals { .. } => {
                range = self.function_range.unwrap();
                self.skip_function_body();
            }
            _ => panic!("Invalid reader state during get binary reader operation"),
        };
        BinaryReader::new(range.slice(self.reader.buffer))
    }

    /// Reads next record from the WebAssembly binary data. It also allows to
    /// control how parser will treat the next record(s). The method accepts the
    /// `ParserInput` parameter that allows e.g. to skip section or function
    /// operators. The methods returns reference to current state of the parser.
    ///
    /// # Examples
    /// ```
    /// # let data: &[u8] = &[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    /// #     0x01, 0x4, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
    /// #     0x0a, 0x05, 0x01, 0x03, 0x00, 0x01, 0x0b];
    /// use wasmparser::WasmDecoder;
    /// let mut parser = wasmparser::Parser::new(data);
    /// let mut next_input = wasmparser::ParserInput::Default;
    /// loop {
    ///     let state = parser.read_with_input(next_input);
    ///     match *state {
    ///         wasmparser::ParserState::EndWasm => break,
    ///         wasmparser::ParserState::BeginWasm { .. } |
    ///         wasmparser::ParserState::EndSection =>
    ///             next_input = wasmparser::ParserInput::Default,
    ///         wasmparser::ParserState::BeginSection { ref code, .. } => {
    ///             println!("Found section: {:?}", code);
    ///             next_input = wasmparser::ParserInput::SkipSection;
    ///         },
    ///         _ => unreachable!()
    ///     }
    /// }
    /// ```
    fn read_with_input(&mut self, input: ParserInput) -> &ParserState<'a> {
        self.push_input(input);
        self.read()
    }

    fn last_state(&self) -> &ParserState<'a> {
        &self.state
    }
}
