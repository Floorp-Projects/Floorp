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

use super::HashSet;
use core::result;
use std::str;
use std::string::String;
use std::vec::Vec;

use crate::limits::{
    MAX_WASM_FUNCTIONS, MAX_WASM_FUNCTION_LOCALS, MAX_WASM_GLOBALS, MAX_WASM_MEMORIES,
    MAX_WASM_MEMORY_PAGES, MAX_WASM_TABLES, MAX_WASM_TYPES,
};

use crate::binary_reader::BinaryReader;

use crate::primitives::{
    BinaryReaderError, ExternalKind, FuncType, GlobalType, ImportSectionEntryType, MemoryType,
    Operator, ResizableLimits, Result, SectionCode, TableType, Type,
};

use crate::parser::{Parser, ParserInput, ParserState, WasmDecoder};

use crate::operators_validator::{
    FunctionEnd, OperatorValidator, OperatorValidatorConfig, WasmModuleResources,
    DEFAULT_OPERATOR_VALIDATOR_CONFIG,
};

use crate::readers::FunctionBody;

type ValidatorResult<'a, T> = result::Result<T, ParserState<'a>>;

struct InitExpressionState {
    ty: Type,
    global_count: usize,
    validated: bool,
}

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq)]
enum SectionOrderState {
    Initial,
    Type,
    Import,
    Function,
    Table,
    Memory,
    Global,
    Export,
    Start,
    Element,
    DataCount,
    Code,
    Data,
}

impl SectionOrderState {
    pub fn from_section_code(code: &SectionCode) -> Option<SectionOrderState> {
        match *code {
            SectionCode::Type => Some(SectionOrderState::Type),
            SectionCode::Import => Some(SectionOrderState::Import),
            SectionCode::Function => Some(SectionOrderState::Function),
            SectionCode::Table => Some(SectionOrderState::Table),
            SectionCode::Memory => Some(SectionOrderState::Memory),
            SectionCode::Global => Some(SectionOrderState::Global),
            SectionCode::Export => Some(SectionOrderState::Export),
            SectionCode::Start => Some(SectionOrderState::Start),
            SectionCode::Element => Some(SectionOrderState::Element),
            SectionCode::Code => Some(SectionOrderState::Code),
            SectionCode::Data => Some(SectionOrderState::Data),
            SectionCode::DataCount => Some(SectionOrderState::DataCount),
            _ => None,
        }
    }
}

#[derive(Copy, Clone)]
pub struct ValidatingParserConfig {
    pub operator_config: OperatorValidatorConfig,
}

const DEFAULT_VALIDATING_PARSER_CONFIG: ValidatingParserConfig = ValidatingParserConfig {
    operator_config: DEFAULT_OPERATOR_VALIDATOR_CONFIG,
};

struct ValidatingParserResources {
    types: Vec<FuncType>,
    tables: Vec<TableType>,
    memories: Vec<MemoryType>,
    globals: Vec<GlobalType>,
    element_count: u32,
    data_count: Option<u32>,
    func_type_indices: Vec<u32>,
}

impl<'a> WasmModuleResources for ValidatingParserResources {
    fn types(&self) -> &[FuncType] {
        &self.types
    }

    fn tables(&self) -> &[TableType] {
        &self.tables
    }

    fn memories(&self) -> &[MemoryType] {
        &self.memories
    }

    fn globals(&self) -> &[GlobalType] {
        &self.globals
    }

    fn func_type_indices(&self) -> &[u32] {
        &self.func_type_indices
    }

    fn element_count(&self) -> u32 {
        self.element_count
    }

    fn data_count(&self) -> u32 {
        self.data_count.unwrap_or(0)
    }
}

pub struct ValidatingParser<'a> {
    parser: Parser<'a>,
    validation_error: Option<ParserState<'a>>,
    read_position: Option<usize>,
    section_order_state: SectionOrderState,
    resources: ValidatingParserResources,
    current_func_index: u32,
    func_imports_count: u32,
    init_expression_state: Option<InitExpressionState>,
    data_found: u32,
    exported_names: HashSet<String>,
    current_operator_validator: Option<OperatorValidator>,
    config: ValidatingParserConfig,
}

