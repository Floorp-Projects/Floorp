//! State relating to validating a WebAssembly module.
//!
use super::{
    check_max, combine_type_sizes,
    operators::OperatorValidator,
    types::{EntityType, Type, TypeId, TypeList},
};
use crate::{
    limits::*, BinaryReaderError, Data, DataKind, Element, ElementItem, ElementKind, ExternalKind,
    FuncType, Global, GlobalType, InitExpr, MemoryType, Operator, Result, TableType, TagType,
    TypeRef, ValType, WasmFeatures, WasmModuleResources,
};
use indexmap::IndexMap;
use std::{collections::HashSet, sync::Arc};

fn check_value_type(ty: ValType, features: &WasmFeatures, offset: usize) -> Result<()> {
    match features.check_value_type(ty) {
        Ok(()) => Ok(()),
        Err(e) => Err(BinaryReaderError::new(e, offset)),
    }
}

// Section order for WebAssembly modules.
//
// Component sections are unordered and allow for duplicates,
// so this isn't used for components.
#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
pub enum Order {
    Initial,
    Type,
    Import,
    Function,
    Table,
    Memory,
    Tag,
    Global,
    Export,
    Start,
    Element,
    DataCount,
    Code,
    Data,
}

impl Default for Order {
    fn default() -> Order {
        Order::Initial
    }
}

#[derive(Default)]
pub(crate) struct ModuleState {
    /// Internal state that is incrementally built-up for the module being
    /// validated. This houses type information for all wasm items, like
    /// functions. Note that this starts out as a solely owned `Arc<T>` so we can
    /// get mutable access, but after we get to the code section this is never
    /// mutated to we can clone it cheaply and hand it to sub-validators.
    pub module: arc::MaybeOwned<Module>,

    /// Where we are, order-wise, in the wasm binary.
    order: Order,

    /// The number of data segments in the data section (if present).
    pub data_segment_count: u32,

    /// The number of functions we expect to be defined in the code section, or
    /// basically the length of the function section if it was found. The next
    /// index is where we are, in the code section index space, for the next
    /// entry in the code section (used to figure out what type is next for the
    /// function being validated).
    pub expected_code_bodies: Option<u32>,

    /// When parsing the code section, represents the current index in the section.
    code_section_index: Option<usize>,
}

impl ModuleState {
    pub fn update_order(&mut self, order: Order, offset: usize) -> Result<()> {
        if self.order >= order {
            return Err(BinaryReaderError::new("section out of order", offset));
        }

        self.order = order;

        Ok(())
    }

    pub fn validate_end(&self, offset: usize) -> Result<()> {
        // Ensure that the data count section, if any, was correct.
        if let Some(data_count) = self.module.data_count {
            if data_count != self.data_segment_count {
                return Err(BinaryReaderError::new(
                    "data count and data section have inconsistent lengths",
                    offset,
                ));
            }
        }
        // Ensure that the function section, if nonzero, was paired with a code
        // section with the appropriate length.
        if let Some(n) = self.expected_code_bodies {
            if n > 0 {
                return Err(BinaryReaderError::new(
                    "function and code section have inconsistent lengths",
                    offset,
                ));
            }
        }

        Ok(())
    }

    pub fn next_code_index_and_type(&mut self, offset: usize) -> Result<(u32, u32)> {
        let index = self
            .code_section_index
            .get_or_insert(self.module.num_imported_functions as usize);

        if *index >= self.module.functions.len() {
            return Err(BinaryReaderError::new(
                "code section entry exceeds number of functions",
                offset,
            ));
        }

        let ty = self.module.functions[*index];
        *index += 1;

        Ok(((*index - 1) as u32, ty))
    }

    pub fn add_global(
        &mut self,
        global: Global,
        features: &WasmFeatures,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        self.module
            .check_global_type(&global.ty, features, offset)?;
        self.check_init_expr(
            &global.init_expr,
            global.ty.content_type,
            features,
            types,
            offset,
        )?;
        self.module.assert_mut().globals.push(global.ty);
        Ok(())
    }

