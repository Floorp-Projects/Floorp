//! Types relating to type information provided by validation.

use super::{
    component::{ComponentState, ExternKind},
    core::Module,
};
use crate::validator::names::KebabString;
use crate::{
    ArrayType, BinaryReaderError, Export, ExternalKind, FuncType, GlobalType, Import, MemoryType,
    PrimitiveValType, RefType, Result, StructType, StructuralType, SubType, TableType, TypeRef,
    ValType,
};
use indexmap::{IndexMap, IndexSet};
use std::collections::HashMap;
use std::collections::HashSet;
use std::ops::Index;
use std::sync::atomic::{AtomicU64, Ordering};
use std::{
    borrow::Borrow,
    hash::{Hash, Hasher},
    mem,
    ops::{Deref, DerefMut},
    sync::Arc,
};

/// The maximum number of parameters in the canonical ABI that can be passed by value.
///
/// Functions that exceed this limit will instead pass parameters indirectly from
/// linear memory via a single pointer parameter.
const MAX_FLAT_FUNC_PARAMS: usize = 16;
/// The maximum number of results in the canonical ABI that can be returned by a function.
///
/// Functions that exceed this limit have their results written to linear memory via an
/// additional pointer parameter (imports) or return a single pointer value (exports).
const MAX_FLAT_FUNC_RESULTS: usize = 1;

/// The maximum lowered types, including a possible type for a return pointer parameter.
const MAX_LOWERED_TYPES: usize = MAX_FLAT_FUNC_PARAMS + 1;

/// A simple alloc-free list of types used for calculating lowered function signatures.
pub(crate) struct LoweredTypes {
    types: [ValType; MAX_LOWERED_TYPES],
    len: usize,
    max: usize,
}

impl LoweredTypes {
    fn new(max: usize) -> Self {
        assert!(max <= MAX_LOWERED_TYPES);
        Self {
            types: [ValType::I32; MAX_LOWERED_TYPES],
            len: 0,
            max,
        }
    }

    fn len(&self) -> usize {
        self.len
    }

    fn maxed(&self) -> bool {
        self.len == self.max
    }

    fn get_mut(&mut self, index: usize) -> Option<&mut ValType> {
        if index < self.len {
            Some(&mut self.types[index])
        } else {
            None
        }
    }

    fn push(&mut self, ty: ValType) -> bool {
        if self.maxed() {
            return false;
        }

        self.types[self.len] = ty;
        self.len += 1;
        true
    }

    fn clear(&mut self) {
        self.len = 0;
    }

    pub fn as_slice(&self) -> &[ValType] {
        &self.types[..self.len]
    }

    pub fn iter(&self) -> impl Iterator<Item = ValType> + '_ {
        self.as_slice().iter().copied()
    }
}

/// Represents information about a component function type lowering.
pub(crate) struct LoweringInfo {
    pub(crate) params: LoweredTypes,
    pub(crate) results: LoweredTypes,
    pub(crate) requires_memory: bool,
    pub(crate) requires_realloc: bool,
}

impl LoweringInfo {
    pub(crate) fn into_func_type(self) -> FuncType {
        FuncType::new(
            self.params.as_slice().iter().copied(),
            self.results.as_slice().iter().copied(),
        )
    }
}

impl Default for LoweringInfo {
    fn default() -> Self {
        Self {
            params: LoweredTypes::new(MAX_FLAT_FUNC_PARAMS),
            results: LoweredTypes::new(MAX_FLAT_FUNC_RESULTS),
            requires_memory: false,
            requires_realloc: false,
        }
    }
}

fn push_primitive_wasm_types(ty: &PrimitiveValType, lowered_types: &mut LoweredTypes) -> bool {
    match ty {
        PrimitiveValType::Bool
        | PrimitiveValType::S8
        | PrimitiveValType::U8
        | PrimitiveValType::S16
        | PrimitiveValType::U16
        | PrimitiveValType::S32
        | PrimitiveValType::U32
        | PrimitiveValType::Char => lowered_types.push(ValType::I32),
        PrimitiveValType::S64 | PrimitiveValType::U64 => lowered_types.push(ValType::I64),
        PrimitiveValType::Float32 => lowered_types.push(ValType::F32),
        PrimitiveValType::Float64 => lowered_types.push(ValType::F64),
        PrimitiveValType::String => {
            lowered_types.push(ValType::I32) && lowered_types.push(ValType::I32)
        }
    }
}

/// Represents a unique identifier for a type known to a [`crate::Validator`].
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
#[repr(C)] // use fixed field layout to ensure minimal size
pub struct TypeId {
    /// The index into the global list of types.
    pub(crate) index: usize,
    /// Metadata about this type and its recursive structure.
    pub(crate) info: TypeInfo,
    /// A unique integer assigned to this type.
    ///
    /// The purpose of this field is to ensure that two different `TypeId`
    /// representations can be handed out for two different aliased types within
    /// a component that actually point to the same underlying type (as pointed
    /// to by the `index` field).
    unique_id: u32,
}

// The size of `TypeId` was seen to have a large-ish impact in #844, so this
// assert ensures that it stays relatively small.
const _: () = {
    assert!(std::mem::size_of::<TypeId>() <= 16);
};

/// Metadata about a type and its transitive structure.
///
/// Currently contains two properties:
///
/// * The "size" of a type - a proxy to the recursive size of a type if
///   everything in the type were unique (e.g. no shared references). Not an
///   approximation of runtime size, but instead of type-complexity size if
///   someone were to visit each element of the type individually. For example
///   `u32` has size 1 and `(list u32)` has size 2 (roughly). Used to prevent
///   massive trees of types.
///
/// * Whether or not a type contains a "borrow" transitively inside of it. For
///   example `(borrow $t)` and `(list (borrow $t))` both contain borrows, but
///   `(list u32)` does not. Used to validate that component function results do
///   not contain borrows.
///
/// Currently this is represented as a compact 32-bit integer to ensure that
/// `TypeId`, which this is stored in, remains relatively small. The maximum
/// type size allowed in wasmparser is 1M at this time which is 20 bits of
/// information, and then one more bit is used for whether or not a borrow is
/// used. Currently this uses the low 24 bits for the type size and the MSB for
/// the borrow bit.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub(crate) struct TypeInfo(u32);

impl TypeInfo {
    /// Creates a new blank set of type information.
    ///
    /// Defaults to size 1 to ensure that this consumes space in the final type
    /// structure.
    pub(crate) fn new() -> TypeInfo {
        TypeInfo::_new(1, false)
    }

    /// Creates a new blank set of information about a leaf "borrow" type which
    /// has size 1.
    pub(crate) fn borrow() -> TypeInfo {
        TypeInfo::_new(1, true)
    }

    /// Creates type information corresponding to a core type of the `size`
    /// specified, meaning no borrows are contained within.
    pub(crate) fn core(size: u32) -> TypeInfo {
        TypeInfo::_new(size, false)
    }

    fn _new(size: u32, contains_borrow: bool) -> TypeInfo {
        assert!(size < (1 << 24));
        TypeInfo(size | ((contains_borrow as u32) << 31))
    }

    /// Combines another set of type information into this one, for example if
    /// this is a record which has `other` as a field.
    ///
    /// Updates the size of `self` and whether or not this type contains a
    /// borrow based on whether `other` contains a borrow.
    ///
    /// Returns an error if the type size would exceed this crate's static limit
    /// of a type size.
    pub(crate) fn combine(&mut self, other: TypeInfo, offset: usize) -> Result<()> {
        *self = TypeInfo::_new(
            super::combine_type_sizes(self.size(), other.size(), offset)?,
            self.contains_borrow() || other.contains_borrow(),
        );
        Ok(())
    }

    pub(crate) fn size(&self) -> u32 {
        self.0 & 0xffffff
    }

    pub(crate) fn contains_borrow(&self) -> bool {
        (self.0 >> 31) != 0
    }
}

/// A unified type definition for validating WebAssembly modules and components.
#[derive(Debug)]
pub enum Type {
    /// The definition is for a sub type.
    Sub(SubType),
    /// The definition is for a core module type.
    ///
    /// This variant is only supported when parsing a component.
    Module(Box<ModuleType>),
    /// The definition is for a core module instance type.
    ///
    /// This variant is only supported when parsing a component.
    Instance(Box<InstanceType>),
    /// The definition is for a component type.
    ///
    /// This variant is only supported when parsing a component.
    Component(Box<ComponentType>),
    /// The definition is for a component instance type.
    ///
    /// This variant is only supported when parsing a component.
    ComponentInstance(Box<ComponentInstanceType>),
    /// The definition is for a component function type.
    ///
    /// This variant is only supported when parsing a component.
    ComponentFunc(ComponentFuncType),
    /// The definition is for a component defined type.
    ///
    /// This variant is only supported when parsing a component.
    Defined(ComponentDefinedType),
    /// This definition is for a resource type in the component model.
    ///
    /// Each resource is identified by a unique identifier specified here.
    Resource(ResourceId),
}

impl Type {
    /// Converts the type to a core function type.
    pub fn unwrap_func(&self) -> &FuncType {
        match self {
            Type::Sub(SubType {
                structural_type: StructuralType::Func(ft),
                ..
            }) => ft,
            _ => panic!("not a function type"),
        }
    }

    /// Converts the type to an array type.
    pub fn unwrap_array(&self) -> &ArrayType {
        match self {
            Self::Sub(SubType {
                structural_type: StructuralType::Array(ty),
                ..
            }) => ty,
            _ => panic!("not an array type"),
        }
    }

    /// Converts the type to a struct type.
    pub fn unwrap_struct(&self) -> &StructType {
        match self {
            Self::Sub(SubType {
                structural_type: StructuralType::Struct(ty),
                ..
            }) => ty,
            _ => panic!("not a struct type"),
        }
    }

    /// Converts the type to a core module type.
    pub fn unwrap_module(&self) -> &ModuleType {
        match self {
            Self::Module(ty) => ty,
            _ => panic!("not a module type"),
        }
    }

    /// Converts the type to a core module instance type.
    pub fn unwrap_instance(&self) -> &InstanceType {
        match self {
            Self::Instance(ty) => ty,
            _ => panic!("not an instance type"),
        }
    }

    /// Converts the type to a component type.
    pub fn unwrap_component(&self) -> &ComponentType {
        match self {
            Self::Component(ty) => ty,
            _ => panic!("not a component type"),
        }
    }

    /// Converts the type to a component instance type.
    pub fn unwrap_component_instance(&self) -> &ComponentInstanceType {
        match self {
            Self::ComponentInstance(ty) => ty,
            _ => panic!("not a component instance type"),
        }
    }

    /// Converts the type to a component function type.
    pub fn unwrap_component_func(&self) -> &ComponentFuncType {
        match self {
            Self::ComponentFunc(ty) => ty,
            _ => panic!("not a component function type"),
        }
    }

    /// Converts the type to a component defined type.
    pub fn unwrap_defined(&self) -> &ComponentDefinedType {
        match self {
            Self::Defined(ty) => ty,
            _ => panic!("not a defined type"),
        }
    }

    /// Converts this type to a resource type, returning the corresponding id.
    pub fn unwrap_resource(&self) -> ResourceId {
        match self {
            Self::Resource(id) => *id,
            _ => panic!("not a resource type"),
        }
    }

    pub(crate) fn info(&self) -> TypeInfo {
        // TODO(#1036): calculate actual size for func, array, struct
        match self {
            Self::Sub(ty) => {
                let size = 1 + match ty.clone().structural_type {
                    StructuralType::Func(ty) => 1 + (ty.params().len() + ty.results().len()) as u32,
                    StructuralType::Array(_) => 2,
                    StructuralType::Struct(ty) => 1 + 2 * ty.fields.len() as u32,
                };
                TypeInfo::core(size)
            }
            Self::Module(ty) => ty.info,
            Self::Instance(ty) => ty.info,
            Self::Component(ty) => ty.info,
            Self::ComponentInstance(ty) => ty.info,
            Self::ComponentFunc(ty) => ty.info,
            Self::Defined(ty) => ty.info(),
            Self::Resource(_) => TypeInfo::new(),
        }
    }
}

/// A component value type.
#[derive(Debug, Clone, Copy)]
pub enum ComponentValType {
    /// The value type is one of the primitive types.
    Primitive(PrimitiveValType),
    /// The type is represented with the given type identifier.
    Type(TypeId),
}

impl ComponentValType {
    pub(crate) fn contains_ptr(&self, types: &TypeList) -> bool {
        match self {
            ComponentValType::Primitive(ty) => ty.contains_ptr(),
            ComponentValType::Type(ty) => types[*ty].unwrap_defined().contains_ptr(types),
        }
    }

