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

use std::collections::HashSet;
use std::result;
use std::str;

use crate::limits::{
    MAX_WASM_FUNCTIONS, MAX_WASM_FUNCTION_LOCALS, MAX_WASM_GLOBALS, MAX_WASM_MEMORIES,
    MAX_WASM_MEMORY_PAGES, MAX_WASM_TABLES, MAX_WASM_TYPES,
};

use crate::binary_reader::BinaryReader;

use crate::primitives::{
    BinaryReaderError, ExternalKind, FuncType, GlobalType, ImportSectionEntryType, MemoryType,
    Operator, ResizableLimits, Result, SectionCode, TableType, Type,
};

use crate::operators_validator::{
    check_value_type, is_subtype_supertype, FunctionEnd, OperatorValidator,
    OperatorValidatorConfig, OperatorValidatorError, DEFAULT_OPERATOR_VALIDATOR_CONFIG,
};
use crate::parser::{Parser, ParserInput, ParserState, WasmDecoder};
use crate::{ElemSectionEntryTable, ElementItem};
use crate::{WasmFuncType, WasmGlobalType, WasmMemoryType, WasmModuleResources, WasmTableType};

use crate::readers::FunctionBody;

type ValidatorResult<'a, T> = result::Result<T, ParserState<'a>>;

struct InitExpressionState {
    ty: Type,
    global_count: usize,
    function_count: usize,
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
    type FuncType = crate::FuncType;
    type TableType = crate::TableType;
    type MemoryType = crate::MemoryType;
    type GlobalType = crate::GlobalType;

    fn type_at(&self, at: u32) -> Option<&Self::FuncType> {
        self.types.get(at as usize)
    }

    fn table_at(&self, at: u32) -> Option<&Self::TableType> {
        self.tables.get(at as usize)
    }

    fn memory_at(&self, at: u32) -> Option<&Self::MemoryType> {
        self.memories.get(at as usize)
    }

    fn global_at(&self, at: u32) -> Option<&Self::GlobalType> {
        self.globals.get(at as usize)
    }

    fn func_type_id_at(&self, at: u32) -> Option<u32> {
        self.func_type_indices.get(at as usize).copied()
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

    pub fn get_resources(
        &self,
    ) -> &dyn WasmModuleResources<
        FuncType = crate::FuncType,
        TableType = crate::TableType,
        MemoryType = crate::MemoryType,
        GlobalType = crate::GlobalType,
    > {
        &self.resources
    }

    fn set_validation_error(&mut self, message: impl Into<String>) {
        self.validation_error = Some(ParserState::Error(BinaryReaderError::new(
            message,
            self.read_position.unwrap(),
        )))
    }

    fn set_operator_validation_error(&mut self, e: OperatorValidatorError) {
        let offset = self.read_position.unwrap();
        self.validation_error = Some(ParserState::Error(e.set_offset(offset)));
    }

    fn create_error<T>(&self, message: impl Into<String>) -> ValidatorResult<'a, T> {
        Err(ParserState::Error(BinaryReaderError::new(
            message,
            self.read_position.unwrap(),
        )))
    }