    pub fn add_data_segment(
        &mut self,
        data: Data,
        features: &WasmFeatures,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        match data.kind {
            DataKind::Passive => Ok(()),
            DataKind::Active {
                memory_index,
                init_expr,
            } => {
                let ty = self.module.memory_at(memory_index, offset)?.index_type();
                self.check_init_expr(&init_expr, ty, features, types, offset)
            }
        }
    }

    pub fn add_element_segment(
        &mut self,
        e: Element,
        features: &WasmFeatures,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        match e.ty {
            ValType::FuncRef => {}
            ValType::ExternRef if features.reference_types => {}
            ValType::ExternRef => {
                return Err(BinaryReaderError::new(
                    "reference types must be enabled for externref elem segment",
                    offset,
                ))
            }
            _ => return Err(BinaryReaderError::new("malformed reference type", offset)),
        }
        match e.kind {
            ElementKind::Active {
                table_index,
                init_expr,
            } => {
                let table = self.module.table_at(table_index, offset)?;
                if e.ty != table.element_type {
                    return Err(BinaryReaderError::new(
                        "invalid element type for table type",
                        offset,
                    ));
                }

                self.check_init_expr(&init_expr, ValType::I32, features, types, offset)?;
            }
            ElementKind::Passive | ElementKind::Declared => {
                if !features.bulk_memory {
                    return Err(BinaryReaderError::new(
                        "bulk memory must be enabled",
                        offset,
                    ));
                }
            }
        }
        let mut items = e.items.get_items_reader()?;
        if items.get_count() > MAX_WASM_TABLE_ENTRIES as u32 {
            return Err(BinaryReaderError::new(
                "number of elements is out of bounds",
                offset,
            ));
        }
        for _ in 0..items.get_count() {
            let offset = items.original_position();
            match items.read()? {
                ElementItem::Expr(expr) => {
                    self.check_init_expr(&expr, e.ty, features, types, offset)?;
                }
                ElementItem::Func(f) => {
                    if e.ty != ValType::FuncRef {
                        return Err(BinaryReaderError::new(
                            "type mismatch: segment does not have funcref type",
                            offset,
                        ));
                    }
                    self.module.get_func_type(f, types, offset)?;
                    self.module.assert_mut().function_references.insert(f);
                }
            }
        }

        self.module.assert_mut().element_types.push(e.ty);
        Ok(())
    }