    fn push_wasm_types(&self, types: &TypeList, lowered_types: &mut LoweredTypes) -> bool {
        match self {
            Self::Primitive(ty) => push_primitive_wasm_types(ty, lowered_types),
            Self::Type(id) => types[*id]
                .unwrap_defined()
                .push_wasm_types(types, lowered_types),
        }
    }

    pub(crate) fn info(&self) -> TypeInfo {
        match self {
            Self::Primitive(_) => TypeInfo::new(),
            Self::Type(id) => id.info,
        }
    }
}

/// The entity type for imports and exports of a module.
#[derive(Debug, Clone, Copy)]
pub enum EntityType {
    /// The entity is a function.
    Func(TypeId),
    /// The entity is a table.
    Table(TableType),
    /// The entity is a memory.
    Memory(MemoryType),
    /// The entity is a global.
    Global(GlobalType),
    /// The entity is a tag.
    Tag(TypeId),
}

impl EntityType {
    pub(crate) fn desc(&self) -> &'static str {
        match self {
            Self::Func(_) => "func",
            Self::Table(_) => "table",
            Self::Memory(_) => "memory",
            Self::Global(_) => "global",
            Self::Tag(_) => "tag",
        }
    }

    pub(crate) fn info(&self) -> TypeInfo {
        match self {
            Self::Func(id) | Self::Tag(id) => id.info,
            Self::Table(_) | Self::Memory(_) | Self::Global(_) => TypeInfo::new(),
        }
    }
}

trait ModuleImportKey {
    fn module(&self) -> &str;
    fn name(&self) -> &str;
}

impl<'a> Borrow<dyn ModuleImportKey + 'a> for (String, String) {
    fn borrow(&self) -> &(dyn ModuleImportKey + 'a) {
        self
    }
}

impl Hash for (dyn ModuleImportKey + '_) {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.module().hash(state);
        self.name().hash(state);
    }
}

impl PartialEq for (dyn ModuleImportKey + '_) {
    fn eq(&self, other: &Self) -> bool {
        self.module() == other.module() && self.name() == other.name()
    }
}

impl Eq for (dyn ModuleImportKey + '_) {}

impl ModuleImportKey for (String, String) {
    fn module(&self) -> &str {
        &self.0
    }

    fn name(&self) -> &str {
        &self.1
    }
}

impl ModuleImportKey for (&str, &str) {
    fn module(&self) -> &str {
        self.0
    }

    fn name(&self) -> &str {
        self.1
    }
}

/// Represents a core module type.
#[derive(Debug, Clone)]
pub struct ModuleType {
    /// Metadata about this module type
    pub(crate) info: TypeInfo,
    /// The imports of the module type.
    pub imports: IndexMap<(String, String), EntityType>,
    /// The exports of the module type.
    pub exports: IndexMap<String, EntityType>,
}

impl ModuleType {
    /// Looks up an import by its module and name.
    ///
    /// Returns `None` if the import was not found.
    pub fn lookup_import(&self, module: &str, name: &str) -> Option<&EntityType> {
        self.imports.get(&(module, name) as &dyn ModuleImportKey)
    }
}

/// Represents the kind of module instance type.
#[derive(Debug, Clone)]
pub enum InstanceTypeKind {
    /// The instance type is the result of instantiating a module type.
    Instantiated(TypeId),
    /// The instance type is the result of instantiating from exported items.
    Exports(IndexMap<String, EntityType>),
}

/// Represents a module instance type.
#[derive(Debug, Clone)]
pub struct InstanceType {
    /// Metadata about this instance type
    pub(crate) info: TypeInfo,
    /// The kind of module instance type.
    pub kind: InstanceTypeKind,
}

impl InstanceType {
    /// Gets the exports of the instance type.
    pub fn exports<'a>(&'a self, types: TypesRef<'a>) -> &'a IndexMap<String, EntityType> {
        self.internal_exports(types.list)
    }

    pub(crate) fn internal_exports<'a>(
        &'a self,
        types: &'a TypeList,
    ) -> &'a IndexMap<String, EntityType> {
        match &self.kind {
            InstanceTypeKind::Instantiated(id) => &types[*id].unwrap_module().exports,
            InstanceTypeKind::Exports(exports) => exports,
        }
    }
}

/// The entity type for imports and exports of a component.
#[derive(Debug, Clone, Copy)]
pub enum ComponentEntityType {
    /// The entity is a core module.
    Module(TypeId),
    /// The entity is a function.
    Func(TypeId),
    /// The entity is a value.
    Value(ComponentValType),
    /// The entity is a type.
    Type {
        /// This is the identifier of the type that was referenced when this
        /// entity was created.
        referenced: TypeId,
        /// This is the identifier of the type that was created when this type
        /// was imported or exported from the component.
        ///
        /// Note that the underlying type information for the `referenced`
        /// field and for this `created` field is the same, but these two types
        /// will hash to different values.
        created: TypeId,
    },
    /// The entity is a component instance.
    Instance(TypeId),
    /// The entity is a component.
    Component(TypeId),
}

impl ComponentEntityType {
    /// Determines if component entity type `a` is a subtype of `b`.
    pub fn is_subtype_of(a: &Self, at: TypesRef, b: &Self, bt: TypesRef) -> bool {
        SubtypeCx::new(at.list, bt.list)
            .component_entity_type(a, b, 0)
            .is_ok()
    }

    pub(crate) fn desc(&self) -> &'static str {
        match self {
            Self::Module(_) => "module",
            Self::Func(_) => "func",
            Self::Value(_) => "value",
            Self::Type { .. } => "type",
            Self::Instance(_) => "instance",
            Self::Component(_) => "component",
        }
    }

    pub(crate) fn info(&self) -> TypeInfo {
        match self {
            Self::Module(ty)
            | Self::Func(ty)
            | Self::Type { referenced: ty, .. }
            | Self::Instance(ty)
            | Self::Component(ty) => ty.info,
            Self::Value(ty) => ty.info(),
        }
    }
}

/// Represents a type of a component.
#[derive(Debug, Clone)]
pub struct ComponentType {
    /// Metadata about this component type
    pub(crate) info: TypeInfo,

    /// The imports of the component type.
    ///
    /// Each import has its own kebab-name and an optional URL listed. Note that
    /// the set of import names is disjoint with the set of export names.
    pub imports: IndexMap<String, ComponentEntityType>,

    /// The exports of the component type.
    ///
    /// Each export has its own kebab-name and an optional URL listed. Note that
    /// the set of export names is disjoint with the set of import names.
    pub exports: IndexMap<String, ComponentEntityType>,

    /// Universally quantified resources required to be provided when
    /// instantiating this component type.
    ///
    /// Each resource in this map is explicitly imported somewhere in the
    /// `imports` map. The "path" to where it's imported is specified by the
    /// `Vec<usize>` payload here. For more information about the indexes see
    /// the documentation on `ComponentState::imported_resources`.
    ///
    /// This should technically be inferrable from the structure of `imports`,
    /// but it's stored as an auxiliary set for subtype checking and
    /// instantiation.
    ///
    /// Note that this is not a set of all resources referred to by the
    /// `imports`. Instead it's only those created, relative to the internals of
    /// this component, by the imports.
    pub imported_resources: Vec<(ResourceId, Vec<usize>)>,

    /// The dual of the `imported_resources`, or the set of defined
    /// resources -- those created through the instantiation process which are
    /// unique to this component.
    ///
    /// This set is similar to the `imported_resources` set but it's those
    /// contained within the `exports`. Instantiating this component will
    /// create fresh new versions of all of these resources. The path here is
    /// within the `exports` array.
    pub defined_resources: Vec<(ResourceId, Vec<usize>)>,

    /// The set of all resources which are explicitly exported by this
    /// component, and where they're exported.
    ///
    /// This mapping is stored separately from `defined_resources` to ensure
    /// that it contains all exported resources, not just those which are
    /// defined. That means that this can cover reexports of imported
    /// resources, exports of local resources, or exports of closed-over
    /// resources for example.
    pub explicit_resources: IndexMap<ResourceId, Vec<usize>>,
}

/// Represents a type of a component instance.
#[derive(Debug, Clone)]
pub struct ComponentInstanceType {
    /// Metadata about this instance type
    pub(crate) info: TypeInfo,

    /// The list of exports, keyed by name, that this instance has.
    ///
    /// An optional URL and type of each export is provided as well.
    pub exports: IndexMap<String, ComponentEntityType>,

    /// The list of "defined resources" or those which are closed over in
    /// this instance type.
    ///
    /// This list is populated, for example, when the type of an instance is
    /// declared and it contains its own resource type exports defined
    /// internally. For example:
    ///
    /// ```wasm
    /// (component
    ///     (type (instance
    ///         (export "x" (type sub resource)) ;; one `defined_resources` entry
    ///     ))
    /// )
    /// ```
    ///
    /// This list is also a bit of an oddity, however, because the type of a
    /// concrete instance will always have this as empty. For example:
    ///
    /// ```wasm
    /// (component
    ///     (type $t (instance (export "x" (type sub resource))))
    ///
    ///     ;; the type of this instance has no defined resources
    ///     (import "i" (instance (type $t)))
    /// )
    /// ```
    ///
    /// This list ends up only being populated for instance types declared in a
    /// module which aren't yet "attached" to anything. Once something is
    /// instantiated, imported, exported, or otherwise refers to a concrete
    /// instance then this list is always empty. For concrete instances
    /// defined resources are tracked in the component state or component type.
    pub defined_resources: Vec<ResourceId>,

    /// The list of all resources that are explicitly exported from this
    /// instance type along with the path they're exported at.
    pub explicit_resources: IndexMap<ResourceId, Vec<usize>>,
}

/// Represents a type of a component function.
#[derive(Debug, Clone)]
pub struct ComponentFuncType {
    /// Metadata about this function type.
    pub(crate) info: TypeInfo,
    /// The function parameters.
    pub params: Box<[(KebabString, ComponentValType)]>,
    /// The function's results.
    pub results: Box<[(Option<KebabString>, ComponentValType)]>,
}

impl ComponentFuncType {
    /// Lowers the component function type to core parameter and result types for the
    /// canonical ABI.
    pub(crate) fn lower(&self, types: &TypeList, is_lower: bool) -> LoweringInfo {
        let mut info = LoweringInfo::default();

        for (_, ty) in self.params.iter() {
            // Check to see if `ty` has a pointer somewhere in it, needed for
            // any type that transitively contains either a string or a list.
            // In this situation lowered functions must specify `memory`, and
            // lifted functions must specify `realloc` as well. Lifted functions
            // gain their memory requirement through the final clause of this
            // function.
            if is_lower {
                if !info.requires_memory {
                    info.requires_memory = ty.contains_ptr(types);
                }
            } else {
                if !info.requires_realloc {
                    info.requires_realloc = ty.contains_ptr(types);
                }
            }

            if !ty.push_wasm_types(types, &mut info.params) {
                // Too many parameters to pass directly
                // Function will have a single pointer parameter to pass the arguments
                // via linear memory
                info.params.clear();
                assert!(info.params.push(ValType::I32));
                info.requires_memory = true;

                // We need realloc as well when lifting a function
                if !is_lower {
                    info.requires_realloc = true;
                }
                break;
            }
        }

        for (_, ty) in self.results.iter() {
            // Results of lowered functions that contains pointers must be
            // allocated by the callee meaning that realloc is required.
            // Results of lifted function are allocated by the guest which
            // means that no realloc option is necessary.
            if is_lower && !info.requires_realloc {
                info.requires_realloc = ty.contains_ptr(types);
            }

            if !ty.push_wasm_types(types, &mut info.results) {
                // Too many results to return directly, either a retptr parameter will be used (import)
                // or a single pointer will be returned (export)
                info.results.clear();
                if is_lower {
                    info.params.max = MAX_LOWERED_TYPES;
                    assert!(info.params.push(ValType::I32));
                } else {
                    assert!(info.results.push(ValType::I32));
                }
                info.requires_memory = true;
                break;
            }
        }

        // Memory is always required when realloc is required
        info.requires_memory |= info.requires_realloc;

        info
    }
}

/// Represents a variant case.
#[derive(Debug, Clone)]
pub struct VariantCase {
    /// The variant case type.
    pub ty: Option<ComponentValType>,
    /// The name of the variant case refined by this one.
    pub refines: Option<KebabString>,
}

/// Represents a record type.
#[derive(Debug, Clone)]
pub struct RecordType {
    /// Metadata about this record type.
    pub(crate) info: TypeInfo,
    /// The map of record fields.
    pub fields: IndexMap<KebabString, ComponentValType>,
}

