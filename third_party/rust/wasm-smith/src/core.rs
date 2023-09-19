//! Generating arbitary core Wasm modules.

mod code_builder;
pub(crate) mod encode;
mod terminate;

use crate::{arbitrary_loop, limited_string, unique_string, Config, DefaultConfig};
use arbitrary::{Arbitrary, Result, Unstructured};
use code_builder::CodeBuilderAllocations;
use flagset::{flags, FlagSet};
use std::collections::HashSet;
use std::convert::TryFrom;
use std::marker;
use std::ops::Range;
use std::rc::Rc;
use std::str::{self, FromStr};
use wasm_encoder::{BlockType, ConstExpr, ExportKind, HeapType, RefType, ValType};
pub(crate) use wasm_encoder::{GlobalType, MemoryType, TableType};

// NB: these constants are used to control the rate at which various events
// occur. For more information see where these constants are used. Their values
// are somewhat random in the sense that they're not scientifically determined
// or anything like that, I just threw a bunch of random data at wasm-smith and
// measured various rates of ooms/traps/etc and adjusted these so abnormal
// events were ~1% of the time.
const CHANCE_OFFSET_INBOUNDS: usize = 10; // bigger = less traps
const CHANCE_SEGMENT_ON_EMPTY: usize = 10; // bigger = less traps
const PCT_INBOUNDS: f64 = 0.995; // bigger = less traps

type Instruction = wasm_encoder::Instruction<'static>;

/// A pseudo-random WebAssembly module.
///
/// Construct instances of this type with [the `Arbitrary`
/// trait](https://docs.rs/arbitrary/*/arbitrary/trait.Arbitrary.html).
///
/// ## Configuring Generated Modules
///
/// This uses the [`DefaultConfig`][crate::DefaultConfig] configuration. If you
/// want to customize the shape of generated modules, define your own
/// configuration type, implement the [`Config`][crate::Config] trait for it,
/// and use [`ConfiguredModule<YourConfigType>`][crate::ConfiguredModule]
/// instead of plain `Module`.
#[derive(Debug)]
pub struct Module {
    config: Rc<dyn Config>,
    duplicate_imports_behavior: DuplicateImportsBehavior,
    valtypes: Vec<ValType>,

    /// All types locally defined in this module (available in the type index
    /// space).
    types: Vec<Type>,

    /// Whether we should encode a types section, even if `self.types` is empty.
    should_encode_types: bool,

    /// All of this module's imports. These don't have their own index space,
    /// but instead introduce entries to each imported entity's associated index
    /// space.
    imports: Vec<Import>,

    /// Whether we should encode an imports section, even if `self.imports` is
    /// empty.
    should_encode_imports: bool,

    /// Indices within `types` that are function types.
    func_types: Vec<u32>,

    /// Number of imported items into this module.
    num_imports: usize,

    /// The number of tags defined in this module (not imported or
    /// aliased).
    num_defined_tags: usize,

    /// The number of functions defined in this module (not imported or
    /// aliased).
    num_defined_funcs: usize,

    /// The number of tables defined in this module (not imported or
    /// aliased).
    num_defined_tables: usize,

    /// The number of memories defined in this module (not imported or
    /// aliased).
    num_defined_memories: usize,

    /// The indexes and initialization expressions of globals defined in this
    /// module.
    defined_globals: Vec<(u32, GlobalInitExpr)>,

    /// All tags available to this module, sorted by their index. The list
    /// entry is the type of each tag.
    tags: Vec<TagType>,

    /// All functions available to this module, sorted by their index. The list
    /// entry points to the index in this module where the function type is
    /// defined (if available) and provides the type of the function.
    funcs: Vec<(u32, Rc<FuncType>)>,

    /// All tables available to this module, sorted by their index. The list
    /// entry is the type of each table.
    tables: Vec<TableType>,

    /// All globals available to this module, sorted by their index. The list
    /// entry is the type of each global.
    globals: Vec<GlobalType>,

    /// All memories available to this module, sorted by their index. The list
    /// entry is the type of each memory.
    memories: Vec<MemoryType>,

    exports: Vec<(String, ExportKind, u32)>,
    start: Option<u32>,
    elems: Vec<ElementSegment>,
    code: Vec<Code>,
    data: Vec<DataSegment>,

    /// The predicted size of the effective type of this module, based on this
    /// module's size of the types of imports/exports.
    type_size: u32,

    /// Names currently exported from this module.
    export_names: HashSet<String>,
}

impl<'a> Arbitrary<'a> for Module {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        Ok(ConfiguredModule::<DefaultConfig>::arbitrary(u)?.module)
    }
}

/// A pseudo-random generated WebAssembly file with custom configuration.
///
/// If you don't care about custom configuration, use [`Module`][crate::Module]
/// instead.
///
/// For details on configuring, see the [`Config`][crate::Config] trait.
#[derive(Debug)]
pub struct ConfiguredModule<C> {
    /// The generated module, controlled by the configuration of `C` in the
    /// `Arbitrary` implementation.
    pub module: Module,
    _marker: marker::PhantomData<C>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum DuplicateImportsBehavior {
    Allowed,
    Disallowed,
}

impl Module {
    /// Returns a reference to the internal configuration.
    pub fn config(&self) -> &dyn Config {
        &*self.config
    }

    /// Creates a new `Module` with the specified `config` for
    /// configuration and `Unstructured` for the DNA of this module.
    pub fn new(config: impl Config, u: &mut Unstructured<'_>) -> Result<Self> {
        Self::new_internal(Rc::new(config), u, DuplicateImportsBehavior::Allowed)
    }

    pub(crate) fn new_internal(
        config: Rc<dyn Config>,
        u: &mut Unstructured<'_>,
        duplicate_imports_behavior: DuplicateImportsBehavior,
    ) -> Result<Self> {
        let mut module = Module::empty(config, duplicate_imports_behavior);
        module.build(u, false)?;
        Ok(module)
    }

    fn empty(config: Rc<dyn Config>, duplicate_imports_behavior: DuplicateImportsBehavior) -> Self {
        Module {
            config,
            duplicate_imports_behavior,
            valtypes: Vec::new(),
            types: Vec::new(),
            should_encode_types: false,
            imports: Vec::new(),
            should_encode_imports: false,
            func_types: Vec::new(),
            num_imports: 0,
            num_defined_tags: 0,
            num_defined_funcs: 0,
            num_defined_tables: 0,
            num_defined_memories: 0,
            defined_globals: Vec::new(),
            tags: Vec::new(),
            funcs: Vec::new(),
            tables: Vec::new(),
            globals: Vec::new(),
            memories: Vec::new(),
            exports: Vec::new(),
            start: None,
            elems: Vec::new(),
            code: Vec::new(),
            data: Vec::new(),
            type_size: 0,
            export_names: HashSet::new(),
        }
    }
}

impl<'a, C: Config + Arbitrary<'a>> Arbitrary<'a> for ConfiguredModule<C> {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        Ok(ConfiguredModule {
            module: Module::new(C::arbitrary(u)?, u)?,
            _marker: marker::PhantomData,
        })
    }
}

/// Same as [`Module`], but may be invalid.
///
/// This module generates function bodies differnetly than `Module` to try to
/// better explore wasm decoders and such.
#[derive(Debug)]
pub struct MaybeInvalidModule {
    module: Module,
}

