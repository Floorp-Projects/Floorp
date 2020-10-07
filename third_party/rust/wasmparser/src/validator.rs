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

use crate::limits::*;
use crate::operators_validator::{
    FunctionEnd, OperatorValidator, OperatorValidatorConfig, DEFAULT_OPERATOR_VALIDATOR_CONFIG,
};
use crate::WasmModuleResources;
use crate::{Alias, AliasedInstance, ExternalKind, Import, ImportSectionEntryType};
use crate::{BinaryReaderError, GlobalType, MemoryType, Range, Result, TableType, Type};
use crate::{DataKind, ElementItem, ElementKind, InitExpr, Instance, Operator};
use crate::{Export, ExportType, FunctionBody, OperatorsReader, Parser, Payload};
use crate::{FuncType, ResizableLimits, SectionReader, SectionWithLimitedItems};
use std::collections::{HashMap, HashSet};
use std::mem;
use std::sync::Arc;

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
/// analogous to [`WebAssembly.validate`][js] in the JS API.
///
/// This functions requires the wasm module is entirely resident in memory and
/// is specified by `bytes`. Additionally this validates the given bytes with
/// the default set of WebAssembly features implemented by `wasmparser`.
///
/// For more fine-tuned control over validation it's recommended to review the
/// documentation of [`Validator`].
///
/// [js]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/validate
pub fn validate(bytes: &[u8]) -> Result<()> {
    Validator::new().validate_all(bytes)
}

#[test]
fn test_validate() {
    assert!(validate(&[0x0, 0x61, 0x73, 0x6d, 0x1, 0x0, 0x0, 0x0]).is_ok());
    assert!(validate(&[0x0, 0x61, 0x73, 0x6d, 0x2, 0x0, 0x0, 0x0]).is_err());
}

mod func;
pub use func::FuncValidator;

/// Validator for a WebAssembly binary module.
///
/// This structure encapsulates state necessary to validate a WebAssembly
/// binary. This implements validation as defined by the [core
/// specification][core]. A `Validator` is designed, like
/// [`Parser`], to accept incremental input over time.
/// Additionally a `Validator` is also designed for parallel validation of
/// functions as they are received.
///
/// It's expected that you'll be using a [`Parser`] in tandem with a
/// `Validator`. As each [`Payload`](crate::Payload) is received from a
/// [`Parser`] you'll pass it into a `Validator` to test the validity of the
/// payload. Note that all payloads received from a [`Parser`] are expected to
/// be passed to a [`Validator`]. For example if you receive
/// [`Payload::TypeSection`](crate::Payload) you'll call
/// [`Validator::type_section`] to validate this.
///
/// The design of [`Validator`] is intended that you'll interleave, in your own
/// application's processing, calls to validation. Each variant, after it's
/// received, will be validated and then your application would proceed as
/// usual. At all times, however, you'll have access to the [`Validator`] and
/// the validation context up to that point. This enables applications to check
/// the types of functions and learn how many globals there are, for example.
///
/// [core]: https://webassembly.github.io/spec/core/valid/index.html
#[derive(Default)]
pub struct Validator {
    /// Internal state that is incrementally built-up for the module being
    /// validate. This houses type information for all wasm items, like
    /// functions. Note that this starts out as a solely owned `Arc<T>` so we can
    /// get mutable access, but after we get to the code section this is never
    /// mutated to we can clone it cheaply and hand it to sub-validators.
    state: arc::MaybeOwned<ModuleState>,

    /// Enabled WebAssembly feature flags, dictating what's valid and what
    /// isn't.
    features: WasmFeatures,

    /// Where we are, order-wise, in the wasm binary.
    order: Order,

    /// The current byte-level offset in the wasm binary. This is updated to
    /// produce error messages in `create_error`.
    offset: usize,

    /// The number of data segments we ended up finding in this module, or 0 if
    /// they either weren't present or none were found.
    data_found: u32,

    /// The number of functions we expect to be defined in the code section, or
    /// basically the length of the function section if it was found. The next
    /// index is where we are, in the function index space, for the next entry
    /// in the code section (used to figure out what type is next for the
    /// function being validated.
    expected_code_bodies: Option<u32>,
    code_section_index: usize,

    /// Similar to code bodies above, but for module bodies instead.
    expected_modules: Option<u32>,
    module_code_section_index: usize,

    /// If this validator is for a nested module then this keeps track of the
    /// type of the module that we're matching against. The `expected_type` is
    /// an entry in our parent's type index space, and the two positional
    /// indices keep track of where we are in matching against imports/exports.
    ///
    /// Note that the exact algorithm for how it's determine that a submodule
    /// matches its declare type is a bit up for debate. For now we go for 1:1
    /// "everything must be equal" matching. This is the subject of
    /// WebAssembly/module-linking#7, though.
    expected_type: Option<Def<u32>>,
    expected_import_pos: usize,
    expected_export_pos: usize,
}

#[derive(Default)]
struct ModuleState {
    depth: usize,
    types: Vec<ValidatedType>,
    tables: Vec<Def<TableType>>,
    memories: Vec<MemoryType>,
    globals: Vec<Def<GlobalType>>,
    element_types: Vec<Type>,
    data_count: Option<u32>,
    func_type_indices: Vec<Def<u32>>,
    module_type_indices: Vec<Def<u32>>,
    instance_type_indices: Vec<Def<InstanceDef>>,
    function_references: HashSet<u32>,
    parent: Option<Arc<ModuleState>>,
}

#[derive(Clone)]
struct WasmFeatures {
    reference_types: bool,
    module_linking: bool,
    simd: bool,
    multi_value: bool,
    threads: bool,
    tail_call: bool,
    bulk_memory: bool,
    deterministic_only: bool,
}

impl Default for WasmFeatures {
    fn default() -> WasmFeatures {
        WasmFeatures {
            // off-by-default features
            reference_types: false,
            module_linking: false,
            simd: false,
            threads: false,
            tail_call: false,
            bulk_memory: false,
            deterministic_only: cfg!(feature = "deterministic"),

            // on-by-default features
            multi_value: true,
        }
    }
}

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
enum Order {
    Initial,
    AfterHeader,
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

impl Default for Order {
    fn default() -> Order {
        Order::Initial
    }
}

enum InstanceDef {
    Imported { type_idx: u32 },
    Instantiated { module_idx: u32 },
}

enum ValidatedType {
    Def(TypeDef),
    Alias(Def<u32>),
}

enum TypeDef {
    Func(FuncType),
    Module(ModuleType),
    Instance(InstanceType),
}

struct ModuleType {
    imports: Vec<(String, Option<String>, ImportSectionEntryType)>,
    exports: Vec<(String, ImportSectionEntryType)>,
}

struct InstanceType {
    exports: Vec<(String, ImportSectionEntryType)>,
}

fn to_import(item: &(String, Option<String>, ImportSectionEntryType)) -> Import<'_> {
    Import {
        module: &item.0,
        field: item.1.as_deref(),
        ty: item.2,
    }
}

fn to_export(item: &(String, ImportSectionEntryType)) -> ExportType<'_> {
    ExportType {
        name: &item.0,
        ty: item.1,
    }
}

