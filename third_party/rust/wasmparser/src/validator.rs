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

use std::collections::{HashMap, HashSet};
use std::mem;
use std::result;
use std::str;

use crate::limits::*;

use crate::binary_reader::BinaryReader;

use crate::primitives::{
    BinaryReaderError, ExportType, ExternalKind, FuncType, GlobalType, ImportSectionEntryType,
    InstanceType, MemoryType, ModuleType, Operator, ResizableLimits, Result, SectionCode,
    TableType, Type, TypeDef,
};

use crate::operators_validator::{
    check_value_type, FunctionEnd, OperatorValidator, OperatorValidatorConfig,
    OperatorValidatorError, DEFAULT_OPERATOR_VALIDATOR_CONFIG,
};
use crate::parser::{Parser, ParserInput, ParserState, WasmDecoder};
use crate::{AliasedInstance, WasmModuleResources};
use crate::{ElemSectionEntryTable, ElementItem};

use crate::readers::FunctionBody;

type ValidatorResult<'a, T> = result::Result<T, ParserState<'a>>;

struct InitExpressionState {
    ty: Type,
    global_count: usize,
    function_count: usize,
    validated: bool,
}

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
enum SectionOrderState {
    Initial,
    Type,
    Import,
    ModuleLinkingHeader,
    Function,
    Table,
    Memory,
    Global,
    Export,
    Start,
    Element,
    DataCount,
    ModuleCode,
    Code,
    Data,
}

impl SectionOrderState {
    pub fn from_section_code(
        code: &SectionCode,
        config: &ValidatingParserConfig,
    ) -> Option<SectionOrderState> {
        match *code {
            SectionCode::Type | SectionCode::Import
                if config.operator_config.enable_module_linking =>
            {
                Some(SectionOrderState::ModuleLinkingHeader)
            }
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
            SectionCode::Alias => Some(SectionOrderState::ModuleLinkingHeader),
            SectionCode::Module => Some(SectionOrderState::ModuleLinkingHeader),
            SectionCode::Instance => Some(SectionOrderState::ModuleLinkingHeader),
            SectionCode::ModuleCode => Some(SectionOrderState::ModuleCode),
            SectionCode::Custom { .. } => None,
        }
    }
}

impl Default for SectionOrderState {
    fn default() -> SectionOrderState {
        SectionOrderState::Initial
    }
}

#[derive(Copy, Clone)]
pub struct ValidatingParserConfig {
    pub operator_config: OperatorValidatorConfig,
}

const DEFAULT_VALIDATING_PARSER_CONFIG: ValidatingParserConfig = ValidatingParserConfig {
    operator_config: DEFAULT_OPERATOR_VALIDATOR_CONFIG,
};

#[derive(Default)]
struct ValidatingParserResources<'a> {
    types: Vec<ValidatedType<'a>>,
    tables: Vec<Def<TableType>>,
    memories: Vec<MemoryType>,
    globals: Vec<Def<GlobalType>>,
    element_types: Vec<Type>,
    data_count: Option<u32>,
    func_type_indices: Vec<Def<u32>>,
    module_type_indices: Vec<Def<u32>>,
    instance_type_indices: Vec<Def<InstanceDef>>,
    function_references: HashSet<u32>,
}

#[derive(Copy, Clone)]
struct Def<T> {
    module: usize,
    item: T,
}

enum ValidatedType<'a> {
    Def(TypeDef<'a>),
    Alias(Def<u32>),
}

impl<T> Def<T> {
    fn as_ref(&self) -> Def<&T> {
        Def {
            module: self.module,
            item: &self.item,
        }
    }

    fn map<U>(self, map: impl FnOnce(T) -> U) -> Def<U> {
        Def {
            module: self.module,
            item: map(self.item),
        }
    }
}

enum InstanceDef {
    Imported { type_idx: u32 },
    Instantiated { module_idx: u32 },
}

pub struct ValidatingParser<'a> {
    parser: Parser<'a>,
    validation_error: Option<ParserState<'a>>,
    read_position: Option<usize>,
    init_expression_state: Option<InitExpressionState>,
    current_operator_validator: Option<OperatorValidator>,
    /// Once we see a `BeginInstantiate` this tracks the type index of the
    /// module type as well as which import index we're currently matching
    /// against.
    module_instantiation: Option<(Def<u32>, usize)>,
    config: ValidatingParserConfig,
    modules: Vec<Module<'a>>,
    total_nested_modules: usize,
}

#[derive(Default)]
struct Module<'a> {
    parser: Parser<'a>,
    section_order_state: SectionOrderState,
    resources: ValidatingParserResources<'a>,
    current_func_index: u32,
    func_nonlocal_count: u32,
    data_found: u32,
    exported_names: HashSet<String>,
}

impl<'a> ValidatingParser<'a> {
    pub fn new(bytes: &[u8], config: Option<ValidatingParserConfig>) -> ValidatingParser {
        ValidatingParser {
            parser: Parser::new(bytes),
            validation_error: None,
            read_position: None,
            current_operator_validator: None,
            init_expression_state: None,
            module_instantiation: None,
            config: config.unwrap_or(DEFAULT_VALIDATING_PARSER_CONFIG),
            modules: vec![Module::default()],
            total_nested_modules: 0,
        }
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
        self.check_value_types(&*func_type.params)?;
        self.check_value_types(&*func_type.returns)?;
        if !self.config.operator_config.enable_multi_value && func_type.returns.len() > 1 {
            self.create_error("invalid result arity: func type returns multiple values")
        } else {
            Ok(())
        }
    }