impl<'a> ValidatingParser<'a> {
    pub fn new(bytes: &[u8], config: Option<ValidatingParserConfig>) -> ValidatingParser {
        ValidatingParser {
            parser: Parser::new(bytes),
            validation_error: None,
            read_position: None,
            section_order_state: SectionOrderState::Initial,
            resources: ValidatingParserResources {
                types: Vec::new(),
                tables: Vec::new(),
                memories: Vec::new(),
                globals: Vec::new(),
                element_count: 0,
                data_count: None,
                func_type_indices: Vec::new(),
            },
            current_func_index: 0,
            func_imports_count: 0,
            current_operator_validator: None,
            init_expression_state: None,
            data_found: 0,
            exported_names: HashSet::new(),
            config: config.unwrap_or(DEFAULT_VALIDATING_PARSER_CONFIG),
        }
    }

    pub fn get_resources(&self) -> &dyn WasmModuleResources {
        &self.resources
    }

    fn create_validation_error(&self, message: &'static str) -> Option<ParserState<'a>> {
        Some(ParserState::Error(BinaryReaderError {
            message,
            offset: self.read_position.unwrap(),
        }))
    }

    fn create_error<T>(&self, message: &'static str) -> ValidatorResult<'a, T> {
        Err(ParserState::Error(BinaryReaderError {
            message,
            offset: self.read_position.unwrap(),
        }))
    }

    fn check_value_type(&self, ty: Type) -> ValidatorResult<'a, ()> {
        match ty {
            Type::I32 | Type::I64 | Type::F32 | Type::F64 => Ok(()),
            Type::V128 => {
                if !self.config.operator_config.enable_simd {
                    return self.create_error("SIMD support is not enabled");
                }
                Ok(())
            }
            _ => self.create_error("invalid value type"),
        }
    }

    fn check_value_types(&self, types: &[Type]) -> ValidatorResult<'a, ()> {
        for ty in types {
            self.check_value_type(*ty)?;
        }
        Ok(())
    }