/// Represents a variant type.
#[derive(Debug, Clone)]
pub struct VariantType {
    /// Metadata about this variant type.
    pub(crate) info: TypeInfo,
    /// The map of variant cases.
    pub cases: IndexMap<KebabString, VariantCase>,
}

/// Represents a tuple type.
#[derive(Debug, Clone)]
pub struct TupleType {
    /// Metadata about this tuple type.
    pub(crate) info: TypeInfo,
    /// The types of the tuple.
    pub types: Box<[ComponentValType]>,
}

/// Represents a component defined type.
#[derive(Debug, Clone)]
pub enum ComponentDefinedType {
    /// The type is a primitive value type.
    Primitive(PrimitiveValType),
    /// The type is a record.
    Record(RecordType),
    /// The type is a variant.
    Variant(VariantType),
    /// The type is a list.
    List(ComponentValType),
    /// The type is a tuple.
    Tuple(TupleType),
    /// The type is a set of flags.
    Flags(IndexSet<KebabString>),
    /// The type is an enumeration.
    Enum(IndexSet<KebabString>),
    /// The type is an `option`.
    Option(ComponentValType),
    /// The type is a `result`.
    Result {
        /// The `ok` type.
        ok: Option<ComponentValType>,
        /// The `error` type.
        err: Option<ComponentValType>,
    },
    /// The type is an owned handle to the specified resource.
    Own(TypeId),
    /// The type is a borrowed handle to the specified resource.
    Borrow(TypeId),
}

impl ComponentDefinedType {
    pub(crate) fn contains_ptr(&self, types: &TypeList) -> bool {
        match self {
            Self::Primitive(ty) => ty.contains_ptr(),
            Self::Record(r) => r.fields.values().any(|ty| ty.contains_ptr(types)),
            Self::Variant(v) => v
                .cases
                .values()
                .any(|case| case.ty.map(|ty| ty.contains_ptr(types)).unwrap_or(false)),
            Self::List(_) => true,
            Self::Tuple(t) => t.types.iter().any(|ty| ty.contains_ptr(types)),
            Self::Flags(_) | Self::Enum(_) | Self::Own(_) | Self::Borrow(_) => false,
            Self::Option(ty) => ty.contains_ptr(types),
            Self::Result { ok, err } => {
                ok.map(|ty| ty.contains_ptr(types)).unwrap_or(false)
                    || err.map(|ty| ty.contains_ptr(types)).unwrap_or(false)
            }
        }
    }

    pub(crate) fn info(&self) -> TypeInfo {
        match self {
            Self::Primitive(_) | Self::Flags(_) | Self::Enum(_) | Self::Own(_) => TypeInfo::new(),
            Self::Borrow(_) => TypeInfo::borrow(),
            Self::Record(r) => r.info,
            Self::Variant(v) => v.info,
            Self::Tuple(t) => t.info,
            Self::List(ty) | Self::Option(ty) => ty.info(),
            Self::Result { ok, err } => {
                let default = TypeInfo::new();
                let mut info = ok.map(|ty| ty.info()).unwrap_or(default);
                info.combine(err.map(|ty| ty.info()).unwrap_or(default), 0)
                    .unwrap();
                info
            }
        }
    }

    fn push_wasm_types(&self, types: &TypeList, lowered_types: &mut LoweredTypes) -> bool {
        match self {
            Self::Primitive(ty) => push_primitive_wasm_types(ty, lowered_types),
            Self::Record(r) => r
                .fields
                .iter()
                .all(|(_, ty)| ty.push_wasm_types(types, lowered_types)),
            Self::Variant(v) => Self::push_variant_wasm_types(
                v.cases.iter().filter_map(|(_, case)| case.ty.as_ref()),
                types,
                lowered_types,
            ),
            Self::List(_) => lowered_types.push(ValType::I32) && lowered_types.push(ValType::I32),
            Self::Tuple(t) => t
                .types
                .iter()
                .all(|ty| ty.push_wasm_types(types, lowered_types)),
            Self::Flags(names) => {
                (0..(names.len() + 31) / 32).all(|_| lowered_types.push(ValType::I32))
            }
            Self::Enum(_) | Self::Own(_) | Self::Borrow(_) => lowered_types.push(ValType::I32),
            Self::Option(ty) => {
                Self::push_variant_wasm_types([ty].into_iter(), types, lowered_types)
            }
            Self::Result { ok, err } => {
                Self::push_variant_wasm_types(ok.iter().chain(err.iter()), types, lowered_types)
            }
        }
    }

    fn push_variant_wasm_types<'a>(
        cases: impl Iterator<Item = &'a ComponentValType>,
        types: &TypeList,
        lowered_types: &mut LoweredTypes,
    ) -> bool {
        // Push the discriminant
        if !lowered_types.push(ValType::I32) {
            return false;
        }

        let start = lowered_types.len();

        for ty in cases {
            let mut temp = LoweredTypes::new(lowered_types.max);

            if !ty.push_wasm_types(types, &mut temp) {
                return false;
            }

            for (i, ty) in temp.iter().enumerate() {
                match lowered_types.get_mut(start + i) {
                    Some(prev) => *prev = Self::join_types(*prev, ty),
                    None => {
                        if !lowered_types.push(ty) {
                            return false;
                        }
                    }
                }
            }
        }

        true
    }

    fn join_types(a: ValType, b: ValType) -> ValType {
        use ValType::*;

        match (a, b) {
            (I32, I32) | (I64, I64) | (F32, F32) | (F64, F64) => a,
            (I32, F32) | (F32, I32) => I32,
            (_, I64 | F64) | (I64 | F64, _) => I64,
            _ => panic!("unexpected wasm type for canonical ABI"),
        }
    }

    fn desc(&self) -> &'static str {
        match self {
            ComponentDefinedType::Record(_) => "record",
            ComponentDefinedType::Primitive(_) => "primitive",
            ComponentDefinedType::Variant(_) => "variant",
            ComponentDefinedType::Tuple(_) => "tuple",
            ComponentDefinedType::Enum(_) => "enum",
            ComponentDefinedType::Flags(_) => "flags",
            ComponentDefinedType::Option(_) => "option",
            ComponentDefinedType::List(_) => "list",
            ComponentDefinedType::Result { .. } => "result",
            ComponentDefinedType::Own(_) => "own",
            ComponentDefinedType::Borrow(_) => "borrow",
        }
    }
}

/// An opaque identifier intended to be used to distinguish whether two
/// resource types are equivalent or not.
#[derive(Debug, Clone, PartialEq, Eq, Hash, Ord, PartialOrd, Copy)]
#[repr(packed(4))] // try to not waste 4 bytes in padding
pub struct ResourceId {
    // This is a globally unique identifier which is assigned once per
    // `TypeAlloc`. This ensures that resource identifiers from different
    // instances of `Types`, for example, are considered unique.
    //
    // Technically 64-bits should be enough for all resource ids ever, but
    // they're allocated so often it's predicted that an atomic increment
    // per resource id is probably too expensive. To amortize that cost each
    // top-level wasm component gets a single globally unique identifier, and
    // then within a component contextually unique identifiers are handed out.
    globally_unique_id: u64,

    // A contextually unique id within the globally unique id above. This is
    // allocated within a `TypeAlloc` with its own counter, and allocations of
    // this are cheap as nothing atomic is required.
    //
    // The 32-bit storage here should ideally be enough for any component
    // containing resources. If memory usage becomes an issue (this struct is
    // 12 bytes instead of 8 or 4) then this coudl get folded into the globally
    // unique id with everything using an atomic increment perhaps.
    contextually_unique_id: u32,
}

#[allow(clippy::large_enum_variant)]
enum TypesKind {
    Module(Arc<Module>),
    Component(ComponentState),
}

/// Represents the types known to a [`crate::Validator`] once validation has completed.
///
/// The type information is returned via the [`crate::Validator::end`] method.
pub struct Types {
    list: TypeList,
    kind: TypesKind,
}

#[derive(Clone, Copy)]
enum TypesRefKind<'a> {
    Module(&'a Module),
    Component(&'a ComponentState),
}

/// Represents the types known to a [`crate::Validator`] during validation.
///
/// Retrieved via the [`crate::Validator::types`] method.
#[derive(Clone, Copy)]
pub struct TypesRef<'a> {
    list: &'a TypeList,
    kind: TypesRefKind<'a>,
}

impl<'a> TypesRef<'a> {
    pub(crate) fn from_module(types: &'a TypeList, module: &'a Module) -> Self {
        Self {
            list: types,
            kind: TypesRefKind::Module(module),
        }
    }

    pub(crate) fn from_component(types: &'a TypeList, component: &'a ComponentState) -> Self {
        Self {
            list: types,
            kind: TypesRefKind::Component(component),
        }
    }