impl MaybeInvalidModule {
    /// Encode this Wasm module into bytes.
    pub fn to_bytes(&self) -> Vec<u8> {
        self.module.to_bytes()
    }
}

impl<'a> Arbitrary<'a> for MaybeInvalidModule {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        let mut module = Module::empty(Rc::new(DefaultConfig), DuplicateImportsBehavior::Allowed);
        module.build(u, true)?;
        Ok(MaybeInvalidModule { module })
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub(crate) enum Type {
    Func(Rc<FuncType>),
}

/// A function signature.
#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub(crate) struct FuncType {
    /// Types of the parameter values.
    pub(crate) params: Vec<ValType>,
    /// Types of the result values.
    pub(crate) results: Vec<ValType>,
}

/// An import of an entity provided externally or by a component.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub(crate) struct Import {
    /// The name of the module providing this entity.
    pub(crate) module: String,
    /// The name of the entity.
    pub(crate) field: String,
    /// The type of this entity.
    pub(crate) entity_type: EntityType,
}

/// Type of an entity.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub(crate) enum EntityType {
    /// A global entity.
    Global(GlobalType),
    /// A table entity.
    Table(TableType),
    /// A memory entity.
    Memory(MemoryType),
    /// A tag entity.
    Tag(TagType),
    /// A function entity.
    Func(u32, Rc<FuncType>),
}

/// Type of a tag.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub(crate) struct TagType {
    /// Index of the function type.
    func_type_idx: u32,
    /// Type of the function.
    func_type: Rc<FuncType>,
}

#[derive(Debug)]
struct ElementSegment {
    kind: ElementKind,
    ty: RefType,
    items: Elements,
}

#[derive(Debug)]
enum ElementKind {
    Passive,
    Declared,
    Active {
        table: Option<u32>, // None == table 0 implicitly
        offset: Offset,
    },
}

#[derive(Debug)]
enum Elements {
    Functions(Vec<u32>),
    Expressions(Vec<Option<u32>>),
}

#[derive(Debug)]
struct Code {
    locals: Vec<ValType>,
    instructions: Instructions,
}

#[derive(Debug)]
enum Instructions {
    Generated(Vec<Instruction>),
    Arbitrary(Vec<u8>),
}

#[derive(Debug)]
struct DataSegment {
    kind: DataSegmentKind,
    init: Vec<u8>,
}

#[derive(Debug)]
enum DataSegmentKind {
    Passive,
    Active { memory_index: u32, offset: Offset },
}

#[derive(Debug)]
pub(crate) enum Offset {
    Const32(i32),
    Const64(i64),
    Global(u32),
}

#[derive(Debug)]
pub(crate) enum GlobalInitExpr {
    FuncRef(u32),
    ConstExpr(ConstExpr),
}

impl Module {
    fn build(&mut self, u: &mut Unstructured, allow_invalid: bool) -> Result<()> {
        self.valtypes = configured_valtypes(&*self.config);

        // We attempt to figure out our available imports *before* creating the types section here,
        // because the types for the imports are already well-known (specified by the user) and we
        // must have those populated for all function/etc. imports, no matter what.
        //
        // This can affect the available capacity for types and such.
        if self.arbitrary_imports_from_available(u)? {
            self.arbitrary_types(u)?;
        } else {
            self.arbitrary_types(u)?;
            self.arbitrary_imports(u)?;
        }

        self.should_encode_types = !self.types.is_empty() || u.arbitrary()?;
        self.should_encode_imports = !self.imports.is_empty() || u.arbitrary()?;

        self.arbitrary_tags(u)?;
        self.arbitrary_funcs(u)?;
        self.arbitrary_tables(u)?;
        self.arbitrary_memories(u)?;
        self.arbitrary_globals(u)?;
        self.arbitrary_exports(u)?;
        self.arbitrary_start(u)?;
        self.arbitrary_elems(u)?;
        self.arbitrary_data(u)?;
        self.arbitrary_code(u, allow_invalid)?;
        Ok(())
    }

    fn arbitrary_types(&mut self, u: &mut Unstructured) -> Result<()> {
        // NB: It isn't guaranteed that `self.types.is_empty()` because when
        // available imports are configured, we may add eagerly specigfic types
        // for the available imports before generating arbitrary types here.
        let min = self.config.min_types().saturating_sub(self.types.len());
        let max = self.config.max_types().saturating_sub(self.types.len());
        arbitrary_loop(u, min, max, |u| {
            let ty = self.arbitrary_type(u)?;
            self.record_type(&ty);
            self.types.push(ty);
            Ok(true)
        })?;
        Ok(())
    }

    fn record_type(&mut self, ty: &Type) {
        let list = match &ty {
            Type::Func(_) => &mut self.func_types,
        };
        list.push(self.types.len() as u32);
    }

    fn arbitrary_type(&mut self, u: &mut Unstructured) -> Result<Type> {
        Ok(Type::Func(self.arbitrary_func_type(u)?))
    }

    fn arbitrary_func_type(&mut self, u: &mut Unstructured) -> Result<Rc<FuncType>> {
        arbitrary_func_type(
            u,
            &self.valtypes,
            if !self.config.multi_value_enabled() {
                Some(1)
            } else {
                None
            },
        )
    }

    fn can_add_local_or_import_tag(&self) -> bool {
        self.config.exceptions_enabled()
            && self.has_tag_func_types()
            && self.tags.len() < self.config.max_tags()
    }

    fn can_add_local_or_import_func(&self) -> bool {
        !self.func_types.is_empty() && self.funcs.len() < self.config.max_funcs()
    }

    fn can_add_local_or_import_table(&self) -> bool {
        self.tables.len() < self.config.max_tables()
    }

    fn can_add_local_or_import_global(&self) -> bool {
        self.globals.len() < self.config.max_globals()
    }

    fn can_add_local_or_import_memory(&self) -> bool {
        self.memories.len() < self.config.max_memories()
    }