    fn check_module_type(&self, ty: &ModuleType<'a>) -> ValidatorResult<'a, ()> {
        if !self.config.operator_config.enable_module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        for i in ty.imports.iter() {
            self.check_import_entry(&i.ty)?;
        }
        let mut names = HashSet::new();
        for e in ty.exports.iter() {
            if !names.insert(e.name) {
                return self.create_error("duplicate export name");
            }
            self.check_import_entry(&e.ty)?;
        }
        Ok(())
    }

    fn check_instance_type(&self, ty: &InstanceType<'a>) -> ValidatorResult<'a, ()> {
        if !self.config.operator_config.enable_module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        let mut names = HashSet::new();
        for e in ty.exports.iter() {
            if !names.insert(e.name) {
                return self.create_error("duplicate export name");
            }
            self.check_import_entry(&e.ty)?;
        }
        Ok(())
    }

    fn check_table_type(&self, table_type: &TableType) -> ValidatorResult<'a, ()> {
        match table_type.element_type {
            Type::FuncRef => {}
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
        if memory_type.shared {
            if !self.config.operator_config.enable_threads {
                return self.create_error("threads must be enabled for shared memories");
            }
            if memory_type.limits.maximum.is_none() {
                return self.create_error("shared memory must have maximum size");
            }
        }
        Ok(())
    }

    fn check_global_type(&self, global_type: GlobalType) -> ValidatorResult<'a, ()> {
        self.check_value_type(global_type.content_type)
    }

    fn cur_module(&self) -> &Module<'a> {
        self.modules.last().unwrap()
    }