/// Possible return values from [`Validator::payload`].
pub enum ValidPayload<'a> {
    /// The payload validated, no further action need be taken.
    Ok,
    /// The payload validated, but it started a nested module.
    ///
    /// This result indicates that the current validator needs to be saved until
    /// later. The returned parser and validator should be used instead.
    Push(Parser, Validator),
    /// The payload validated, and the current validator is finished. The last
    /// validator that was in use should be popped off the stack to resume.
    Pop,
    /// A function was found to be validate.
    ///
    /// The function validator will need to be run over the operators in
    /// [`OperatorsReader`] to finish validation.
    Func(FuncValidator, OperatorsReader<'a>),
}

impl Validator {
    /// Creates a new [`Validator`] ready to validate a WebAssembly module.
    ///
    /// The new validator will receive payloads parsed from
    /// [`Parser`], and expects the first payload received to be
    /// the version header from the parser.
    pub fn new() -> Validator {
        Validator::default()
    }

    /// Configures whether the reference types proposal for WebAssembly is
    /// enabled.
    ///
    /// The default for this option is `false`.
    pub fn wasm_reference_types(&mut self, enabled: bool) -> &mut Validator {
        self.features.reference_types = enabled;
        self
    }

    /// Configures whether the module linking proposal for WebAssembly is
    /// enabled.
    ///
    /// The default for this option is `false`.
    pub fn wasm_module_linking(&mut self, enabled: bool) -> &mut Validator {
        self.features.module_linking = enabled;
        self
    }

    /// Configures whether the SIMD proposal for WebAssembly is
    /// enabled.
    ///
    /// The default for this option is `false`.
    pub fn wasm_simd(&mut self, enabled: bool) -> &mut Validator {
        self.features.simd = enabled;
        self
    }

    /// Configures whether the threads proposal for WebAssembly is
    /// enabled.
    ///
    /// The default for this option is `false`.
    pub fn wasm_threads(&mut self, enabled: bool) -> &mut Validator {
        self.features.threads = enabled;
        self
    }

    /// Configures whether the tail call proposal for WebAssembly is
    /// enabled.
    ///
    /// The default for this option is `false`.
    pub fn wasm_tail_call(&mut self, enabled: bool) -> &mut Validator {
        self.features.tail_call = enabled;
        self
    }

    /// Configures whether the multi-value proposal for WebAssembly is
    /// enabled.
    ///
    /// The default for this option is `true`.
    pub fn wasm_multi_value(&mut self, enabled: bool) -> &mut Validator {
        self.features.multi_value = enabled;
        self
    }

    /// Configures whether the bulk-memory proposal for WebAssembly is
    /// enabled.
    ///
    /// The default for this option is `true`.
    pub fn wasm_bulk_memory(&mut self, enabled: bool) -> &mut Validator {
        self.features.bulk_memory = enabled;
        self
    }

    /// Configures whether only deterministic instructions are allowed to validate.
    ///
    /// The default for this option is `true`.
    pub fn deterministic_only(&mut self, enabled: bool) -> &mut Validator {
        self.features.deterministic_only = enabled;
        self
    }

    /// Validates an entire in-memory module with this validator.
    ///
    /// This function will internally create a [`Parser`] to parse the `bytes`
    /// provided. The entire wasm module specified by `bytes` will be parsed and
    /// validated. Parse and validation errors will be returned through
    /// `Err(_)`, and otherwise a successful validation means `Ok(())` is
    /// returned.
    pub fn validate_all(self, bytes: &[u8]) -> Result<()> {
        let mut functions_to_validate = Vec::new();
        let mut stack = Vec::new();
        let mut cur = self;
        for payload in Parser::new(0).parse_all(bytes) {
            match cur.payload(&payload?)? {
                ValidPayload::Ok => {}
                ValidPayload::Pop => cur = stack.pop().unwrap(),
                ValidPayload::Push(_parser, validator) => {
                    stack.push(cur);
                    cur = validator
                }
                ValidPayload::Func(validator, ops) => functions_to_validate.push((validator, ops)),
            }
        }

        for (mut validator, ops) in functions_to_validate {
            for item in ops.into_iter_with_offsets() {
                let (op, offset) = item?;
                validator.op(offset, &op)?;
            }
            validator.finish()?;
        }
        Ok(())
    }