    fn check_init_expr(
        &mut self,
        expr: &InitExpr<'_>,
        expected_ty: ValType,
        features: &WasmFeatures,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        let mut ops = expr.get_operators_reader();
        let mut validator = OperatorValidator::new_init_expr(features, expected_ty);
        let mut uninserted_funcref = false;

        while !ops.eof() {
            let offset = ops.original_position();
            let op = ops.read()?;
            match &op {
                // These are always valid in const expressions.
                Operator::I32Const { .. }
                | Operator::I64Const { .. }
                | Operator::F32Const { .. }
                | Operator::F64Const { .. }
                | Operator::RefNull { .. }
                | Operator::V128Const { .. }
                | Operator::End => {}

                // These are valid const expressions when the extended-const proposal is enabled.
                Operator::I32Add
                | Operator::I32Sub
                | Operator::I32Mul
                | Operator::I64Add
                | Operator::I64Sub
                | Operator::I64Mul
                    if features.extended_const => {}

                // `global.get` is a valid const expression for imported, immutable globals.
                Operator::GlobalGet { global_index } => {
                    let global = self.module.global_at(*global_index, offset)?;
                    if *global_index >= self.module.num_imported_globals {
                        return Err(BinaryReaderError::new(
                            "constant expression required: global.get of locally defined global",
                            offset,
                        ));
                    }
                    if global.mutable {
                        return Err(BinaryReaderError::new(
                            "constant expression required: global.get of mutable global",
                            offset,
                        ));
                    }
                }

                // Functions in initialization expressions are only valid in
                // element segment initialization expressions and globals. In
                // these contexts we want to record all function references.
                //
                // Initialization expressions can also be found in the data
                // section, however. A `RefFunc` instruction in those situations
                // is always invalid and needs to produce a validation error. In
                // this situation, though, we can no longer modify
                // the state since it's been "snapshot" already for
                // parallel validation of functions.
                //
                // If we cannot modify the function references then this function
                // *should* result in a validation error, but we defer that
                // validation error to happen later. The `uninserted_funcref`
                // boolean here is used to track this and will cause a panic
                // (aka a fuzz bug) if we somehow forget to emit an error somewhere
                // else.
                Operator::RefFunc { function_index } => {
                    if self.order == Order::Data {
                        uninserted_funcref = true;
                    } else {
                        self.module
                            .assert_mut()
                            .function_references
                            .insert(*function_index);
                    }
                }
                _ => {
                    return Err(BinaryReaderError::new(
                        "constant expression required: invalid init_expr operator",
                        offset,
                    ));
                }
            }

            validator
                .process_operator(
                    &op,
                    &OperatorValidatorResources {
                        module: &self.module,
                        types,
                    },
                )
                .map_err(|e| e.set_offset(offset))?;
        }

        validator.finish().map_err(|e| e.set_offset(offset))?;

        // See comment in `RefFunc` above for why this is an assert.
        assert!(!uninserted_funcref);

        Ok(())
    }
}

pub(crate) struct Module {
    // This is set once the code section starts.
    // `WasmModuleResources` implementations use the snapshot to
    // enable parallel validation of functions.
    pub snapshot: Option<Arc<TypeList>>,
    // Stores indexes into the validator's types list.
    pub types: Vec<TypeId>,
    pub tables: Vec<TableType>,
    pub memories: Vec<MemoryType>,
    pub globals: Vec<GlobalType>,
    pub element_types: Vec<ValType>,
    pub data_count: Option<u32>,
    // Stores indexes into `types`.
    pub functions: Vec<u32>,
    pub tags: Vec<TypeId>,
    pub function_references: HashSet<u32>,
    pub imports: IndexMap<(String, String), Vec<EntityType>>,
    pub exports: IndexMap<String, EntityType>,
    pub type_size: usize,
    num_imported_globals: u32,
    num_imported_functions: u32,
}