    fn arbitrary_imports(&mut self, u: &mut Unstructured) -> Result<()> {
        if self.config.max_type_size() < self.type_size {
            return Ok(());
        }

        let mut import_strings = HashSet::new();
        let mut choices: Vec<fn(&mut Unstructured, &mut Module) -> Result<EntityType>> =
            Vec::with_capacity(5);
        let min = self.config.min_imports().saturating_sub(self.num_imports);
        let max = self.config.max_imports().saturating_sub(self.num_imports);
        arbitrary_loop(u, min, max, |u| {
            choices.clear();
            if self.can_add_local_or_import_tag() {
                choices.push(|u, m| {
                    let ty = m.arbitrary_tag_type(u)?;
                    Ok(EntityType::Tag(ty))
                });
            }
            if self.can_add_local_or_import_func() {
                choices.push(|u, m| {
                    let idx = *u.choose(&m.func_types)?;
                    let ty = m.func_type(idx).clone();
                    Ok(EntityType::Func(idx, ty))
                });
            }
            if self.can_add_local_or_import_global() {
                choices.push(|u, m| {
                    let ty = m.arbitrary_global_type(u)?;
                    Ok(EntityType::Global(ty))
                });
            }
            if self.can_add_local_or_import_memory() {
                choices.push(|u, m| {
                    let ty = arbitrary_memtype(u, m.config())?;
                    Ok(EntityType::Memory(ty))
                });
            }
            if self.can_add_local_or_import_table() {
                choices.push(|u, m| {
                    let ty = arbitrary_table_type(u, m.config())?;
                    Ok(EntityType::Table(ty))
                });
            }

            if choices.is_empty() {
                // We are out of choices. If we have not have reached the
                // minimum yet, then we have no way to satisfy the constraint,
                // but we follow max-constraints before the min-import
                // constraint.
                return Ok(false);
            }

            // Generate a type to import, but only actually add the item if the
            // type size budget allows us to.
            let f = u.choose(&choices)?;
            let entity_type = f(u, self)?;
            let budget = self.config.max_type_size() - self.type_size;
            if entity_type.size() + 1 > budget {
                return Ok(false);
            }
            self.type_size += entity_type.size() + 1;

            // Generate an arbitrary module/name pair to name this import.
            let mut import_pair = unique_import_strings(1_000, u)?;
            if self.duplicate_imports_behavior == DuplicateImportsBehavior::Disallowed {
                while import_strings.contains(&import_pair) {
                    use std::fmt::Write;
                    write!(&mut import_pair.1, "{}", import_strings.len()).unwrap();
                }
                import_strings.insert(import_pair.clone());
            }
            let (module, field) = import_pair;

            // Once our name is determined, then we push the typed item into the
            // appropriate namespace.
            match &entity_type {
                EntityType::Tag(ty) => self.tags.push(ty.clone()),
                EntityType::Func(idx, ty) => self.funcs.push((*idx, ty.clone())),
                EntityType::Global(ty) => self.globals.push(*ty),
                EntityType::Table(ty) => self.tables.push(*ty),
                EntityType::Memory(ty) => self.memories.push(*ty),
            }

            self.num_imports += 1;
            self.imports.push(Import {
                module,
                field,
                entity_type,
            });
            Ok(true)
        })?;

        Ok(())
    }

    /// Generate some arbitrary imports from the list of available imports.
    ///
    /// Returns `true` if there was a list of available imports configured. Otherwise `false` and
    /// the caller should generate arbitrary imports.
    fn arbitrary_imports_from_available(&mut self, u: &mut Unstructured) -> Result<bool> {
        let example_module = if let Some(wasm) = self.config.available_imports() {
            wasm
        } else {
            return Ok(false);
        };

        // First, parse the module-by-example to collect the types and imports.
        //
        // `available_types` will map from a signature index (which is the same as the index into
        // this vector) as it appears in the parsed code, to the type itself as well as to the
        // index in our newly generated module. Initially the option is `None` and will become a
        // `Some` when we encounter an import that uses this signature in the next portion of this
        // function. See also the `make_func_type` closure below.
        let mut available_types = Vec::new();
        let mut available_imports = Vec::<wasmparser::Import>::new();
        for payload in wasmparser::Parser::new(0).parse_all(&example_module) {
            match payload.expect("could not parse the available import payload") {
                wasmparser::Payload::TypeSection(type_reader) => {
                    for ty in type_reader.into_iter_err_on_gc_types() {
                        let ty = ty.expect("could not parse type section");
                        available_types.push((ty, None));
                    }
                }
                wasmparser::Payload::ImportSection(import_reader) => {
                    for im in import_reader {
                        let im = im.expect("could not read import");
                        // We can immediately filter whether this is an import we want to
                        // use.
                        let use_import = u.arbitrary().unwrap_or(false);
                        if !use_import {
                            continue;
                        }
                        available_imports.push(im);
                    }
                }
                _ => {}
            }
        }

        // In this function we need to place imported function/tag types in the types section and
        // generate import entries (which refer to said types) at the same time.
        let max_types = self.config.max_types();
        let multi_value_enabled = self.config.multi_value_enabled();
        let mut new_imports = Vec::with_capacity(available_imports.len());
        let first_type_index = self.types.len();
        let mut new_types = Vec::<Type>::new();

        // Returns the index to the translated type in the to-be type section, and the reference to
        // the type itself.
        let mut make_func_type = |parsed_sig_idx: u32| {
            let serialized_sig_idx = match available_types.get_mut(parsed_sig_idx as usize) {
                None => panic!("signature index refers to a type out of bounds"),
                Some((_, Some(idx))) => *idx as usize,
                Some((func_type, index_store)) => {
                    let multi_value_required = func_type.results().len() > 1;
                    let new_index = first_type_index + new_types.len();
                    if new_index >= max_types || (multi_value_required && !multi_value_enabled) {
                        return None;
                    }
                    let func_type = Rc::new(FuncType {
                        params: func_type
                            .params()
                            .iter()
                            .map(|t| convert_type(*t))
                            .collect(),
                        results: func_type
                            .results()
                            .iter()
                            .map(|t| convert_type(*t))
                            .collect(),
                    });
                    index_store.replace(new_index as u32);
                    new_types.push(Type::Func(Rc::clone(&func_type)));
                    new_index
                }
            };
            match &new_types[serialized_sig_idx - first_type_index] {
                Type::Func(f) => Some((serialized_sig_idx as u32, Rc::clone(f))),
            }
        };

        for import in available_imports {
            let type_size_budget = self.config.max_type_size() - self.type_size;
            let entity_type = match &import.ty {
                wasmparser::TypeRef::Func(sig_idx) => {
                    if self.funcs.len() >= self.config.max_funcs() {
                        continue;
                    } else if let Some((sig_idx, func_type)) = make_func_type(*sig_idx) {
                        let entity = EntityType::Func(sig_idx as u32, Rc::clone(&func_type));
                        if type_size_budget < entity.size() {
                            continue;
                        }
                        self.funcs.push((sig_idx, func_type));
                        entity
                    } else {
                        continue;
                    }
                }

                wasmparser::TypeRef::Tag(wasmparser::TagType { func_type_idx, .. }) => {
                    let can_add_tag = self.tags.len() < self.config.max_tags();
                    if !self.config.exceptions_enabled() || !can_add_tag {
                        continue;
                    } else if let Some((sig_idx, func_type)) = make_func_type(*func_type_idx) {
                        let tag_type = TagType {
                            func_type_idx: sig_idx,
                            func_type,
                        };
                        let entity = EntityType::Tag(tag_type.clone());
                        if type_size_budget < entity.size() {
                            continue;
                        }
                        self.tags.push(tag_type);
                        entity
                    } else {
                        continue;
                    }
                }

                wasmparser::TypeRef::Table(table_ty) => {
                    let table_ty = TableType {
                        element_type: convert_reftype(table_ty.element_type),
                        minimum: table_ty.initial,
                        maximum: table_ty.maximum,
                    };
                    let entity = EntityType::Table(table_ty);
                    let type_size = entity.size();
                    if type_size_budget < type_size || !self.can_add_local_or_import_table() {
                        continue;
                    }
                    self.type_size += type_size;
                    self.tables.push(table_ty);
                    entity
                }

                wasmparser::TypeRef::Memory(memory_ty) => {
                    let memory_ty = MemoryType {
                        minimum: memory_ty.initial,
                        maximum: memory_ty.maximum,
                        memory64: memory_ty.memory64,
                        shared: memory_ty.shared,
                    };
                    let entity = EntityType::Memory(memory_ty);
                    let type_size = entity.size();
                    if type_size_budget < type_size || !self.can_add_local_or_import_memory() {
                        continue;
                    }
                    self.type_size += type_size;
                    self.memories.push(memory_ty);
                    entity
                }

                wasmparser::TypeRef::Global(global_ty) => {
                    let global_ty = GlobalType {
                        val_type: convert_type(global_ty.content_type),
                        mutable: global_ty.mutable,
                    };
                    let entity = EntityType::Global(global_ty);
                    let type_size = entity.size();
                    if type_size_budget < type_size || !self.can_add_local_or_import_global() {
                        continue;
                    }
                    self.type_size += type_size;
                    self.globals.push(global_ty);
                    entity
                }
            };
            new_imports.push(Import {
                module: import.module.to_string(),
                field: import.name.to_string(),
                entity_type,
            });
            self.num_imports += 1;
        }

        // Finally, add the entities we just generated.
        self.types.extend(new_types);
        self.imports.extend(new_imports);

        Ok(true)
    }