    /// Convenience function to validate a single [`Payload`].
    ///
    /// This function is intended to be used as a convenience. It will
    /// internally perform any validation necessary to validate the [`Payload`]
    /// provided. The convenience part is that you're likely already going to
    /// be matching on [`Payload`] in your application, at which point it's more
    /// appropriate to call the individual methods on [`Validator`] per-variant
    /// in [`Payload`], such as [`Validator::type_section`].
    ///
    /// This function returns a [`ValidPayload`] variant on success, indicating
    /// one of a few possible actions that need to be taken after a payload is
    /// validated. For example function contents are not validated here, they're
    /// returned through [`ValidPayload`] for validation by the caller.
    pub fn payload<'a>(&mut self, payload: &Payload<'a>) -> Result<ValidPayload<'a>> {
        use crate::Payload::*;
        match payload {
            Version { num, range } => self.version(*num, range)?,
            TypeSection(s) => self.type_section(s)?,
            ImportSection(s) => self.import_section(s)?,
            AliasSection(s) => self.alias_section(s)?,
            InstanceSection(s) => self.instance_section(s)?,
            ModuleSection(s) => self.module_section(s)?,
            FunctionSection(s) => self.function_section(s)?,
            TableSection(s) => self.table_section(s)?,
            MemorySection(s) => self.memory_section(s)?,
            GlobalSection(s) => self.global_section(s)?,
            ExportSection(s) => self.export_section(s)?,
            StartSection { func, range } => self.start_section(*func, range)?,
            ElementSection(s) => self.element_section(s)?,
            DataCountSection { count, range } => self.data_count_section(*count, range)?,
            CodeSectionStart {
                count,
                range,
                size: _,
            } => self.code_section_start(*count, range)?,
            CodeSectionEntry(body) => {
                let (func_validator, ops) = self.code_section_entry(body)?;
                return Ok(ValidPayload::Func(func_validator, ops));
            }
            ModuleCodeSectionStart {
                count,
                range,
                size: _,
            } => self.module_code_section_start(*count, range)?,
            DataSection(s) => self.data_section(s)?,
            End => {
                self.end()?;
                return Ok(if self.state.depth > 0 {
                    ValidPayload::Pop
                } else {
                    ValidPayload::Ok
                });
            }

            CustomSection { .. } => {} // no validation for custom sections
            UnknownSection { id, range, .. } => self.unknown_section(*id, range)?,
            ModuleCodeSectionEntry { parser, range: _ } => {
                let subvalidator = self.module_code_section_entry();
                return Ok(ValidPayload::Push(parser.clone(), subvalidator));
            }
        }
        Ok(ValidPayload::Ok)
    }

    fn create_error<T>(&self, msg: impl Into<String>) -> Result<T> {
        Err(BinaryReaderError::new(msg.into(), self.offset))
    }

    /// Validates [`Payload::Version`](crate::Payload)
    pub fn version(&mut self, num: u32, range: &Range) -> Result<()> {
        self.offset = range.start;
        if self.order != Order::Initial {
            return self.create_error("wasm version header out of order");
        }
        self.order = Order::AfterHeader;
        if num != 1 {
            return self.create_error("bad wasm file version");
        }
        Ok(())
    }

    fn update_order(&mut self, order: Order) -> Result<()> {
        let prev = mem::replace(&mut self.order, order);
        // If the previous section came before this section, then that's always
        // valid.
        if prev < order {
            return Ok(());
        }
        // ... otherwise if this is a repeated section then only the "module
        // linking header" is allows to have repeats
        if order == self.order && self.order == Order::ModuleLinkingHeader {
            return Ok(());
        }
        self.create_error("section out of order")
    }

    fn header_order(&mut self, order: Order) -> Order {
        if self.features.module_linking {
            Order::ModuleLinkingHeader
        } else {
            order
        }
    }

    fn get_type<'me>(&'me self, idx: Def<u32>) -> Result<Def<&'me TypeDef>> {
        match self.state.get_type(idx) {
            Some(t) => Ok(t),
            None => self.create_error("unknown type: type index out of bounds"),
        }
    }

    fn get_table<'me>(&'me self, idx: Def<u32>) -> Result<&'me Def<TableType>> {
        match self.state.get_table(idx) {
            Some(t) => Ok(t),
            None => self.create_error("unknown table: table index out of bounds"),
        }
    }

    fn get_memory<'me>(&'me self, idx: Def<u32>) -> Result<&'me MemoryType> {
        match self.state.get_memory(idx) {
            Some(t) => Ok(t),
            None => self.create_error("unknown memory: memory index out of bounds"),
        }
    }

    fn get_global<'me>(&'me self, idx: Def<u32>) -> Result<&'me Def<GlobalType>> {
        match self.state.get_global(idx) {
            Some(t) => Ok(t),
            None => self.create_error("unknown global: global index out of bounds"),
        }
    }

    fn get_func_type_index<'me>(&'me self, idx: Def<u32>) -> Result<Def<u32>> {
        match self.state.get_func_type_index(idx) {
            Some(t) => Ok(t),
            None => self.create_error(format!(
                "unknown function {}: func index out of bounds",
                idx.item
            )),
        }
    }

    fn get_module_type_index<'me>(&'me self, idx: Def<u32>) -> Result<Def<u32>> {
        match self.state.get_module_type_index(idx) {
            Some(t) => Ok(t),
            None => self.create_error("unknown module: module index out of bounds"),
        }
    }

    fn get_instance_def<'me>(&'me self, idx: Def<u32>) -> Result<&'me Def<InstanceDef>> {
        match self.state.get_instance_def(idx) {
            Some(t) => Ok(t),
            None => self.create_error("unknown instance: instance index out of bounds"),
        }
    }

    fn func_type_at<'me>(&'me self, type_index: Def<u32>) -> Result<Def<&'me FuncType>> {
        let def = self.get_type(type_index)?;
        match &def.item {
            TypeDef::Func(item) => Ok(def.with(item)),
            _ => self.create_error("type index is not a function"),
        }
    }

    fn module_type_at<'me>(&'me self, type_index: Def<u32>) -> Result<Def<&'me ModuleType>> {
        if !self.features.module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        let ty = self.get_type(type_index)?;
        match &ty.item {
            TypeDef::Module(item) => Ok(ty.with(item)),
            _ => self.create_error("type index is not a module"),
        }
    }

    fn instance_type_at<'me>(&'me self, type_index: Def<u32>) -> Result<Def<&'me InstanceType>> {
        if !self.features.module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        let def = self.get_type(type_index)?;
        match &def.item {
            TypeDef::Instance(item) => Ok(def.with(item)),
            _ => self.create_error("type index is not an instance"),
        }
    }

    fn check_max(&self, cur_len: usize, amt_added: u32, max: usize, desc: &str) -> Result<()> {
        let overflow = max
            .checked_sub(cur_len)
            .and_then(|amt| amt.checked_sub(amt_added as usize))
            .is_none();
        if overflow {
            return if max == 1 {
                self.create_error(format!("multiple {}", desc))
            } else {
                self.create_error(format!("{} count is out of bounds", desc))
            };
        }
        Ok(())
    }

    fn section<T>(
        &mut self,
        order: Order,
        section: &T,
        mut validate_item: impl FnMut(&mut Self, T::Item) -> Result<()>,
    ) -> Result<()>
    where
        T: SectionReader + Clone + SectionWithLimitedItems,
    {
        self.offset = section.range().start;
        self.update_order(order)?;

        let mut section = section.clone();
        for _ in 0..section.get_count() {
            self.offset = section.original_position();
            let item = section.read()?;
            validate_item(self, item)?;
        }
        self.offset = section.range().end;
        section.ensure_end()?;
        Ok(())
    }

    /// Validates [`Payload::TypeSection`](crate::Payload)
    pub fn type_section(&mut self, section: &crate::TypeSectionReader<'_>) -> Result<()> {
        let order = self.header_order(Order::Type);
        self.check_max(
            self.state.types.len(),
            section.get_count(),
            MAX_WASM_TYPES,
            "types",
        )?;
        self.section(order, section, |me, item| me.type_def(item))
    }

    fn type_def(&mut self, def: crate::TypeDef<'_>) -> Result<()> {
        let def = match def {
            crate::TypeDef::Func(t) => {
                for ty in t.params.iter().chain(t.returns.iter()) {
                    self.value_type(*ty)?;
                }
                if t.returns.len() > 1 && !self.features.multi_value {
                    return self
                        .create_error("invalid result arity: func type returns multiple values");
                }
                TypeDef::Func(t)
            }
            crate::TypeDef::Module(t) => {
                if !self.features.module_linking {
                    return self.create_error("module linking proposal not enabled");
                }
                let imports = t
                    .imports
                    .iter()
                    .map(|i| {
                        self.import_entry_type(&i.ty)?;
                        Ok((i.module.to_string(), i.field.map(|i| i.to_string()), i.ty))
                    })
                    .collect::<Result<_>>()?;
                let mut names = HashSet::new();
                let exports = t
                    .exports
                    .iter()
                    .map(|e| {
                        if !names.insert(e.name) {
                            return self.create_error("duplicate export name");
                        }
                        self.import_entry_type(&e.ty)?;
                        Ok((e.name.to_string(), e.ty))
                    })
                    .collect::<Result<_>>()?;
                TypeDef::Module(ModuleType { imports, exports })
            }
            crate::TypeDef::Instance(t) => {
                if !self.features.module_linking {
                    return self.create_error("module linking proposal not enabled");
                }
                let mut names = HashSet::new();
                let exports = t
                    .exports
                    .iter()
                    .map(|e| {
                        if !names.insert(e.name) {
                            return self.create_error("duplicate export name");
                        }
                        self.import_entry_type(&e.ty)?;
                        Ok((e.name.to_string(), e.ty))
                    })
                    .collect::<Result<_>>()?;
                TypeDef::Instance(InstanceType { exports })
            }
        };
        let def = ValidatedType::Def(def);
        self.state.assert_mut().types.push(def);
        Ok(())
    }

    fn value_type(&self, ty: Type) -> Result<()> {
        match ty {
            Type::I32 | Type::I64 | Type::F32 | Type::F64 => Ok(()),
            Type::FuncRef | Type::ExternRef => {
                if self.features.reference_types {
                    Ok(())
                } else {
                    self.create_error("reference types support is not enabled")
                }
            }
            Type::V128 => {
                if self.features.simd {
                    Ok(())
                } else {
                    self.create_error("SIMD support is not enabled")
                }
            }
            _ => self.create_error("invalid value type"),
        }
    }

    fn import_entry_type(&self, import_type: &ImportSectionEntryType) -> Result<()> {
        match import_type {
            ImportSectionEntryType::Function(type_index) => {
                self.func_type_at(self.state.def(*type_index))?;
                Ok(())
            }
            ImportSectionEntryType::Table(t) => self.table_type(t),
            ImportSectionEntryType::Memory(t) => self.memory_type(t),
            ImportSectionEntryType::Global(t) => self.global_type(t),
            ImportSectionEntryType::Module(type_index) => {
                self.module_type_at(self.state.def(*type_index))?;
                Ok(())
            }
            ImportSectionEntryType::Instance(type_index) => {
                self.instance_type_at(self.state.def(*type_index))?;
                Ok(())
            }
        }
    }

    fn table_type(&self, ty: &TableType) -> Result<()> {
        match ty.element_type {
            Type::FuncRef => {}
            Type::ExternRef => {
                if !self.features.reference_types {
                    return self.create_error("element is not anyfunc");
                }
            }
            _ => return self.create_error("element is not reference type"),
        }
        self.limits(&ty.limits)
    }

    fn memory_type(&self, ty: &MemoryType) -> Result<()> {
        self.limits(&ty.limits)?;
        let initial = ty.limits.initial;
        if initial as usize > MAX_WASM_MEMORY_PAGES {
            return self.create_error("memory size must be at most 65536 pages (4GiB)");
        }
        let maximum = ty.limits.maximum;
        if maximum.is_some() && maximum.unwrap() as usize > MAX_WASM_MEMORY_PAGES {
            return self.create_error("memory size must be at most 65536 pages (4GiB)");
        }
        if ty.shared {
            if !self.features.threads {
                return self.create_error("threads must be enabled for shared memories");
            }
            if ty.limits.maximum.is_none() {
                return self.create_error("shared memory must have maximum size");
            }
        }
        Ok(())
    }

    fn global_type(&self, ty: &GlobalType) -> Result<()> {
        self.value_type(ty.content_type)
    }

    fn limits(&self, limits: &ResizableLimits) -> Result<()> {
        if let Some(max) = limits.maximum {
            if limits.initial > max {
                return self.create_error("size minimum must not be greater than maximum");
            }
        }
        Ok(())
    }

    /// Validates [`Payload::ImportSection`](crate::Payload)
    pub fn import_section(&mut self, section: &crate::ImportSectionReader<'_>) -> Result<()> {
        let order = self.header_order(Order::Import);
        self.section(order, section, |me, item| me.import(item))
    }

    fn import(&mut self, entry: Import<'_>) -> Result<()> {
        self.import_entry_type(&entry.ty)?;
        let (len, max, desc) = match entry.ty {
            ImportSectionEntryType::Function(type_index) => {
                let def = self.state.def(type_index);
                let state = self.state.assert_mut();
                state.func_type_indices.push(def);
                (state.func_type_indices.len(), MAX_WASM_FUNCTIONS, "funcs")
            }
            ImportSectionEntryType::Table(ty) => {
                let def = self.state.def(ty);
                let state = self.state.assert_mut();
                state.tables.push(def);
                (state.tables.len(), self.max_tables(), "tables")
            }
            ImportSectionEntryType::Memory(ty) => {
                let state = self.state.assert_mut();
                state.memories.push(ty);
                (state.memories.len(), MAX_WASM_MEMORIES, "memories")
            }
            ImportSectionEntryType::Global(ty) => {
                let def = self.state.def(ty);
                let state = self.state.assert_mut();
                state.globals.push(def);
                (state.globals.len(), MAX_WASM_GLOBALS, "globals")
            }
            ImportSectionEntryType::Instance(type_idx) => {
                let def = self.state.def(InstanceDef::Imported { type_idx });
                let state = self.state.assert_mut();
                state.instance_type_indices.push(def);
                (
                    state.instance_type_indices.len(),
                    MAX_WASM_INSTANCES,
                    "instances",
                )
            }
            ImportSectionEntryType::Module(type_index) => {
                let def = self.state.def(type_index);
                let state = self.state.assert_mut();
                state.module_type_indices.push(def);
                (state.module_type_indices.len(), MAX_WASM_MODULES, "modules")
            }
        };
        self.check_max(len, 0, max, desc)?;

        if let Some(ty) = self.expected_type {
            let idx = self.expected_import_pos;
            self.expected_import_pos += 1;
            let module_ty = self.module_type_at(ty)?;
            let equal = match module_ty.item.imports.get(idx) {
                Some(import) => {
                    self.imports_equal(self.state.def(entry), module_ty.with(to_import(import)))
                }
                None => false,
            };
            if !equal {
                return self.create_error("inline module type does not match declared type");
            }
        }
        Ok(())
    }

    /// Validates [`Payload::AliasSection`](crate::Payload)
    pub fn alias_section(&mut self, section: &crate::AliasSectionReader<'_>) -> Result<()> {
        if !self.features.module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        self.section(Order::ModuleLinkingHeader, section, |me, a| me.alias(a))
    }

    fn alias(&mut self, alias: Alias) -> Result<()> {
        match alias.instance {
            AliasedInstance::Child(instance_idx) => {
                let ty = self.get_instance_def(self.state.def(instance_idx))?;
                let exports = match ty.item {
                    InstanceDef::Imported { type_idx } => {
                        let ty = self.instance_type_at(ty.with(type_idx))?;
                        ty.map(|t| &t.exports)
                    }
                    InstanceDef::Instantiated { module_idx } => {
                        let ty = self.get_module_type_index(ty.with(module_idx))?;
                        let ty = self.module_type_at(ty)?;
                        ty.map(|t| &t.exports)
                    }
                };
                let export = match exports.item.get(alias.index as usize) {
                    Some(e) => e,
                    None => {
                        return self.create_error("aliased export index out of bounds");
                    }
                };
                match (export.1, alias.kind) {
                    (ImportSectionEntryType::Function(ty), ExternalKind::Function) => {
                        let def = exports.with(ty);
                        self.state.assert_mut().func_type_indices.push(def);
                    }
                    (ImportSectionEntryType::Table(ty), ExternalKind::Table) => {
                        let def = exports.with(ty);
                        self.state.assert_mut().tables.push(def);
                    }
                    (ImportSectionEntryType::Memory(ty), ExternalKind::Memory) => {
                        self.state.assert_mut().memories.push(ty);
                    }
                    (ImportSectionEntryType::Global(ty), ExternalKind::Global) => {
                        let def = exports.with(ty);
                        self.state.assert_mut().globals.push(def);
                    }
                    (ImportSectionEntryType::Instance(ty), ExternalKind::Instance) => {
                        let def = exports.with(InstanceDef::Imported { type_idx: ty });
                        self.state.assert_mut().instance_type_indices.push(def);
                    }
                    (ImportSectionEntryType::Module(ty), ExternalKind::Module) => {
                        let def = exports.with(ty);
                        self.state.assert_mut().module_type_indices.push(def);
                    }
                    _ => return self.create_error("alias kind mismatch with export kind"),
                }
            }
            AliasedInstance::Parent => {
                let idx = match self.state.depth.checked_sub(1) {
                    None => return self.create_error("no parent module to alias from"),
                    Some(depth) => Def {
                        depth,
                        item: alias.index,
                    },
                };
                match alias.kind {
                    ExternalKind::Module => {
                        let ty = self.get_module_type_index(idx)?;
                        self.state.assert_mut().module_type_indices.push(ty);
                    }
                    ExternalKind::Type => {
                        // make sure this type actually exists, then push it as
                        // ourselve aliasing that type.
                        self.get_type(idx)?;
                        self.state
                            .assert_mut()
                            .types
                            .push(ValidatedType::Alias(idx));
                    }
                    _ => return self.create_error("only parent types/modules can be aliased"),
                }
            }
        }

        Ok(())
    }

    /// Validates [`Payload::ModuleSection`](crate::Payload)
    pub fn module_section(&mut self, section: &crate::ModuleSectionReader<'_>) -> Result<()> {
        if !self.features.module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        self.check_max(
            self.state.module_type_indices.len(),
            section.get_count(),
            MAX_WASM_MODULES,
            "modules",
        )?;
        self.expected_modules = Some(section.get_count() + self.expected_modules.unwrap_or(0));
        self.section(Order::ModuleLinkingHeader, section, |me, type_index| {
            let type_index = me.state.def(type_index);
            me.module_type_at(type_index)?;
            me.state.assert_mut().module_type_indices.push(type_index);
            Ok(())
        })
    }

    /// Validates [`Payload::InstanceSection`](crate::Payload)
    pub fn instance_section(&mut self, section: &crate::InstanceSectionReader<'_>) -> Result<()> {
        if !self.features.module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        self.check_max(
            self.state.instance_type_indices.len(),
            section.get_count(),
            MAX_WASM_INSTANCES,
            "instances",
        )?;
        self.section(Order::ModuleLinkingHeader, section, |me, i| me.instance(i))
    }

    fn instance(&mut self, instance: Instance<'_>) -> Result<()> {
        // Fetch the type of the instantiated module so we can typecheck all of
        // the import items.
        let module_idx = instance.module();
        let module_ty = self.get_module_type_index(self.state.def(module_idx))?;
        let ty = self.module_type_at(module_ty)?;

        // Make sure the right number of imports are provided
        let mut args = instance.args()?;
        if args.get_count() as usize != ty.item.imports.len() {
            return self.create_error("wrong number of imports provided");
        }

        // Now pairwise match each import against the expected type, making sure
        // that the provided type is a subtype of the expected type.
        for import_ty in ty.item.imports.iter() {
            let (kind, index) = args.read()?;
            let index = self.state.def(index);
            let actual = match kind {
                ExternalKind::Function => self
                    .get_func_type_index(index)?
                    .map(ImportSectionEntryType::Function),
                ExternalKind::Table => self.get_table(index)?.map(ImportSectionEntryType::Table),
                ExternalKind::Memory => self
                    .state
                    .def(ImportSectionEntryType::Memory(*self.get_memory(index)?)),
                ExternalKind::Global => self.get_global(index)?.map(ImportSectionEntryType::Global),
                ExternalKind::Module => self
                    .get_module_type_index(index)?
                    .map(ImportSectionEntryType::Module),
                ExternalKind::Instance => {
                    let def = self.get_instance_def(index)?;
                    match def.item {
                        InstanceDef::Imported { type_idx } => {
                            def.with(ImportSectionEntryType::Instance(type_idx))
                        }
                        InstanceDef::Instantiated { module_idx } => {
                            let expected = match import_ty.2 {
                                ImportSectionEntryType::Instance(idx) => ty.with(idx),
                                _ => {
                                    return self
                                        .create_error("wrong kind of item used for instantiate")
                                }
                            };
                            let expected = self.instance_type_at(expected)?;
                            let module_idx = def.with(module_idx);
                            let actual = self.get_module_type_index(module_idx)?;
                            let actual = self.module_type_at(actual)?;
                            self.check_export_sets_match(
                                expected.map(|m| &*m.exports),
                                actual.map(|m| &*m.exports),
                            )?;
                            continue;
                        }
                    }
                }
                ExternalKind::Type => return self.create_error("cannot export types"),
            };
            let item = actual.item;
            self.check_imports_match(ty.with(&import_ty.2), actual.map(|_| &item))?;
        }
        args.ensure_end()?;

        let def = self.state.def(InstanceDef::Instantiated { module_idx });
        self.state.assert_mut().instance_type_indices.push(def);
        Ok(())
    }

    // Note that this function is basically implementing
    // https://webassembly.github.io/spec/core/exec/modules.html#import-matching
    fn check_imports_match(
        &self,
        expected: Def<&ImportSectionEntryType>,
        actual: Def<&ImportSectionEntryType>,
    ) -> Result<()> {
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
                    self.check_imports_match(expected.map(|_| &a.2), actual.map(|_| &b.2))?;
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
        expected: Def<&[(String, ImportSectionEntryType)]>,
        actual: Def<&[(String, ImportSectionEntryType)]>,
    ) -> Result<()> {
        let name_to_idx = actual
            .item
            .iter()
            .enumerate()
            .map(|(i, e)| (&e.0, i))
            .collect::<HashMap<_, _>>();
        for expected_export in expected.item {
            let idx = match name_to_idx.get(&expected_export.0) {
                Some(i) => *i,
                None => {
                    return self.create_error(&format!("no export named `{}`", expected_export.0))
                }
            };
            self.check_imports_match(
                expected.map(|_| &expected_export.1),
                actual.map(|_| &actual.item[idx].1),
            )?;
        }
        Ok(())
    }

    /// Validates [`Payload::FunctionSection`](crate::Payload)
    pub fn function_section(&mut self, section: &crate::FunctionSectionReader<'_>) -> Result<()> {
        self.expected_code_bodies = Some(section.get_count());
        self.check_max(
            self.state.func_type_indices.len(),
            section.get_count(),
            MAX_WASM_FUNCTIONS,
            "funcs",
        )?;
        // Assert that each type index is indeed a function type, and otherwise
        // just push it for handling later.
        self.section(Order::Function, section, |me, i| {
            let type_index = me.state.def(i);
            me.func_type_at(type_index)?;
            me.state.assert_mut().func_type_indices.push(type_index);
            Ok(())
        })
    }

    fn max_tables(&self) -> usize {
        if self.features.reference_types || self.features.module_linking {
            MAX_WASM_TABLES
        } else {
            1
        }
    }

    /// Validates [`Payload::TableSection`](crate::Payload)
    pub fn table_section(&mut self, section: &crate::TableSectionReader<'_>) -> Result<()> {
        self.check_max(
            self.state.tables.len(),
            section.get_count(),
            self.max_tables(),
            "tables",
        )?;
        self.section(Order::Table, section, |me, ty| {
            me.table_type(&ty)?;
            let def = me.state.def(ty);
            me.state.assert_mut().tables.push(def);
            Ok(())
        })
    }

    pub fn memory_section(&mut self, section: &crate::MemorySectionReader<'_>) -> Result<()> {
        self.check_max(
            self.state.memories.len(),
            section.get_count(),
            MAX_WASM_MEMORIES,
            "memories",
        )?;
        self.section(Order::Memory, section, |me, ty| {
            me.memory_type(&ty)?;
            me.state.assert_mut().memories.push(ty);
            Ok(())
        })
    }

    /// Validates [`Payload::GlobalSection`](crate::Payload)
    pub fn global_section(&mut self, section: &crate::GlobalSectionReader<'_>) -> Result<()> {
        self.check_max(
            self.state.globals.len(),
            section.get_count(),
            MAX_WASM_GLOBALS,
            "globals",
        )?;
        self.section(Order::Global, section, |me, g| {
            me.global_type(&g.ty)?;
            me.init_expr(&g.init_expr, g.ty.content_type)?;
            let def = me.state.def(g.ty);
            me.state.assert_mut().globals.push(def);
            Ok(())
        })
    }

    fn init_expr(&mut self, expr: &InitExpr<'_>, expected_ty: Type) -> Result<()> {
        let mut ops = expr.get_operators_reader().into_iter_with_offsets();
        let (op, offset) = match ops.next() {
            Some(Err(e)) => return Err(e),
            Some(Ok(pair)) => pair,
            None => return self.create_error("type mismatch: init_expr is empty"),
        };
        self.offset = offset;
        let ty = match op {
            Operator::I32Const { .. } => Type::I32,
            Operator::I64Const { .. } => Type::I64,
            Operator::F32Const { .. } => Type::F32,
            Operator::F64Const { .. } => Type::F64,
            Operator::RefNull { ty } => ty,
            Operator::V128Const { .. } => Type::V128,
            Operator::GlobalGet { global_index } => {
                self.get_global(self.state.def(global_index))?
                    .item
                    .content_type
            }
            Operator::RefFunc { function_index } => {
                self.get_func_type_index(self.state.def(function_index))?;
                self.state
                    .assert_mut()
                    .function_references
                    .insert(function_index);
                Type::FuncRef
            }
            Operator::End => return self.create_error("type mismatch: init_expr is empty"),
            _ => {
                return self
                    .create_error("constant expression required: invalid init_expr operator")
            }
        };
        if ty != expected_ty {
            return self.create_error("type mismatch: invalid init_expr type");
        }

        // Make sure the next instruction is an `end`
        match ops.next() {
            Some(Err(e)) => return Err(e),
            Some(Ok((Operator::End, _))) => {}
            Some(Ok(_)) => {
                return self
                    .create_error("constant expression required: type mismatch: only one init_expr operator is expected")
            }
            None => return self.create_error("type mismatch: init_expr is not terminated"),
        }

        // ... and verify we're done after that
        match ops.next() {
            Some(Err(e)) => Err(e),
            Some(Ok(_)) => {
                self.create_error("constant expression required: invalid init_expr operator")
            }
            None => Ok(()),
        }
    }

    /// Validates [`Payload::ExportSection`](crate::Payload)
    pub fn export_section(&mut self, section: &crate::ExportSectionReader<'_>) -> Result<()> {
        let mut exported_names = HashSet::new();
        self.section(Order::Export, section, |me, e| {
            if !exported_names.insert(e.field.to_string()) {
                return me.create_error("duplicate export name");
            }
            if let ExternalKind::Type = e.kind {
                return me.create_error("cannot export types");
            }
            me.check_external_kind("exported", e.kind, e.index)?;
            if !me.export_is_expected(e)? {
                return me.create_error("inline module type does not match declared type");
            }
            Ok(())
        })
    }

    fn check_external_kind(&mut self, desc: &str, kind: ExternalKind, index: u32) -> Result<()> {
        let (ty, total) = match kind {
            ExternalKind::Function => ("function", self.state.func_type_indices.len()),
            ExternalKind::Table => ("table", self.state.tables.len()),
            ExternalKind::Memory => ("memory", self.state.memories.len()),
            ExternalKind::Global => ("global", self.state.globals.len()),
            ExternalKind::Module => ("module", self.state.module_type_indices.len()),
            ExternalKind::Instance => ("instance", self.state.instance_type_indices.len()),
            ExternalKind::Type => return self.create_error("cannot export types"),
        };
        if index as usize >= total {
            return self.create_error(&format!(
                "unknown {ty} {index}: {desc} {ty} index out of bounds",
                desc = desc,
                index = index,
                ty = ty,
            ));
        }
        if let ExternalKind::Function = kind {
            self.state.assert_mut().function_references.insert(index);
        }
        Ok(())
    }

    fn export_is_expected(&mut self, actual: Export<'_>) -> Result<bool> {
        let expected_ty = match self.expected_type {
            Some(ty) => ty,
            None => return Ok(true),
        };
        let idx = self.expected_export_pos;
        self.expected_export_pos += 1;
        let module_ty = self.module_type_at(expected_ty)?;
        let expected = match module_ty.item.exports.get(idx) {
            Some(expected) => module_ty.with(to_export(expected)),
            None => return Ok(false),
        };
        let index = self.state.def(actual.index);
        let ty = match actual.kind {
            ExternalKind::Function => self
                .get_func_type_index(index)?
                .map(ImportSectionEntryType::Function),
            ExternalKind::Table => self.get_table(index)?.map(ImportSectionEntryType::Table),
            ExternalKind::Memory => {
                let mem = *self.get_memory(index)?;
                let ty = ImportSectionEntryType::Memory(mem);
                self.state.def(ty)
            }
            ExternalKind::Global => self.get_global(index)?.map(ImportSectionEntryType::Global),
            ExternalKind::Module => self
                .get_module_type_index(index)?
                .map(ImportSectionEntryType::Module),
            ExternalKind::Instance => {
                let def = self.get_instance_def(index)?;
                match def.item {
                    InstanceDef::Imported { type_idx } => {
                        def.with(ImportSectionEntryType::Instance(type_idx))
                    }
                    InstanceDef::Instantiated { module_idx } => {
                        let a = self.get_module_type_index(def.with(module_idx))?;
                        let a = self.module_type_at(a)?;
                        let b = match expected.item.ty {
                            ImportSectionEntryType::Instance(idx) => {
                                self.instance_type_at(expected.with(idx))?
                            }
                            _ => return Ok(false),
                        };
                        return Ok(actual.field == expected.item.name
                            && a.item.exports.len() == b.item.exports.len()
                            && a.item
                                .exports
                                .iter()
                                .map(to_export)
                                .zip(b.item.exports.iter().map(to_export))
                                .all(|(ae, be)| self.exports_equal(a.with(ae), b.with(be))));
                    }
                }
            }
            ExternalKind::Type => unreachable!(), // already validated to not exist
        };
        let actual = ty.map(|ty| ExportType {
            name: actual.field,
            ty,
        });
        Ok(self.exports_equal(actual, expected))
    }

    fn imports_equal(&self, a: Def<Import<'_>>, b: Def<Import<'_>>) -> bool {
        a.item.module == b.item.module
            && a.item.field == b.item.field
            && self.import_ty_equal(a.with(&a.item.ty), b.with(&b.item.ty))
    }

    fn exports_equal(&self, a: Def<ExportType<'_>>, b: Def<ExportType<'_>>) -> bool {
        a.item.name == b.item.name && self.import_ty_equal(a.with(&a.item.ty), b.with(&b.item.ty))
    }

    fn import_ty_equal(
        &self,
        a: Def<&ImportSectionEntryType>,
        b: Def<&ImportSectionEntryType>,
    ) -> bool {
        match (a.item, b.item) {
            (ImportSectionEntryType::Function(ai), ImportSectionEntryType::Function(bi)) => {
                self.func_type_at(a.with(*ai)).unwrap().item
                    == self.func_type_at(b.with(*bi)).unwrap().item
            }
            (ImportSectionEntryType::Table(a), ImportSectionEntryType::Table(b)) => a == b,
            (ImportSectionEntryType::Memory(a), ImportSectionEntryType::Memory(b)) => a == b,
            (ImportSectionEntryType::Global(a), ImportSectionEntryType::Global(b)) => a == b,
            (ImportSectionEntryType::Instance(ai), ImportSectionEntryType::Instance(bi)) => {
                let a = self.instance_type_at(a.with(*ai)).unwrap();
                let b = self.instance_type_at(b.with(*bi)).unwrap();
                a.item.exports.len() == b.item.exports.len()
                    && a.item
                        .exports
                        .iter()
                        .map(to_export)
                        .zip(b.item.exports.iter().map(to_export))
                        .all(|(ae, be)| self.exports_equal(a.with(ae), b.with(be)))
            }
            (ImportSectionEntryType::Module(ai), ImportSectionEntryType::Module(bi)) => {
                let a = self.module_type_at(a.with(*ai)).unwrap();
                let b = self.module_type_at(b.with(*bi)).unwrap();
                a.item.imports.len() == b.item.imports.len()
                    && a.item
                        .imports
                        .iter()
                        .map(to_import)
                        .zip(b.item.imports.iter().map(to_import))
                        .all(|(ai, bi)| self.imports_equal(a.with(ai), b.with(bi)))
                    && a.item.exports.len() == b.item.exports.len()
                    && a.item
                        .exports
                        .iter()
                        .map(to_export)
                        .zip(b.item.exports.iter().map(to_export))
                        .all(|(ae, be)| self.exports_equal(a.with(ae), b.with(be)))
            }
            _ => false,
        }
    }

    /// Validates [`Payload::StartSection`](crate::Payload)
    pub fn start_section(&mut self, func: u32, range: &Range) -> Result<()> {
        self.offset = range.start;
        self.update_order(Order::Start)?;
        let ty = self.get_func_type_index(self.state.def(func))?;
        let ty = self.func_type_at(ty)?;
        if !ty.item.params.is_empty() || !ty.item.returns.is_empty() {
            return self.create_error("invalid start function type");
        }
        Ok(())
    }

    /// Validates [`Payload::ElementSection`](crate::Payload)
    pub fn element_section(&mut self, section: &crate::ElementSectionReader<'_>) -> Result<()> {
        self.section(Order::Element, section, |me, e| {
            match e.ty {
                Type::FuncRef => {}
                Type::ExternRef if me.features.reference_types => {}
                Type::ExternRef => {
                    return me
                        .create_error("reference types must be enabled for anyref elem segment");
                }
                _ => return me.create_error("invalid reference type"),
            }
            match e.kind {
                ElementKind::Active {
                    table_index,
                    init_expr,
                } => {
                    let table = me.get_table(me.state.def(table_index))?;
                    if e.ty != table.item.element_type {
                        return me.create_error("element_type != table type");
                    }
                    me.init_expr(&init_expr, Type::I32)?;
                }
                ElementKind::Passive | ElementKind::Declared => {
                    if !me.features.bulk_memory {
                        return me.create_error("reference types must be enabled");
                    }
                }
            }
            let mut items = e.items.get_items_reader()?;
            if items.get_count() > MAX_WASM_TABLE_ENTRIES as u32 {
                return me.create_error("num_elements is out of bounds");
            }
            for _ in 0..items.get_count() {
                me.offset = items.original_position();
                match items.read()? {
                    ElementItem::Null(ty) => {
                        if ty != e.ty {
                            return me.create_error("null type doesn't match element type");
                        }
                    }
                    ElementItem::Func(f) => {
                        me.get_func_type_index(me.state.def(f))?;
                        me.state.assert_mut().function_references.insert(f);
                    }
                }
            }

            me.state.assert_mut().element_types.push(e.ty);
            Ok(())
        })
    }

    /// Validates [`Payload::DataCountSection`](crate::Payload)
    pub fn data_count_section(&mut self, count: u32, range: &Range) -> Result<()> {
        self.offset = range.start;
        self.update_order(Order::DataCount)?;
        self.state.assert_mut().data_count = Some(count);
        Ok(())
    }

    /// Validates [`Payload::ModuleCodeSectionStart`](crate::Payload)
    pub fn module_code_section_start(&mut self, count: u32, range: &Range) -> Result<()> {
        if !self.features.module_linking {
            return self.create_error("module linking proposal not enabled");
        }
        self.offset = range.start;
        self.update_order(Order::ModuleCode)?;
        match self.expected_modules.take() {
            Some(n) if n == count => {}
            Some(_) => {
                return self
                    .create_error("module and module code section have inconsistent lengths");
            }
            None if count == 0 => {}
            None => return self.create_error("module code section without module section"),
        }
        self.module_code_section_index = self.state.module_type_indices.len() - (count as usize);
        Ok(())
    }

    /// Validates [`Payload::ModuleCodeSectionEntry`](crate::Payload).
    ///
    /// Note that this does not actually perform any validation itself. The
    /// `ModuleCodeSectionEntry` payload is associated with a sub-parser for the
    /// sub-module, and it's expected that the returned [`Validator`] will be
    /// paired with the [`Parser`] otherwise used with the module.
    ///
    /// Note that the returned [`Validator`] should be used for the nested
    /// module. It will correctly work with parent aliases as well as ensure the
    /// type of the inline module matches the declared type. Using
    /// [`Validator::new`] will result in incorrect validation.
    pub fn module_code_section_entry<'a>(&mut self) -> Validator {
        let mut ret = Validator::default();
        ret.features = self.features.clone();
        ret.expected_type = Some(self.state.module_type_indices[self.module_code_section_index]);
        self.module_code_section_index += 1;
        let state = ret.state.assert_mut();
        state.parent = Some(self.state.arc().clone());
        state.depth = self.state.depth + 1;
        return ret;
    }

    /// Validates [`Payload::CodeSectionStart`](crate::Payload).
    pub fn code_section_start(&mut self, count: u32, range: &Range) -> Result<()> {
        self.offset = range.start;
        self.update_order(Order::Code)?;
        match self.expected_code_bodies.take() {
            Some(n) if n == count => {}
            Some(_) => {
                return self.create_error("function and code section have inconsistent lengths");
            }
            // empty code sections are allowed even if the function section is
            // missing
            None if count == 0 => {}
            None => return self.create_error("code section without function section"),
        }
        self.code_section_index = self.state.func_type_indices.len() - (count as usize);
        Ok(())
    }

    /// Validates [`Payload::CodeSectionEntry`](crate::Payload).
    ///
    /// This function will prepare a [`FuncValidator`] which can be used to
    /// validate the function. The function body provided will be parsed only
    /// enough to create the function validation context. After this the
    /// [`OperatorsReader`] returned can be used to read the opcodes of the
    /// function as well as feed information into the validator.
    ///
    /// Note that the returned [`FuncValidator`] is "connected" to this
    /// [`Validator`] in that it uses the internal context of this validator for
    /// validating the function. The [`FuncValidator`] can be sent to
    /// another thread, for example, to offload actual processing of functions
    /// elsewhere.
    pub fn code_section_entry<'a>(
        &mut self,
        body: &FunctionBody<'a>,
    ) -> Result<(FuncValidator, OperatorsReader<'a>)> {
        let ty_index = self.state.func_type_indices[self.code_section_index];
        self.code_section_index += 1;
        self.offset = body.get_binary_reader().original_position();

        let mut locals = body.get_locals_reader()?;

        let mut list = Vec::new();
        let mut total = 0u32;
        let max = MAX_WASM_FUNCTION_LOCALS as u32;
        if locals.get_count() > max {
            return self.create_error("locals exceed maximum");
        }
        for _ in 0..locals.get_count() {
            self.offset = locals.original_position();
            let (cnt, ty) = locals.read()?;
            total = match total.checked_add(cnt) {
                Some(total) => total,
                None => return self.create_error("locals overflow"),
            };
            if total > max {
                return self.create_error("locals exceed maximum");
            }
            list.push((cnt, ty));
        }
        let config = OperatorValidatorConfig {
            enable_module_linking: self.features.module_linking,
            enable_reference_types: self.features.reference_types,
            enable_multi_value: self.features.multi_value,
            enable_simd: self.features.simd,
            enable_bulk_memory: self.features.bulk_memory,
            enable_threads: self.features.threads,
            enable_tail_call: self.features.tail_call,
            #[cfg(feature = "deterministic")]
            deterministic_only: self.features.deterministic_only,
        };
        let ty = self.func_type_at(ty_index)?;
        Ok((
            FuncValidator {
                validator: OperatorValidator::new(ty.item, &list, config).map_err(|e| e.0)?,
                state: self.state.arc().clone(),
                offset: body.get_binary_reader().original_position(),
                eof_found: false,
            },
            body.get_operators_reader()?,
        ))
    }

    /// Validates [`Payload::DataSection`](crate::Payload).
    pub fn data_section(&mut self, section: &crate::DataSectionReader<'_>) -> Result<()> {
        self.data_found = section.get_count();
        self.section(Order::Data, section, |me, d| {
            match d.kind {
                DataKind::Passive => {}
                DataKind::Active {
                    memory_index,
                    init_expr,
                } => {
                    me.get_memory(me.state.def(memory_index))?;
                    me.init_expr(&init_expr, Type::I32)?;
                }
            }
            Ok(())
        })
    }

    /// Validates [`Payload::UnknownSection`](crate::Payload).
    ///
    /// Currently always returns an error.
    pub fn unknown_section(&mut self, id: u8, range: &Range) -> Result<()> {
        self.offset = range.start;
        self.create_error(format!("invalid section code: {}", id))
    }

    /// Validates [`Payload::End`](crate::Payload).
    pub fn end(&mut self) -> Result<()> {
        if let Some(data_count) = self.state.data_count {
            if data_count != self.data_found {
                return self.create_error("data count section and passive data mismatch");
            }
        }
        if let Some(n) = self.expected_code_bodies.take() {
            if n > 0 {
                return self.create_error("function and code sections have inconsistent lengths");
            }
        }
        if let Some(n) = self.expected_modules.take() {
            if n > 0 {
                return self
                    .create_error("module and module code sections have inconsistent lengths");
            }
        }
        if let Some(t) = self.expected_type {
            let ty = self.module_type_at(t)?;
            if self.expected_import_pos != ty.item.imports.len()
                || self.expected_export_pos != ty.item.exports.len()
            {
                return self.create_error("inline module type does not match declared type");
            }
        }
        Ok(())
    }
}