impl Module {
    pub fn add_type(
        &mut self,
        ty: crate::Type,
        features: &WasmFeatures,
        types: &mut TypeList,
        offset: usize,
        check_limit: bool,
    ) -> Result<()> {
        let ty = match ty {
            crate::Type::Func(t) => {
                for ty in t.params.iter().chain(t.returns.iter()) {
                    check_value_type(*ty, features, offset)?;
                }
                if t.returns.len() > 1 && !features.multi_value {
                    return Err(BinaryReaderError::new(
                        "func type returns multiple values but the multi-value feature is not enabled",
                        offset,
                    ));
                }
                Type::Func(t)
            }
        };

        if check_limit {
            check_max(self.types.len(), 1, MAX_WASM_TYPES, "types", offset)?;
        }

        self.types.push(TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        });
        types.push(ty);
        Ok(())
    }

    pub fn add_import(
        &mut self,
        import: crate::Import,
        features: &WasmFeatures,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        let entity = self.check_type_ref(&import.ty, features, types, offset)?;

        let (len, max, desc) = match import.ty {
            TypeRef::Func(type_index) => {
                self.functions.push(type_index);
                self.num_imported_functions += 1;
                (self.functions.len(), MAX_WASM_FUNCTIONS, "functions")
            }
            TypeRef::Table(ty) => {
                self.tables.push(ty);
                (self.tables.len(), self.max_tables(features), "tables")
            }
            TypeRef::Memory(ty) => {
                self.memories.push(ty);
                (self.memories.len(), self.max_memories(features), "memories")
            }
            TypeRef::Tag(ty) => {
                self.tags.push(self.types[ty.func_type_idx as usize]);
                (self.tags.len(), MAX_WASM_TAGS, "tags")
            }
            TypeRef::Global(ty) => {
                if !features.mutable_global && ty.mutable {
                    return Err(BinaryReaderError::new(
                        "mutable global support is not enabled",
                        offset,
                    ));
                }
                self.globals.push(ty);
                self.num_imported_globals += 1;
                (self.globals.len(), MAX_WASM_GLOBALS, "globals")
            }
        };

        check_max(len, 0, max, desc, offset)?;

        self.type_size = combine_type_sizes(self.type_size, entity.type_size(), offset)?;

        self.imports
            .entry((import.module.to_string(), import.name.to_string()))
            .or_default()
            .push(entity);

        Ok(())
    }

    pub fn add_export(
        &mut self,
        name: &str,
        ty: EntityType,
        features: &WasmFeatures,
        offset: usize,
        check_limit: bool,
    ) -> Result<()> {
        if !features.mutable_global {
            if let EntityType::Global(global_type) = ty {
                if global_type.mutable {
                    return Err(BinaryReaderError::new(
                        "mutable global support is not enabled",
                        offset,
                    ));
                }
            }
        }

        if check_limit {
            check_max(self.exports.len(), 1, MAX_WASM_EXPORTS, "exports", offset)?;
        }

        self.type_size = combine_type_sizes(self.type_size, ty.type_size(), offset)?;

        match self.exports.insert(name.to_string(), ty) {
            Some(_) => Err(BinaryReaderError::new(
                format!("duplicate export name `{}` already defined", name),
                offset,
            )),
            None => Ok(()),
        }
    }

    pub fn add_function(&mut self, type_index: u32, types: &TypeList, offset: usize) -> Result<()> {
        self.func_type_at(type_index, types, offset)?;
        self.functions.push(type_index);
        Ok(())
    }

    pub fn add_table(
        &mut self,
        ty: TableType,
        features: &WasmFeatures,
        offset: usize,
    ) -> Result<()> {
        self.check_table_type(&ty, features, offset)?;
        self.tables.push(ty);
        Ok(())
    }

    pub fn add_memory(
        &mut self,
        ty: MemoryType,
        features: &WasmFeatures,
        offset: usize,
    ) -> Result<()> {
        self.check_memory_type(&ty, features, offset)?;
        self.memories.push(ty);
        Ok(())
    }

    pub fn add_tag(
        &mut self,
        ty: TagType,
        features: &WasmFeatures,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        self.check_tag_type(&ty, features, types, offset)?;
        self.tags.push(self.types[ty.func_type_idx as usize]);
        Ok(())
    }

    pub fn type_at(&self, idx: u32, offset: usize) -> Result<TypeId> {
        self.types.get(idx as usize).copied().ok_or_else(|| {
            BinaryReaderError::new(
                format!("unknown type {}: type index out of bounds", idx),
                offset,
            )
        })
    }

    fn func_type_at<'a>(
        &self,
        type_index: u32,
        types: &'a TypeList,
        offset: usize,
    ) -> Result<&'a FuncType> {
        types[self.type_at(type_index, offset)?]
            .as_func_type()
            .ok_or_else(|| {
                BinaryReaderError::new(
                    format!("type index {} is not a function type", type_index),
                    offset,
                )
            })
    }

    pub fn check_type_ref(
        &self,
        type_ref: &TypeRef,
        features: &WasmFeatures,
        types: &TypeList,
        offset: usize,
    ) -> Result<EntityType> {
        Ok(match type_ref {
            TypeRef::Func(type_index) => {
                self.func_type_at(*type_index, types, offset)?;
                EntityType::Func(self.types[*type_index as usize])
            }
            TypeRef::Table(t) => {
                self.check_table_type(t, features, offset)?;
                EntityType::Table(*t)
            }
            TypeRef::Memory(t) => {
                self.check_memory_type(t, features, offset)?;
                EntityType::Memory(*t)
            }
            TypeRef::Tag(t) => {
                self.check_tag_type(t, features, types, offset)?;
                EntityType::Tag(self.types[t.func_type_idx as usize])
            }
            TypeRef::Global(t) => {
                self.check_global_type(t, features, offset)?;
                EntityType::Global(*t)
            }
        })
    }

    fn check_table_type(
        &self,
        ty: &TableType,
        features: &WasmFeatures,
        offset: usize,
    ) -> Result<()> {
        match ty.element_type {
            ValType::FuncRef => {}
            ValType::ExternRef => {
                if !features.reference_types {
                    return Err(BinaryReaderError::new("element is not anyfunc", offset));
                }
            }
            _ => {
                return Err(BinaryReaderError::new(
                    "element is not reference type",
                    offset,
                ))
            }
        }
        self.check_limits(ty.initial, ty.maximum, offset)?;
        if ty.initial > MAX_WASM_TABLE_ENTRIES as u32 {
            return Err(BinaryReaderError::new(
                "minimum table size is out of bounds",
                offset,
            ));
        }
        Ok(())
    }

    fn check_memory_type(
        &self,
        ty: &MemoryType,
        features: &WasmFeatures,
        offset: usize,
    ) -> Result<()> {
        self.check_limits(ty.initial, ty.maximum, offset)?;
        let (true_maximum, err) = if ty.memory64 {
            if !features.memory64 {
                return Err(BinaryReaderError::new(
                    "memory64 must be enabled for 64-bit memories",
                    offset,
                ));
            }
            (
                MAX_WASM_MEMORY64_PAGES,
                "memory size must be at most 2**48 pages",
            )
        } else {
            (
                MAX_WASM_MEMORY32_PAGES,
                "memory size must be at most 65536 pages (4GiB)",
            )
        };
        if ty.initial > true_maximum {
            return Err(BinaryReaderError::new(err, offset));
        }
        if let Some(maximum) = ty.maximum {
            if maximum > true_maximum {
                return Err(BinaryReaderError::new(err, offset));
            }
        }
        if ty.shared {
            if !features.threads {
                return Err(BinaryReaderError::new(
                    "threads must be enabled for shared memories",
                    offset,
                ));
            }
            if ty.maximum.is_none() {
                return Err(BinaryReaderError::new(
                    "shared memory must have maximum size",
                    offset,
                ));
            }
        }
        Ok(())
    }

    pub(crate) fn imports_for_module_type(
        &self,
        offset: usize,
    ) -> Result<IndexMap<(String, String), EntityType>> {
        // Ensure imports are unique, which is a requirement of the component model
        self.imports.iter().map(|((module, name), types)| {
            if types.len() != 1 {
                return Err(BinaryReaderError::new(
                    format!(
                        "module has a duplicate import name `{}:{}` that is not allowed in components",
                        module, name
                    ),
                    offset,
                ));
            }
            Ok(((module.clone(), name.clone()), types[0]))
        }).collect::<Result<_>>()
    }

    fn check_tag_type(
        &self,
        ty: &TagType,
        features: &WasmFeatures,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        if !features.exceptions {
            return Err(BinaryReaderError::new(
                "exceptions proposal not enabled",
                offset,
            ));
        }
        let ty = self.func_type_at(ty.func_type_idx, types, offset)?;
        if ty.returns.len() > 0 {
            return Err(BinaryReaderError::new(
                "invalid exception type: non-empty tag result type",
                offset,
            ));
        }
        Ok(())
    }

    fn check_global_type(
        &self,
        ty: &GlobalType,
        features: &WasmFeatures,
        offset: usize,
    ) -> Result<()> {
        check_value_type(ty.content_type, features, offset)
    }

    fn check_limits<T>(&self, initial: T, maximum: Option<T>, offset: usize) -> Result<()>
    where
        T: Into<u64>,
    {
        if let Some(max) = maximum {
            if initial.into() > max.into() {
                return Err(BinaryReaderError::new(
                    "size minimum must not be greater than maximum",
                    offset,
                ));
            }
        }
        Ok(())
    }

    pub fn max_tables(&self, features: &WasmFeatures) -> usize {
        if features.reference_types {
            MAX_WASM_TABLES
        } else {
            1
        }
    }

    pub fn max_memories(&self, features: &WasmFeatures) -> usize {
        if features.multi_memory {
            MAX_WASM_MEMORIES
        } else {
            1
        }
    }

    pub fn export_to_entity_type(
        &mut self,
        export: &crate::Export,
        offset: usize,
    ) -> Result<EntityType> {
        let check = |ty: &str, index: u32, total: usize| {
            if index as usize >= total {
                Err(BinaryReaderError::new(
                    format!(
                        "unknown {ty} {index}: exported {ty} index out of bounds",
                        index = index,
                        ty = ty,
                    ),
                    offset,
                ))
            } else {
                Ok(())
            }
        };

        Ok(match export.kind {
            ExternalKind::Func => {
                check("function", export.index, self.functions.len())?;
                self.function_references.insert(export.index);
                EntityType::Func(self.types[self.functions[export.index as usize] as usize])
            }
            ExternalKind::Table => {
                check("table", export.index, self.tables.len())?;
                EntityType::Table(self.tables[export.index as usize])
            }
            ExternalKind::Memory => {
                check("memory", export.index, self.memories.len())?;
                EntityType::Memory(self.memories[export.index as usize])
            }
            ExternalKind::Global => {
                check("global", export.index, self.globals.len())?;
                EntityType::Global(self.globals[export.index as usize])
            }
            ExternalKind::Tag => {
                check("tag", export.index, self.tags.len())?;
                EntityType::Tag(self.tags[export.index as usize])
            }
        })
    }

    pub fn get_func_type<'a>(
        &self,
        func_idx: u32,
        types: &'a TypeList,
        offset: usize,
    ) -> Result<&'a FuncType> {
        match self.functions.get(func_idx as usize) {
            Some(idx) => self.func_type_at(*idx, types, offset),
            None => Err(BinaryReaderError::new(
                format!("unknown function {}: func index out of bounds", func_idx),
                offset,
            )),
        }
    }

    fn global_at(&self, idx: u32, offset: usize) -> Result<&GlobalType> {
        match self.globals.get(idx as usize) {
            Some(t) => Ok(t),
            None => Err(BinaryReaderError::new(
                format!("unknown global {}: global index out of bounds", idx,),
                offset,
            )),
        }
    }

    fn table_at(&self, idx: u32, offset: usize) -> Result<&TableType> {
        match self.tables.get(idx as usize) {
            Some(t) => Ok(t),
            None => Err(BinaryReaderError::new(
                format!("unknown table {}: table index out of bounds", idx),
                offset,
            )),
        }
    }

    fn memory_at(&self, idx: u32, offset: usize) -> Result<&MemoryType> {
        match self.memories.get(idx as usize) {
            Some(t) => Ok(t),
            None => Err(BinaryReaderError::new(
                format!("unknown memory {}: memory index out of bounds", idx,),
                offset,
            )),
        }
    }
}