    /// Gets a type based on its type id.
    ///
    /// Returns `None` if the type id is unknown.
    pub fn get(&self, id: TypeId) -> Option<&'a Type> {
        self.list.get(id.index)
    }

    /// Gets a core WebAssembly type id from a type index.
    ///
    /// Note that this is in contrast to [`TypesRef::component_type_at`] which
    /// gets a component type from its index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds.
    pub fn core_type_at(&self, index: u32) -> TypeId {
        match &self.kind {
            TypesRefKind::Module(module) => module.types[index as usize],
            TypesRefKind::Component(component) => component.core_types[index as usize],
        }
    }

    /// Gets a type id from a type index.
    ///
    /// # Panics
    ///
    /// Panics if `index` is not a valid function index or if this type
    /// information represents a core module.
    pub fn component_type_at(&self, index: u32) -> TypeId {
        match &self.kind {
            TypesRefKind::Module(_) => panic!("not a component"),
            TypesRefKind::Component(component) => component.types[index as usize],
        }
    }

    /// Returns the number of core types defined so far.
    pub fn core_type_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(module) => module.types.len() as u32,
            TypesRefKind::Component(component) => component.core_types.len() as u32,
        }
    }

    /// Returns the number of component types defined so far.
    pub fn component_type_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(_module) => 0,
            TypesRefKind::Component(component) => component.types.len() as u32,
        }
    }

    /// Gets the type of a table at the given table index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds.
    pub fn table_at(&self, index: u32) -> TableType {
        let tables = match &self.kind {
            TypesRefKind::Module(module) => &module.tables,
            TypesRefKind::Component(component) => &component.core_tables,
        };
        tables[index as usize]
    }

    /// Returns the number of tables defined so far.
    pub fn table_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(module) => module.tables.len() as u32,
            TypesRefKind::Component(component) => component.core_tables.len() as u32,
        }
    }

    /// Gets the type of a memory at the given memory index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds.
    pub fn memory_at(&self, index: u32) -> MemoryType {
        let memories = match &self.kind {
            TypesRefKind::Module(module) => &module.memories,
            TypesRefKind::Component(component) => &component.core_memories,
        };

        memories[index as usize]
    }

    /// Returns the number of memories defined so far.
    pub fn memory_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(module) => module.memories.len() as u32,
            TypesRefKind::Component(component) => component.core_memories.len() as u32,
        }
    }

    /// Gets the type of a global at the given global index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds.
    pub fn global_at(&self, index: u32) -> GlobalType {
        let globals = match &self.kind {
            TypesRefKind::Module(module) => &module.globals,
            TypesRefKind::Component(component) => &component.core_globals,
        };

        globals[index as usize]
    }

    /// Returns the number of globals defined so far.
    pub fn global_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(module) => module.globals.len() as u32,
            TypesRefKind::Component(component) => component.core_globals.len() as u32,
        }
    }

    /// Gets the type of a tag at the given tag index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds.
    pub fn tag_at(&self, index: u32) -> TypeId {
        let tags = match &self.kind {
            TypesRefKind::Module(module) => &module.tags,
            TypesRefKind::Component(component) => &component.core_tags,
        };
        tags[index as usize]
    }

    /// Returns the number of tags defined so far.
    pub fn tag_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(module) => module.tags.len() as u32,
            TypesRefKind::Component(component) => component.core_tags.len() as u32,
        }
    }

    /// Gets the type of a core function at the given function index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds.
    pub fn function_at(&self, index: u32) -> TypeId {
        match &self.kind {
            TypesRefKind::Module(module) => module.types[module.functions[index as usize] as usize],
            TypesRefKind::Component(component) => component.core_funcs[index as usize],
        }
    }

    /// Gets the count of core functions defined so far.
    ///
    /// Note that this includes imported functions, defined functions, and for
    /// components lowered/aliased functions.
    pub fn function_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(module) => module.functions.len() as u32,
            TypesRefKind::Component(component) => component.core_funcs.len() as u32,
        }
    }

    /// Gets the type of an element segment at the given element segment index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds.
    pub fn element_at(&self, index: u32) -> RefType {
        match &self.kind {
            TypesRefKind::Module(module) => module.element_types[index as usize],
            TypesRefKind::Component(_) => {
                panic!("no elements on a component")
            }
        }
    }

    /// Returns the number of elements defined so far.
    pub fn element_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(module) => module.element_types.len() as u32,
            TypesRefKind::Component(_) => 0,
        }
    }

    /// Gets the type of a component function at the given function index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn component_function_at(&self, index: u32) -> TypeId {
        match &self.kind {
            TypesRefKind::Module(_) => panic!("not a component"),
            TypesRefKind::Component(component) => component.funcs[index as usize],
        }
    }

    /// Returns the number of component functions defined so far.
    pub fn component_function_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(_module) => 0,
            TypesRefKind::Component(component) => component.funcs.len() as u32,
        }
    }

    /// Gets the type of a module at the given module index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn module_at(&self, index: u32) -> TypeId {
        match &self.kind {
            TypesRefKind::Module(_) => panic!("not a component"),
            TypesRefKind::Component(component) => component.core_modules[index as usize],
        }
    }

    /// Returns the number of core wasm modules defined so far.
    pub fn module_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(_module) => 0,
            TypesRefKind::Component(component) => component.core_modules.len() as u32,
        }
    }

    /// Gets the type of a module instance at the given module instance index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn instance_at(&self, index: u32) -> TypeId {
        match &self.kind {
            TypesRefKind::Module(_) => panic!("not a component"),
            TypesRefKind::Component(component) => component.core_instances[index as usize],
        }
    }

    /// Returns the number of core wasm instances defined so far.
    pub fn instance_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(_module) => 0,
            TypesRefKind::Component(component) => component.core_instances.len() as u32,
        }
    }

    /// Gets the type of a component at the given component index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn component_at(&self, index: u32) -> TypeId {
        match &self.kind {
            TypesRefKind::Module(_) => panic!("not a component"),
            TypesRefKind::Component(component) => component.components[index as usize],
        }
    }

    /// Returns the number of components defined so far.
    pub fn component_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(_module) => 0,
            TypesRefKind::Component(component) => component.components.len() as u32,
        }
    }

    /// Gets the type of an component instance at the given component instance index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn component_instance_at(&self, index: u32) -> TypeId {
        match &self.kind {
            TypesRefKind::Module(_) => panic!("not a component"),
            TypesRefKind::Component(component) => component.instances[index as usize],
        }
    }

    /// Returns the number of component instances defined so far.
    pub fn component_instance_count(&self) -> u32 {
        match &self.kind {
            TypesRefKind::Module(_module) => 0,
            TypesRefKind::Component(component) => component.instances.len() as u32,
        }
    }

    /// Gets the type of a value at the given value index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn value_at(&self, index: u32) -> ComponentValType {
        match &self.kind {
            TypesRefKind::Module(_) => panic!("not a component"),
            TypesRefKind::Component(component) => component.values[index as usize].0,
        }
    }

    /// Gets the entity type for the given import.
    pub fn entity_type_from_import(&self, import: &Import) -> Option<EntityType> {
        match &self.kind {
            TypesRefKind::Module(module) => Some(match import.ty {
                TypeRef::Func(idx) => EntityType::Func(*module.types.get(idx as usize)?),
                TypeRef::Table(ty) => EntityType::Table(ty),
                TypeRef::Memory(ty) => EntityType::Memory(ty),
                TypeRef::Global(ty) => EntityType::Global(ty),
                TypeRef::Tag(ty) => EntityType::Tag(*module.types.get(ty.func_type_idx as usize)?),
            }),
            TypesRefKind::Component(_) => None,
        }
    }

    /// Gets the entity type from the given export.
    pub fn entity_type_from_export(&self, export: &Export) -> Option<EntityType> {
        match &self.kind {
            TypesRefKind::Module(module) => Some(match export.kind {
                ExternalKind::Func => EntityType::Func(
                    module.types[*module.functions.get(export.index as usize)? as usize],
                ),
                ExternalKind::Table => {
                    EntityType::Table(*module.tables.get(export.index as usize)?)
                }
                ExternalKind::Memory => {
                    EntityType::Memory(*module.memories.get(export.index as usize)?)
                }
                ExternalKind::Global => {
                    EntityType::Global(*module.globals.get(export.index as usize)?)
                }
                ExternalKind::Tag => EntityType::Tag(
                    module.types[*module.functions.get(export.index as usize)? as usize],
                ),
            }),
            TypesRefKind::Component(_) => None,
        }
    }

    /// Gets the component entity type for the given component import.
    pub fn component_entity_type_of_import(&self, name: &str) -> Option<ComponentEntityType> {
        match &self.kind {
            TypesRefKind::Module(_) => None,
            TypesRefKind::Component(component) => Some(*component.imports.get(name)?),
        }
    }

    /// Gets the component entity type for the given component export.
    pub fn component_entity_type_of_export(&self, name: &str) -> Option<ComponentEntityType> {
        match &self.kind {
            TypesRefKind::Module(_) => None,
            TypesRefKind::Component(component) => Some(*component.exports.get(name)?),
        }
    }
}

impl Index<TypeId> for TypesRef<'_> {
    type Output = Type;
    fn index(&self, id: TypeId) -> &Type {
        &self.list[id.index]
    }
}

impl Types {
    pub(crate) fn from_module(types: TypeList, module: Arc<Module>) -> Self {
        Self {
            list: types,
            kind: TypesKind::Module(module),
        }
    }

    pub(crate) fn from_component(types: TypeList, component: ComponentState) -> Self {
        Self {
            list: types,
            kind: TypesKind::Component(component),
        }
    }

    /// Gets a reference to this validation type information.
    pub fn as_ref(&self) -> TypesRef {
        TypesRef {
            list: &self.list,
            kind: match &self.kind {
                TypesKind::Module(module) => TypesRefKind::Module(module),
                TypesKind::Component(component) => TypesRefKind::Component(component),
            },
        }
    }

    /// Gets a type based on its type id.
    ///
    /// Returns `None` if the type id is unknown.
    pub fn get(&self, id: TypeId) -> Option<&Type> {
        self.as_ref().get(id)
    }

    /// Gets a core WebAssembly type at the given type index.
    ///
    /// Note that this is in contrast to [`TypesRef::component_type_at`] which
    /// gets a component type from its index.
    ///
    /// # Panics
    ///
    /// Panics if `index` is not a valid function index.
    pub fn core_type_at(&self, index: u32) -> TypeId {
        self.as_ref().core_type_at(index)
    }

    /// Gets a component type from the given component type index.
    ///
    /// # Panics
    ///
    /// Panics if `index` is not a valid function index or if this type
    /// information represents a core module.
    pub fn component_type_at(&self, index: u32) -> TypeId {
        self.as_ref().component_type_at(index)
    }

    /// Gets the count of core types.
    pub fn type_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(module) => module.types.len(),
            TypesKind::Component(component) => component.core_types.len(),
        }
    }

    /// Gets the type of a table at the given table index.
    ///
    /// # Panics
    ///
    /// Panics if `index` is not a valid function index.
    pub fn table_at(&self, index: u32) -> TableType {
        self.as_ref().table_at(index)
    }

    /// Gets the count of imported and defined tables.
    pub fn table_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(module) => module.tables.len(),
            TypesKind::Component(component) => component.core_tables.len(),
        }
    }

    /// Gets the type of a memory at the given memory index.
    ///
    /// # Panics
    ///
    /// Panics if `index` is not a valid function index.
    pub fn memory_at(&self, index: u32) -> MemoryType {
        self.as_ref().memory_at(index)
    }

    /// Gets the count of imported and defined memories.
    pub fn memory_count(&self) -> u32 {
        self.as_ref().memory_count()
    }

    /// Gets the type of a global at the given global index.
    ///
    /// # Panics
    ///
    /// Panics if `index` is not a valid function index.
    pub fn global_at(&self, index: u32) -> GlobalType {
        self.as_ref().global_at(index)
    }

    /// Gets the count of imported and defined globals.
    pub fn global_count(&self) -> u32 {
        self.as_ref().global_count()
    }

    /// Gets the type of a tag at the given tag index.
    ///
    /// # Panics
    ///
    /// Panics if `index` is not a valid function index.
    pub fn tag_at(&self, index: u32) -> TypeId {
        self.as_ref().tag_at(index)
    }

    /// Gets the count of imported and defined tags.
    pub fn tag_count(&self) -> u32 {
        self.as_ref().tag_count()
    }

    /// Gets the type of a core function at the given function index.
    ///
    /// # Panics
    ///
    /// Panics if `index` is not a valid function index.
    pub fn function_at(&self, index: u32) -> TypeId {
        self.as_ref().function_at(index)
    }

    /// Gets the count of core functions defined so far.
    ///
    /// Note that this includes imported functions, defined functions, and for
    /// components lowered/aliased functions.
    pub fn function_count(&self) -> u32 {
        self.as_ref().function_count()
    }

    /// Gets the type of an element segment at the given element segment index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds.
    pub fn element_at(&self, index: u32) -> RefType {
        self.as_ref().element_at(index)
    }

    /// Gets the count of element segments.
    pub fn element_count(&self) -> u32 {
        self.as_ref().element_count()
    }

    /// Gets the type of a component function at the given function index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn component_function_at(&self, index: u32) -> TypeId {
        self.as_ref().component_function_at(index)
    }

    /// Gets the count of imported, exported, or aliased component functions.
    pub fn component_function_count(&self) -> u32 {
        self.as_ref().component_function_count()
    }

    /// Gets the type of a module at the given module index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn module_at(&self, index: u32) -> TypeId {
        self.as_ref().module_at(index)
    }

    /// Gets the count of imported, exported, or aliased modules.
    pub fn module_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(_) => 0,
            TypesKind::Component(component) => component.core_modules.len(),
        }
    }

    /// Gets the type of a module instance at the given module instance index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn instance_at(&self, index: u32) -> TypeId {
        self.as_ref().instance_at(index)
    }

    /// Gets the count of imported, exported, or aliased core module instances.
    pub fn instance_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(_) => 0,
            TypesKind::Component(component) => component.core_instances.len(),
        }
    }

    /// Gets the type of a component at the given component index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn component_at(&self, index: u32) -> TypeId {
        self.as_ref().component_at(index)
    }

    /// Gets the count of imported, exported, or aliased components.
    pub fn component_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(_) => 0,
            TypesKind::Component(component) => component.components.len(),
        }
    }

    /// Gets the type of an component instance at the given component instance index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn component_instance_at(&self, index: u32) -> TypeId {
        self.as_ref().component_instance_at(index)
    }

    /// Gets the count of imported, exported, or aliased component instances.
    pub fn component_instance_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(_) => 0,
            TypesKind::Component(component) => component.instances.len(),
        }
    }

    /// Gets the type of a value at the given value index.
    ///
    /// # Panics
    ///
    /// This will panic if the `index` provided is out of bounds or if this type
    /// information represents a core module.
    pub fn value_at(&self, index: u32) -> ComponentValType {
        self.as_ref().value_at(index)
    }

    /// Gets the count of imported, exported, or aliased values.
    pub fn value_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(_) => 0,
            TypesKind::Component(component) => component.values.len(),
        }
    }

    /// Gets the entity type from the given import.
    pub fn entity_type_from_import(&self, import: &Import) -> Option<EntityType> {
        self.as_ref().entity_type_from_import(import)
    }

    /// Gets the entity type from the given export.
    pub fn entity_type_from_export(&self, export: &Export) -> Option<EntityType> {
        self.as_ref().entity_type_from_export(export)
    }

    /// Gets the component entity type for the given component import name.
    pub fn component_entity_type_of_import(&self, name: &str) -> Option<ComponentEntityType> {
        self.as_ref().component_entity_type_of_import(name)
    }

    /// Gets the component entity type for the given component export name.
    pub fn component_entity_type_of_export(&self, name: &str) -> Option<ComponentEntityType> {
        self.as_ref().component_entity_type_of_export(name)
    }

    /// Attempts to lookup the type id that `ty` is an alias of.
    ///
    /// Returns `None` if `ty` wasn't listed as aliasing a prior type.
    pub fn peel_alias(&self, ty: TypeId) -> Option<TypeId> {
        self.list.peel_alias(ty)
    }
}

impl Index<TypeId> for Types {
    type Output = Type;

    fn index(&self, id: TypeId) -> &Type {
        &self.list[id.index]
    }
}