    fn check_value_type(&self, ty: Type) -> ValidatorResult<'a, ()> {
        check_value_type(ty, &self.config.operator_config).map_err(|e| {
            let offset = self.read_position.unwrap();
            ParserState::Error(e.set_offset(offset))
        })
    }

    fn check_value_types(&self, types: &[Type]) -> ValidatorResult<'a, ()> {
        for ty in types {
            self.check_value_type(*ty)?;
        }
        Ok(())
    }

    fn check_limits(&self, limits: &ResizableLimits) -> ValidatorResult<'a, ()> {
        if limits.maximum.is_some() && limits.initial > limits.maximum.unwrap() {
            return self.create_error("size minimum must not be greater than maximum");
        }
        Ok(())
    }

    fn check_func_type(&self, func_type: &FuncType) -> ValidatorResult<'a, ()> {
        if let Type::Func = func_type.form {
            self.check_value_types(&*func_type.params)?;
            self.check_value_types(&*func_type.returns)?;
            if !self.config.operator_config.enable_multi_value && func_type.returns.len() > 1 {
                self.create_error("invalid result arity: func type returns multiple values")
            } else {
                Ok(())
            }
        } else {
            self.create_error("type signature is not a func")
        }
    }

    fn check_table_type(&self, table_type: &TableType) -> ValidatorResult<'a, ()> {
        match table_type.element_type {
            Type::AnyFunc => {}
            _ => {
                if !self.config.operator_config.enable_reference_types {
                    return self.create_error("element is not anyfunc");
                }
            }
        }
        self.check_limits(&table_type.limits)
    }

    fn check_memory_type(&self, memory_type: &MemoryType) -> ValidatorResult<'a, ()> {
        self.check_limits(&memory_type.limits)?;
        let initial = memory_type.limits.initial;
        if initial as usize > MAX_WASM_MEMORY_PAGES {
            return self.create_error("memory size must be at most 65536 pages (4GiB)");
        }
        let maximum = memory_type.limits.maximum;
        if maximum.is_some() && maximum.unwrap() as usize > MAX_WASM_MEMORY_PAGES {
            return self.create_error("memory size must be at most 65536 pages (4GiB)");
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
                    return self.create_error("unknown type: type index out of bounds");
                }
                Ok(())
            }
            ImportSectionEntryType::Table(ref table_type) => {
                if !self.config.operator_config.enable_reference_types
                    && self.resources.tables.len() >= MAX_WASM_TABLES
                {
                    return self.create_error("multiple tables: tables count must be at most 1");
                }
                self.check_table_type(table_type)
            }
            ImportSectionEntryType::Memory(ref memory_type) => {
                if self.resources.memories.len() >= MAX_WASM_MEMORIES {
                    return self.create_error("multiple memories: memory count must be at most 1");
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
            return self.create_error(
                "constant expression required: type mismatch: only one init_expr operator is expected",
            );
        }
        let ty = match *operator {
            Operator::I32Const { .. } => Type::I32,
            Operator::I64Const { .. } => Type::I64,
            Operator::F32Const { .. } => Type::F32,
            Operator::F64Const { .. } => Type::F64,
            Operator::RefNull => {
                if !self.config.operator_config.enable_reference_types {
                    return self.create_error("reference types support is not enabled");
                }
                Type::NullRef
            }
            Operator::V128Const { .. } => {
                if !self.config.operator_config.enable_simd {
                    return self.create_error("SIMD support is not enabled");
                }
                Type::V128
            }
            Operator::GlobalGet { global_index } => {
                if global_index as usize >= state.global_count {
                    return self
                        .create_error("unknown global: init_expr global index out of bounds");
                }
                self.resources.globals[global_index as usize].content_type
            }
            Operator::RefFunc { function_index } => {
                if function_index as usize >= state.function_count {
                    return self.create_error(format!(
                        "unknown function {}: init_expr function index out of bounds",
                        function_index
                    ));
                }
                Type::AnyFunc
            }
            _ => {
                return self
                    .create_error("constant expression required: invalid init_expr operator")
            }
        };
        if !is_subtype_supertype(ty, state.ty) {
            return self.create_error("type mismatch: invalid init_expr type");
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
            return self.create_error("duplicate export name");
        }
        match kind {
            ExternalKind::Function => {
                if index as usize >= self.resources.func_type_indices.len() {
                    return self
                        .create_error("unknown function: exported function index out of bounds");
                }
            }
            ExternalKind::Table => {
                if index as usize >= self.resources.tables.len() {
                    return self.create_error("unknown table: exported table index out of bounds");
                }
            }
            ExternalKind::Memory => {
                if index as usize >= self.resources.memories.len() {
                    return self
                        .create_error("unknown memory: exported memory index out of bounds");
                }
            }
            ExternalKind::Global => {
                if index as usize >= self.resources.globals.len() {
                    return self
                        .create_error("unknown global: exported global index out of bounds");
                }
            }
        };
        Ok(())
    }

    fn check_start(&self, func_index: u32) -> ValidatorResult<'a, ()> {
        if func_index as usize >= self.resources.func_type_indices.len() {
            return self.create_error("unknown function: start function index out of bounds");
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
            SectionOrderState::Initial => match order_state {
                Some(section) => section,
                _ => SectionOrderState::Initial,
            },
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
                    self.set_validation_error("bad wasm file version");
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
                    self.set_validation_error("types count is out of bounds");
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
                    self.set_validation_error("unknown type: func type index out of bounds");
                } else if self.resources.func_type_indices.len() >= MAX_WASM_FUNCTIONS {
                    self.set_validation_error("functions count out of bounds");
                } else {
                    self.resources.func_type_indices.push(type_index);
                }
            }
            ParserState::TableSectionEntry(ref table_type) => {
                if !self.config.operator_config.enable_reference_types
                    && self.resources.tables.len() >= MAX_WASM_TABLES
                {
                    self.set_validation_error("multiple tables: tables count must be at most 1");
                } else {
                    self.validation_error = self.check_table_type(table_type).err();
                    self.resources.tables.push(table_type.clone());
                }
            }
            ParserState::MemorySectionEntry(ref memory_type) => {
                if self.resources.memories.len() >= MAX_WASM_MEMORIES {
                    self.set_validation_error(
                        "multiple memories: memories count must be at most 1",
                    );
                } else {
                    self.validation_error = self.check_memory_type(memory_type).err();
                    self.resources.memories.push(memory_type.clone());
                }
            }
            ParserState::BeginGlobalSectionEntry(global_type) => {
                if self.resources.globals.len() >= MAX_WASM_GLOBALS {
                    self.set_validation_error("globals count out of bounds");
                } else {
                    self.validation_error = self.check_global_type(global_type).err();
                    self.init_expression_state = Some(InitExpressionState {
                        ty: global_type.content_type,
                        global_count: self.resources.globals.len(),
                        function_count: self.resources.func_type_indices.len(),
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
                    self.set_validation_error("type mismatch: init_expr is empty");
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
            ParserState::BeginElementSectionEntry { table, ty } => {
                self.resources.element_count += 1;
                if let ElemSectionEntryTable::Active(table_index) = table {
                    let table = match self.resources.tables.get(table_index as usize) {
                        Some(t) => t,
                        None => {
                            self.set_validation_error(
                                "unknown table: element section table index out of bounds",
                            );
                            return;
                        }
                    };
                    if !is_subtype_supertype(ty, table.element_type) {
                        self.set_validation_error("element_type != table type");
                        return;
                    }
                    if !self.config.operator_config.enable_reference_types && ty != Type::AnyFunc {
                        self.set_validation_error("element_type != anyfunc is not supported yet");
                        return;
                    }
                    self.init_expression_state = Some(InitExpressionState {
                        ty: Type::I32,
                        global_count: self.resources.globals.len(),
                        function_count: self.resources.func_type_indices.len(),
                        validated: false,
                    });
                }
            }
            ParserState::ElementSectionEntryBody(ref indices) => {
                for item in &**indices {
                    if let ElementItem::Func(func_index) = item {
                        if *func_index as usize >= self.resources.func_type_indices.len() {
                            self.set_validation_error(
                                "unknown function: element func index out of bounds",
                            );
                            break;
                        }
                    }
                }
            }
            ParserState::BeginFunctionBody { .. } => {
                let index = (self.current_func_index + self.func_imports_count) as usize;
                if index as usize >= self.resources.func_type_indices.len() {
                    self.set_validation_error("func type is not defined");
                }
            }
            ParserState::FunctionBodyLocals { ref locals } => {
                let index = (self.current_func_index + self.func_imports_count) as usize;
                let func_type =
                    &self.resources.types[self.resources.func_type_indices[index] as usize];
                let operator_config = self.config.operator_config;
                match OperatorValidator::new(func_type, locals, operator_config) {
                    Ok(validator) => self.current_operator_validator = Some(validator),
                    Err(err) => {
                        self.validation_error = Some(ParserState::Error(
                            err.set_offset(self.read_position.unwrap()),
                        ));
                    }
                }
            }
            ParserState::CodeOperator(ref operator) => {
                let check = self
                    .current_operator_validator
                    .as_mut()
                    .unwrap()
                    .process_operator(operator, &self.resources);

                if let Err(err) = check {
                    self.set_operator_validation_error(err);
                }
            }
            ParserState::EndFunctionBody => {
                let check = self
                    .current_operator_validator
                    .as_ref()
                    .unwrap()
                    .process_end_function();
                if let Err(err) = check {
                    self.set_operator_validation_error(err);
                }
                self.current_func_index += 1;
                self.current_operator_validator = None;
            }
            ParserState::BeginDataSectionEntryBody(_) => {
                self.data_found += 1;
            }
            ParserState::BeginActiveDataSectionEntry(memory_index) => {
                if memory_index as usize >= self.resources.memories.len() {
                    self.set_validation_error(
                        "unknown memory: data section memory index out of bounds",
                    );
                } else {
                    self.init_expression_state = Some(InitExpressionState {
                        ty: Type::I32,
                        global_count: self.resources.globals.len(),
                        function_count: self.resources.func_type_indices.len(),
                        validated: false,
                    });
                }
            }
            ParserState::EndWasm => {
                if self.resources.func_type_indices.len()
                    != self.current_func_index as usize + self.func_imports_count as usize
                {
                    self.set_validation_error(
                        "function and code section have inconsistent lengths",
                    );
                }
                if let Some(data_count) = self.resources.data_count {
                    if data_count != self.data_found {
                        self.set_validation_error("data count section and passive data mismatch");
                    }
                }
            }
            _ => (),
        };
    }

    pub fn create_validating_operator_parser<'b>(
        &mut self,
    ) -> ValidatorResult<ValidatingOperatorParser<'b>>
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
                    .map_err(|e| ParserState::Error(e.set_offset(self.read_position.unwrap())))?
            }
            _ => panic!("Invalid reader state"),
        };
        let reader = self.create_binary_reader();
        Ok(ValidatingOperatorParser::new(
            operator_validator,
            reader,
            func_body_offset,
        ))
    }

    pub fn current_position(&self) -> usize {
        self.parser.current_position()
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
    ///     let mut reader = parser
    ///         .create_validating_operator_parser()
    ///         .expect("validating parser");
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
    pub fn next<'c, F: WasmFuncType, T: WasmTableType, M: WasmMemoryType, G: WasmGlobalType>(
        &mut self,
        resources: &dyn WasmModuleResources<
            FuncType = F,
            TableType = T,
            MemoryType = M,
            GlobalType = G,
        >,
    ) -> Result<Operator<'c>>
    where
        'b: 'c,
    {
        let op = self.reader.read_operator()?;
        match self.operator_validator.process_operator(&op, resources) {
            Err(err) => {
                let offset = self.func_body_offset + self.reader.current_position();
                return Err(err.set_offset(offset));
            }
            Ok(FunctionEnd::Yes) => {
                self.end_function = true;
                if !self.reader.eof() {
                    return Err(BinaryReaderError::new(
                        "unexpected end of function",
                        self.func_body_offset + self.reader.current_position(),
                    ));
                }
            }
            _ => (),
        };
        Ok(op)
    }
}