    fn check_limits(&self, limits: &ResizableLimits) -> ValidatorResult<'a, ()> {
        if limits.maximum.is_some() && limits.initial > limits.maximum.unwrap() {
            return self.create_error("maximum limits less than initial");
        }
        Ok(())
    }

    fn check_func_type(&self, func_type: &FuncType) -> ValidatorResult<'a, ()> {
        if let Type::Func = func_type.form {
            self.check_value_types(&*func_type.params)?;
            self.check_value_types(&*func_type.returns)?;
            if !self.config.operator_config.enable_multi_value && func_type.returns.len() > 1 {
                self.create_error("func type returns multiple values")
            } else {
                Ok(())
            }
        } else {
            self.create_error("type signature is not a func")
        }
    }

    fn check_table_type(&self, table_type: &TableType) -> ValidatorResult<'a, ()> {
        if let Type::AnyFunc = table_type.element_type {
            self.check_limits(&table_type.limits)
        } else {
            self.create_error("element is not anyfunc")
        }
    }

    fn check_memory_type(&self, memory_type: &MemoryType) -> ValidatorResult<'a, ()> {
        self.check_limits(&memory_type.limits)?;
        let initial = memory_type.limits.initial;
        if initial as usize > MAX_WASM_MEMORY_PAGES {
            return self.create_error("memory initial value exceeds limit");
        }
        let maximum = memory_type.limits.maximum;
        if maximum.is_some() && maximum.unwrap() as usize > MAX_WASM_MEMORY_PAGES {
            return self.create_error("memory maximum value exceeds limit");
        }
        Ok(())
    }

    fn check_global_type(&self, global_type: GlobalType) -> ValidatorResult<'a, ()> {
        self.check_value_type(global_type.content_type)
    }

    fn check_import_entry(&self, import_type: &ImportSectionEntryType) -> ValidatorResult<'a, ()> {
        match *import_type {
            ImportSectionEntryType::Function(type_index) => {
                if self.resources.func_type_indices.len() >= MAX_WASM_FUNCTIONS {
                    return self.create_error("functions count out of bounds");
                }
                if type_index as usize >= self.resources.types.len() {
                    return self.create_error("type index out of bounds");
                }
                Ok(())
            }
            ImportSectionEntryType::Table(ref table_type) => {
                if self.resources.tables.len() >= MAX_WASM_TABLES {
                    return self.create_error("tables count must be at most 1");
                }
                self.check_table_type(table_type)
            }
            ImportSectionEntryType::Memory(ref memory_type) => {
                if self.resources.memories.len() >= MAX_WASM_MEMORIES {
                    return self.create_error("memory count must be at most 1");
                }
                self.check_memory_type(memory_type)
            }
            ImportSectionEntryType::Global(global_type) => {
                if self.resources.globals.len() >= MAX_WASM_GLOBALS {
                    return self.create_error("functions count out of bounds");
                }
                self.check_global_type(global_type)
            }
        }
    }

    fn check_init_expression_operator(&self, operator: &Operator) -> ValidatorResult<'a, ()> {
        let state = self.init_expression_state.as_ref().unwrap();
        if state.validated {
            return self.create_error("only one init_expr operator is expected");
        }
        let ty = match *operator {
            Operator::I32Const { .. } => Type::I32,
            Operator::I64Const { .. } => Type::I64,
            Operator::F32Const { .. } => Type::F32,
            Operator::F64Const { .. } => Type::F64,
            Operator::V128Const { .. } => {
                if !self.config.operator_config.enable_simd {
                    return self.create_error("SIMD support is not enabled");
                }
                Type::V128
            }
            Operator::GetGlobal { global_index } => {
                if global_index as usize >= state.global_count {
                    return self.create_error("init_expr global index out of bounds");
                }
                self.resources.globals[global_index as usize].content_type
            }
            _ => return self.create_error("invalid init_expr operator"),
        };
        if ty != state.ty {
            return self.create_error("invalid init_expr type");
        }
        Ok(())
    }

    fn check_export_entry(
        &self,
        field: &str,
        kind: ExternalKind,
        index: u32,
    ) -> ValidatorResult<'a, ()> {
        if self.exported_names.contains(field) {
            return self.create_error("non-unique export name");
        }
        match kind {
            ExternalKind::Function => {
                if index as usize >= self.resources.func_type_indices.len() {
                    return self.create_error("exported function index out of bounds");
                }
            }
            ExternalKind::Table => {
                if index as usize >= self.resources.tables.len() {
                    return self.create_error("exported table index out of bounds");
                }
            }
            ExternalKind::Memory => {
                if index as usize >= self.resources.memories.len() {
                    return self.create_error("exported memory index out of bounds");
                }
            }
            ExternalKind::Global => {
                if index as usize >= self.resources.globals.len() {
                    return self.create_error("exported global index out of bounds");
                }
            }
        };
        Ok(())
    }

    fn check_start(&self, func_index: u32) -> ValidatorResult<'a, ()> {
        if func_index as usize >= self.resources.func_type_indices.len() {
            return self.create_error("start function index out of bounds");
        }
        let type_index = self.resources.func_type_indices[func_index as usize];
        let ty = &self.resources.types[type_index as usize];
        if !ty.params.is_empty() || !ty.returns.is_empty() {
            return self.create_error("invlid start function type");
        }
        Ok(())
    }

    fn process_begin_section(&self, code: &SectionCode) -> ValidatorResult<'a, SectionOrderState> {
        let order_state = SectionOrderState::from_section_code(code);
        Ok(match self.section_order_state {
            SectionOrderState::Initial => {
                if order_state.is_none() {
                    SectionOrderState::Initial
                } else {
                    order_state.unwrap()
                }
            }
            previous => {
                if let Some(order_state_unwraped) = order_state {
                    if previous >= order_state_unwraped {
                        return self.create_error("section out of order");
                    }
                    order_state_unwraped
                } else {
                    previous
                }
            }
        })
    }

    fn process_state(&mut self) {
        match *self.parser.last_state() {
            ParserState::BeginWasm { version } => {
                if version != 1 {
                    self.validation_error = self.create_validation_error("bad wasm file version");
                }
            }
            ParserState::BeginSection { ref code, .. } => {
                let check = self.process_begin_section(code);
                if check.is_err() {
                    self.validation_error = check.err();
                } else {
                    self.section_order_state = check.ok().unwrap();
                }
            }
            ParserState::TypeSectionEntry(ref func_type) => {
                let check = self.check_func_type(func_type);
                if check.is_err() {
                    self.validation_error = check.err();
                } else if self.resources.types.len() > MAX_WASM_TYPES {
                    self.validation_error =
                        self.create_validation_error("types count is out of bounds");
                } else {
                    self.resources.types.push(func_type.clone());
                }
            }
            ParserState::ImportSectionEntry { ref ty, .. } => {
                let check = self.check_import_entry(ty);
                if check.is_err() {
                    self.validation_error = check.err();
                } else {
                    match *ty {
                        ImportSectionEntryType::Function(type_index) => {
                            self.func_imports_count += 1;
                            self.resources.func_type_indices.push(type_index);
                        }
                        ImportSectionEntryType::Table(ref table_type) => {
                            self.resources.tables.push(table_type.clone());
                        }
                        ImportSectionEntryType::Memory(ref memory_type) => {
                            self.resources.memories.push(memory_type.clone());
                        }
                        ImportSectionEntryType::Global(ref global_type) => {
                            self.resources.globals.push(global_type.clone());
                        }
                    }
                }
            }
            ParserState::FunctionSectionEntry(type_index) => {
                if type_index as usize >= self.resources.types.len() {
                    self.validation_error =
                        self.create_validation_error("func type index out of bounds");
                } else if self.resources.func_type_indices.len() >= MAX_WASM_FUNCTIONS {
                    self.validation_error =
                        self.create_validation_error("functions count out of bounds");
                } else {
                    self.resources.func_type_indices.push(type_index);
                }
            }
            ParserState::TableSectionEntry(ref table_type) => {
                if self.resources.tables.len() >= MAX_WASM_TABLES {
                    self.validation_error =
                        self.create_validation_error("tables count must be at most 1");
                } else {
                    self.validation_error = self.check_table_type(table_type).err();
                    self.resources.tables.push(table_type.clone());
                }
            }
            ParserState::MemorySectionEntry(ref memory_type) => {
                if self.resources.memories.len() >= MAX_WASM_MEMORIES {
                    self.validation_error =
                        self.create_validation_error("memories count must be at most 1");
                } else {
                    self.validation_error = self.check_memory_type(memory_type).err();
                    self.resources.memories.push(memory_type.clone());
                }
            }
            ParserState::BeginGlobalSectionEntry(global_type) => {
                if self.resources.globals.len() >= MAX_WASM_GLOBALS {
                    self.validation_error =
                        self.create_validation_error("globals count out of bounds");
                } else {
                    self.validation_error = self.check_global_type(global_type).err();
                    self.init_expression_state = Some(InitExpressionState {
                        ty: global_type.content_type,
                        global_count: self.resources.globals.len(),
                        validated: false,
                    });
                    self.resources.globals.push(global_type);
                }
            }
            ParserState::BeginInitExpressionBody => {
                assert!(self.init_expression_state.is_some());
            }
            ParserState::InitExpressionOperator(ref operator) => {
                self.validation_error = self.check_init_expression_operator(operator).err();
                self.init_expression_state.as_mut().unwrap().validated = true;
            }
            ParserState::EndInitExpressionBody => {
                if !self.init_expression_state.as_ref().unwrap().validated {
                    self.validation_error = self.create_validation_error("init_expr is empty");
                }
                self.init_expression_state = None;
            }
            ParserState::ExportSectionEntry { field, kind, index } => {
                self.validation_error = self.check_export_entry(field, kind, index).err();
                self.exported_names.insert(String::from(field));
            }
            ParserState::StartSectionEntry(func_index) => {
                self.validation_error = self.check_start(func_index).err();
            }
            ParserState::DataCountSectionEntry(count) => {
                self.resources.data_count = Some(count);
            }
            ParserState::BeginPassiveElementSectionEntry(_ty) => {
                self.resources.element_count += 1;
            }
            ParserState::BeginActiveElementSectionEntry(table_index) => {
                self.resources.element_count += 1;
                if table_index as usize >= self.resources.tables.len() {
                    self.validation_error =
                        self.create_validation_error("element section table index out of bounds");
                } else {
                    assert!(
                        self.resources.tables[table_index as usize].element_type == Type::AnyFunc
                    );
                    self.init_expression_state = Some(InitExpressionState {
                        ty: Type::I32,
                        global_count: self.resources.globals.len(),
                        validated: false,
                    });
                }
            }
            ParserState::ElementSectionEntryBody(ref indices) => {
                for func_index in &**indices {
                    if *func_index as usize >= self.resources.func_type_indices.len() {
                        self.validation_error =
                            self.create_validation_error("element func index out of bounds");
                        break;
                    }
                }
            }
            ParserState::BeginFunctionBody { .. } => {
                let index = (self.current_func_index + self.func_imports_count) as usize;
                if index as usize >= self.resources.func_type_indices.len() {
                    self.validation_error =
                        self.create_validation_error("func type is not defined");
                }
            }
            ParserState::FunctionBodyLocals { ref locals } => {
                let index = (self.current_func_index + self.func_imports_count) as usize;
                let func_type =
                    &self.resources.types[self.resources.func_type_indices[index] as usize];
                let operator_config = self.config.operator_config;
                self.current_operator_validator =
                    Some(OperatorValidator::new(func_type, locals, operator_config));
            }
            ParserState::CodeOperator(ref operator) => {
                let check = self
                    .current_operator_validator
                    .as_mut()
                    .unwrap()
                    .process_operator(operator, &self.resources);
                match check {
                    Ok(_) => (),
                    Err(err) => {
                        self.validation_error = self.create_validation_error(err);
                    }
                }
            }
            ParserState::EndFunctionBody => {
                let check = self
                    .current_operator_validator
                    .as_ref()
                    .unwrap()
                    .process_end_function();
                if check.is_err() {
                    self.validation_error = self.create_validation_error(check.err().unwrap());
                }
                self.current_func_index += 1;
                self.current_operator_validator = None;
            }
            ParserState::BeginDataSectionEntryBody(_) => {
                self.data_found += 1;
            }
            ParserState::BeginActiveDataSectionEntry(memory_index) => {
                if memory_index as usize >= self.resources.memories.len() {
                    self.validation_error =
                        self.create_validation_error("data section memory index out of bounds");
                } else {
                    self.init_expression_state = Some(InitExpressionState {
                        ty: Type::I32,
                        global_count: self.resources.globals.len(),
                        validated: false,
                    });
                }
            }
            ParserState::EndWasm => {
                if self.resources.func_type_indices.len()
                    != self.current_func_index as usize + self.func_imports_count as usize
                {
                    self.validation_error = self.create_validation_error(
                        "function and code section have inconsistent lengths",
                    );
                }
                if let Some(data_count) = self.resources.data_count {
                    if data_count != self.data_found {
                        self.validation_error = self.create_validation_error(
                            "data count section and passive data mismatch",
                        );
                    }
                }
            }
            _ => (),
        };
    }

    pub fn create_validating_operator_parser<'b>(&mut self) -> ValidatingOperatorParser<'b>
    where
        'a: 'b,
    {
        let func_body_offset = match *self.last_state() {
            ParserState::BeginFunctionBody { .. } => self.parser.current_position(),
            _ => panic!("Invalid reader state"),
        };
        self.read();
        let operator_validator = match *self.last_state() {
            ParserState::FunctionBodyLocals { ref locals } => {
                let index = (self.current_func_index + self.func_imports_count) as usize;
                let func_type =
                    &self.resources.types[self.resources.func_type_indices[index] as usize];
                let operator_config = self.config.operator_config;
                OperatorValidator::new(func_type, locals, operator_config)
            }
            _ => panic!("Invalid reader state"),
        };
        let reader = self.create_binary_reader();
        ValidatingOperatorParser::new(operator_validator, reader, func_body_offset)
    }
}