/// This is a type which mirrors a subset of the `Vec<T>` API, but is intended
/// to be able to be cheaply snapshotted and cloned.
///
/// When each module's code sections start we "commit" the current list of types
/// in the global list of types. This means that the temporary `cur` vec here is
/// pushed onto `snapshots` and wrapped up in an `Arc`. At that point we clone
/// this entire list (which is then O(modules), not O(types in all modules)) and
/// pass out as a context to each function validator.
///
/// Otherwise, though, this type behaves as if it were a large `Vec<T>`, but
/// it's represented by lists of contiguous chunks.
pub(crate) struct SnapshotList<T> {
    // All previous snapshots, the "head" of the list that this type represents.
    // The first entry in this pair is the starting index for all elements
    // contained in the list, and the second element is the list itself. Note
    // the `Arc` wrapper around sub-lists, which makes cloning time for this
    // `SnapshotList` O(snapshots) rather than O(snapshots_total), which for
    // us in this context means the number of modules, not types.
    //
    // Note that this list is sorted least-to-greatest in order of the index for
    // binary searching.
    snapshots: Vec<Arc<Snapshot<T>>>,

    // This is the total length of all lists in the `snapshots` array.
    snapshots_total: usize,

    // The current list of types for the current snapshot that are being built.
    cur: Vec<T>,

    unique_mappings: HashMap<u32, u32>,
    unique_counter: u32,
}

struct Snapshot<T> {
    prior_types: usize,
    unique_counter: u32,
    unique_mappings: HashMap<u32, u32>,
    items: Vec<T>,
}

impl<T> SnapshotList<T> {
    /// Same as `<&[T]>::get`
    pub(crate) fn get(&self, index: usize) -> Option<&T> {
        // Check to see if this index falls on our local list
        if index >= self.snapshots_total {
            return self.cur.get(index - self.snapshots_total);
        }
        // ... and failing that we do a binary search to figure out which bucket
        // it's in. Note the `i-1` in the `Err` case because if we don't find an
        // exact match the type is located in the previous bucket.
        let i = match self
            .snapshots
            .binary_search_by_key(&index, |snapshot| snapshot.prior_types)
        {
            Ok(i) => i,
            Err(i) => i - 1,
        };
        let snapshot = &self.snapshots[i];
        Some(&snapshot.items[index - snapshot.prior_types])
    }

    /// Same as `Vec::push`
    pub(crate) fn push(&mut self, val: T) {
        self.cur.push(val);
    }

    /// Same as `<[T]>::len`
    pub(crate) fn len(&self) -> usize {
        self.cur.len() + self.snapshots_total
    }

    /// Reserve space for an additional count of items.
    pub(crate) fn reserve(&mut self, additional: usize) {
        self.cur.reserve(additional);
    }

    /// Commits previously pushed types into this snapshot vector, and returns a
    /// clone of this list.
    ///
    /// The returned `SnapshotList` can be used to access all the same types as
    /// this list itself. This list also is not changed (from an external
    /// perspective) and can continue to access all the same types.
    pub(crate) fn commit(&mut self) -> SnapshotList<T> {
        // If the current chunk has new elements, commit them in to an
        // `Arc`-wrapped vector in the snapshots list. Note the `shrink_to_fit`
        // ahead of time to hopefully keep memory usage lower than it would
        // otherwise be. Additionally note that the `unique_counter` is bumped
        // here to ensure that the previous value of the unique counter is
        // never used for an actual type so it's suitable for lookup via a
        // binary search.
        let len = self.cur.len();
        if len > 0 {
            self.unique_counter += 1;
            self.cur.shrink_to_fit();
            self.snapshots.push(Arc::new(Snapshot {
                prior_types: self.snapshots_total,
                unique_counter: self.unique_counter - 1,
                unique_mappings: mem::take(&mut self.unique_mappings),
                items: mem::take(&mut self.cur),
            }));
            self.snapshots_total += len;
        }
        SnapshotList {
            snapshots: self.snapshots.clone(),
            snapshots_total: self.snapshots_total,
            unique_mappings: HashMap::new(),
            unique_counter: self.unique_counter,
            cur: Vec::new(),
        }
    }

    /// Modifies a `TypeId` to have the same contents but a fresh new unique id.
    ///
    /// This is used during aliasing with components to assign types a unique
    /// identifier that isn't equivalent to anything else but still
    /// points to the same underlying type.
    pub fn with_unique(&mut self, mut ty: TypeId) -> TypeId {
        self.unique_mappings
            .insert(self.unique_counter, ty.unique_id);
        ty.unique_id = self.unique_counter;
        self.unique_counter += 1;
        ty
    }

    /// Attempts to lookup the type id that `ty` is an alias of.
    ///
    /// Returns `None` if `ty` wasn't listed as aliasing a prior type.
    pub fn peel_alias(&self, ty: TypeId) -> Option<TypeId> {
        // The unique counter in each snapshot is the unique counter at the
        // time of the snapshot so it's guaranteed to never be used, meaning
        // that `Ok` should never show up here. With an `Err` it's where the
        // index would be placed meaning that the index in question is the
        // smallest value over the unique id's value, meaning that slot has the
        // mapping we're interested in.
        let i = match self
            .snapshots
            .binary_search_by_key(&ty.unique_id, |snapshot| snapshot.unique_counter)
        {
            Ok(_) => unreachable!(),
            Err(i) => i,
        };

        // If the `i` index is beyond the snapshot array then lookup in the
        // current mappings instead since it may refer to a type not snapshot
        // yet.
        let unique_id = match self.snapshots.get(i) {
            Some(snapshot) => *snapshot.unique_mappings.get(&ty.unique_id)?,
            None => *self.unique_mappings.get(&ty.unique_id)?,
        };
        Some(TypeId { unique_id, ..ty })
    }
}

impl<T> Index<usize> for SnapshotList<T> {
    type Output = T;

    #[inline]
    fn index(&self, index: usize) -> &T {
        self.get(index).unwrap()
    }
}

impl<T> Index<TypeId> for SnapshotList<T> {
    type Output = T;

    #[inline]
    fn index(&self, id: TypeId) -> &T {
        self.get(id.index).unwrap()
    }
}

impl<T> Default for SnapshotList<T> {
    fn default() -> SnapshotList<T> {
        SnapshotList {
            snapshots: Vec::new(),
            snapshots_total: 0,
            cur: Vec::new(),
            unique_counter: 1,
            unique_mappings: HashMap::new(),
        }
    }
}

/// A snapshot list of types.
pub(crate) type TypeList = SnapshotList<Type>;

/// Thin wrapper around `TypeList` which provides an allocator of unique ids for
/// types contained within this list.
pub(crate) struct TypeAlloc {
    list: TypeList,

    // This is assigned at creation of a `TypeAlloc` and then never changed.
    // It's used in one entry for all `ResourceId`s contained within.
    globally_unique_id: u64,

    // This is a counter that's incremeneted each time `alloc_resource_id` is
    // called.
    next_resource_id: u32,
}

impl Default for TypeAlloc {
    fn default() -> TypeAlloc {
        static NEXT_GLOBAL_ID: AtomicU64 = AtomicU64::new(0);
        TypeAlloc {
            list: TypeList::default(),
            globally_unique_id: NEXT_GLOBAL_ID.fetch_add(1, Ordering::Relaxed),
            next_resource_id: 0,
        }
    }
}

impl Deref for TypeAlloc {
    type Target = TypeList;
    fn deref(&self) -> &TypeList {
        &self.list
    }
}

impl DerefMut for TypeAlloc {
    fn deref_mut(&mut self) -> &mut TypeList {
        &mut self.list
    }
}

impl TypeAlloc {
    /// Pushes a new type into this list, returning an identifier which can be
    /// used to later retrieve it.
    ///
    /// The returned identifier is unique within this `TypeAlloc` and won't be
    /// hash-equivalent to anything else.
    pub fn push_ty(&mut self, ty: Type) -> TypeId {
        let index = self.list.len();
        let info = ty.info();
        self.list.push(ty);
        TypeId {
            index,
            info,
            unique_id: 0,
        }
    }

    /// Allocates a new unique resource identifier.
    ///
    /// Note that uniqueness is only a property within this `TypeAlloc`.
    pub fn alloc_resource_id(&mut self) -> ResourceId {
        let contextually_unique_id = self.next_resource_id;
        self.next_resource_id = self.next_resource_id.checked_add(1).unwrap();
        ResourceId {
            globally_unique_id: self.globally_unique_id,
            contextually_unique_id,
        }
    }

    /// Adds the set of "free variables" of the `id` provided to the `set`
    /// provided.
    ///
    /// Free variables are defined as resources. Any resource, perhaps
    /// transitively, referred to but not defined by `id` is added to the `set`
    /// and returned.
    pub fn free_variables_type_id(&self, id: TypeId, set: &mut IndexSet<ResourceId>) {
        match &self[id] {
            // Core wasm constructs cannot reference resources.
            Type::Sub(_) | Type::Module(_) | Type::Instance(_) => {}

            // Recurse on the imports/exports of components, but remove the
            // imported and defined resources within the component itself.
            //
            // Technically this needs to add all the free variables of the
            // exports, remove the defined resources, then add the free
            // variables of imports, then remove the imported resources. Given
            // prior validation of component types, however, the defined
            // and imported resources are disjoint and imports can't refer to
            // defined resources, so doing this all in one go should be
            // equivalent.
            Type::Component(i) => {
                for ty in i.imports.values().chain(i.exports.values()) {
                    self.free_variables_component_entity(ty, set);
                }
                for (id, _path) in i.imported_resources.iter().chain(&i.defined_resources) {
                    set.remove(id);
                }
            }

            // Like components, add in all the free variables of referenced
            // types but then remove those defined by this component instance
            // itself.
            Type::ComponentInstance(i) => {
                for ty in i.exports.values() {
                    self.free_variables_component_entity(ty, set);
                }
                for id in i.defined_resources.iter() {
                    set.remove(id);
                }
            }

            Type::Resource(r) => {
                set.insert(*r);
            }

            Type::ComponentFunc(i) => {
                for ty in i
                    .params
                    .iter()
                    .map(|(_, ty)| ty)
                    .chain(i.results.iter().map(|(_, ty)| ty))
                {
                    self.free_variables_valtype(ty, set);
                }
            }

            Type::Defined(i) => match i {
                ComponentDefinedType::Primitive(_)
                | ComponentDefinedType::Flags(_)
                | ComponentDefinedType::Enum(_) => {}
                ComponentDefinedType::Record(r) => {
                    for ty in r.fields.values() {
                        self.free_variables_valtype(ty, set);
                    }
                }
                ComponentDefinedType::Tuple(r) => {
                    for ty in r.types.iter() {
                        self.free_variables_valtype(ty, set);
                    }
                }
                ComponentDefinedType::Variant(r) => {
                    for ty in r.cases.values() {
                        if let Some(ty) = &ty.ty {
                            self.free_variables_valtype(ty, set);
                        }
                    }
                }
                ComponentDefinedType::List(ty) | ComponentDefinedType::Option(ty) => {
                    self.free_variables_valtype(ty, set);
                }
                ComponentDefinedType::Result { ok, err } => {
                    if let Some(ok) = ok {
                        self.free_variables_valtype(ok, set);
                    }
                    if let Some(err) = err {
                        self.free_variables_valtype(err, set);
                    }
                }
                ComponentDefinedType::Own(id) | ComponentDefinedType::Borrow(id) => {
                    self.free_variables_type_id(*id, set);
                }
            },
        }
    }

    /// Same as `free_variables_type_id`, but for `ComponentEntityType`.
    pub fn free_variables_component_entity(
        &self,
        ty: &ComponentEntityType,
        set: &mut IndexSet<ResourceId>,
    ) {
        match ty {
            ComponentEntityType::Module(id)
            | ComponentEntityType::Func(id)
            | ComponentEntityType::Instance(id)
            | ComponentEntityType::Component(id) => self.free_variables_type_id(*id, set),
            ComponentEntityType::Type { created, .. } => {
                self.free_variables_type_id(*created, set);
            }
            ComponentEntityType::Value(ty) => self.free_variables_valtype(ty, set),
        }
    }

    /// Same as `free_variables_type_id`, but for `ComponentValType`.
    fn free_variables_valtype(&self, ty: &ComponentValType, set: &mut IndexSet<ResourceId>) {
        match ty {
            ComponentValType::Primitive(_) => {}
            ComponentValType::Type(id) => self.free_variables_type_id(*id, set),
        }
    }