impl Default for Module {
    fn default() -> Self {
        Self {
            snapshot: Default::default(),
            types: Default::default(),
            tables: Default::default(),
            memories: Default::default(),
            globals: Default::default(),
            element_types: Default::default(),
            data_count: Default::default(),
            functions: Default::default(),
            tags: Default::default(),
            function_references: Default::default(),
            imports: Default::default(),
            exports: Default::default(),
            type_size: 1,
            num_imported_globals: Default::default(),
            num_imported_functions: Default::default(),
        }
    }
}

struct OperatorValidatorResources<'a> {
    module: &'a Module,
    types: &'a TypeList,
}

impl WasmModuleResources for OperatorValidatorResources<'_> {
    type FuncType = crate::FuncType;

    fn table_at(&self, at: u32) -> Option<TableType> {
        self.module.tables.get(at as usize).cloned()
    }

    fn memory_at(&self, at: u32) -> Option<MemoryType> {
        self.module.memories.get(at as usize).cloned()
    }

    fn tag_at(&self, at: u32) -> Option<&Self::FuncType> {
        Some(
            self.types[*self.module.tags.get(at as usize)?]
                .as_func_type()
                .unwrap(),
        )
    }

    fn global_at(&self, at: u32) -> Option<GlobalType> {
        self.module.globals.get(at as usize).cloned()
    }

    fn func_type_at(&self, at: u32) -> Option<&Self::FuncType> {
        Some(
            self.types[*self.module.types.get(at as usize)?]
                .as_func_type()
                .unwrap(),
        )
    }

    fn type_of_function(&self, at: u32) -> Option<&Self::FuncType> {
        self.func_type_at(*self.module.functions.get(at as usize)?)
    }

    fn element_type_at(&self, at: u32) -> Option<ValType> {
        self.module.element_types.get(at as usize).cloned()
    }

    fn element_count(&self) -> u32 {
        self.module.element_types.len() as u32
    }

    fn data_count(&self) -> Option<u32> {
        self.module.data_count
    }

    fn is_function_referenced(&self, idx: u32) -> bool {
        self.module.function_references.contains(&idx)
    }
}