impl<'a> WasmDecoder<'a> for ValidatingParser<'a> {
    fn read(&mut self) -> &ParserState<'a> {
        if self.validation_error.is_some() {
            panic!("Parser in error state: validation");
        }
        self.read_position = Some(self.parser.current_position());
        self.parser.read();
        self.process_state();
        self.last_state()
    }

    fn push_input(&mut self, input: ParserInput) {
        match input {
            ParserInput::SkipSection => panic!("Not supported"),
            ParserInput::ReadSectionRawData => panic!("Not supported"),
            ParserInput::SkipFunctionBody => {
                self.current_func_index += 1;
                self.parser.push_input(input);
            }
            _ => self.parser.push_input(input),
        }
    }

    fn read_with_input(&mut self, input: ParserInput) -> &ParserState<'a> {
        self.push_input(input);
        self.read()
    }

    fn create_binary_reader<'b>(&mut self) -> BinaryReader<'b>
    where
        'a: 'b,
    {
        if let ParserState::BeginSection { .. } = *self.parser.last_state() {
            panic!("Not supported");
        }
        self.parser.create_binary_reader()
    }

    fn last_state(&self) -> &ParserState<'a> {
        if self.validation_error.is_some() {
            self.validation_error.as_ref().unwrap()
        } else {
            self.parser.last_state()
        }
    }
}