    fn cur_module_mut(&mut self) -> &mut Module<'a> {
        self.modules.last_mut().unwrap()
    }

    fn check_import_entry(&self, import_type: &ImportSectionEntryType) -> ValidatorResult<'a, ()> {
        match *import_type {
            ImportSectionEntryType::Function(type_index) => {
                if self.cur_module().resources.func_type_indices.len() >= MAX_WASM_FUNCTIONS {
                    return self.create_error("functions count out of bounds");
                }
                self.func_type_at(self.def(type_index))?;
                Ok(())
            }
            ImportSectionEntryType::Table(ref table_type) => {
                if !self.config.operator_config.enable_reference_types
                    && !self.config.operator_config.enable_module_linking
                    && self.cur_module().resources.tables.len() >= MAX_WASM_TABLES
                {
                    return self.create_error("multiple tables: tables count must be at most 1");
                }
                self.check_table_type(table_type)
            }
            ImportSectionEntryType::Memory(ref memory_type) => {
                if !self.config.operator_config.enable_module_linking
                    && self.cur_module().resources.memories.len() >= MAX_WASM_MEMORIES
                {
                    return self.create_error("multiple memories: memory count must be at most 1");
                }
                self.check_memory_type(memory_type)
            }
            ImportSectionEntryType::Global(global_type) => {
                if self.cur_module().resources.globals.len() >= MAX_WASM_GLOBALS {
                    return self.create_error("globals count out of bounds");
                }
                self.check_global_type(global_type)
            }
            ImportSectionEntryType::Module(type_index) => {
                if self.cur_module().resources.module_type_indices.len() >= MAX_WASM_MODULES {
                    return self.create_error("modules count out of bounds");
                }
                self.module_type_at(self.def(type_index))?;
                Ok(())
            }
            ImportSectionEntryType::Instance(type_index) => {
                if self.cur_module().resources.instance_type_indices.len() >= MAX_WASM_INSTANCES {
                    return self.create_error("instance count out of bounds");
                }
                self.instance_type_at(self.def(type_index))?;
                Ok(())
            }
        }
    }

    fn check_init_expression_operator(&mut self, operator: Operator) -> ValidatorResult<'a, ()> {
        let state = self.init_expression_state.as_ref().unwrap();
        if state.validated {
            return self.create_error(
                "constant expression required: type mismatch: only one init_expr operator is expected",
            );
        }
        let expected_ty = state.ty;
        let ty = match operator {
            Operator::I32Const { .. } => Type::I32,
            Operator::I64Const { .. } => Type::I64,
            Operator::F32Const { .. } => Type::F32,
            Operator::F64Const { .. } => Type::F64,
            Operator::RefNull { ty } => {
                if !self.config.operator_config.enable_reference_types {
                    return self.create_error("reference types support is not enabled");
                }
                ty
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
                self.get_global(self.def(global_index))?.item.content_type
            }
            Operator::RefFunc { function_index } => {
                if function_index as usize >= state.function_count {
                    return self.create_error(format!(
                        "unknown function {}: init_expr function index out of bounds",
                        function_index
                    ));
                }
                self.cur_module_mut()
                    .resources
                    .function_references
                    .insert(function_index);
                Type::FuncRef
            }
            _ => {
                return self
                    .create_error("constant expression required: invalid init_expr operator")
            }
        };
        if ty != expected_ty {
            return self.create_error("type mismatch: invalid init_expr type");
        }
        Ok(())
    }

    fn check_export_entry(
        &mut self,
        field: &str,
        kind: ExternalKind,
        index: u32,
    ) -> ValidatorResult<'a, ()> {
        if self.cur_module().exported_names.contains(field) {
            return self.create_error("duplicate export name");
        }
        if let ExternalKind::Type = kind {
            return self.create_error("cannot export types");
        }
        self.check_external_kind("exported", kind, index)?;
        Ok(())
    }

    fn check_external_kind(
        &mut self,
        desc: &str,
        kind: ExternalKind,
        index: u32,
    ) -> ValidatorResult<'a, ()> {
        let module = self.cur_module_mut();
        let (ty, total) = match kind {
            ExternalKind::Function => ("function", module.resources.func_type_indices.len()),
            ExternalKind::Table => ("table", module.resources.tables.len()),
            ExternalKind::Memory => ("memory", module.resources.memories.len()),
            ExternalKind::Global => ("global", module.resources.globals.len()),
            ExternalKind::Module => ("module", module.resources.module_type_indices.len()),
            ExternalKind::Instance => ("instance", module.resources.instance_type_indices.len()),
            ExternalKind::Type => return self.create_error("cannot export types"),
        };
        if index as usize >= total {
            return self.create_error(&format!(
                "unknown {0}: {1} {0} index out of bounds",
                ty, desc
            ));
        }
        if let ExternalKind::Function = kind {
            module.resources.function_references.insert(index);
        }
        Ok(())
    }

    fn def<T>(&self, item: T) -> Def<T> {
        Def {
            module: self.modules.len() - 1,
            item,
        }
    }

    fn current_func_index(&self) -> Def<u32> {
        let module = &self.cur_module();
        self.def(module.current_func_index + module.func_nonlocal_count)
    }

    fn get<'me, T>(
        &'me self,
        idx: Def<u32>,
        desc: &str,
        get_list: impl FnOnce(&'me ValidatingParserResources<'a>) -> &'me [T],
    ) -> ValidatorResult<'a, &'me T> {
        match get_list(&self.modules[idx.module].resources).get(idx.item as usize) {
            Some(ty) => Ok(ty),
            None => self.create_error(&format!("unknown {0}: {0} index out of bounds", desc)),
        }
    }

    fn get_type<'me>(&'me self, mut idx: Def<u32>) -> ValidatorResult<'a, Def<&'me TypeDef<'a>>> {
        loop {
            let def = self.get(idx, "type", |v| &v.types)?;
            match def {
                ValidatedType::Def(item) => {
                    break Ok(Def {
                        module: idx.module,
                        item,
                    })
                }
                ValidatedType::Alias(other) => idx = *other,
            }
        }
    }

    fn get_table<'me>(&'me self, idx: Def<u32>) -> ValidatorResult<'a, &'me Def<TableType>> {
        self.get(idx, "table", |v| &v.tables)
    }

    fn get_memory<'me>(&'me self, idx: Def<u32>) -> ValidatorResult<'a, &'me MemoryType> {
        self.get(idx, "memory", |v| &v.memories)
    }

    fn get_global<'me>(&'me self, idx: Def<u32>) -> ValidatorResult<'a, &'me Def<GlobalType>> {
        self.get(idx, "global", |v| &v.globals)
    }

    fn get_func_type_index<'me>(&'me self, idx: Def<u32>) -> ValidatorResult<'a, Def<u32>> {
        Ok(*self.get(idx, "func", |v| &v.func_type_indices)?)
    }

    fn get_module_type_index<'me>(&'me self, idx: Def<u32>) -> ValidatorResult<'a, Def<u32>> {
        Ok(*self.get(idx, "module", |v| &v.module_type_indices)?)
    }

    fn get_instance_def<'me>(
        &'me self,
        idx: Def<u32>,
    ) -> ValidatorResult<'a, &'me Def<InstanceDef>> {
        self.get(idx, "module", |v| &v.instance_type_indices)
    }

    fn func_type_at<'me>(
        &'me self,
        type_index: Def<u32>,
    ) -> ValidatorResult<'a, Def<&'me FuncType>> {
        let def = self.get_type(type_index)?;
        match &def.item {
            TypeDef::Func(item) => Ok(Def {
                module: def.module,
                item,
            }),
            _ => self.create_error("type index is not a function"),
        }
    }

    fn module_type_at<'me>(
        &'me self,
        type_index: Def<u32>,
    ) -> ValidatorResult<'a, Def<&'me ModuleType<'a>>> {
        if !self.config.operator_config.enable_module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        let ty = self.get_type(type_index)?;
        match &ty.item {
            TypeDef::Module(item) => Ok(Def {
                module: ty.module,
                item,
            }),
            _ => self.create_error("type index is not a module"),
        }
    }

    fn instance_type_at<'me>(
        &'me self,
        type_index: Def<u32>,
    ) -> ValidatorResult<'a, Def<&'me InstanceType<'a>>> {
        if !self.config.operator_config.enable_module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        let def = self.get_type(type_index)?;
        match &def.item {
            TypeDef::Instance(item) => Ok(Def {
                module: def.module,
                item,
            }),
            _ => self.create_error("type index is not an instance"),
        }
    }

    fn check_start(&self, func_index: u32) -> ValidatorResult<'a, ()> {
        let ty = match self.get_func_type_index(self.def(func_index)) {
            Ok(ty) => self.func_type_at(ty)?,
            Err(_) => {
                return self.create_error("unknown function: start function index out of bounds")
            }
        };
        if !ty.item.params.is_empty() || !ty.item.returns.is_empty() {
            return self.create_error("invlid start function type");
        }
        Ok(())
    }

    fn process_begin_section(&self, code: &SectionCode) -> ValidatorResult<'a, SectionOrderState> {
        use SectionOrderState::*;

        let state = SectionOrderState::from_section_code(code, &self.config);
        let state = match state {
            Some(state) => state,
            None => return Ok(self.cur_module().section_order_state),
        };
        Ok(match self.cur_module().section_order_state {
            // Did we just start? In that case move to our newly-found state.
            Initial => state,

            // If our previous state comes before our current state, nothing to
            // worry about, just advance ourselves.
            previous if previous < state => state,

            // In the module linking proposal we can see this state multiple
            // times in a row.
            ModuleLinkingHeader
                if state == ModuleLinkingHeader
                    && self.config.operator_config.enable_module_linking =>
            {
                ModuleLinkingHeader
            }

            // otherwise the sections are out of order
            _ => return self.create_error("section out of order"),
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
                    self.cur_module_mut().section_order_state = check.ok().unwrap();
                }
            }
            ParserState::TypeSectionEntry(ref def) => {
                let check = match def {
                    TypeDef::Func(ty) => self.check_func_type(ty),
                    TypeDef::Instance(ty) => self.check_instance_type(ty),
                    TypeDef::Module(ty) => self.check_module_type(ty),
                };
                if check.is_err() {
                    self.validation_error = check.err();
                } else if self.cur_module().resources.types.len() > MAX_WASM_TYPES {
                    self.set_validation_error("types count is out of bounds");
                } else {
                    let def = ValidatedType::Def(def.clone());
                    self.cur_module_mut().resources.types.push(def);
                }
            }
            ParserState::ImportSectionEntry { ref ty, .. } => {
                let check = self.check_import_entry(ty);
                if check.is_err() {
                    self.validation_error = check.err();
                } else {
                    match *ty {
                        ImportSectionEntryType::Function(type_index) => {
                            let def = self.def(type_index);
                            self.cur_module_mut().resources.func_type_indices.push(def);
                            self.cur_module_mut().func_nonlocal_count += 1;
                        }
                        ImportSectionEntryType::Table(ref table_type) => {
                            let def = self.def(table_type.clone());
                            self.cur_module_mut().resources.tables.push(def);
                        }
                        ImportSectionEntryType::Memory(ref memory_type) => {
                            let ty = memory_type.clone();
                            self.cur_module_mut().resources.memories.push(ty);
                        }
                        ImportSectionEntryType::Global(ref global_type) => {
                            let def = self.def(global_type.clone());
                            self.cur_module_mut().resources.globals.push(def);
                        }

                        ImportSectionEntryType::Instance(type_index) => {
                            let def = self.def(InstanceDef::Imported {
                                type_idx: type_index,
                            });
                            self.cur_module_mut()
                                .resources
                                .instance_type_indices
                                .push(def);
                        }
                        ImportSectionEntryType::Module(type_index) => {
                            let def = self.def(type_index);
                            self.cur_module_mut()
                                .resources
                                .module_type_indices
                                .push(def);
                        }
                    }
                }
            }
            ParserState::FunctionSectionEntry(type_index) => {
                let type_index = self.def(type_index);
                if self.cur_module().resources.func_type_indices.len() >= MAX_WASM_FUNCTIONS {
                    self.set_validation_error("functions count out of bounds");
                } else if let Err(err) = self.func_type_at(type_index) {
                    self.validation_error = Some(err);
                } else {
                    self.cur_module_mut()
                        .resources
                        .func_type_indices
                        .push(type_index);
                }
            }
            ParserState::TableSectionEntry(ref table_type) => {
                if !self.config.operator_config.enable_reference_types
                    && !self.config.operator_config.enable_module_linking
                    && self.cur_module().resources.tables.len() >= MAX_WASM_TABLES
                {
                    self.set_validation_error("multiple tables: tables count must be at most 1");
                } else {
                    self.validation_error = self.check_table_type(table_type).err();
                    let def = self.def(table_type.clone());
                    self.cur_module_mut().resources.tables.push(def);
                }
            }
            ParserState::MemorySectionEntry(ref memory_type) => {
                if !self.config.operator_config.enable_module_linking
                    && self.cur_module().resources.memories.len() >= MAX_WASM_MEMORIES
                {
                    self.set_validation_error(
                        "multiple memories: memories count must be at most 1",
                    );
                } else {
                    self.validation_error = self.check_memory_type(memory_type).err();
                    let ty = memory_type.clone();
                    self.cur_module_mut().resources.memories.push(ty);
                }
            }
            ParserState::BeginGlobalSectionEntry(global_type) => {
                if self.cur_module().resources.globals.len() >= MAX_WASM_GLOBALS {
                    self.set_validation_error("globals count out of bounds");
                } else {
                    self.validation_error = self.check_global_type(global_type).err();
                    self.set_init_expression_state(global_type.content_type);
                    let def = self.def(global_type);
                    self.cur_module_mut().resources.globals.push(def);
                }
            }
            ParserState::BeginInitExpressionBody => {
                assert!(self.init_expression_state.is_some());
            }
            ParserState::InitExpressionOperator(ref operator) => {
                let operator = operator.clone();
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
                self.cur_module_mut()
                    .exported_names
                    .insert(String::from(field));
            }
            ParserState::StartSectionEntry(func_index) => {
                self.validation_error = self.check_start(func_index).err();
            }
            ParserState::DataCountSectionEntry(count) => {
                self.cur_module_mut().resources.data_count = Some(count);
            }
            ParserState::BeginElementSectionEntry { table, ty } => {
                self.cur_module_mut().resources.element_types.push(ty);
                match table {
                    ElemSectionEntryTable::Active(table_index) => {
                        let table = match self.get_table(self.def(table_index)) {
                            Ok(table) => table,
                            Err(_) => {
                                self.set_validation_error(
                                    "unknown table: element section table index out of bounds",
                                );
                                return;
                            }
                        };
                        if ty != table.item.element_type {
                            self.set_validation_error("element_type != table type");
                            return;
                        }
                        self.set_init_expression_state(Type::I32);
                    }
                    ElemSectionEntryTable::Passive | ElemSectionEntryTable::Declared => {
                        if !self.config.operator_config.enable_bulk_memory {
                            self.set_validation_error("reference types must be enabled");
                            return;
                        }
                    }
                }
                match ty {
                    Type::FuncRef => {}
                    Type::ExternRef if self.config.operator_config.enable_reference_types => {}
                    Type::ExternRef => {
                        self.set_validation_error(
                            "reference types must be enabled for anyref elem segment",
                        );
                        return;
                    }
                    _ => {
                        self.set_validation_error("invalid reference type");
                        return;
                    }
                }
            }
            ParserState::ElementSectionEntryBody(ref indices) => {
                let mut references = Vec::with_capacity(indices.len());
                for item in &**indices {
                    if let ElementItem::Func(func_index) = *item {
                        if self.get_func_type_index(self.def(func_index)).is_err() {
                            self.set_validation_error(
                                "unknown function: element func index out of bounds",
                            );
                            break;
                        }
                        references.push(func_index);
                    }
                }
                self.cur_module_mut()
                    .resources
                    .function_references
                    .extend(references);
            }
            ParserState::BeginFunctionBody { .. } => {
                let index = self.current_func_index();
                if self.get_func_type_index(index).is_err() {
                    self.set_validation_error("func type is not defined");
                }
            }
            ParserState::FunctionBodyLocals { ref locals } => {
                let index = self.current_func_index();
                let func_type = self.get_func_type_index(index).unwrap();
                let func_type = self.func_type_at(func_type).unwrap();
                let operator_config = self.config.operator_config;
                match OperatorValidator::new(func_type.item, locals, operator_config) {
                    Ok(validator) => self.current_operator_validator = Some(validator),
                    Err(err) => {
                        self.validation_error = Some(ParserState::Error(
                            err.set_offset(self.read_position.unwrap()),
                        ));
                    }
                }
            }
            ParserState::CodeOperator(ref operator) => {
                let mut validator = self.current_operator_validator.take().unwrap();
                let check = validator.process_operator(operator, self);
                self.current_operator_validator = Some(validator);

                if let Err(err) = check {
                    self.set_operator_validation_error(err);
                }
            }
            ParserState::EndFunctionBody => {
                let check = self
                    .current_operator_validator
                    .take()
                    .unwrap()
                    .process_end_function();
                if let Err(err) = check {
                    self.set_operator_validation_error(err);
                }
                self.cur_module_mut().current_func_index += 1;
            }
            ParserState::BeginDataSectionEntryBody(_) => {
                self.cur_module_mut().data_found += 1;
            }
            ParserState::BeginActiveDataSectionEntry(memory_index) => {
                if self.get_memory(self.def(memory_index)).is_err() {
                    self.set_validation_error(
                        "unknown memory: data section memory index out of bounds",
                    );
                } else {
                    self.set_init_expression_state(Type::I32);
                }
            }
            ParserState::EndWasm => {
                let current_func = self.current_func_index();
                let module = &mut self.cur_module();
                if module.resources.func_type_indices.len() != current_func.item as usize {
                    self.set_validation_error(
                        "function and code section have inconsistent lengths",
                    );
                    return;
                }
                if let Some(data_count) = module.resources.data_count {
                    if data_count != module.data_found {
                        self.set_validation_error("data count section and passive data mismatch");
                    }
                    return;
                }
                if self.modules.len() > 1 {
                    // Pop our nested module from the stack since it's no longer
                    // needed
                    self.modules.pop();

                    // Restore the parser back to the previous state
                    mem::swap(
                        &mut self.parser,
                        &mut self.modules.last_mut().unwrap().parser,
                    );
                }
            }

            ParserState::ModuleSectionEntry(type_index) => {
                if !self.config.operator_config.enable_module_linking {
                    self.set_validation_error("module linking proposal not enabled");
                } else if self.cur_module().resources.module_type_indices.len() >= MAX_WASM_MODULES
                {
                    self.set_validation_error("modules count out of bounds");
                } else {
                    let type_index = self.def(type_index);
                    match self.module_type_at(type_index) {
                        Ok(_) => self
                            .cur_module_mut()
                            .resources
                            .module_type_indices
                            .push(type_index),
                        Err(e) => self.validation_error = Some(e),
                    }
                }
            }
            ParserState::BeginInstantiate { module, count } => {
                if !self.config.operator_config.enable_module_linking {
                    self.set_validation_error("module linking proposal not enabled");
                } else if self.cur_module().resources.instance_type_indices.len()
                    >= MAX_WASM_INSTANCES
                {
                    self.set_validation_error("instance count out of bounds");
                } else {
                    let def = self.def(InstanceDef::Instantiated { module_idx: module });
                    self.cur_module_mut()
                        .resources
                        .instance_type_indices
                        .push(def);
                    let module_ty = match self.get_module_type_index(self.def(module)) {
                        Ok(ty) => ty,
                        Err(e) => {
                            self.validation_error = Some(e);
                            return;
                        }
                    };
                    let ty = self.module_type_at(module_ty).unwrap();
                    if count as usize != ty.item.imports.len() {
                        self.set_validation_error("wrong number of imports provided");
                    } else {
                        self.module_instantiation = Some((module_ty, 0));
                    }
                }
            }
            ParserState::InstantiateParameter { kind, index } => {
                let (module_ty_idx, import_idx) = self.module_instantiation.take().unwrap();
                let module_ty = self.module_type_at(module_ty_idx).unwrap();
                let ty = module_ty.item.imports[import_idx].ty.clone();
                let ty = module_ty.map(|_| &ty);
                match self.check_instantiate_field(ty, kind, index) {
                    Ok(()) => {
                        self.module_instantiation = Some((module_ty_idx, import_idx + 1));
                    }
                    Err(e) => self.validation_error = Some(e),
                }
            }
            ParserState::EndInstantiate => {
                let (module_ty, import_idx) = self.module_instantiation.take().unwrap();
                let module_ty = self.module_type_at(module_ty).unwrap();
                if import_idx != module_ty.item.imports.len() {
                    self.set_validation_error("not enough imports provided");
                }
            }
            ParserState::AliasSectionEntry(ref alias) => {
                let instance_idx = match alias.instance {
                    AliasedInstance::Parent => None,
                    AliasedInstance::Child(instance_idx) => Some(instance_idx),
                };
                let (kind, index) = (alias.kind, alias.index);
                match self.check_alias_entry(instance_idx, kind, index) {
                    Ok(()) => {}
                    Err(e) => self.validation_error = Some(e),
                }
            }
            ParserState::InlineModule(ref module) => {
                let parser = match module.module() {
                    Ok(m) => m,
                    Err(e) => {
                        self.validation_error = Some(ParserState::Error(e));
                        return;
                    }
                };
                self.total_nested_modules += 1;
                if self.total_nested_modules > MAX_WASM_MODULES {
                    self.set_validation_error("too many nested modules");
                }

                // Save the state of our parser in our module
                let old_parser = mem::replace(&mut self.parser, parser.into());
                self.cur_module_mut().parser = old_parser;

                // Then allocate a child module and push it onto our stack of
                // modules we're validating.
                self.modules.push(Module::default());
            }
            _ => (),
        };
    }

    fn set_init_expression_state(&mut self, ty: Type) {
        self.init_expression_state = Some(InitExpressionState {
            ty,
            global_count: self.cur_module().resources.globals.len(),
            function_count: self.cur_module().resources.func_type_indices.len(),
            validated: false,
        });
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
                let index = self.current_func_index();
                let func_type = self.func_type_at(self.get_func_type_index(index)?).unwrap();
                let operator_config = self.config.operator_config;
                OperatorValidator::new(func_type.item, locals, operator_config)
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

    fn check_instantiate_field(
        &mut self,
        expected: Def<&ImportSectionEntryType>,
        kind: ExternalKind,
        index: u32,
    ) -> ValidatorResult<'a, ()> {
        let index = self.def(index);
        let actual = match kind {
            ExternalKind::Function => self
                .get_func_type_index(index)?
                .map(ImportSectionEntryType::Function),
            ExternalKind::Table => self.get_table(index)?.map(ImportSectionEntryType::Table),
            ExternalKind::Memory => {
                self.def(ImportSectionEntryType::Memory(*self.get_memory(index)?))
            }
            ExternalKind::Global => self.get_global(index)?.map(ImportSectionEntryType::Global),
            ExternalKind::Module => self
                .get_module_type_index(index)?
                .map(ImportSectionEntryType::Module),
            ExternalKind::Instance => {
                let def = self.get_instance_def(index)?;
                match def.item {
                    InstanceDef::Imported { type_idx } => def
                        .as_ref()
                        .map(|_| ImportSectionEntryType::Instance(type_idx)),
                    InstanceDef::Instantiated { module_idx } => {
                        let expected = match expected.item {
                            ImportSectionEntryType::Instance(idx) => expected.map(|_| *idx),
                            _ => {
                                return self.create_error("wrong kind of item used for instantiate")
                            }
                        };
                        let expected = self.instance_type_at(expected)?;
                        let module_idx = def.as_ref().map(|_| module_idx);
                        let actual = self.get_module_type_index(module_idx)?;
                        let actual = self.module_type_at(actual)?;
                        return self.check_export_sets_match(
                            expected.map(|m| &*m.exports),
                            actual.map(|m| &*m.exports),
                        );
                    }
                }
            }
            ExternalKind::Type => return self.create_error("cannot export types"),
        };
        let item = actual.item;
        self.check_imports_match(expected, actual.map(|_| &item))
    }

    // Note that this function is basically implementing
    // https://webassembly.github.io/spec/core/exec/modules.html#import-matching
    fn check_imports_match(
        &self,
        expected: Def<&ImportSectionEntryType>,
        actual: Def<&ImportSectionEntryType>,
    ) -> ValidatorResult<'a, ()> {
        let limits_match = |expected: &ResizableLimits, actual: &ResizableLimits| {
            actual.initial >= expected.initial
                && match expected.maximum {
                    Some(expected_max) => match actual.maximum {
                        Some(actual_max) => actual_max <= expected_max,
                        None => false,
                    },
                    None => true,
                }
        };
        match (expected.item, actual.item) {
            (
                ImportSectionEntryType::Function(expected_idx),
                ImportSectionEntryType::Function(actual_idx),
            ) => {
                let expected = self.func_type_at(expected.map(|_| *expected_idx))?;
                let actual = self.func_type_at(actual.map(|_| *actual_idx))?;
                if actual.item == expected.item {
                    return Ok(());
                }
                self.create_error("function provided for instantiation has wrong type")
            }
            (ImportSectionEntryType::Table(expected), ImportSectionEntryType::Table(actual)) => {
                if expected.element_type == actual.element_type
                    && limits_match(&expected.limits, &actual.limits)
                {
                    return Ok(());
                }
                self.create_error("table provided for instantiation has wrong type")
            }
            (ImportSectionEntryType::Memory(expected), ImportSectionEntryType::Memory(actual)) => {
                if limits_match(&expected.limits, &actual.limits)
                    && expected.shared == actual.shared
                {
                    return Ok(());
                }
                self.create_error("memory provided for instantiation has wrong type")
            }
            (ImportSectionEntryType::Global(expected), ImportSectionEntryType::Global(actual)) => {
                if expected == actual {
                    return Ok(());
                }
                self.create_error("global provided for instantiation has wrong type")
            }
            (
                ImportSectionEntryType::Instance(expected_idx),
                ImportSectionEntryType::Instance(actual_idx),
            ) => {
                let expected = self.instance_type_at(expected.map(|_| *expected_idx))?;
                let actual = self.instance_type_at(actual.map(|_| *actual_idx))?;
                self.check_export_sets_match(
                    expected.map(|i| &*i.exports),
                    actual.map(|i| &*i.exports),
                )?;
                Ok(())
            }
            (
                ImportSectionEntryType::Module(expected_idx),
                ImportSectionEntryType::Module(actual_idx),
            ) => {
                let expected = self.module_type_at(expected.map(|_| *expected_idx))?;
                let actual = self.module_type_at(actual.map(|_| *actual_idx))?;
                if expected.item.imports.len() != actual.item.imports.len() {
                    return self.create_error("mismatched number of module imports");
                }
                for (a, b) in expected.item.imports.iter().zip(actual.item.imports.iter()) {
                    self.check_imports_match(expected.map(|_| &a.ty), actual.map(|_| &b.ty))?;
                }
                self.check_export_sets_match(
                    expected.map(|i| &*i.exports),
                    actual.map(|i| &*i.exports),
                )?;
                Ok(())
            }
            _ => self.create_error("wrong kind of item used for instantiate"),
        }
    }

    fn check_export_sets_match(
        &self,
        expected: Def<&[ExportType<'_>]>,
        actual: Def<&[ExportType<'_>]>,
    ) -> ValidatorResult<'a, ()> {
        let name_to_idx = actual
            .item
            .iter()
            .enumerate()
            .map(|(i, e)| (e.name, i))
            .collect::<HashMap<_, _>>();
        for expected_export in expected.item {
            let idx = match name_to_idx.get(expected_export.name) {
                Some(i) => *i,
                None => {
                    return self
                        .create_error(&format!("no export named `{}`", expected_export.name))
                }
            };
            self.check_imports_match(
                expected.map(|_| &expected_export.ty),
                actual.map(|_| &actual.item[idx].ty),
            )?;
        }
        Ok(())
    }

    fn check_alias_entry(
        &mut self,
        instance_idx: Option<u32>,
        kind: ExternalKind,
        idx: u32,
    ) -> ValidatorResult<'a, ()> {
        match instance_idx {
            Some(instance_idx) => {
                let ty = self.get_instance_def(self.def(instance_idx))?;
                let exports = match ty.item {
                    InstanceDef::Imported { type_idx } => {
                        let ty = self.instance_type_at(ty.as_ref().map(|_| type_idx))?;
                        ty.map(|t| &t.exports)
                    }
                    InstanceDef::Instantiated { module_idx } => {
                        let ty = self.get_module_type_index(ty.as_ref().map(|_| module_idx))?;
                        let ty = self.module_type_at(ty)?;
                        ty.map(|t| &t.exports)
                    }
                };
                let export = match exports.item.get(idx as usize) {
                    Some(e) => e,
                    None => {
                        return self.create_error("aliased export index out of bounds");
                    }
                };
                match (export.ty, kind) {
                    (ImportSectionEntryType::Function(ty), ExternalKind::Function) => {
                        let def = exports.map(|_| ty);
                        self.cur_module_mut().resources.func_type_indices.push(def);
                        self.cur_module_mut().func_nonlocal_count += 1;
                    }
                    (ImportSectionEntryType::Table(ty), ExternalKind::Table) => {
                        let def = exports.map(|_| ty);
                        self.cur_module_mut().resources.tables.push(def);
                    }
                    (ImportSectionEntryType::Memory(ty), ExternalKind::Memory) => {
                        self.cur_module_mut().resources.memories.push(ty);
                    }
                    (ImportSectionEntryType::Global(ty), ExternalKind::Global) => {
                        let def = exports.map(|_| ty);
                        self.cur_module_mut().resources.globals.push(def);
                    }
                    (ImportSectionEntryType::Instance(ty), ExternalKind::Instance) => {
                        let def = exports.map(|_| InstanceDef::Imported { type_idx: ty });
                        self.cur_module_mut()
                            .resources
                            .instance_type_indices
                            .push(def);
                    }
                    (ImportSectionEntryType::Module(ty), ExternalKind::Module) => {
                        let def = exports.map(|_| ty);
                        self.cur_module_mut()
                            .resources
                            .module_type_indices
                            .push(def);
                    }
                    _ => return self.create_error("alias kind mismatch with export kind"),
                }
            }
            None => {
                let idx = match self.modules.len().checked_sub(2) {
                    None => return self.create_error("no parent module to alias from"),
                    Some(module) => Def { module, item: idx },
                };
                match kind {
                    ExternalKind::Module => {
                        let ty = self.get_module_type_index(idx)?;
                        self.cur_module_mut().resources.module_type_indices.push(ty);
                    }
                    ExternalKind::Type => {
                        // make sure this type actually exists, then push it as
                        // ourselve aliasing that type.
                        self.get_type(idx)?;
                        self.cur_module_mut()
                            .resources
                            .types
                            .push(ValidatedType::Alias(idx));
                    }
                    _ => return self.create_error("only parent types/modules can be aliased"),
                }
            }
        }

        Ok(())
    }
}