impl ModuleState {
    fn def<T>(&self, item: T) -> Def<T> {
        Def {
            depth: self.depth,
            item,
        }
    }

    fn get<'me, T>(
        &'me self,
        idx: Def<u32>,
        get_list: impl FnOnce(&'me ModuleState) -> &'me [T],
    ) -> Option<&'me T> {
        let mut cur = self;
        for _ in 0..(self.depth - idx.depth) {
            cur = cur.parent.as_ref().unwrap();
        }
        get_list(cur).get(idx.item as usize)
    }

    fn get_type<'me>(&'me self, mut idx: Def<u32>) -> Option<Def<&'me TypeDef>> {
        loop {
            let def = self.get(idx, |v| &v.types)?;
            match def {
                ValidatedType::Def(item) => break Some(idx.with(item)),
                ValidatedType::Alias(other) => idx = *other,
            }
        }
    }

    fn get_table<'me>(&'me self, idx: Def<u32>) -> Option<&'me Def<TableType>> {
        self.get(idx, |v| &v.tables)
    }

    fn get_memory<'me>(&'me self, idx: Def<u32>) -> Option<&'me MemoryType> {
        self.get(idx, |v| &v.memories)
    }

    fn get_global<'me>(&'me self, idx: Def<u32>) -> Option<&'me Def<GlobalType>> {
        self.get(idx, |v| &v.globals)
    }

    fn get_func_type_index<'me>(&'me self, idx: Def<u32>) -> Option<Def<u32>> {
        Some(*self.get(idx, |v| &v.func_type_indices)?)
    }

    fn get_module_type_index<'me>(&'me self, idx: Def<u32>) -> Option<Def<u32>> {
        Some(*self.get(idx, |v| &v.module_type_indices)?)
    }

    fn get_instance_def<'me>(&'me self, idx: Def<u32>) -> Option<&'me Def<InstanceDef>> {
        self.get(idx, |v| &v.instance_type_indices)
    }
}