    /// Returns whether the type `id` is "named" where named types are presented
    /// via the provided `set`.
    ///
    /// This requires that `id` is a `Defined` type.
    pub(crate) fn type_named_type_id(&self, id: TypeId, set: &HashSet<TypeId>) -> bool {
        let ty = self[id].unwrap_defined();
        match ty {
            // Primitives are always considered named
            ComponentDefinedType::Primitive(_) => true,

            // These structures are never allowed to be anonymous, so they
            // themselves must be named.
            ComponentDefinedType::Flags(_)
            | ComponentDefinedType::Enum(_)
            | ComponentDefinedType::Record(_)
            | ComponentDefinedType::Variant(_) => set.contains(&id),

            // All types below here are allowed to be anonymous, but their
            // own components must be appropriately named.
            ComponentDefinedType::Tuple(r) => {
                r.types.iter().all(|t| self.type_named_valtype(t, set))
            }
            ComponentDefinedType::Result { ok, err } => {
                ok.as_ref()
                    .map(|t| self.type_named_valtype(t, set))
                    .unwrap_or(true)
                    && err
                        .as_ref()
                        .map(|t| self.type_named_valtype(t, set))
                        .unwrap_or(true)
            }
            ComponentDefinedType::List(ty) | ComponentDefinedType::Option(ty) => {
                self.type_named_valtype(ty, set)
            }

            // own/borrow themselves don't have to be named, but the resource
            // they refer to must be named.
            ComponentDefinedType::Own(id) | ComponentDefinedType::Borrow(id) => set.contains(id),
        }
    }

    pub(crate) fn type_named_valtype(&self, ty: &ComponentValType, set: &HashSet<TypeId>) -> bool {
        match ty {
            ComponentValType::Primitive(_) => true,
            ComponentValType::Type(id) => self.type_named_type_id(*id, set),
        }
    }
}

/// A helper trait to provide the functionality necessary to resources within a
/// type.
///
/// This currently exists to abstract over `TypeAlloc` and `SubtypeArena` which
/// both need to perform remapping operations.
pub(crate) trait Remap: Index<TypeId, Output = Type> {
    /// Pushes a new anonymous type within this object, returning an identifier
    /// which can be used to refer to it.
    fn push_ty(&mut self, ty: Type) -> TypeId;

    /// Applies a resource and type renaming map to the `id` specified,
    /// returning `true` if the `id` was modified or `false` if it didn't need
    /// changing.
    ///
    /// This will recursively modify the structure of the `id` specified and
    /// update all references to resources found. The resource identifier keys
    /// in the `map` specified will become the corresponding value, in addition
    /// to any existing types in `map` becoming different tyeps.
    ///
    /// The `id` argument will be rewritten to a new identifier if `true` is
    /// returned.
    fn remap_type_id(&mut self, id: &mut TypeId, map: &mut Remapping) -> bool {
        if let Some(new) = map.types.get(id) {
            let changed = *new != *id;
            *id = *new;
            return changed;
        }

        // This function attempts what ends up probably being a relatively
        // minor optimization where a new `id` isn't manufactured unless
        // something about the type actually changed. That means that if the
        // type doesn't actually use any resources, for example, then no new
        // id is allocated.
        let mut any_changed = false;

        let map_map = |tmp: &mut IndexMap<ResourceId, Vec<usize>>,
                       any_changed: &mut bool,
                       map: &mut Remapping| {
            for (id, path) in mem::take(tmp) {
                let id = match map.resources.get(&id) {
                    Some(id) => {
                        *any_changed = true;
                        *id
                    }
                    None => id,
                };
                tmp.insert(id, path);
            }
        };

        let ty = match &self[*id] {
            // Core wasm functions/modules/instances don't have resource types
            // in them.
            Type::Sub(_) | Type::Module(_) | Type::Instance(_) => return false,

            Type::Component(i) => {
                let mut tmp = i.clone();
                for ty in tmp.imports.values_mut().chain(tmp.exports.values_mut()) {
                    if self.remap_component_entity(ty, map) {
                        any_changed = true;
                    }
                }
                for (id, _) in tmp
                    .imported_resources
                    .iter_mut()
                    .chain(&mut tmp.defined_resources)
                {
                    if let Some(new) = map.resources.get(id) {
                        *id = *new;
                        any_changed = true;
                    }
                }
                map_map(&mut tmp.explicit_resources, &mut any_changed, map);
                Type::Component(tmp)
            }

            Type::ComponentInstance(i) => {
                let mut tmp = i.clone();
                for ty in tmp.exports.values_mut() {
                    if self.remap_component_entity(ty, map) {
                        any_changed = true;
                    }
                }
                for id in tmp.defined_resources.iter_mut() {
                    if let Some(new) = map.resources.get(id) {
                        *id = *new;
                        any_changed = true;
                    }
                }
                map_map(&mut tmp.explicit_resources, &mut any_changed, map);
                Type::ComponentInstance(tmp)
            }

            Type::Resource(id) => {
                let id = match map.resources.get(id).copied() {
                    Some(id) => id,
                    None => return false,
                };
                any_changed = true;
                Type::Resource(id)
            }

            Type::ComponentFunc(i) => {
                let mut tmp = i.clone();
                for ty in tmp
                    .params
                    .iter_mut()
                    .map(|(_, ty)| ty)
                    .chain(tmp.results.iter_mut().map(|(_, ty)| ty))
                {
                    if self.remap_valtype(ty, map) {
                        any_changed = true;
                    }
                }
                Type::ComponentFunc(tmp)
            }
            Type::Defined(i) => {
                let mut tmp = i.clone();
                match &mut tmp {
                    ComponentDefinedType::Primitive(_)
                    | ComponentDefinedType::Flags(_)
                    | ComponentDefinedType::Enum(_) => {}
                    ComponentDefinedType::Record(r) => {
                        for ty in r.fields.values_mut() {
                            if self.remap_valtype(ty, map) {
                                any_changed = true;
                            }
                        }
                    }
                    ComponentDefinedType::Tuple(r) => {
                        for ty in r.types.iter_mut() {
                            if self.remap_valtype(ty, map) {
                                any_changed = true;
                            }
                        }
                    }
                    ComponentDefinedType::Variant(r) => {
                        for ty in r.cases.values_mut() {
                            if let Some(ty) = &mut ty.ty {
                                if self.remap_valtype(ty, map) {
                                    any_changed = true;
                                }
                            }
                        }
                    }
                    ComponentDefinedType::List(ty) | ComponentDefinedType::Option(ty) => {
                        if self.remap_valtype(ty, map) {
                            any_changed = true;
                        }
                    }
                    ComponentDefinedType::Result { ok, err } => {
                        if let Some(ok) = ok {
                            if self.remap_valtype(ok, map) {
                                any_changed = true;
                            }
                        }
                        if let Some(err) = err {
                            if self.remap_valtype(err, map) {
                                any_changed = true;
                            }
                        }
                    }
                    ComponentDefinedType::Own(id) | ComponentDefinedType::Borrow(id) => {
                        if self.remap_type_id(id, map) {
                            any_changed = true;
                        }
                    }
                }
                Type::Defined(tmp)
            }
        };

        let new = if any_changed { self.push_ty(ty) } else { *id };
        let prev = map.types.insert(*id, new);
        assert!(prev.is_none());
        let changed = *id != new;
        *id = new;
        changed
    }

    /// Same as `remap_type_id`, but works with `ComponentEntityType`.
    fn remap_component_entity(
        &mut self,
        ty: &mut ComponentEntityType,
        map: &mut Remapping,
    ) -> bool {
        match ty {
            ComponentEntityType::Module(id)
            | ComponentEntityType::Func(id)
            | ComponentEntityType::Instance(id)
            | ComponentEntityType::Component(id) => self.remap_type_id(id, map),
            ComponentEntityType::Type {
                referenced,
                created,
            } => {
                let changed = self.remap_type_id(referenced, map);
                if *referenced == *created {
                    *created = *referenced;
                    changed
                } else {
                    self.remap_type_id(created, map) || changed
                }
            }
            ComponentEntityType::Value(ty) => self.remap_valtype(ty, map),
        }
    }

    /// Same as `remap_type_id`, but works with `ComponentValType`.
    fn remap_valtype(&mut self, ty: &mut ComponentValType, map: &mut Remapping) -> bool {
        match ty {
            ComponentValType::Primitive(_) => false,
            ComponentValType::Type(id) => self.remap_type_id(id, map),
        }
    }
}

#[derive(Default)]
pub(crate) struct Remapping {
    /// A mapping from old resource ID to new resource ID.
    pub(crate) resources: HashMap<ResourceId, ResourceId>,

    /// A mapping filled in during the remapping process which records how a
    /// type was remapped, if applicable. This avoids remapping multiple
    /// references to the same type and instead only processing it once.
    types: HashMap<TypeId, TypeId>,
}

impl Remap for TypeAlloc {
    fn push_ty(&mut self, ty: Type) -> TypeId {
        <TypeAlloc>::push_ty(self, ty)
    }
}

impl Index<TypeId> for TypeAlloc {
    type Output = Type;

    #[inline]
    fn index(&self, id: TypeId) -> &Type {
        &self.list[id]
    }
}

/// Helper structure used to perform subtyping computations.
///
/// This type is used whenever a subtype needs to be tested in one direction or
/// the other. The methods of this type are the various entry points for
/// subtyping.
///
/// Internally this contains arenas for two lists of types. The `a` arena is
/// intended to be used for lookup of the first argument to all of the methods
/// below, and the `b` arena is used for lookup of the second argument.
///
/// Arenas here are used specifically for component-based subtyping queries. In
/// these situations new types must be created based on substitution mappings,
/// but the types all have temporary lifetimes. Everything in these arenas is
/// thrown away once the subtyping computation has finished.
///
/// Note that this subtyping context also explicitly supports being created
/// from to different lists `a` and `b` originally, for testing subtyping
/// between two different components for example.
pub(crate) struct SubtypeCx<'a> {
    pub(crate) a: SubtypeArena<'a>,
    pub(crate) b: SubtypeArena<'a>,
}