impl<'a> WasmModuleResources for ValidatingParser<'a> {
    type TypeDef = crate::TypeDef<'a>;
    type TableType = crate::TableType;
    type MemoryType = crate::MemoryType;
    type GlobalType = crate::GlobalType;

    fn type_at(&self, at: u32) -> Option<&Self::TypeDef> {
        self.get_type(self.def(at)).ok().map(|t| t.item)
    }

    fn table_at(&self, at: u32) -> Option<&Self::TableType> {
        self.get_table(self.def(at)).ok().map(|t| &t.item)
    }

    fn memory_at(&self, at: u32) -> Option<&Self::MemoryType> {
        self.get_memory(self.def(at)).ok()
    }

    fn global_at(&self, at: u32) -> Option<&Self::GlobalType> {
        self.get_global(self.def(at)).ok().map(|t| &t.item)
    }

    fn func_type_at(&self, at: u32) -> Option<&FuncType> {
        let ty = self.get_func_type_index(self.def(at)).ok()?;
        let ty = self.func_type_at(ty).ok()?;
        Some(ty.item)
    }

    fn element_type_at(&self, at: u32) -> Option<Type> {
        self.cur_module()
            .resources
            .element_types
            .get(at as usize)
            .cloned()
    }

    fn element_count(&self) -> u32 {
        self.cur_module().resources.element_types.len() as u32
    }