#[derive(Copy, Clone)]
struct Def<T> {
    depth: usize,
    item: T,
}

impl<T> Def<T> {
    fn map<U>(self, map: impl FnOnce(T) -> U) -> Def<U> {
        Def {
            depth: self.depth,
            item: map(self.item),
        }
    }

    fn with<U>(&self, item: U) -> Def<U> {
        Def {
            depth: self.depth,
            item: item,
        }
    }
}

mod arc {
    use std::ops::Deref;
    use std::sync::Arc;

    pub struct MaybeOwned<T> {
        owned: bool,
        arc: Arc<T>,
    }

    impl<T> MaybeOwned<T> {
        pub fn as_mut(&mut self) -> Option<&mut T> {
            if !self.owned {
                return None;
            }
            debug_assert!(Arc::get_mut(&mut self.arc).is_some());
            Some(unsafe { &mut *(&*self.arc as *const T as *mut T) })
        }

        pub fn assert_mut(&mut self) -> &mut T {
            self.as_mut().unwrap()
        }

        pub fn arc(&mut self) -> &Arc<T> {
            self.owned = false;
            &self.arc
        }
    }

    impl<T: Default> Default for MaybeOwned<T> {
        fn default() -> MaybeOwned<T> {
            MaybeOwned {
                owned: true,
                arc: Arc::default(),
            }
        }
    }

    impl<T> Deref for MaybeOwned<T> {
        type Target = T;

        fn deref(&self) -> &T {
            &self.arc
        }
    }
}