impl<'a> SubtypeCx<'a> {
    pub(crate) fn new(a: &'a TypeList, b: &'a TypeList) -> SubtypeCx<'a> {
        SubtypeCx {
            a: SubtypeArena::new(a),
            b: SubtypeArena::new(b),
        }
    }

    fn swap(&mut self) {
        mem::swap(&mut self.a, &mut self.b);
    }

    /// Executes the closure `f`, resetting the internal arenas to their
    /// original size after the closure finishes.
    ///
    /// This enables `f` to modify the internal arenas while relying on all
    /// changes being discarded after the closure finishes.
    fn mark<T>(&mut self, f: impl FnOnce(&mut Self) -> T) -> T {
        let a_len = self.a.list.len();
        let b_len = self.b.list.len();
        let result = f(self);
        self.a.list.truncate(a_len);
        self.b.list.truncate(b_len);
        result
    }

    /// Tests whether `a` is a subtype of `b`.
    ///
    /// Errors are reported at the `offset` specified.
    pub fn component_entity_type(
        &mut self,
        a: &ComponentEntityType,
        b: &ComponentEntityType,
        offset: usize,
    ) -> Result<()> {
        use ComponentEntityType::*;

        match (a, b) {
            (Module(a), Module(b)) => {
                // For module type subtyping, all exports in the other module
                // type must be present in this module type's exports (i.e. it
                // can export *more* than what this module type needs).
                // However, for imports, the check is reversed (i.e. it is okay
                // to import *less* than what this module type needs).
                self.swap();
                let a_imports = &self.b[*a].unwrap_module().imports;
                let b_imports = &self.a[*b].unwrap_module().imports;
                for (k, a) in a_imports {
                    match b_imports.get(k) {
                        Some(b) => self.entity_type(b, a, offset).with_context(|| {
                            format!("type mismatch in import `{}::{}`", k.0, k.1)
                        })?,
                        None => bail!(offset, "missing expected import `{}::{}`", k.0, k.1),
                    }
                }
                self.swap();
                let a = self.a[*a].unwrap_module();
                let b = self.b[*b].unwrap_module();
                for (k, b) in b.exports.iter() {
                    match a.exports.get(k) {
                        Some(a) => self
                            .entity_type(a, b, offset)
                            .with_context(|| format!("type mismatch in export `{k}`"))?,
                        None => bail!(offset, "missing expected export `{k}`"),
                    }
                }
                Ok(())
            }
            (Module(_), b) => bail!(offset, "expected {}, found module", b.desc()),

            (Func(a), Func(b)) => {
                let a = self.a[*a].unwrap_component_func();
                let b = self.b[*b].unwrap_component_func();

                // Note that this intentionally diverges from the upstream
                // specification in terms of subtyping. This is a full
                // type-equality check which ensures that the structure of `a`
                // exactly matches the structure of `b`. The rationale for this
                // is:
                //
                // * Primarily in Wasmtime subtyping based on function types is
                //   not implemented. This includes both subtyping a host
                //   import and additionally handling subtyping as functions
                //   cross component boundaries. The host import subtyping (or
                //   component export subtyping) is not clear how to handle at
                //   all at this time. The subtyping of functions between
                //   components can more easily be handled by extending the
                //   `fact` compiler, but that hasn't been done yet.
                //
                // * The upstream specification is currently pretty
                //   intentionally vague precisely what subtyping is allowed.
                //   Implementing a strict check here is intended to be a
                //   conservative starting point for the component model which
                //   can be extended in the future if necessary.
                //
                // * The interaction with subtyping on bindings generation, for
                //   example, is a tricky problem that doesn't have a clear
                //   answer at this time.  Effectively this is more rationale
                //   for being conservative in the first pass of the component
                //   model.
                //
                // So, in conclusion, the test here (and other places that
                // reference this comment) is for exact type equality with no
                // differences.
                if a.params.len() != b.params.len() {
                    bail!(
                        offset,
                        "expected {} parameters, found {}",
                        b.params.len(),
                        a.params.len(),
                    );
                }
                if a.results.len() != b.results.len() {
                    bail!(
                        offset,
                        "expected {} results, found {}",
                        b.results.len(),
                        a.results.len(),
                    );
                }
                for ((an, a), (bn, b)) in a.params.iter().zip(b.params.iter()) {
                    if an != bn {
                        bail!(offset, "expected parameter named `{bn}`, found `{an}`");
                    }
                    self.component_val_type(a, b, offset)
                        .with_context(|| format!("type mismatch in function parameter `{an}`"))?;
                }
                for ((an, a), (bn, b)) in a.results.iter().zip(b.results.iter()) {
                    if an != bn {
                        bail!(offset, "mismatched result names");
                    }
                    self.component_val_type(a, b, offset)
                        .with_context(|| "type mismatch with result type")?;
                }
                Ok(())
            }
            (Func(_), b) => bail!(offset, "expected {}, found func", b.desc()),

            (Value(a), Value(b)) => self.component_val_type(a, b, offset),
            (Value(_), b) => bail!(offset, "expected {}, found value", b.desc()),

            (Type { referenced: a, .. }, Type { referenced: b, .. }) => {
                use self::Type::*;
                match (&self.a[*a], &self.b[*b]) {
                    (Defined(a), Defined(b)) => self.component_defined_type(a, b, offset),
                    (Defined(_), Resource(_)) => bail!(offset, "expected resource, found type"),
                    (Resource(a), Resource(b)) => {
                        if a == b {
                            Ok(())
                        } else {
                            bail!(offset, "resource types are not the same")
                        }
                    }
                    (Resource(_), Defined(_)) => bail!(offset, "expected type, found resource"),
                    _ => unreachable!(),
                }
            }
            (Type { .. }, b) => bail!(offset, "expected {}, found type", b.desc()),

            (Instance(a_id), Instance(b_id)) => {
                // For instance type subtyping, all exports in the other
                // instance type must be present in this instance type's
                // exports (i.e. it can export *more* than what this instance
                // type needs).
                let a = self.a[*a_id].unwrap_component_instance();
                let b = self.b[*b_id].unwrap_component_instance();

                let mut exports = Vec::with_capacity(b.exports.len());
                for (k, b) in b.exports.iter() {
                    match a.exports.get(k) {
                        Some(a) => exports.push((*a, *b)),
                        None => bail!(offset, "missing expected export `{k}`"),
                    }
                }
                for (i, (a, b)) in exports.iter().enumerate() {
                    let err = match self.component_entity_type(a, b, offset) {
                        Ok(()) => continue,
                        Err(e) => e,
                    };
                    // On failure attach the name of this export as context to
                    // the error message to leave a breadcrumb trail.
                    let (name, _) = self.b[*b_id]
                        .unwrap_component_instance()
                        .exports
                        .get_index(i)
                        .unwrap();
                    return Err(
                        err.with_context(|| format!("type mismatch in instance export `{name}`"))
                    );
                }
                Ok(())
            }
            (Instance(_), b) => bail!(offset, "expected {}, found instance", b.desc()),

            (Component(a), Component(b)) => {
                // Components are ... tricky. They follow the same basic
                // structure as core wasm modules, but they also have extra
                // logic to handle resource types. Resources are effectively
                // abstract types so this is sort of where an ML module system
                // in the component model becomes a reality.
                //
                // This also leverages the `open_instance_type` method below
                // heavily which internally has its own quite large suite of
                // logic. More-or-less what's happening here is:
                //
                // 1. Pretend that the imports of B are given as values to the
                //    imports of A. If A didn't import anything, for example,
                //    that's great and the subtyping definitely passes there.
                //    This operation produces a mapping of all the resources of
                //    A's imports to resources in B's imports.
                //
                // 2. This mapping is applied to all of A's exports. This means
                //    that all exports of A referring to A's imported resources
                //    now instead refer to B's. Note, though that A's exports
                //    still refer to its own defined resources.
                //
                // 3. The same `open_instance_type` method used during the
                //    first step is used again, but this time on the exports
                //    in the reverse direction. This performs a similar
                //    operation, though, by creating a mapping from B's
                //    defined resources to A's defined resources. The map
                //    itself is discarded as it's not needed.
                //
                // The order that everything passed here is intentional, but
                // also subtle. I personally think of it as
                // `open_instance_type` takes a list of things to satisfy a
                // signature and produces a mapping of resources in the
                // signature to those provided in the list of things. The
                // order of operations then goes:
                //
                // * Someone thinks they have a component of type B, but they
                //   actually have a component of type A (e.g. due to this
                //   subtype check passing).
                // * This person provides the imports of B and that must be
                //   sufficient to satisfy the imports of A. This is the first
                //   `open_instance_type` check.
                // * Now though the resources provided by B are substituted
                //   into A's exports since that's what was provided.
                // * A's exports are then handed back to the original person,
                //   and these exports must satisfy the signature required by B
                //   since that's what they're expecting.
                // * This is the second `open_instance_type` which, to get
                //   resource types to line up, will map from A's defined
                //   resources to B's defined resources.
                //
                // If all that passes then the resources should all line up
                // perfectly. Any misalignment is reported as a subtyping
                // error.
                let b_imports = self.b[*b]
                    .unwrap_component()
                    .imports
                    .iter()
                    .map(|(name, ty)| (name.clone(), ty.clone()))
                    .collect();
                self.swap();
                let mut import_mapping =
                    self.open_instance_type(&b_imports, *a, ExternKind::Import, offset)?;
                self.swap();
                self.mark(|this| {
                    let mut a_exports = this.a[*a]
                        .unwrap_component()
                        .exports
                        .iter()
                        .map(|(name, ty)| (name.clone(), ty.clone()))
                        .collect::<IndexMap<_, _>>();
                    for ty in a_exports.values_mut() {
                        this.a.remap_component_entity(ty, &mut import_mapping);
                    }
                    this.open_instance_type(&a_exports, *b, ExternKind::Export, offset)?;
                    Ok(())
                })
            }
            (Component(_), b) => bail!(offset, "expected {}, found component", b.desc()),
        }
    }

    /// The building block for subtyping checks when components are
    /// instantiated and when components are tested if they're subtypes of each
    /// other.
    ///
    /// This method takes a number of arguments:
    ///
    /// * `a` - this is a list of typed items which can be thought of as
    ///   concrete values to test against `b`.
    /// * `b` - this `TypeId` must point to `Type::Component`.
    /// * `kind` - indicates whether the `imports` or `exports` of `b` are
    ///   being tested against for the values in `a`.
    /// * `offset` - the binary offset at which to report errors if one happens.
    ///
    /// This will attempt to determine if the items in `a` satisfy the
    /// signature required by the `kind` items of `b`. For example component
    /// instantiation will have `a` as the list of arguments provided to
    /// instantiation, `b` is the component being instantiated, and `kind` is
    /// `ExternKind::Import`.
    ///
    /// This function, if successful, will return a mapping of the resources in
    /// `b` to the resources in `a` provided. This mapping is guaranteed to
    /// contain all the resources for `b` (all imported resources for
    /// `ExternKind::Import` or all defined resources for `ExternKind::Export`).
    pub fn open_instance_type(
        &mut self,
        a: &IndexMap<String, ComponentEntityType>,
        b: TypeId,
        kind: ExternKind,
        offset: usize,
    ) -> Result<Remapping> {
        // First, determine the mapping from resources in `b` to those supplied
        // by arguments in `a`.
        //
        // This loop will iterate over all the appropriate resources in `b`
        // and find the corresponding resource in `args`. The exact lists
        // in use here depend on the `kind` provided. This necessarily requires
        // a sequence of string lookups to find the corresponding items in each
        // list.
        //
        // The path to each resource in `resources` is precomputed as a list of
        // indexes. The first index is into `b`'s list of `entities`, and gives
        // the name that `b` assigns to the resource.  Each subsequent index,
        // if present, means that this resource was present through a layer of
        // an instance type, and the index is into the instance type's exports.
        // More information about this can be found on
        // `ComponentState::imported_resources`.
        //
        // This loop will follow the list of indices for each resource and, at
        // the same time, walk through the arguments supplied to instantiating
        // the `component_type`. This means that within `component_type`
        // index-based lookups are performed while in `args` name-based
        // lookups are performed.
        //
        // Note that here it's possible that `args` doesn't actually supply the
        // correct type of import for each item since argument checking has
        // not proceeded yet. These type errors, however, aren't handled by
        // this loop and are deferred below to the main subtyping check. That
        // means that `mapping` won't necessarily have a mapping for all
        // imported resources into `component_type`, but that should be ok.
        let component_type = self.b[b].unwrap_component();
        let entities = match kind {
            ExternKind::Import => &component_type.imports,
            ExternKind::Export => &component_type.exports,
        };
        let resources = match kind {
            ExternKind::Import => &component_type.imported_resources,
            ExternKind::Export => &component_type.defined_resources,
        };
        let mut mapping = Remapping::default();
        'outer: for (resource, path) in resources.iter() {
            // Lookup the first path item in `imports` and the corresponding
            // entry in `args` by name.
            let (name, ty) = entities.get_index(path[0]).unwrap();
            let mut ty = *ty;
            let mut arg = a.get(name);

            // Lookup all the subsequent `path` entries, if any, by index in
            // `ty` and by name in `arg`. Type errors in `arg` are skipped over
            // entirely.
            for i in path.iter().skip(1).copied() {
                let id = match ty {
                    ComponentEntityType::Instance(id) => id,
                    _ => unreachable!(),
                };
                let (name, next_ty) = self.b[id]
                    .unwrap_component_instance()
                    .exports
                    .get_index(i)
                    .unwrap();
                ty = *next_ty;
                arg = match arg {
                    Some(ComponentEntityType::Instance(id)) => {
                        self.a[*id].unwrap_component_instance().exports.get(name)
                    }
                    _ => continue 'outer,
                };
            }

            // Double-check that `ty`, the leaf type of `component_type`, is
            // indeed the expected resource.
            if cfg!(debug_assertions) {
                let id = match ty {
                    ComponentEntityType::Type { created, .. } => match &self.a[created] {
                        Type::Resource(r) => *r,
                        _ => unreachable!(),
                    },
                    _ => unreachable!(),
                };
                assert_eq!(id, *resource);
            }

            // The leaf of `arg` should be a type which is a resource. If not
            // it's skipped and this'll wind up generating an error later on in
            // subtype checking below.
            if let Some(ComponentEntityType::Type { created, .. }) = arg {
                if let Type::Resource(r) = &self.b[*created] {
                    mapping.resources.insert(*resource, *r);
                }
            }
        }

        // Now that a mapping from the resources in `b` to the resources in `a`
        // has been determined it's possible to perform the actual subtype
        // check.
        //
        // This subtype check notably needs to ensure that all resource types
        // line up. To achieve this the `mapping` previously calculated is used
        // to perform a substitution on each component entity type.
        //
        // The first loop here performs a name lookup to create a list of
        // values from `a` to expected items in `b`. Once the list is created
        // the substitution check is performed on each element.
        let mut to_typecheck = Vec::new();
        for (name, expected) in entities.iter() {
            match a.get(name) {
                Some(arg) => to_typecheck.push((arg.clone(), expected.clone())),
                None => bail!(offset, "missing {} named `{name}`", kind.desc()),
            }
        }
        let mut type_map = HashMap::new();
        for (i, (actual, expected)) in to_typecheck.into_iter().enumerate() {
            let result = self.mark(|this| {
                let mut expected = expected;
                this.b.remap_component_entity(&mut expected, &mut mapping);
                mapping.types.clear();
                this.component_entity_type(&actual, &expected, offset)
            });
            let err = match result {
                Ok(()) => {
                    // On a successful type-check record a mapping of
                    // type-to-type in `type_map` for any type imports that were
                    // satisfied. This is then used afterwards when performing
                    // type substitution to remap all component-local types to
                    // those that were provided in the imports.
                    self.register_type_renamings(actual, expected, &mut type_map);
                    continue;
                }
                Err(e) => e,
            };

            // If an error happens then attach the name of the entity to the
            // error message using the `i` iteration counter.
            let component_type = self.b[b].unwrap_component();
            let entities = match kind {
                ExternKind::Import => &component_type.imports,
                ExternKind::Export => &component_type.exports,
            };
            let (name, _) = entities.get_index(i).unwrap();
            return Err(err.with_context(|| format!("type mismatch for {} `{name}`", kind.desc())));
        }
        mapping.types = type_map;
        Ok(mapping)
    }

    pub(crate) fn entity_type(&self, a: &EntityType, b: &EntityType, offset: usize) -> Result<()> {
        macro_rules! limits_match {
            ($a:expr, $b:expr) => {{
                let a = $a;
                let b = $b;
                a.initial >= b.initial
                    && match b.maximum {
                        Some(b_max) => match a.maximum {
                            Some(a_max) => a_max <= b_max,
                            None => false,
                        },
                        None => true,
                    }
            }};
        }

        match (a, b) {
            (EntityType::Func(a), EntityType::Func(b)) => {
                self.func_type(self.a[*a].unwrap_func(), self.b[*b].unwrap_func(), offset)
            }
            (EntityType::Func(_), b) => bail!(offset, "expected {}, found func", b.desc()),
            (EntityType::Table(a), EntityType::Table(b)) => {
                if a.element_type != b.element_type {
                    bail!(
                        offset,
                        "expected table element type {}, found {}",
                        b.element_type,
                        a.element_type,
                    )
                }
                if limits_match!(a, b) {
                    Ok(())
                } else {
                    bail!(offset, "mismatch in table limits")
                }
            }
            (EntityType::Table(_), b) => bail!(offset, "expected {}, found table", b.desc()),
            (EntityType::Memory(a), EntityType::Memory(b)) => {
                if a.shared != b.shared {
                    bail!(offset, "mismatch in the shared flag for memories")
                }
                if a.memory64 != b.memory64 {
                    bail!(offset, "mismatch in index type used for memories")
                }
                if limits_match!(a, b) {
                    Ok(())
                } else {
                    bail!(offset, "mismatch in memory limits")
                }
            }
            (EntityType::Memory(_), b) => bail!(offset, "expected {}, found memory", b.desc()),
            (EntityType::Global(a), EntityType::Global(b)) => {
                if a.mutable != b.mutable {
                    bail!(offset, "global types differ in mutability")
                }
                if a.content_type == b.content_type {
                    Ok(())
                } else {
                    bail!(
                        offset,
                        "expected global type {}, found {}",
                        b.content_type,
                        a.content_type,
                    )
                }
            }
            (EntityType::Global(_), b) => bail!(offset, "expected {}, found global", b.desc()),
            (EntityType::Tag(a), EntityType::Tag(b)) => {
                self.func_type(self.a[*a].unwrap_func(), self.b[*b].unwrap_func(), offset)
            }
            (EntityType::Tag(_), b) => bail!(offset, "expected {}, found tag", b.desc()),
        }
    }

    fn func_type(&self, a: &FuncType, b: &FuncType, offset: usize) -> Result<()> {
        if a == b {
            Ok(())
        } else {
            bail!(
                offset,
                "expected: {}\n\
                 found:    {}",
                b.desc(),
                a.desc(),
            )
        }
    }

    pub(crate) fn component_val_type(
        &self,
        a: &ComponentValType,
        b: &ComponentValType,
        offset: usize,
    ) -> Result<()> {
        match (a, b) {
            (ComponentValType::Primitive(a), ComponentValType::Primitive(b)) => {
                self.primitive_val_type(*a, *b, offset)
            }
            (ComponentValType::Type(a), ComponentValType::Type(b)) => self.component_defined_type(
                self.a[*a].unwrap_defined(),
                self.b[*b].unwrap_defined(),
                offset,
            ),
            (ComponentValType::Primitive(a), ComponentValType::Type(b)) => {
                match self.b[*b].unwrap_defined() {
                    ComponentDefinedType::Primitive(b) => self.primitive_val_type(*a, *b, offset),
                    b => bail!(offset, "expected {}, found {a}", b.desc()),
                }
            }
            (ComponentValType::Type(a), ComponentValType::Primitive(b)) => {
                match self.a[*a].unwrap_defined() {
                    ComponentDefinedType::Primitive(a) => self.primitive_val_type(*a, *b, offset),
                    a => bail!(offset, "expected {b}, found {}", a.desc()),
                }
            }
        }
    }

    fn component_defined_type(
        &self,
        a: &ComponentDefinedType,
        b: &ComponentDefinedType,
        offset: usize,
    ) -> Result<()> {
        use ComponentDefinedType::*;

        // Note that the implementation of subtyping here diverges from the
        // upstream specification intentionally, see the documentation on
        // function subtyping for more information.
        match (a, b) {
            (Primitive(a), Primitive(b)) => self.primitive_val_type(*a, *b, offset),
            (Primitive(a), b) => bail!(offset, "expected {}, found {a}", b.desc()),
            (Record(a), Record(b)) => {
                if a.fields.len() != b.fields.len() {
                    bail!(
                        offset,
                        "expected {} fields, found {}",
                        b.fields.len(),
                        a.fields.len(),
                    );
                }

                for ((aname, a), (bname, b)) in a.fields.iter().zip(b.fields.iter()) {
                    if aname != bname {
                        bail!(offset, "expected field name `{bname}`, found `{aname}`");
                    }
                    self.component_val_type(a, b, offset)
                        .with_context(|| format!("type mismatch in record field `{aname}`"))?;
                }
                Ok(())
            }
            (Record(_), b) => bail!(offset, "expected {}, found record", b.desc()),
            (Variant(a), Variant(b)) => {
                if a.cases.len() != b.cases.len() {
                    bail!(
                        offset,
                        "expected {} cases, found {}",
                        b.cases.len(),
                        a.cases.len(),
                    );
                }
                for ((aname, a), (bname, b)) in a.cases.iter().zip(b.cases.iter()) {
                    if aname != bname {
                        bail!(offset, "expected case named `{bname}`, found `{aname}`");
                    }
                    match (&a.ty, &b.ty) {
                        (Some(a), Some(b)) => self
                            .component_val_type(a, b, offset)
                            .with_context(|| format!("type mismatch in variant case `{aname}`"))?,
                        (None, None) => {}
                        (None, Some(_)) => {
                            bail!(offset, "expected case `{aname}` to have a type, found none")
                        }
                        (Some(_), None) => bail!(offset, "expected case `{aname}` to have no type"),
                    }
                }
                Ok(())
            }
            (Variant(_), b) => bail!(offset, "expected {}, found variant", b.desc()),
            (List(a), List(b)) | (Option(a), Option(b)) => self.component_val_type(a, b, offset),
            (List(_), b) => bail!(offset, "expected {}, found list", b.desc()),
            (Option(_), b) => bail!(offset, "expected {}, found option", b.desc()),
            (Tuple(a), Tuple(b)) => {
                if a.types.len() != b.types.len() {
                    bail!(
                        offset,
                        "expected {} types, found {}",
                        b.types.len(),
                        a.types.len(),
                    );
                }
                for (i, (a, b)) in a.types.iter().zip(b.types.iter()).enumerate() {
                    self.component_val_type(a, b, offset)
                        .with_context(|| format!("type mismatch in tuple field {i}"))?;
                }
                Ok(())
            }
            (Tuple(_), b) => bail!(offset, "expected {}, found tuple", b.desc()),
            (at @ Flags(a), Flags(b)) | (at @ Enum(a), Enum(b)) => {
                let desc = match at {
                    Flags(_) => "flags",
                    _ => "enum",
                };
                if a.len() == b.len() && a.iter().eq(b.iter()) {
                    Ok(())
                } else {
                    bail!(offset, "mismatch in {desc} elements")
                }
            }
            (Flags(_), b) => bail!(offset, "expected {}, found flags", b.desc()),
            (Enum(_), b) => bail!(offset, "expected {}, found enum", b.desc()),
            (Result { ok: ao, err: ae }, Result { ok: bo, err: be }) => {
                match (ao, bo) {
                    (None, None) => {}
                    (Some(a), Some(b)) => self
                        .component_val_type(a, b, offset)
                        .with_context(|| "type mismatch in ok variant")?,
                    (None, Some(_)) => bail!(offset, "expected ok type, but found none"),
                    (Some(_), None) => bail!(offset, "expected ok type to not be present"),
                }
                match (ae, be) {
                    (None, None) => {}
                    (Some(a), Some(b)) => self
                        .component_val_type(a, b, offset)
                        .with_context(|| "type mismatch in err variant")?,
                    (None, Some(_)) => bail!(offset, "expected err type, but found none"),
                    (Some(_), None) => bail!(offset, "expected err type to not be present"),
                }
                Ok(())
            }
            (Result { .. }, b) => bail!(offset, "expected {}, found result", b.desc()),
            (Own(a), Own(b)) | (Borrow(a), Borrow(b)) => {
                let a = self.a[*a].unwrap_resource();
                let b = self.b[*b].unwrap_resource();
                if a == b {
                    Ok(())
                } else {
                    bail!(offset, "resource types are not the same")
                }
            }
            (Own(_), b) => bail!(offset, "expected {}, found own", b.desc()),
            (Borrow(_), b) => bail!(offset, "expected {}, found borrow", b.desc()),
        }
    }

    fn primitive_val_type(
        &self,
        a: PrimitiveValType,
        b: PrimitiveValType,
        offset: usize,
    ) -> Result<()> {
        // Note that this intentionally diverges from the upstream specification
        // at this time and only considers exact equality for subtyping
        // relationships.
        //
        // More information can be found in the subtyping implementation for
        // component functions.
        if a == b {
            Ok(())
        } else {
            bail!(offset, "expected primitive `{b}` found primitive `{a}`")
        }
    }

    fn register_type_renamings(
        &self,
        actual: ComponentEntityType,
        expected: ComponentEntityType,
        type_map: &mut HashMap<TypeId, TypeId>,
    ) {
        match (expected, actual) {
            (
                ComponentEntityType::Type {
                    created: expected, ..
                },
                ComponentEntityType::Type {
                    created: actual, ..
                },
            ) => {
                let prev = type_map.insert(expected, actual);
                assert!(prev.is_none());
            }
            (ComponentEntityType::Instance(expected), ComponentEntityType::Instance(actual)) => {
                let actual = self.a[actual].unwrap_component_instance();
                for (name, expected) in self.b[expected].unwrap_component_instance().exports.iter()
                {
                    let actual = actual.exports[name];
                    self.register_type_renamings(actual, *expected, type_map);
                }
            }
            _ => {}
        }
    }
}