pub struct ValidatingOperatorParser<'b> {
    operator_validator: OperatorValidator,
    reader: BinaryReader<'b>,
    func_body_offset: usize,
    end_function: bool,
}

impl<'b> ValidatingOperatorParser<'b> {
    pub(crate) fn new<'c>(
        operator_validator: OperatorValidator,
        reader: BinaryReader<'c>,
        func_body_offset: usize,
    ) -> ValidatingOperatorParser<'c>
    where
        'b: 'c,
    {
        ValidatingOperatorParser {
            operator_validator,
            reader,
            func_body_offset,
            end_function: false,
        }
    }

    pub fn eof(&self) -> bool {
        self.end_function
    }

    pub fn current_position(&self) -> usize {
        self.reader.current_position()
    }

    pub fn is_dead_code(&self) -> bool {
        self.operator_validator.is_dead_code()
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
    /// use wasmparser::{WasmDecoder, ParserState, ValidatingParser};
    /// let mut parser = ValidatingParser::new(data, None);
    /// let mut i = 0;
    /// loop {
    ///     {
    ///         match *parser.read() {
    ///             ParserState::Error(_) |
    ///             ParserState::EndWasm => break,
    ///             ParserState::BeginFunctionBody {..} => (),
    ///             _ => continue
    ///         }
    ///     }
    ///     let mut reader = parser.create_validating_operator_parser();
    ///     println!("Function {}", i);
    ///     i += 1;
    ///     while !reader.eof() {
    ///       let read = reader.next(parser.get_resources());
    ///       if let Ok(ref op) = read {
    ///           println!("  {:?}", op);
    ///       } else {
    ///           panic!("  Bad wasm code {:?}", read.err());
    ///       }
    ///     }
    /// }
    /// ```
    pub fn next<'c>(&mut self, resources: &dyn WasmModuleResources) -> Result<Operator<'c>>
    where
        'b: 'c,
    {
        let op = self.reader.read_operator()?;
        match self.operator_validator.process_operator(&op, resources) {
            Err(err) => {
                return Err(BinaryReaderError {
                    message: err,
                    offset: self.func_body_offset + self.reader.current_position(),
                });
            }
            Ok(FunctionEnd::Yes) => {
                self.end_function = true;
                if !self.reader.eof() {
                    return Err(BinaryReaderError {
                        message: "unexpected end of function",
                        offset: self.func_body_offset + self.reader.current_position(),
                    });
                }
            }
            _ => (),
        };
        Ok(op)
    }
}