/// Test whether the given buffer contains a valid WebAssembly function.
/// The resources parameter contains all needed data to validate the operators.
pub fn validate_function_body<
    F: WasmFuncType,
    T: WasmTableType,
    M: WasmMemoryType,
    G: WasmGlobalType,
>(
    bytes: &[u8],
    offset: usize,
    func_index: u32,
    resources: &dyn WasmModuleResources<
        FuncType = F,
        TableType = T,
        MemoryType = M,
        GlobalType = G,
    >,
    operator_config: Option<OperatorValidatorConfig>,
) -> Result<()> {
    let operator_config = operator_config.unwrap_or(DEFAULT_OPERATOR_VALIDATOR_CONFIG);
    let function_body = FunctionBody::new(offset, bytes);
    let mut locals_reader = function_body.get_locals_reader()?;
    let local_count = locals_reader.get_count() as usize;
    if local_count > MAX_WASM_FUNCTION_LOCALS {
        return Err(BinaryReaderError::new(
            "locals exceed maximum",
            locals_reader.original_position(),
        ));
    }
    let mut locals: Vec<(u32, Type)> = Vec::with_capacity(local_count);
    let mut locals_total: usize = 0;
    for _ in 0..local_count {
        let (count, ty) = locals_reader.read()?;
        locals_total = locals_total.checked_add(count as usize).ok_or_else(|| {
            BinaryReaderError::new("locals overflow", locals_reader.original_position())
        })?;
        if locals_total > MAX_WASM_FUNCTION_LOCALS {
            return Err(BinaryReaderError::new(
                "locals exceed maximum",
                locals_reader.original_position(),
            ));
        }
        locals.push((count, ty));
    }
    let operators_reader = function_body.get_operators_reader()?;
    let func_type_index = resources
        .func_type_id_at(func_index)
        // Note: This was an out-of-bounds access before the change to return `Option`
        // so I assumed it is considered a bug to access a non-existing function
        // id here and went with panicking instead of returning a proper error.
        .expect("the function index of the validated function itself is out of bounds");
    let func_type = resources
        .type_at(func_type_index)
        // Note: This was an out-of-bounds access before the change to return `Option`
        // so I assumed it is considered a bug to access a non-existing function
        // id here and went with panicking instead of returning a proper error.
        .expect("the function type indexof the validated function itself is out of bounds");
    let mut operator_validator = OperatorValidator::new(func_type, &locals, operator_config)
        .map_err(|e| e.set_offset(offset))?;
    let mut eof_found = false;
    let mut last_op = 0;
    for item in operators_reader.into_iter_with_offsets() {
        let (ref op, offset) = item?;
        match operator_validator
            .process_operator(op, resources)
            .map_err(|e| e.set_offset(offset))?
        {
            FunctionEnd::Yes => {
                eof_found = true;
            }
            FunctionEnd::No => {
                last_op = offset;
            }
        }
    }
    if !eof_found {
        return Err(BinaryReaderError::new("end of function not found", last_op));
    }
    Ok(())
}

/// Test whether the given buffer contains a valid WebAssembly module,
/// analogous to WebAssembly.validate in the JS API.
pub fn validate(bytes: &[u8], config: Option<ValidatingParserConfig>) -> Result<()> {
    let mut parser = ValidatingParser::new(bytes, config);
    let mut parser_input = None;
    let mut func_ranges = Vec::new();
    loop {
        let next_input = parser_input.take().unwrap_or(ParserInput::Default);
        let state = parser.read_with_input(next_input);
        match *state {
            ParserState::EndWasm => break,
            ParserState::Error(ref e) => return Err(e.clone()),
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
        validate_function_body(
            function_body,
            range.start,
            function_index,
            &parser.resources,
            operator_config,
        )?;
    }
    Ok(())
}

#[test]
fn test_validate() {
    assert!(validate(&[0x0, 0x61, 0x73, 0x6d, 0x1, 0x0, 0x0, 0x0], None).is_ok());
    assert!(validate(&[0x0, 0x61, 0x73, 0x6d, 0x2, 0x0, 0x0, 0x0], None).is_err());
}