/// The implementation of [`WasmModuleResources`] used by
/// [`Validator`](crate::Validator).
pub struct ValidatorResources(pub(crate) Arc<Module>);

impl WasmModuleResources for ValidatorResources {
    type FuncType = crate::FuncType;

    fn table_at(&self, at: u32) -> Option<TableType> {
        self.0.tables.get(at as usize).cloned()
    }

    fn memory_at(&self, at: u32) -> Option<MemoryType> {
        self.0.memories.get(at as usize).cloned()
    }

    fn tag_at(&self, at: u32) -> Option<&Self::FuncType> {
        Some(
            self.0.snapshot.as_ref().unwrap()[*self.0.tags.get(at as usize)?]
                .as_func_type()
                .unwrap(),
        )
    }

    fn global_at(&self, at: u32) -> Option<GlobalType> {
        self.0.globals.get(at as usize).cloned()
    }

    fn func_type_at(&self, at: u32) -> Option<&Self::FuncType> {
        Some(
            self.0.snapshot.as_ref().unwrap()[*self.0.types.get(at as usize)?]
                .as_func_type()
                .unwrap(),
        )
    }

    fn type_of_function(&self, at: u32) -> Option<&Self::FuncType> {
        self.func_type_at(*self.0.functions.get(at as usize)?)
    }

    fn element_type_at(&self, at: u32) -> Option<ValType> {
        self.0.element_types.get(at as usize).cloned()
    }

    fn element_count(&self) -> u32 {
        self.0.element_types.len() as u32
    }

    fn data_count(&self) -> Option<u32> {
        self.0.data_count
    }

    fn is_function_referenced(&self, idx: u32) -> bool {
        self.0.function_references.contains(&idx)
    }
}

const _: () = {
    fn assert_send<T: Send>() {}

    // Assert that `ValidatorResources` is Send so function validation
    // can be parallelizable
    fn assert() {
        assert_send::<ValidatorResources>();
    }
};

mod arc {
    use std::ops::Deref;
    use std::sync::Arc;

    pub struct MaybeOwned<T> {
        owned: bool,
        arc: Arc<T>,
    }

    impl<T> MaybeOwned<T> {
        #[allow(clippy::cast_ref_to_mut)]
        fn as_mut(&mut self) -> Option<&mut T> {
            if !self.owned {
                return None;
            }
            debug_assert!(Arc::get_mut(&mut self.arc).is_some());
            Some(unsafe { &mut *(Arc::as_ptr(&self.arc) as *mut T) })
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