    fn data_count(&self) -> u32 {
        self.cur_module().resources.data_count.unwrap_or(0)
    }

    fn is_function_referenced(&self, idx: u32) -> bool {
        self.cur_module()
            .resources
            .function_references
            .contains(&idx)
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
                self.cur_module_mut().current_func_index += 1;
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
    ///     match parser.read() {
    ///         ParserState::Error(_) |
    ///         ParserState::EndWasm => break,
    ///         ParserState::BeginFunctionBody {..} => (),
    ///         _ => continue
    ///     }
    ///     let mut reader = parser
    ///         .create_validating_operator_parser()
    ///         .expect("validating parser");
    ///     println!("Function {}", i);
    ///     i += 1;
    ///     while !reader.eof() {
    ///       let read = reader.next(&parser);
    ///       if let Ok(ref op) = read {
    ///           println!("  {:?}", op);
    ///       } else {
    ///           panic!("  Bad wasm code {:?}", read.err());
    ///       }
    ///     }
    /// }
    /// ```
    pub fn next<'c>(&mut self, resources: impl WasmModuleResources) -> Result<Operator<'c>>
    where
        'b: 'c,
    {
        let op = self.reader.read_operator()?;
        match self.operator_validator.process_operator(&op, &resources) {
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
pub fn validate_function_body(
    bytes: &[u8],
    offset: usize,
    func_index: u32,
    resources: impl WasmModuleResources,
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
    let func_type = resources
        .func_type_at(func_index)
        // Note: This was an out-of-bounds access before the change to return `Option`
        // so I assumed it is considered a bug to access a non-existing function
        // id here and went with panicking instead of returning a proper error.
        .expect("the function index of the validated function itself is out of bounds");
    let mut operator_validator = OperatorValidator::new(func_type, &locals, operator_config)
        .map_err(|e| e.set_offset(offset))?;
    let mut eof_found = false;
    let mut last_op = 0;
    for item in operators_reader.into_iter_with_offsets() {
        let (ref op, offset) = item?;
        match operator_validator
            .process_operator(op, &resources)
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
                if parser.modules.len() == 1 {
                    parser_input = Some(ParserInput::SkipFunctionBody);
                    func_ranges.push(range);
                }
            }
            _ => (),
        }
    }
    let operator_config = config.map(|c| c.operator_config);
    for (i, range) in func_ranges.into_iter().enumerate() {
        let function_body = range.slice(bytes);
        let function_index = i as u32 + parser.modules[0].func_nonlocal_count;
        validate_function_body(
            function_body,
            range.start,
            function_index,
            &parser,
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