    fn type_of(&self, kind: ExportKind, index: u32) -> EntityType {
        match kind {
            ExportKind::Global => EntityType::Global(self.globals[index as usize]),
            ExportKind::Memory => EntityType::Memory(self.memories[index as usize]),
            ExportKind::Table => EntityType::Table(self.tables[index as usize]),
            ExportKind::Func => {
                let (_idx, ty) = &self.funcs[index as usize];
                EntityType::Func(u32::max_value(), ty.clone())
            }
            ExportKind::Tag => EntityType::Tag(self.tags[index as usize].clone()),
        }
    }

    fn ty(&self, idx: u32) -> &Type {
        &self.types[idx as usize]
    }

    fn func_types(&self) -> impl Iterator<Item = (u32, &FuncType)> + '_ {
        self.func_types
            .iter()
            .copied()
            .map(move |type_i| (type_i, &**self.func_type(type_i)))
    }

    fn func_type(&self, idx: u32) -> &Rc<FuncType> {
        match self.ty(idx) {
            Type::Func(f) => f,
        }
    }

    fn tags(&self) -> impl Iterator<Item = (u32, &TagType)> + '_ {
        self.tags
            .iter()
            .enumerate()
            .map(move |(i, ty)| (i as u32, ty))
    }

    fn funcs(&self) -> impl Iterator<Item = (u32, &Rc<FuncType>)> + '_ {
        self.funcs
            .iter()
            .enumerate()
            .map(move |(i, (_, ty))| (i as u32, ty))
    }

    fn has_tag_func_types(&self) -> bool {
        self.tag_func_types().next().is_some()
    }

    fn tag_func_types(&self) -> impl Iterator<Item = u32> + '_ {
        self.func_types
            .iter()
            .copied()
            .filter(move |i| self.func_type(*i).results.is_empty())
    }

    fn arbitrary_valtype(&self, u: &mut Unstructured) -> Result<ValType> {
        Ok(*u.choose(&self.valtypes)?)
    }

    fn arbitrary_global_type(&self, u: &mut Unstructured) -> Result<GlobalType> {
        Ok(GlobalType {
            val_type: self.arbitrary_valtype(u)?,
            mutable: u.arbitrary()?,
        })
    }

    fn arbitrary_tag_type(&self, u: &mut Unstructured) -> Result<TagType> {
        let candidate_func_types: Vec<_> = self.tag_func_types().collect();
        arbitrary_tag_type(u, &candidate_func_types, |ty_idx| {
            self.func_type(ty_idx).clone()
        })
    }

    fn arbitrary_tags(&mut self, u: &mut Unstructured) -> Result<()> {
        if !self.config.exceptions_enabled() || !self.has_tag_func_types() {
            return Ok(());
        }

        arbitrary_loop(u, self.config.min_tags(), self.config.max_tags(), |u| {
            if !self.can_add_local_or_import_tag() {
                return Ok(false);
            }
            self.tags.push(self.arbitrary_tag_type(u)?);
            self.num_defined_tags += 1;
            Ok(true)
        })
    }

    fn arbitrary_funcs(&mut self, u: &mut Unstructured) -> Result<()> {
        if self.func_types.is_empty() {
            return Ok(());
        }

        arbitrary_loop(u, self.config.min_funcs(), self.config.max_funcs(), |u| {
            if !self.can_add_local_or_import_func() {
                return Ok(false);
            }
            let max = self.func_types.len() - 1;
            let ty = self.func_types[u.int_in_range(0..=max)?];
            self.funcs.push((ty, self.func_type(ty).clone()));
            self.num_defined_funcs += 1;
            Ok(true)
        })
    }

    fn arbitrary_tables(&mut self, u: &mut Unstructured) -> Result<()> {
        arbitrary_loop(
            u,
            self.config.min_tables() as usize,
            self.config.max_tables() as usize,
            |u| {
                if !self.can_add_local_or_import_table() {
                    return Ok(false);
                }
                self.num_defined_tables += 1;
                let ty = arbitrary_table_type(u, self.config())?;
                self.tables.push(ty);
                Ok(true)
            },
        )
    }

    fn arbitrary_memories(&mut self, u: &mut Unstructured) -> Result<()> {
        arbitrary_loop(
            u,
            self.config.min_memories() as usize,
            self.config.max_memories() as usize,
            |u| {
                if !self.can_add_local_or_import_memory() {
                    return Ok(false);
                }
                self.num_defined_memories += 1;
                self.memories.push(arbitrary_memtype(u, self.config())?);
                Ok(true)
            },
        )
    }

    fn arbitrary_globals(&mut self, u: &mut Unstructured) -> Result<()> {
        let mut choices: Vec<Box<dyn Fn(&mut Unstructured, ValType) -> Result<GlobalInitExpr>>> =
            vec![];
        let num_imported_globals = self.globals.len();

        arbitrary_loop(
            u,
            self.config.min_globals(),
            self.config.max_globals(),
            |u| {
                if !self.can_add_local_or_import_global() {
                    return Ok(false);
                }

                let ty = self.arbitrary_global_type(u)?;

                choices.clear();
                let num_funcs = self.funcs.len() as u32;
                choices.push(Box::new(move |u, ty| {
                    Ok(GlobalInitExpr::ConstExpr(match ty {
                        ValType::I32 => ConstExpr::i32_const(u.arbitrary()?),
                        ValType::I64 => ConstExpr::i64_const(u.arbitrary()?),
                        ValType::F32 => ConstExpr::f32_const(u.arbitrary()?),
                        ValType::F64 => ConstExpr::f64_const(u.arbitrary()?),
                        ValType::V128 => ConstExpr::v128_const(u.arbitrary()?),
                        ValType::Ref(ty) => {
                            assert!(ty.nullable);
                            if ty.heap_type == HeapType::Func && num_funcs > 0 && u.arbitrary()? {
                                let func = u.int_in_range(0..=num_funcs - 1)?;
                                return Ok(GlobalInitExpr::FuncRef(func));
                            }
                            ConstExpr::ref_null(ty.heap_type)
                        }
                    }))
                }));

                for (i, g) in self.globals[..num_imported_globals].iter().enumerate() {
                    if !g.mutable && g.val_type == ty.val_type {
                        choices.push(Box::new(move |_, _| {
                            Ok(GlobalInitExpr::ConstExpr(ConstExpr::global_get(i as u32)))
                        }));
                    }
                }

                let f = u.choose(&choices)?;
                let expr = f(u, ty.val_type)?;
                let global_idx = self.globals.len() as u32;
                self.globals.push(ty);
                self.defined_globals.push((global_idx, expr));
                Ok(true)
            },
        )
    }

    fn arbitrary_exports(&mut self, u: &mut Unstructured) -> Result<()> {
        if self.config.max_type_size() < self.type_size && !self.config.export_everything() {
            return Ok(());
        }

        // Build up a list of candidates for each class of import
        let mut choices: Vec<Vec<(ExportKind, u32)>> = Vec::with_capacity(6);
        choices.push(
            (0..self.funcs.len())
                .map(|i| (ExportKind::Func, i as u32))
                .collect(),
        );
        choices.push(
            (0..self.tables.len())
                .map(|i| (ExportKind::Table, i as u32))
                .collect(),
        );
        choices.push(
            (0..self.memories.len())
                .map(|i| (ExportKind::Memory, i as u32))
                .collect(),
        );
        choices.push(
            (0..self.globals.len())
                .map(|i| (ExportKind::Global, i as u32))
                .collect(),
        );

        // If the configuration demands exporting everything, we do so here and
        // early-return.
        if self.config.export_everything() {
            for choices_by_kind in choices {
                for (kind, idx) in choices_by_kind {
                    let name = unique_string(1_000, &mut self.export_names, u)?;
                    self.add_arbitrary_export(name, kind, idx)?;
                }
            }
            return Ok(());
        }

        arbitrary_loop(
            u,
            self.config.min_exports(),
            self.config.max_exports(),
            |u| {
                // Remove all candidates for export whose type size exceeds our
                // remaining budget for type size. Then also remove any classes
                // of exports which no longer have any candidates.
                //
                // If there's nothing remaining after this, then we're done.
                let max_size = self.config.max_type_size() - self.type_size;
                for list in choices.iter_mut() {
                    list.retain(|(kind, idx)| self.type_of(*kind, *idx).size() + 1 < max_size);
                }
                choices.retain(|list| !list.is_empty());
                if choices.is_empty() {
                    return Ok(false);
                }

                // Pick a name, then pick the export, and then we can record
                // information about the chosen export.
                let name = unique_string(1_000, &mut self.export_names, u)?;
                let list = u.choose(&choices)?;
                let (kind, idx) = *u.choose(list)?;
                self.add_arbitrary_export(name, kind, idx)?;
                Ok(true)
            },
        )
    }

    fn add_arbitrary_export(&mut self, name: String, kind: ExportKind, idx: u32) -> Result<()> {
        let ty = self.type_of(kind, idx);
        self.type_size += 1 + ty.size();
        if self.type_size <= self.config.max_type_size() {
            self.exports.push((name, kind, idx));
            Ok(())
        } else {
            // If our addition of exports takes us above the allowed number of
            // types, we fail; this error code is not the most illustrative of
            // the cause but is the best available from `arbitrary`.
            Err(arbitrary::Error::IncorrectFormat)
        }
    }

    fn arbitrary_start(&mut self, u: &mut Unstructured) -> Result<()> {
        if !self.config.allow_start_export() {
            return Ok(());
        }

        let mut choices = Vec::with_capacity(self.funcs.len() as usize);

        for (func_idx, ty) in self.funcs() {
            if ty.params.is_empty() && ty.results.is_empty() {
                choices.push(func_idx);
            }
        }

        if !choices.is_empty() && u.arbitrary().unwrap_or(false) {
            let f = *u.choose(&choices)?;
            self.start = Some(f);
        }

        Ok(())
    }

    fn arbitrary_elems(&mut self, u: &mut Unstructured) -> Result<()> {
        let func_max = self.funcs.len() as u32;

        // Create a helper closure to choose an arbitrary offset.
        let mut offset_global_choices = vec![];
        if !self.config.disallow_traps() {
            for (i, g) in self.globals[..self.globals.len() - self.defined_globals.len()]
                .iter()
                .enumerate()
            {
                if !g.mutable && g.val_type == ValType::I32 {
                    offset_global_choices.push(i as u32);
                }
            }
        }
        let arbitrary_active_elem = |u: &mut Unstructured,
                                     min_mem_size: u32,
                                     table: Option<u32>,
                                     disallow_traps: bool,
                                     table_ty: &TableType| {
            let (offset, max_size_hint) = if !offset_global_choices.is_empty() && u.arbitrary()? {
                let g = u.choose(&offset_global_choices)?;
                (Offset::Global(*g), None)
            } else {
                let max_mem_size = if disallow_traps {
                    table_ty.minimum
                } else {
                    u32::MAX
                };
                let offset =
                    arbitrary_offset(u, min_mem_size.into(), max_mem_size.into(), 0)? as u32;
                let max_size_hint = if disallow_traps
                    || (offset <= min_mem_size && u.int_in_range(0..=CHANCE_OFFSET_INBOUNDS)? != 0)
                {
                    Some(min_mem_size - offset)
                } else {
                    None
                };
                (Offset::Const32(offset as i32), max_size_hint)
            };
            Ok((ElementKind::Active { table, offset }, max_size_hint))
        };

        type GenElemSegment<'a> =
            dyn Fn(&mut Unstructured) -> Result<(ElementKind, Option<u32>)> + 'a;
        let mut funcrefs: Vec<Box<GenElemSegment>> = Vec::new();
        let mut externrefs: Vec<Box<GenElemSegment>> = Vec::new();
        let disallow_traps = self.config().disallow_traps();
        for (i, ty) in self.tables.iter().enumerate() {
            // If this table starts with no capacity then any non-empty element
            // segment placed onto it will immediately trap, which isn't too
            // too interesting. If that's the case give it an unlikely chance
            // of proceeding.
            if ty.minimum == 0 && u.int_in_range(0..=CHANCE_SEGMENT_ON_EMPTY)? != 0 {
                continue;
            }

            let dst = if ty.element_type == RefType::FUNCREF {
                &mut funcrefs
            } else {
                &mut externrefs
            };
            let minimum = ty.minimum;
            // If the first table is a funcref table then it's a candidate for
            // the MVP encoding of element segments.
            if i == 0 && ty.element_type == RefType::FUNCREF {
                dst.push(Box::new(move |u| {
                    arbitrary_active_elem(u, minimum, None, disallow_traps, ty)
                }));
            }
            if self.config.bulk_memory_enabled() {
                let idx = Some(i as u32);
                dst.push(Box::new(move |u| {
                    arbitrary_active_elem(u, minimum, idx, disallow_traps, ty)
                }));
            }
        }

        // Reference types allows us to create passive and declared element
        // segments.
        if self.config.reference_types_enabled() {
            funcrefs.push(Box::new(|_| Ok((ElementKind::Passive, None))));
            externrefs.push(Box::new(|_| Ok((ElementKind::Passive, None))));
            funcrefs.push(Box::new(|_| Ok((ElementKind::Declared, None))));
            externrefs.push(Box::new(|_| Ok((ElementKind::Declared, None))));
        }

        let mut choices = Vec::new();
        if !funcrefs.is_empty() {
            choices.push((&funcrefs, RefType::FUNCREF));
        }
        if !externrefs.is_empty() {
            choices.push((&externrefs, RefType::EXTERNREF));
        }

        if choices.is_empty() {
            return Ok(());
        }
        arbitrary_loop(
            u,
            self.config.min_element_segments(),
            self.config.max_element_segments(),
            |u| {
                // Choose whether to generate a segment whose elements are initialized via
                // expressions, or one whose elements are initialized via function indices.
                let (kind_candidates, ty) = *u.choose(&choices)?;

                // Select a kind for this segment now that we know the number of
                // items the segment will hold.
                let (kind, max_size_hint) = u.choose(kind_candidates)?(u)?;
                let max = max_size_hint
                    .map(|i| usize::try_from(i).unwrap())
                    .unwrap_or_else(|| self.config.max_elements());

                // Pick whether we're going to use expression elements or
                // indices. Note that externrefs must use expressions,
                // and functions without reference types must use indices.
                let items = if ty == RefType::EXTERNREF
                    || (self.config.reference_types_enabled() && u.arbitrary()?)
                {
                    let mut init = vec![];
                    arbitrary_loop(u, self.config.min_elements(), max, |u| {
                        init.push(
                            if ty == RefType::EXTERNREF || func_max == 0 || u.arbitrary()? {
                                None
                            } else {
                                Some(u.int_in_range(0..=func_max - 1)?)
                            },
                        );
                        Ok(true)
                    })?;
                    Elements::Expressions(init)
                } else {
                    let mut init = vec![];
                    if func_max > 0 {
                        arbitrary_loop(u, self.config.min_elements(), max, |u| {
                            let func_idx = u.int_in_range(0..=func_max - 1)?;
                            init.push(func_idx);
                            Ok(true)
                        })?;
                    }
                    Elements::Functions(init)
                };

                self.elems.push(ElementSegment { kind, ty, items });
                Ok(true)
            },
        )
    }

    fn arbitrary_code(&mut self, u: &mut Unstructured, allow_invalid: bool) -> Result<()> {
        self.code.reserve(self.num_defined_funcs);
        let mut allocs = CodeBuilderAllocations::new(self);
        for (_, ty) in self.funcs[self.funcs.len() - self.num_defined_funcs..].iter() {
            let body = self.arbitrary_func_body(u, ty, &mut allocs, allow_invalid)?;
            self.code.push(body);
        }
        allocs.finish(u, self)?;
        Ok(())
    }

    fn arbitrary_func_body(
        &self,
        u: &mut Unstructured,
        ty: &FuncType,
        allocs: &mut CodeBuilderAllocations,
        allow_invalid: bool,
    ) -> Result<Code> {
        let mut locals = self.arbitrary_locals(u)?;
        let builder = allocs.builder(ty, &mut locals);
        let instructions = if allow_invalid && u.arbitrary().unwrap_or(false) {
            Instructions::Arbitrary(arbitrary_vec_u8(u)?)
        } else {
            Instructions::Generated(builder.arbitrary(u, self)?)
        };

        Ok(Code {
            locals,
            instructions,
        })
    }

    fn arbitrary_locals(&self, u: &mut Unstructured) -> Result<Vec<ValType>> {
        let mut ret = Vec::new();
        arbitrary_loop(u, 0, 100, |u| {
            ret.push(self.arbitrary_valtype(u)?);
            Ok(true)
        })?;
        Ok(ret)
    }

    fn arbitrary_data(&mut self, u: &mut Unstructured) -> Result<()> {
        // With bulk-memory we can generate passive data, otherwise if there are
        // no memories we can't generate any data.
        let memories = self.memories.len() as u32;
        if memories == 0 && !self.config.bulk_memory_enabled() {
            return Ok(());
        }
        let disallow_traps = self.config.disallow_traps();
        let mut choices32: Vec<Box<dyn Fn(&mut Unstructured, u64, usize) -> Result<Offset>>> =
            vec![];
        choices32.push(Box::new(|u, min_size, data_len| {
            let min = u32::try_from(min_size.saturating_mul(64 * 1024))
                .unwrap_or(u32::MAX)
                .into();
            let max = if disallow_traps { min } else { u32::MAX.into() };
            Ok(Offset::Const32(
                arbitrary_offset(u, min, max, data_len)? as i32
            ))
        }));
        let mut choices64: Vec<Box<dyn Fn(&mut Unstructured, u64, usize) -> Result<Offset>>> =
            vec![];
        choices64.push(Box::new(|u, min_size, data_len| {
            let min = min_size.saturating_mul(64 * 1024);
            let max = if disallow_traps { min } else { u64::MAX };
            Ok(Offset::Const64(
                arbitrary_offset(u, min, max, data_len)? as i64
            ))
        }));
        if !self.config().disallow_traps() {
            for (i, g) in self.globals[..self.globals.len() - self.defined_globals.len()]
                .iter()
                .enumerate()
            {
                if g.mutable {
                    continue;
                }
                if g.val_type == ValType::I32 {
                    choices32.push(Box::new(move |_, _, _| Ok(Offset::Global(i as u32))));
                } else if g.val_type == ValType::I64 {
                    choices64.push(Box::new(move |_, _, _| Ok(Offset::Global(i as u32))));
                }
            }
        }

        // Build a list of candidate memories that we'll add data initializers
        // for. If a memory doesn't have an initial size then any initializers
        // for that memory will trap instantiation, which isn't too
        // interesting. Try to make this happen less often by making it less
        // likely that a memory with 0 size will have a data segment.
        let mut memories = Vec::new();
        for (i, mem) in self.memories.iter().enumerate() {
            if mem.minimum > 0 || u.int_in_range(0..=CHANCE_SEGMENT_ON_EMPTY)? == 0 {
                memories.push(i as u32);
            }
        }

        // With memories we can generate data segments, and with bulk memory we
        // can generate passive segments. Without these though we can't create
        // a valid module with data segments.
        if memories.is_empty() && !self.config.bulk_memory_enabled() {
            return Ok(());
        }

        arbitrary_loop(
            u,
            self.config.min_data_segments(),
            self.config.max_data_segments(),
            |u| {
                let mut init: Vec<u8> = u.arbitrary()?;

                // Passive data can only be generated if bulk memory is enabled.
                // Otherwise if there are no memories we *only* generate passive
                // data. Finally if all conditions are met we use an input byte to
                // determine if it should be passive or active.
                let kind = if self.config.bulk_memory_enabled()
                    && (memories.is_empty() || u.arbitrary()?)
                {
                    DataSegmentKind::Passive
                } else {
                    let memory_index = *u.choose(&memories)?;
                    let mem = &self.memories[memory_index as usize];
                    let f = if mem.memory64 {
                        u.choose(&choices64)?
                    } else {
                        u.choose(&choices32)?
                    };
                    let mut offset = f(u, mem.minimum, init.len())?;

                    // If traps are disallowed then truncate the size of the
                    // data segment to the minimum size of memory to guarantee
                    // it will fit. Afterwards ensure that the offset of the
                    // data segment is in-bounds by clamping it to the
                    if self.config.disallow_traps() {
                        let max_size = (u64::MAX / 64 / 1024).min(mem.minimum) * 64 * 1024;
                        init.truncate(max_size as usize);
                        let max_offset = max_size - init.len() as u64;
                        match &mut offset {
                            Offset::Const32(x) => {
                                *x = (*x as u64).min(max_offset) as i32;
                            }
                            Offset::Const64(x) => {
                                *x = (*x as u64).min(max_offset) as i64;
                            }
                            Offset::Global(_) => unreachable!(),
                        }
                    }
                    DataSegmentKind::Active {
                        offset,
                        memory_index,
                    }
                };
                self.data.push(DataSegment { kind, init });
                Ok(true)
            },
        )
    }

    fn params_results(&self, ty: &BlockType) -> (Vec<ValType>, Vec<ValType>) {
        match ty {
            BlockType::Empty => (vec![], vec![]),
            BlockType::Result(t) => (vec![], vec![*t]),
            BlockType::FunctionType(ty) => {
                let ty = self.func_type(*ty);
                (ty.params.to_vec(), ty.results.to_vec())
            }
        }
    }
}

