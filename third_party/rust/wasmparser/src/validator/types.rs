//! Types relating to type information provided by validation.

use super::{component::ComponentState, core::Module};
use crate::{FuncType, GlobalType, MemoryType, PrimitiveValType, TableType, ValType};
use indexmap::{IndexMap, IndexSet};
use std::{
    borrow::Borrow,
    hash::{Hash, Hasher},
    mem,
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
        FuncType {
            params: self.params.as_slice().to_vec().into_boxed_slice(),
            returns: self.results.as_slice().to_vec().into_boxed_slice(),
        }
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
        PrimitiveValType::Unit => true,
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
pub struct TypeId {
    /// The effective type size for the type.
    ///
    /// This is stored as part of the ID to avoid having to recurse through
    /// the global type list when calculating type sizes.
    pub(crate) type_size: usize,
    /// The index into the global list of types.
    pub(crate) index: usize,
}

/// A unified type definition for validating WebAssembly modules and components.
#[derive(Debug)]
pub enum Type {
    /// The definition is for a core function type.
    Func(FuncType),
    /// The definition is for a core module type.
    ///
    /// This variant is only supported when parsing a component.
    Module(ModuleType),
    /// The definition is for a core module instance type.
    ///
    /// This variant is only supported when parsing a component.
    Instance(InstanceType),
    /// The definition is for a component type.
    ///
    /// This variant is only supported when parsing a component.
    Component(ComponentType),
    /// The definition is for a component instance type.
    ///
    /// This variant is only supported when parsing a component.
    ComponentInstance(ComponentInstanceType),
    /// The definition is for a component function type.
    ///
    /// This variant is only supported when parsing a component.
    ComponentFunc(ComponentFuncType),
    /// The definition is for a component defined type.
    ///
    /// This variant is only supported when parsing a component.
    Defined(ComponentDefinedType),
}

impl Type {
    /// Converts the type to a core function type.
    pub fn as_func_type(&self) -> Option<&FuncType> {
        match self {
            Self::Func(ty) => Some(ty),
            _ => None,
        }
    }

    /// Converts the type to a core module type.
    pub fn as_module_type(&self) -> Option<&ModuleType> {
        match self {
            Self::Module(ty) => Some(ty),
            _ => None,
        }
    }

    /// Converts the type to a core module instance type.
    pub fn as_instance_type(&self) -> Option<&InstanceType> {
        match self {
            Self::Instance(ty) => Some(ty),
            _ => None,
        }
    }

    /// Converts the type to a component type.
    pub fn as_component_type(&self) -> Option<&ComponentType> {
        match self {
            Self::Component(ty) => Some(ty),
            _ => None,
        }
    }

    /// Converts the type to a component instance type.
    pub fn as_component_instance_type(&self) -> Option<&ComponentInstanceType> {
        match self {
            Self::ComponentInstance(ty) => Some(ty),
            _ => None,
        }
    }

    /// Converts the type to a component function type.
    pub fn as_component_func_type(&self) -> Option<&ComponentFuncType> {
        match self {
            Self::ComponentFunc(ty) => Some(ty),
            _ => None,
        }
    }

    /// Converts the type to a component defined type.
    pub fn as_defined_type(&self) -> Option<&ComponentDefinedType> {
        match self {
            Self::Defined(ty) => Some(ty),
            _ => None,
        }
    }

    pub(crate) fn type_size(&self) -> usize {
        match self {
            Self::Func(ty) => 1 + ty.params.len() + ty.returns.len(),
            Self::Module(ty) => ty.type_size,
            Self::Instance(ty) => ty.type_size,
            Self::Component(ty) => ty.type_size,
            Self::ComponentInstance(ty) => ty.type_size,
            Self::ComponentFunc(ty) => ty.type_size,
            Self::Defined(ty) => ty.type_size(),
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
    pub(crate) fn requires_realloc(&self, types: &TypeList) -> bool {
        match self {
            ComponentValType::Primitive(ty) => ty.requires_realloc(),
            ComponentValType::Type(ty) => types[*ty]
                .as_defined_type()
                .unwrap()
                .requires_realloc(types),
        }
    }

    pub(crate) fn is_optional(&self, types: &TypeList) -> bool {
        match self {
            ComponentValType::Primitive(_) => false,
            ComponentValType::Type(ty) => {
                matches!(
                    types[*ty].as_defined_type().unwrap(),
                    ComponentDefinedType::Option(_)
                )
            }
        }
    }

    /// Determines if this component value type is a subtype of the given one.
    pub fn is_subtype_of(&self, other: &Self, types: TypesRef) -> bool {
        self.internal_is_subtype_of(other, types.list)
    }

    pub(crate) fn internal_is_subtype_of(&self, other: &Self, types: &TypeList) -> bool {
        match (self, other) {
            (ComponentValType::Primitive(ty), ComponentValType::Primitive(other_ty)) => {
                ty.is_subtype_of(other_ty)
            }
            (ComponentValType::Type(ty), ComponentValType::Type(other_ty)) => types[*ty]
                .as_defined_type()
                .unwrap()
                .internal_is_subtype_of(types[*other_ty].as_defined_type().unwrap(), types),
            (ComponentValType::Primitive(ty), ComponentValType::Type(other_ty)) => {
                match types[*other_ty].as_defined_type().unwrap() {
                    ComponentDefinedType::Primitive(other_ty) => ty.is_subtype_of(other_ty),
                    _ => false,
                }
            }
            (ComponentValType::Type(ty), ComponentValType::Primitive(other_ty)) => {
                match types[*ty].as_defined_type().unwrap() {
                    ComponentDefinedType::Primitive(ty) => ty.is_subtype_of(other_ty),
                    _ => false,
                }
            }
        }
    }

    fn push_wasm_types(&self, types: &TypeList, lowered_types: &mut LoweredTypes) -> bool {
        match self {
            Self::Primitive(ty) => push_primitive_wasm_types(ty, lowered_types),
            Self::Type(id) => types[*id]
                .as_defined_type()
                .unwrap()
                .push_wasm_types(types, lowered_types),
        }
    }

    pub(crate) fn type_size(&self) -> usize {
        match self {
            Self::Primitive(ty) => ty.type_size(),
            Self::Type(id) => id.type_size,
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
    /// Determines if this entity type is a subtype of the given one.
    pub fn is_subtype_of(&self, other: &Self, types: TypesRef) -> bool {
        self.internal_is_subtype_of(other, types.list)
    }

    pub(crate) fn internal_is_subtype_of(&self, b: &Self, types: &TypeList) -> bool {
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

        match (self, b) {
            (EntityType::Func(a), EntityType::Func(b)) => {
                types[*a].as_func_type().unwrap() == types[*b].as_func_type().unwrap()
            }
            (EntityType::Table(a), EntityType::Table(b)) => {
                a.element_type == b.element_type && limits_match!(a, b)
            }
            (EntityType::Memory(a), EntityType::Memory(b)) => {
                a.shared == b.shared && a.memory64 == b.memory64 && limits_match!(a, b)
            }
            (EntityType::Global(a), EntityType::Global(b)) => a == b,
            (EntityType::Tag(a), EntityType::Tag(b)) => {
                types[*a].as_func_type().unwrap() == types[*b].as_func_type().unwrap()
            }
            _ => false,
        }
    }

    pub(crate) fn desc(&self) -> &'static str {
        match self {
            Self::Func(_) => "function",
            Self::Table(_) => "table",
            Self::Memory(_) => "memory",
            Self::Global(_) => "global",
            Self::Tag(_) => "tag",
        }
    }

    pub(crate) fn type_size(&self) -> usize {
        match self {
            Self::Func(id) | Self::Tag(id) => id.type_size,
            Self::Table(_) | Self::Memory(_) | Self::Global(_) => 1,
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
    /// The effective type size for the module type.
    pub(crate) type_size: usize,
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

    /// Determines if this module type is a subtype of the given one.
    pub fn is_subtype_of(&self, other: &Self, types: TypesRef) -> bool {
        self.internal_is_subtype_of(other, types.list)
    }

    pub(crate) fn internal_is_subtype_of(&self, other: &Self, types: &TypeList) -> bool {
        // For module type subtyping, all exports in the other module type
        // must be present in this module type's exports (i.e. it can export
        // *more* than what this module type needs).
        // However, for imports, the check is reversed (i.e. it is okay
        // to import *less* than what this module type needs).
        self.imports
            .iter()
            .all(|(k, ty)| match other.imports.get(k) {
                Some(other) => other.internal_is_subtype_of(ty, types),
                None => false,
            })
            && other
                .exports
                .iter()
                .all(|(k, other)| match self.exports.get(k) {
                    Some(ty) => ty.internal_is_subtype_of(other, types),
                    None => false,
                })
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
    /// The effective type size for the module instance type.
    pub(crate) type_size: usize,
    /// The kind of module instance type.
    pub kind: InstanceTypeKind,
}

impl InstanceType {
    pub(crate) fn exports<'a>(&'a self, types: &'a TypeList) -> &'a IndexMap<String, EntityType> {
        match &self.kind {
            InstanceTypeKind::Instantiated(id) => &types[*id].as_module_type().unwrap().exports,
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
    Type(TypeId),
    /// The entity is a component instance.
    Instance(TypeId),
    /// The entity is a component.
    Component(TypeId),
}

impl ComponentEntityType {
    /// Determines if this component entity type is a subtype of the given one.
    pub fn is_subtype_of(&self, other: &Self, types: TypesRef) -> bool {
        self.internal_is_subtype_of(other, types.list)
    }

    pub(crate) fn internal_is_subtype_of(&self, other: &Self, types: &TypeList) -> bool {
        match (self, other) {
            (Self::Module(ty), Self::Module(other_ty)) => types[*ty]
                .as_module_type()
                .unwrap()
                .internal_is_subtype_of(types[*other_ty].as_module_type().unwrap(), types),
            (Self::Func(ty), Self::Func(other_ty)) => types[*ty]
                .as_component_func_type()
                .unwrap()
                .internal_is_subtype_of(types[*other_ty].as_component_func_type().unwrap(), types),
            (Self::Value(ty), Self::Value(other_ty)) => ty.internal_is_subtype_of(other_ty, types),
            (Self::Type(ty), Self::Type(other_ty)) => types[*ty]
                .as_defined_type()
                .unwrap()
                .internal_is_subtype_of(types[*other_ty].as_defined_type().unwrap(), types),
            (Self::Instance(ty), Self::Instance(other_ty)) => types[*ty]
                .as_component_instance_type()
                .unwrap()
                .internal_is_subtype_of(
                    types[*other_ty].as_component_instance_type().unwrap(),
                    types,
                ),
            (Self::Component(ty), Self::Component(other_ty)) => types[*ty]
                .as_component_type()
                .unwrap()
                .internal_is_subtype_of(types[*other_ty].as_component_type().unwrap(), types),
            _ => false,
        }
    }

    pub(crate) fn desc(&self) -> &'static str {
        match self {
            Self::Module(_) => "module",
            Self::Func(_) => "function",
            Self::Value(_) => "value",
            Self::Type(_) => "type",
            Self::Instance(_) => "instance",
            Self::Component(_) => "component",
        }
    }

    pub(crate) fn type_size(&self) -> usize {
        match self {
            Self::Module(ty)
            | Self::Func(ty)
            | Self::Type(ty)
            | Self::Instance(ty)
            | Self::Component(ty) => ty.type_size,
            Self::Value(ty) => ty.type_size(),
        }
    }
}

/// Represents a type of a component.
#[derive(Debug, Clone)]
pub struct ComponentType {
    /// The effective type size for the component type.
    pub(crate) type_size: usize,
    /// The imports of the component type.
    pub imports: IndexMap<String, ComponentEntityType>,
    /// The exports of the component type.
    pub exports: IndexMap<String, ComponentEntityType>,
}

impl ComponentType {
    /// Determines if this component type is a subtype of the given one.
    pub fn is_subtype_of(&self, other: &Self, types: TypesRef) -> bool {
        self.internal_is_subtype_of(other, types.list)
    }

    pub(crate) fn internal_is_subtype_of(&self, other: &Self, types: &TypeList) -> bool {
        // For component type subtyping, all exports in the other component type
        // must be present in this component type's exports (i.e. it can export
        // *more* than what this component type needs).
        // However, for imports, the check is reversed (i.e. it is okay
        // to import *less* than what this component type needs).
        self.imports
            .iter()
            .all(|(k, ty)| match other.imports.get(k) {
                Some(other) => other.internal_is_subtype_of(ty, types),
                None => false,
            })
            && other
                .exports
                .iter()
                .all(|(k, other)| match self.exports.get(k) {
                    Some(ty) => ty.internal_is_subtype_of(other, types),
                    None => false,
                })
    }
}

/// Represents the kind of a component instance.
#[derive(Debug, Clone)]
pub enum ComponentInstanceTypeKind {
    /// The instance type is from a definition.
    Defined(IndexMap<String, ComponentEntityType>),
    /// The instance type is the result of instantiating a component type.
    Instantiated(TypeId),
    /// The instance type is the result of instantiating from exported items.
    Exports(IndexMap<String, ComponentEntityType>),
}

/// Represents a type of a component instance.
#[derive(Debug, Clone)]
pub struct ComponentInstanceType {
    /// The effective type size for the instance type.
    pub(crate) type_size: usize,
    /// The kind of instance type.
    pub kind: ComponentInstanceTypeKind,
}

impl ComponentInstanceType {
    pub(crate) fn exports<'a>(
        &'a self,
        types: &'a TypeList,
    ) -> &'a IndexMap<String, ComponentEntityType> {
        match &self.kind {
            ComponentInstanceTypeKind::Defined(exports)
            | ComponentInstanceTypeKind::Exports(exports) => exports,
            ComponentInstanceTypeKind::Instantiated(id) => {
                &types[*id].as_component_type().unwrap().exports
            }
        }
    }

    /// Determines if this component instance type is a subtype of the given one.
    pub fn is_subtype_of(&self, other: &Self, types: TypesRef) -> bool {
        self.internal_is_subtype_of(other, types.list)
    }

    pub(crate) fn internal_is_subtype_of(&self, other: &Self, types: &TypeList) -> bool {
        let exports = self.exports(types);

        // For instance type subtyping, all exports in the other instance type
        // must be present in this instance type's exports (i.e. it can export
        // *more* than what this instance type needs).
        other
            .exports(types)
            .iter()
            .all(|(k, other)| match exports.get(k) {
                Some(ty) => ty.internal_is_subtype_of(other, types),
                None => false,
            })
    }
}

/// Represents a type of a component function.
#[derive(Debug, Clone)]
pub struct ComponentFuncType {
    /// The effective type size for the component function type.
    pub(crate) type_size: usize,
    /// The function parameters.
    pub params: Box<[(Option<String>, ComponentValType)]>,
    /// The function's result type.
    pub result: ComponentValType,
}

impl ComponentFuncType {
    /// Determines if this component function type is a subtype of the given one.
    pub fn is_subtype_of(&self, other: &Self, types: TypesRef) -> bool {
        self.internal_is_subtype_of(other, types.list)
    }

    pub(crate) fn internal_is_subtype_of(&self, other: &Self, types: &TypeList) -> bool {
        // Subtyping rules:
        // https://github.com/WebAssembly/component-model/blob/17f94ed1270a98218e0e796ca1dad1feb7e5c507/design/mvp/Subtyping.md

        // Covariant on return type
        if !self.result.internal_is_subtype_of(&other.result, types) {
            return false;
        }

        // The supertype cannot have fewer parameters than the subtype.
        if other.params.len() < self.params.len() {
            return false;
        }

        // All overlapping parameters must have the same name and are contravariant subtypes
        for ((name, ty), (other_name, other_ty)) in self.params.iter().zip(other.params.iter()) {
            if name != other_name {
                return false;
            }

            if !other_ty.internal_is_subtype_of(ty, types) {
                return false;
            }
        }

        // All remaining parameters in the supertype must be optional
        // All superfluous parameters in the subtype are ignored
        other
            .params
            .iter()
            .skip(self.params.len())
            .all(|(_, ty)| ty.is_optional(types))
    }

    /// Lowers the component function type to core parameter and result types for the
    /// canonical ABI.
    pub(crate) fn lower(&self, types: &TypeList, import: bool) -> LoweringInfo {
        let mut info = LoweringInfo::default();

        for (_, ty) in self.params.iter() {
            // When `import` is false, it means we're lifting a core function,
            // check if the parameters needs realloc
            if !import {
                info.requires_realloc = info.requires_realloc || ty.requires_realloc(types);
            }

            if !ty.push_wasm_types(types, &mut info.params) {
                // Too many parameters to pass directly
                // Function will have a single pointer parameter to pass the arguments
                // via linear memory
                info.params.clear();
                assert!(info.params.push(ValType::I32));
                info.requires_memory = true;
                info.requires_realloc = true;
                break;
            }
        }

        // When `import` is true, it means we're lowering a component function,
        // check if the result needs realloc
        if import {
            info.requires_realloc = info.requires_realloc || self.result.requires_realloc(types);
        }

        if !self.result.push_wasm_types(types, &mut info.results) {
            // Too many results to return directly, either a retptr parameter will be used (import)
            // or a single pointer will be returned (export)
            info.results.clear();
            if import {
                info.params.max = MAX_LOWERED_TYPES;
                assert!(info.params.push(ValType::I32));
            } else {
                assert!(info.results.push(ValType::I32));
            }
            info.requires_memory = true;
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
    pub ty: ComponentValType,
    /// The name of the variant case refined by this one.
    pub refines: Option<String>,
}

/// Represents a record type.
#[derive(Debug, Clone)]
pub struct RecordType {
    /// The effective type size for the record type.
    pub(crate) type_size: usize,
    /// The map of record fields.
    pub fields: IndexMap<String, ComponentValType>,
}

/// Represents a variant type.
#[derive(Debug, Clone)]
pub struct VariantType {
    /// The effective type size for the variant type.
    pub(crate) type_size: usize,
    /// The map of variant cases.
    pub cases: IndexMap<String, VariantCase>,
}

/// Represents a tuple type.
#[derive(Debug, Clone)]
pub struct TupleType {
    /// The effective type size for the tuple type.
    pub(crate) type_size: usize,
    /// The types of the tuple.
    pub types: Box<[ComponentValType]>,
}

/// Represents a union type.
#[derive(Debug, Clone)]
pub struct UnionType {
    /// The inclusive type count for the union type.
    pub(crate) type_size: usize,
    /// The types of the union.
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
    Flags(IndexSet<String>),
    /// The type is an enumeration.
    Enum(IndexSet<String>),
    /// The type is a union.
    Union(UnionType),
    /// The type is an `option`.
    Option(ComponentValType),
    /// The type is an `expected`.
    Expected(ComponentValType, ComponentValType),
}

impl ComponentDefinedType {
    pub(crate) fn requires_realloc(&self, types: &TypeList) -> bool {
        match self {
            Self::Primitive(ty) => ty.requires_realloc(),
            Self::Record(r) => r.fields.values().any(|ty| ty.requires_realloc(types)),
            Self::Variant(v) => v.cases.values().any(|case| case.ty.requires_realloc(types)),
            Self::List(_) => true,
            Self::Tuple(t) => t.types.iter().any(|ty| ty.requires_realloc(types)),
            Self::Union(u) => u.types.iter().any(|ty| ty.requires_realloc(types)),
            Self::Flags(_) | Self::Enum(_) => false,
            Self::Option(ty) => ty.requires_realloc(types),
            Self::Expected(ok, error) => {
                ok.requires_realloc(types) || error.requires_realloc(types)
            }
        }
    }

    /// Determines if this component defined type is a subtype of the given one.
    pub fn is_subtype_of(&self, other: &Self, types: TypesRef) -> bool {
        self.internal_is_subtype_of(other, types.list)
    }

    pub(crate) fn internal_is_subtype_of(&self, other: &Self, types: &TypeList) -> bool {
        // Subtyping rules according to
        // https://github.com/WebAssembly/component-model/blob/17f94ed1270a98218e0e796ca1dad1feb7e5c507/design/mvp/Subtyping.md
        match (self, other) {
            (Self::Primitive(ty), Self::Primitive(other_ty)) => ty.is_subtype_of(other_ty),
            (Self::Record(r), Self::Record(other_r)) => {
                for (name, ty) in r.fields.iter() {
                    if let Some(other_ty) = other_r.fields.get(name) {
                        if !ty.internal_is_subtype_of(other_ty, types) {
                            return false;
                        }
                    } else {
                        // Superfluous fields can be ignored in the subtype
                    }
                }
                // Check for missing required fields in the supertype
                for (other_name, other_ty) in other_r.fields.iter() {
                    if !other_ty.is_optional(types) && !r.fields.contains_key(other_name) {
                        return false;
                    }
                }
                true
            }
            (Self::Variant(v), Self::Variant(other_v)) => {
                for (name, case) in v.cases.iter() {
                    if let Some(other_case) = other_v.cases.get(name) {
                        // Covariant subtype on the case type
                        if !case.ty.internal_is_subtype_of(&other_case.ty, types) {
                            return false;
                        }
                    } else if let Some(refines) = &case.refines {
                        if !other_v.cases.contains_key(refines) {
                            // The refined value is not in the supertype
                            return false;
                        }
                    } else {
                        // This case is not in the supertype and there is no
                        // default
                        return false;
                    }
                }
                true
            }
            (Self::List(ty), Self::List(other_ty)) | (Self::Option(ty), Self::Option(other_ty)) => {
                ty.internal_is_subtype_of(other_ty, types)
            }
            (Self::Tuple(t), Self::Tuple(other_t)) => {
                if t.types.len() != other_t.types.len() {
                    return false;
                }
                t.types
                    .iter()
                    .zip(other_t.types.iter())
                    .all(|(ty, other_ty)| ty.internal_is_subtype_of(other_ty, types))
            }
            (Self::Union(u), Self::Union(other_u)) => {
                if u.types.len() != other_u.types.len() {
                    return false;
                }
                u.types
                    .iter()
                    .zip(other_u.types.iter())
                    .all(|(ty, other_ty)| ty.internal_is_subtype_of(other_ty, types))
            }
            (Self::Flags(set), Self::Flags(other_set))
            | (Self::Enum(set), Self::Enum(other_set)) => set.is_subset(other_set),
            (Self::Expected(ok, error), Self::Expected(other_ok, other_error)) => {
                ok.internal_is_subtype_of(other_ok, types)
                    && error.internal_is_subtype_of(other_error, types)
            }
            _ => false,
        }
    }

    pub(crate) fn type_size(&self) -> usize {
        match self {
            Self::Primitive(ty) => ty.type_size(),
            Self::Flags(_) | Self::Enum(_) => 1,
            Self::Record(r) => r.type_size,
            Self::Variant(v) => v.type_size,
            Self::Tuple(t) => t.type_size,
            Self::Union(u) => u.type_size,
            Self::List(ty) | Self::Option(ty) => ty.type_size(),
            Self::Expected(ok, error) => ok.type_size() + error.type_size(),
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
                v.cases.iter().map(|(_, case)| &case.ty),
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
            Self::Enum(_) => lowered_types.push(ValType::I32),
            Self::Union(u) => Self::push_variant_wasm_types(u.types.iter(), types, lowered_types),
            Self::Option(ty) => {
                Self::push_variant_wasm_types([ty].into_iter(), types, lowered_types)
            }
            Self::Expected(ok, error) => {
                Self::push_variant_wasm_types([ok, error].into_iter(), types, lowered_types)
            }
        }
    }

    fn push_variant_wasm_types<'a>(
        cases: impl ExactSizeIterator<Item = &'a ComponentValType>,
        types: &TypeList,
        lowered_types: &mut LoweredTypes,
    ) -> bool {
        let pushed = if cases.len() <= u32::max_value() as usize {
            lowered_types.push(ValType::I32)
        } else {
            lowered_types.push(ValType::I64)
        };

        if !pushed {
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

    fn types(&self, core: bool) -> Option<&'a [TypeId]> {
        Some(match &self.kind {
            TypesRefKind::Module(module) => {
                if core {
                    &module.types
                } else {
                    return None;
                }
            }
            TypesRefKind::Component(component) => {
                if core {
                    &component.core_types
                } else {
                    &component.types
                }
            }
        })
    }

    /// Gets a type based on its type id.
    ///
    /// Returns `None` if the type id is unknown.
    pub fn type_from_id(&self, id: TypeId) -> Option<&'a Type> {
        self.list.get(id.index)
    }

    /// Gets a type id from a type index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn id_from_type_index(&self, index: u32, core: bool) -> Option<TypeId> {
        self.types(core)?.get(index as usize).copied()
    }

    /// Gets a type at the given type index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn type_at(&self, index: u32, core: bool) -> Option<&'a Type> {
        self.type_from_id(*self.types(core)?.get(index as usize)?)
    }

    /// Gets a defined core function type at the given type index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn func_type_at(&self, index: u32) -> Option<&'a FuncType> {
        match self.type_at(index, true)? {
            Type::Func(ty) => Some(ty),
            _ => None,
        }
    }

    /// Gets the type of a table at the given table index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn table_at(&self, index: u32) -> Option<TableType> {
        let tables = match &self.kind {
            TypesRefKind::Module(module) => &module.tables,
            TypesRefKind::Component(component) => &component.core_tables,
        };

        tables.get(index as usize).copied()
    }

    /// Gets the type of a memory at the given memory index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn memory_at(&self, index: u32) -> Option<MemoryType> {
        let memories = match &self.kind {
            TypesRefKind::Module(module) => &module.memories,
            TypesRefKind::Component(component) => &component.core_memories,
        };

        memories.get(index as usize).copied()
    }

    /// Gets the type of a global at the given global index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn global_at(&self, index: u32) -> Option<GlobalType> {
        let globals = match &self.kind {
            TypesRefKind::Module(module) => &module.globals,
            TypesRefKind::Component(component) => &component.core_globals,
        };

        globals.get(index as usize).copied()
    }

    /// Gets the type of a tag at the given tag index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn tag_at(&self, index: u32) -> Option<&'a FuncType> {
        let tags = match &self.kind {
            TypesRefKind::Module(module) => &module.tags,
            TypesRefKind::Component(component) => &component.core_tags,
        };

        Some(
            self.list[*tags.get(index as usize)?]
                .as_func_type()
                .unwrap(),
        )
    }

    /// Gets the type of a core function at the given function index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn function_at(&self, index: u32) -> Option<&'a FuncType> {
        let id = match &self.kind {
            TypesRefKind::Module(module) => {
                &module.types[*module.functions.get(index as usize)? as usize]
            }
            TypesRefKind::Component(component) => component.core_funcs.get(index as usize)?,
        };

        match &self.list[*id] {
            Type::Func(ty) => Some(ty),
            _ => None,
        }
    }

    /// Gets the type of an element segment at the given element segment index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn element_at(&self, index: u32) -> Option<ValType> {
        match &self.kind {
            TypesRefKind::Module(module) => module.element_types.get(index as usize).copied(),
            TypesRefKind::Component(_) => None,
        }
    }

    /// Gets the type of a component function at the given function index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn component_function_at(&self, index: u32) -> Option<&'a ComponentFuncType> {
        match &self.kind {
            TypesRefKind::Module(_) => None,
            TypesRefKind::Component(component) => Some(
                self.list[*component.funcs.get(index as usize)?]
                    .as_component_func_type()
                    .unwrap(),
            ),
        }
    }

    /// Gets the type of a module at the given module index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn module_at(&self, index: u32) -> Option<&'a ModuleType> {
        match &self.kind {
            TypesRefKind::Module(_) => None,
            TypesRefKind::Component(component) => Some(
                self.list[*component.core_modules.get(index as usize)?]
                    .as_module_type()
                    .unwrap(),
            ),
        }
    }

    /// Gets the type of a module instance at the given module instance index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn instance_at(&self, index: u32) -> Option<&'a InstanceType> {
        match &self.kind {
            TypesRefKind::Module(_) => None,
            TypesRefKind::Component(component) => {
                let id = component.core_instances.get(index as usize)?;
                match &self.list[*id] {
                    Type::Instance(ty) => Some(ty),
                    _ => None,
                }
            }
        }
    }

    /// Gets the type of a component at the given component index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn component_at(&self, index: u32) -> Option<&'a ComponentType> {
        match &self.kind {
            TypesRefKind::Module(_) => None,
            TypesRefKind::Component(component) => Some(
                self.list[*component.components.get(index as usize)?]
                    .as_component_type()
                    .unwrap(),
            ),
        }
    }

    /// Gets the type of an component instance at the given component instance index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn component_instance_at(&self, index: u32) -> Option<&'a ComponentInstanceType> {
        match &self.kind {
            TypesRefKind::Module(_) => None,
            TypesRefKind::Component(component) => {
                let id = component.instances.get(index as usize)?;
                match &self.list[*id] {
                    Type::ComponentInstance(ty) => Some(ty),
                    _ => None,
                }
            }
        }
    }

    /// Gets the type of a value at the given value index.
    ///
    /// Returns `None` if the type index is out of bounds or the type has not
    /// been parsed yet.
    pub fn value_at(&self, index: u32) -> Option<ComponentValType> {
        match &self.kind {
            TypesRefKind::Module(_) => None,
            TypesRefKind::Component(component) => {
                component.values.get(index as usize).map(|(r, _)| *r)
            }
        }
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
    pub fn type_from_id(&self, id: TypeId) -> Option<&Type> {
        self.as_ref().type_from_id(id)
    }

    /// Gets a type id from a type index.
    ///
    /// Returns `None` if the type index is out of bounds.
    pub fn id_from_type_index(&self, index: u32, core: bool) -> Option<TypeId> {
        self.as_ref().id_from_type_index(index, core)
    }

    /// Gets a type at the given type index.
    ///
    /// Returns `None` if the index is out of bounds.
    pub fn type_at(&self, index: u32, core: bool) -> Option<&Type> {
        self.as_ref().type_at(index, core)
    }

    /// Gets a defined core function type at the given type index.
    ///
    /// Returns `None` if the index is out of bounds.
    pub fn func_type_at(&self, index: u32) -> Option<&FuncType> {
        self.as_ref().func_type_at(index)
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
    /// Returns `None` if the index is out of bounds.
    pub fn table_at(&self, index: u32) -> Option<TableType> {
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
    /// Returns `None` if the index is out of bounds.
    pub fn memory_at(&self, index: u32) -> Option<MemoryType> {
        self.as_ref().memory_at(index)
    }

    /// Gets the count of imported and defined memories.
    pub fn memory_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(module) => module.memories.len(),
            TypesKind::Component(component) => component.core_memories.len(),
        }
    }

    /// Gets the type of a global at the given global index.
    ///
    /// Returns `None` if the index is out of bounds.
    pub fn global_at(&self, index: u32) -> Option<GlobalType> {
        self.as_ref().global_at(index)
    }

    /// Gets the count of imported and defined globals.
    pub fn global_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(module) => module.globals.len(),
            TypesKind::Component(component) => component.core_globals.len(),
        }
    }

    /// Gets the type of a tag at the given tag index.
    ///
    /// Returns `None` if the index is out of bounds.
    pub fn tag_at(&self, index: u32) -> Option<&FuncType> {
        self.as_ref().tag_at(index)
    }

    /// Gets the count of imported and defined tags.
    pub fn tag_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(module) => module.tags.len(),
            TypesKind::Component(component) => component.core_tags.len(),
        }
    }

    /// Gets the type of a core function at the given function index.
    ///
    /// Returns `None` if the index is out of bounds.
    pub fn function_at(&self, index: u32) -> Option<&FuncType> {
        self.as_ref().function_at(index)
    }

    /// Gets the count of imported and defined core functions.
    ///
    /// The count also includes aliased core functions in components.
    pub fn function_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(module) => module.functions.len(),
            TypesKind::Component(component) => component.core_funcs.len(),
        }
    }

    /// Gets the type of an element segment at the given element segment index.
    ///
    /// Returns `None` if the index is out of bounds.
    pub fn element_at(&self, index: u32) -> Option<ValType> {
        self.as_ref().element_at(index)
    }

    /// Gets the count of element segments.
    pub fn element_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(module) => module.element_types.len(),
            TypesKind::Component(_) => 0,
        }
    }

    /// Gets the type of a component function at the given function index.
    ///
    /// Returns `None` if the index is out of bounds.
    pub fn component_function_at(&self, index: u32) -> Option<&ComponentFuncType> {
        self.as_ref().component_function_at(index)
    }

    /// Gets the count of imported, exported, or aliased component functions.
    pub fn component_function_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(_) => 0,
            TypesKind::Component(component) => component.funcs.len(),
        }
    }

    /// Gets the type of a module at the given module index.
    ///
    /// Returns `None` if the index is out of bounds.
    pub fn module_at(&self, index: u32) -> Option<&ModuleType> {
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
    /// Returns `None` if the index is out of bounds.
    pub fn instance_at(&self, index: u32) -> Option<&InstanceType> {
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
    /// Returns `None` if the index is out of bounds.
    pub fn component_at(&self, index: u32) -> Option<&ComponentType> {
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
    /// Returns `None` if the index is out of bounds.
    pub fn component_instance_at(&self, index: u32) -> Option<&ComponentInstanceType> {
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
    /// Returns `None` if the index is out of bounds.
    pub fn value_at(&self, index: u32) -> Option<ComponentValType> {
        self.as_ref().value_at(index)
    }

    /// Gets the count of imported, exported, or aliased values.
    pub fn value_count(&self) -> usize {
        match &self.kind {
            TypesKind::Module(_) => 0,
            TypesKind::Component(component) => component.values.len(),
        }
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
    snapshots: Vec<(usize, Arc<Vec<T>>)>,

    // This is the total length of all lists in the `snapshots` array.
    snapshots_total: usize,

    // The current list of types for the current snapshot that are being built.
    cur: Vec<T>,
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
        let i = match self.snapshots.binary_search_by_key(&index, |(i, _)| *i) {
            Ok(i) => i,
            Err(i) => i - 1,
        };
        let (len, list) = &self.snapshots[i];
        Some(&list[index - len])
    }

    /// Same as `<&mut [T]>::get_mut`, except only works for indexes into the
    /// current snapshot being built.
    ///
    /// # Panics
    ///
    /// Panics if an index is passed in which falls within the
    /// previously-snapshotted list of types. This should never happen in our
    /// context and the panic is intended to weed out possible bugs in
    /// wasmparser.
    pub(crate) fn get_mut(&mut self, index: usize) -> Option<&mut T> {
        if index >= self.snapshots_total {
            return self.cur.get_mut(index - self.snapshots_total);
        }
        panic!("cannot get a mutable reference in snapshotted part of list")
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
        // otherwise be.
        let len = self.cur.len();
        if len > 0 {
            self.cur.shrink_to_fit();
            self.snapshots
                .push((self.snapshots_total, Arc::new(mem::take(&mut self.cur))));
            self.snapshots_total += len;
        }
        SnapshotList {
            snapshots: self.snapshots.clone(),
            snapshots_total: self.snapshots_total,
            cur: Vec::new(),
        }
    }
}

impl<T> std::ops::Index<usize> for SnapshotList<T> {
    type Output = T;

    fn index(&self, index: usize) -> &T {
        self.get(index).unwrap()
    }
}

impl<T> std::ops::IndexMut<usize> for SnapshotList<T> {
    fn index_mut(&mut self, index: usize) -> &mut T {
        self.get_mut(index).unwrap()
    }
}

impl<T> std::ops::Index<TypeId> for SnapshotList<T> {
    type Output = T;

    fn index(&self, id: TypeId) -> &T {
        self.get(id.index).unwrap()
    }
}

impl<T> std::ops::IndexMut<TypeId> for SnapshotList<T> {
    fn index_mut(&mut self, id: TypeId) -> &mut T {
        self.get_mut(id.index).unwrap()
    }
}

impl<T> Default for SnapshotList<T> {
    fn default() -> SnapshotList<T> {
        SnapshotList {
            snapshots: Vec::new(),
            snapshots_total: 0,
            cur: Vec::new(),
        }
    }
}

/// A snapshot list of types.
pub(crate) type TypeList = SnapshotList<Type>;