/// Test whether the given buffer contains a valid WebAssembly function.
/// The resources parameter contains all needed data to validate the operators.
pub fn validate_function_body(
    bytes: &[u8],
    func_index: u32,
    resources: &dyn WasmModuleResources,
    operator_config: Option<OperatorValidatorConfig>,
) -> bool {
    let operator_config = operator_config.unwrap_or(DEFAULT_OPERATOR_VALIDATOR_CONFIG);
    let function_body = FunctionBody::new(0, bytes);
    let mut locals_reader = if let Ok(r) = function_body.get_locals_reader() {
        r
    } else {
        return false;
    };
    let local_count = locals_reader.get_count() as usize;
    if local_count > MAX_WASM_FUNCTION_LOCALS {
        return false;
    }
    let mut locals: Vec<(u32, Type)> = Vec::with_capacity(local_count);
    let mut locals_total: usize = 0;
    for _ in 0..local_count {
        let (count, ty) = if let Ok(r) = locals_reader.read() {
            r
        } else {
            return false;
        };
        locals_total = if let Some(r) = locals_total.checked_add(count as usize) {
            r
        } else {
            return false;
        };
        if locals_total > MAX_WASM_FUNCTION_LOCALS {
            return false;
        }
        locals.push((count, ty));
    }
    let operators_reader = if let Ok(r) = function_body.get_operators_reader() {
        r
    } else {
        return false;
    };
    let func_type_index = resources.func_type_indices()[func_index as usize];
    let func_type = &resources.types()[func_type_index as usize];
    let mut operator_validator = OperatorValidator::new(func_type, &locals, operator_config);
    let mut eof_found = false;
    for op in operators_reader.into_iter() {
        let op = match op {
            Err(_) => return false,
            Ok(ref op) => op,
        };
        match operator_validator.process_operator(op, resources) {
            Err(_) => return false,
            Ok(FunctionEnd::Yes) => {
                eof_found = true;
            }
            Ok(FunctionEnd::No) => (),
        }
    }
    eof_found
}