pub(crate) fn arbitrary_limits32(
    u: &mut Unstructured,
    min_minimum: Option<u32>,
    max_minimum: u32,
    max_required: bool,
    max_inbounds: u32,
) -> Result<(u32, Option<u32>)> {
    let (min, max) = arbitrary_limits64(
        u,
        min_minimum.map(Into::into),
        max_minimum.into(),
        max_required,
        max_inbounds.into(),
    )?;
    Ok((
        u32::try_from(min).unwrap(),
        max.map(|i| u32::try_from(i).unwrap()),
    ))
}

pub(crate) fn arbitrary_limits64(
    u: &mut Unstructured,
    min_minimum: Option<u64>,
    max_minimum: u64,
    max_required: bool,
    max_inbounds: u64,
) -> Result<(u64, Option<u64>)> {
    let min = gradually_grow(u, min_minimum.unwrap_or(0), max_inbounds, max_minimum)?;
    let max = if max_required || u.arbitrary().unwrap_or(false) {
        Some(u.int_in_range(min..=max_minimum)?)
    } else {
        None
    };
    Ok((min, max))
}

pub(crate) fn configured_valtypes(config: &dyn Config) -> Vec<ValType> {
    let mut valtypes = Vec::with_capacity(7);
    valtypes.push(ValType::I32);
    valtypes.push(ValType::I64);
    valtypes.push(ValType::F32);
    valtypes.push(ValType::F64);
    if config.simd_enabled() {
        valtypes.push(ValType::V128);
    }
    if config.reference_types_enabled() {
        valtypes.push(ValType::EXTERNREF);
        valtypes.push(ValType::FUNCREF);
    }
    valtypes
}