/// A helper typed used purely during subtyping as part of `SubtypeCx`.
///
/// This takes a `types` list as input which is the "base" of the ids that can
/// be indexed through this arena. All future types pushed into this, if any,
/// are stored in `self.list`.
///
/// This is intended to have arena-like behavior where everything pushed onto
/// `self.list` is thrown away after a subtyping computation is performed. All
/// new types pushed into this arena are purely temporary.
pub(crate) struct SubtypeArena<'a> {
    types: &'a TypeList,
    list: Vec<Type>,
}

impl<'a> SubtypeArena<'a> {
    fn new(types: &'a TypeList) -> SubtypeArena<'a> {
        SubtypeArena {
            types,
            list: Vec::new(),
        }
    }
}

impl Index<TypeId> for SubtypeArena<'_> {
    type Output = Type;

    fn index(&self, id: TypeId) -> &Type {
        if id.index < self.types.len() {
            &self.types[id]
        } else {
            &self.list[id.index - self.types.len()]
        }
    }
}

impl Remap for SubtypeArena<'_> {
    fn push_ty(&mut self, ty: Type) -> TypeId {
        let index = self.list.len() + self.types.len();
        let info = ty.info();
        self.list.push(ty);
        TypeId {
            index,
            info,
            unique_id: 0,
        }
    }
}

/// Helper trait for adding contextual information to an error, modeled after
/// `anyhow::Context`.
pub(crate) trait Context {
    fn with_context<S>(self, context: impl FnOnce() -> S) -> Self
    where
        S: Into<String>;
}

impl<T> Context for Result<T> {
    fn with_context<S>(self, context: impl FnOnce() -> S) -> Self
    where
        S: Into<String>,
    {
        match self {
            Ok(val) => Ok(val),
            Err(e) => Err(e.with_context(context)),
        }
    }
}

impl Context for BinaryReaderError {
    fn with_context<S>(mut self, context: impl FnOnce() -> S) -> Self
    where
        S: Into<String>,
    {
        self.add_context(context().into());
        self
    }
}