/// Test whether the given buffer contains a valid WebAssembly module,
/// analogous to WebAssembly.validate in the JS API.
pub fn validate(bytes: &[u8], config: Option<ValidatingParserConfig>) -> bool {
    let mut parser = ValidatingParser::new(bytes, config);
    let mut parser_input = None;
    let mut func_ranges = Vec::new();
    loop {
        let next_input = parser_input.take().unwrap_or(ParserInput::Default);
        let state = parser.read_with_input(next_input);
        match *state {
            ParserState::EndWasm => break,
            ParserState::Error(_) => return false,
            ParserState::BeginFunctionBody { range } => {
                parser_input = Some(ParserInput::SkipFunctionBody);
                func_ranges.push(range);
            }
            _ => (),
        }
    }
    let operator_config = config.map(|c| c.operator_config);
    for (i, range) in func_ranges.into_iter().enumerate() {
        let function_body = range.slice(bytes);
        let function_index = i as u32 + parser.func_imports_count;
        if !validate_function_body(
            function_body,
            function_index,
            &parser.resources,
            operator_config,
        ) {
            return false;
        }
    }
    true
}

#[test]
fn test_validate() {
    assert!(validate(&[0x0, 0x61, 0x73, 0x6d, 0x1, 0x0, 0x0, 0x0], None));
    assert!(!validate(
        &[0x0, 0x61, 0x73, 0x6d, 0x2, 0x0, 0x0, 0x0],
        None
    ));
}