pub(crate) fn arbitrary_func_type(
    u: &mut Unstructured,
    valtypes: &[ValType],
    max_results: Option<usize>,
) -> Result<Rc<FuncType>> {
    let mut params = vec![];
    let mut results = vec![];
    arbitrary_loop(u, 0, 20, |u| {
        params.push(arbitrary_valtype(u, valtypes)?);
        Ok(true)
    })?;
    arbitrary_loop(u, 0, max_results.unwrap_or(20), |u| {
        results.push(arbitrary_valtype(u, valtypes)?);
        Ok(true)
    })?;
    Ok(Rc::new(FuncType { params, results }))
}

fn arbitrary_valtype(u: &mut Unstructured, valtypes: &[ValType]) -> Result<ValType> {
    Ok(*u.choose(valtypes)?)
}

pub(crate) fn arbitrary_table_type(u: &mut Unstructured, config: &dyn Config) -> Result<TableType> {
    // We don't want to generate tables that are too large on average, so
    // keep the "inbounds" limit here a bit smaller.
    let max_inbounds = 10_000;
    let min_elements = if config.disallow_traps() {
        Some(1)
    } else {
        None
    };
    let max_elements = min_elements.unwrap_or(0).max(config.max_table_elements());
    let (minimum, maximum) = arbitrary_limits32(
        u,
        min_elements,
        max_elements,
        config.table_max_size_required(),
        max_inbounds.min(max_elements),
    )?;
    if config.disallow_traps() {
        assert!(minimum > 0);
    }
    Ok(TableType {
        element_type: if config.reference_types_enabled() {
            *u.choose(&[RefType::FUNCREF, RefType::EXTERNREF])?
        } else {
            RefType::FUNCREF
        },
        minimum,
        maximum,
    })
}

pub(crate) fn arbitrary_memtype(u: &mut Unstructured, config: &dyn Config) -> Result<MemoryType> {
    // When threads are enabled, we only want to generate shared memories about
    // 25% of the time.
    let shared = config.threads_enabled() && u.ratio(1, 4)?;
    // We want to favor memories <= 1gb in size, allocate at most 16k pages,
    // depending on the maximum number of memories.
    let memory64 = config.memory64_enabled() && u.arbitrary()?;
    let max_inbounds = 16 * 1024 / u64::try_from(config.max_memories()).unwrap();
    let min_pages = if config.disallow_traps() {
        Some(1)
    } else {
        None
    };
    let max_pages = min_pages
        .unwrap_or(0)
        .max(config.max_memory_pages(memory64));
    let (minimum, maximum) = arbitrary_limits64(
        u,
        min_pages,
        max_pages,
        config.memory_max_size_required() || shared,
        max_inbounds.min(max_pages),
    )?;
    Ok(MemoryType {
        minimum,
        maximum,
        memory64,
        shared,
    })
}

pub(crate) fn arbitrary_tag_type(
    u: &mut Unstructured,
    candidate_func_types: &[u32],
    get_func_type: impl FnOnce(u32) -> Rc<FuncType>,
) -> Result<TagType> {
    let max = candidate_func_types.len() - 1;
    let ty = candidate_func_types[u.int_in_range(0..=max)?];
    Ok(TagType {
        func_type_idx: ty,
        func_type: get_func_type(ty),
    })
}

/// This function generates a number between `min` and `max`, favoring values
/// between `min` and `max_inbounds`.
///
/// The thinking behind this function is that it's used for things like offsets
/// and minimum sizes which, when very large, can trivially make the wasm oom or
/// abort with a trap. This isn't the most interesting thing to do so it tries
/// to favor numbers in the `min..max_inbounds` range to avoid immediate ooms.
fn gradually_grow(u: &mut Unstructured, min: u64, max_inbounds: u64, max: u64) -> Result<u64> {
    if min == max {
        return Ok(min);
    }
    let min = min as f64;
    let max = max as f64;
    let max_inbounds = max_inbounds as f64;
    let x = u.arbitrary::<u32>()?;
    let x = f64::from(x);
    let x = map_custom(
        x,
        f64::from(u32::MIN)..f64::from(u32::MAX),
        min..max_inbounds,
        min..max,
    );
    return Ok(x.round() as u64);

    /// Map a value from within the input range to the output range(s).
    ///
    /// This will first map the input range into the `0..1` input range, and
    /// then depending on the value it will either map it exponentially
    /// (favoring small values) into the `output_inbounds` range or it will map
    /// it into the `output` range.
    fn map_custom(
        value: f64,
        input: Range<f64>,
        output_inbounds: Range<f64>,
        output: Range<f64>,
    ) -> f64 {
        assert!(!value.is_nan(), "{}", value);
        assert!(value.is_finite(), "{}", value);
        assert!(input.start < input.end, "{} < {}", input.start, input.end);
        assert!(
            output.start < output.end,
            "{} < {}",
            output.start,
            output.end
        );
        assert!(value >= input.start, "{} >= {}", value, input.start);
        assert!(value <= input.end, "{} <= {}", value, input.end);
        assert!(
            output.start <= output_inbounds.start,
            "{} <= {}",
            output.start,
            output_inbounds.start
        );
        assert!(
            output_inbounds.end <= output.end,
            "{} <= {}",
            output_inbounds.end,
            output.end
        );

        let x = map_linear(value, input, 0.0..1.0);
        let result = if x < PCT_INBOUNDS {
            if output_inbounds.start == output_inbounds.end {
                output_inbounds.start
            } else {
                let unscaled = x * x * x * x * x * x;
                map_linear(unscaled, 0.0..1.0, output_inbounds)
            }
        } else {
            map_linear(x, 0.0..1.0, output.clone())
        };

        assert!(result >= output.start, "{} >= {}", result, output.start);
        assert!(result <= output.end, "{} <= {}", result, output.end);
        result
    }

    /// Map a value from within the input range linearly to the output range.
    ///
    /// For example, mapping `0.5` from the input range `0.0..1.0` to the output
    /// range `1.0..3.0` produces `2.0`.
    fn map_linear(
        value: f64,
        Range {
            start: in_low,
            end: in_high,
        }: Range<f64>,
        Range {
            start: out_low,
            end: out_high,
        }: Range<f64>,
    ) -> f64 {
        assert!(!value.is_nan(), "{}", value);
        assert!(value.is_finite(), "{}", value);
        assert!(in_low < in_high, "{} < {}", in_low, in_high);
        assert!(out_low < out_high, "{} < {}", out_low, out_high);
        assert!(value >= in_low, "{} >= {}", value, in_low);
        assert!(value <= in_high, "{} <= {}", value, in_high);

        let dividend = out_high - out_low;
        let divisor = in_high - in_low;
        let slope = dividend / divisor;
        let result = out_low + (slope * (value - in_low));

        assert!(result >= out_low, "{} >= {}", result, out_low);
        assert!(result <= out_high, "{} <= {}", result, out_high);
        result
    }
}

/// Selects a reasonable offset for an element or data segment. This favors
/// having the segment being in-bounds, but it may still generate
/// any offset.
fn arbitrary_offset(
    u: &mut Unstructured,
    limit_min: u64,
    limit_max: u64,
    segment_size: usize,
) -> Result<u64> {
    let size = u64::try_from(segment_size).unwrap();

    // If the segment is too big for the whole memory, just give it any
    // offset.
    if size > limit_min {
        u.int_in_range(0..=limit_max)
    } else {
        gradually_grow(u, 0, limit_min - size, limit_max)
    }
}

fn unique_import_strings(max_size: usize, u: &mut Unstructured) -> Result<(String, String)> {
    let module = limited_string(max_size, u)?;
    let field = limited_string(max_size, u)?;
    Ok((module, field))
}

fn arbitrary_vec_u8(u: &mut Unstructured) -> Result<Vec<u8>> {
    let size = u.arbitrary_len::<u8>()?;
    Ok(u.bytes(size)?.to_vec())
}

/// Convert a wasmparser's `ValType` to a `wasm_encoder::ValType`.
fn convert_type(parsed_type: wasmparser::ValType) -> ValType {
    use wasmparser::ValType::*;
    match parsed_type {
        I32 => ValType::I32,
        I64 => ValType::I64,
        F32 => ValType::F32,
        F64 => ValType::F64,
        V128 => ValType::V128,
        Ref(ty) => ValType::Ref(convert_reftype(ty)),
    }
}

fn convert_reftype(ty: wasmparser::RefType) -> RefType {
    wasm_encoder::RefType {
        nullable: ty.is_nullable(),
        heap_type: match ty.heap_type() {
            wasmparser::HeapType::Func => HeapType::Func,
            wasmparser::HeapType::Extern => HeapType::Extern,
            wasmparser::HeapType::Any => HeapType::Any,
            wasmparser::HeapType::None => HeapType::None,
            wasmparser::HeapType::NoExtern => HeapType::NoExtern,
            wasmparser::HeapType::NoFunc => HeapType::NoFunc,
            wasmparser::HeapType::Eq => HeapType::Eq,
            wasmparser::HeapType::Struct => HeapType::Struct,
            wasmparser::HeapType::Array => HeapType::Array,
            wasmparser::HeapType::I31 => HeapType::I31,
            wasmparser::HeapType::Indexed(i) => HeapType::Indexed(i.into()),
        },
    }
}

impl EntityType {
    fn size(&self) -> u32 {
        match self {
            EntityType::Tag(_)
            | EntityType::Global(_)
            | EntityType::Table(_)
            | EntityType::Memory(_) => 1,
            EntityType::Func(_, ty) => 1 + (ty.params.len() + ty.results.len()) as u32,
        }
    }
}

// A helper structure used when generating module/instance types to limit the
// amount of each kind of import created.
#[derive(Default, Clone, Copy, PartialEq)]
struct Entities {
    globals: usize,
    memories: usize,
    tables: usize,
    funcs: usize,
    tags: usize,
}

/// A container for the kinds of instructions that wasm-smith is allowed to
/// emit.
///
/// # Example
///
/// ```
/// # use wasm_smith::{InstructionKinds, InstructionKind};
/// let kinds = InstructionKinds::new(&[InstructionKind::Numeric, InstructionKind::Memory]);
/// assert!(kinds.contains(InstructionKind::Memory));
/// ```
#[derive(Clone, Copy, Debug, Default)]
pub struct InstructionKinds(pub(crate) FlagSet<InstructionKind>);
impl InstructionKinds {
    /// Create a new container.
    pub fn new(kinds: &[InstructionKind]) -> Self {
        Self(kinds.iter().fold(FlagSet::default(), |ks, k| ks | *k))
    }

    /// Include all [InstructionKind]s.
    pub fn all() -> Self {
        Self(FlagSet::full())
    }

    /// Include no [InstructionKind]s.
    pub fn none() -> Self {
        Self(FlagSet::default())
    }

    /// Check if the [InstructionKind] is contained in this set.
    #[inline]
    pub fn contains(&self, kind: InstructionKind) -> bool {
        self.0.contains(kind)
    }
}

flags! {
    /// Enumerate the categories of instructions defined in the [WebAssembly
    /// specification](https://webassembly.github.io/spec/core/syntax/instructions.html).
    #[allow(missing_docs)]
    #[cfg_attr(feature = "_internal_cli", derive(serde::Deserialize))]
    pub enum InstructionKind: u16 {
        Numeric,
        Vector,
        Reference,
        Parametric,
        Variable,
        Table,
        Memory,
        Control,
    }
}

impl FromStr for InstructionKind {
    type Err = String;
    fn from_str(s: &str) -> std::result::Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "numeric" => Ok(InstructionKind::Numeric),
            "vector" => Ok(InstructionKind::Vector),
            "reference" => Ok(InstructionKind::Reference),
            "parametric" => Ok(InstructionKind::Parametric),
            "variable" => Ok(InstructionKind::Variable),
            "table" => Ok(InstructionKind::Table),
            "memory" => Ok(InstructionKind::Memory),
            "control" => Ok(InstructionKind::Control),
            _ => Err(format!("unknown instruction kind: {}", s)),
        }
    }
}
