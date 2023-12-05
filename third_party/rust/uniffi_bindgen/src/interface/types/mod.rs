/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Basic typesystem for defining a component interface.
//!
//! This module provides the "API-level" typesystem of a UniFFI Rust Component, that is,
//! the types provided by the Rust implementation and consumed callers of the foreign language
//! bindings. Think "objects" and "enums" and "records".
//!
//! The [`Type`] enum represents high-level types that would appear in the public API of
//! a component, such as enums and records as well as primitives like ints and strings.
//! The Rust code that implements a component, and the foreign language bindings that consume it,
//! will both typically deal with such types as their core concern.
//!
//! The set of all [`Type`]s used in a component interface is represented by a `TypeUniverse`,
//! which can be used by the bindings generator code to determine what type-related helper
//! functions to emit for a given component.
//!
//! As a developer working on UniFFI itself, you're likely to spend a fair bit of time thinking
//! about how these API-level types map into the lower-level types of the FFI layer as represented
//! by the [`ffi::FfiType`](super::ffi::FfiType) enum, but that's a detail that is invisible to end users.

use std::{collections::hash_map::Entry, collections::BTreeSet, collections::HashMap, iter};

use anyhow::{bail, Result};
use uniffi_meta::Checksum;

use super::ffi::FfiType;

mod finder;
pub(super) use finder::TypeFinder;
mod resolver;
pub(super) use resolver::{resolve_builtin_type, TypeResolver};

#[derive(Debug, Copy, Clone, Eq, PartialEq, Checksum, Ord, PartialOrd)]
pub enum ObjectImpl {
    Struct,
    Trait,
}

impl ObjectImpl {
    /// Return the fully qualified name which should be used by Rust code for
    /// an object with the given name.
    /// Includes `r#`, traits get a leading `dyn`. If we ever supported associated types, then
    /// this would also include them.
    pub fn rust_name_for(&self, name: &str) -> String {
        if self == &ObjectImpl::Trait {
            format!("dyn r#{name}")
        } else {
            format!("r#{name}")
        }
    }

    // uniffi_meta and procmacro support tend to carry around `is_trait` bools. This makes that
    // mildly less painful
    pub fn from_is_trait(is_trait: bool) -> Self {
        if is_trait {
            ObjectImpl::Trait
        } else {
            ObjectImpl::Struct
        }
    }
}

/// Represents all the different high-level types that can be used in a component interface.
/// At this level we identify user-defined types by name, without knowing any details
/// of their internal structure apart from what type of thing they are (record, enum, etc).
#[derive(Debug, Clone, Eq, PartialEq, Checksum, Ord, PartialOrd)]
pub enum Type {
    // Primitive types.
    UInt8,
    Int8,
    UInt16,
    Int16,
    UInt32,
    Int32,
    UInt64,
    Int64,
    Float32,
    Float64,
    Boolean,
    String,
    Bytes,
    Timestamp,
    Duration,
    Object {
        // The name in the "type universe"
        name: String,
        // How the object is implemented.
        imp: ObjectImpl,
    },
    ForeignExecutor,
    // Types defined in the component API, each of which has a string name.
    Record(String),
    Enum(String),
    CallbackInterface(String),
    // Structurally recursive types.
    Optional(Box<Type>),
    Sequence(Box<Type>),
    Map(Box<Type>, Box<Type>),
    // An FfiConverter we `use` from an external crate
    External {
        name: String,
        crate_name: String,
        kind: ExternalKind,
    },
    // Custom type on the scaffolding side
    Custom {
        name: String,
        builtin: Box<Type>,
    },
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Checksum, Ord, PartialOrd)]
pub enum ExternalKind {
    Interface,
    // Either a record or enum
    DataClass,
}

impl Type {
    pub fn ffi_type(&self) -> FfiType {
        self.into()
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        let nested_types = match self {
            Type::Optional(t) | Type::Sequence(t) => t.iter_types(),
            Type::Map(k, v) => Box::new(k.iter_types().chain(v.iter_types())),
            _ => Box::new(iter::empty()),
        };
        Box::new(std::iter::once(self).chain(nested_types))
    }
}

/// When passing data across the FFI, each `Type` value will be lowered into a corresponding
/// `FfiType` value. This conversion tells you which one.
///
/// Note that the conversion is one-way - given an FfiType, it is not in general possible to
/// tell what the corresponding Type is that it's being used to represent.
impl From<&Type> for FfiType {
    fn from(t: &Type) -> FfiType {
        match t {
            // Types that are the same map to themselves, naturally.
            Type::UInt8 => FfiType::UInt8,
            Type::Int8 => FfiType::Int8,
            Type::UInt16 => FfiType::UInt16,
            Type::Int16 => FfiType::Int16,
            Type::UInt32 => FfiType::UInt32,
            Type::Int32 => FfiType::Int32,
            Type::UInt64 => FfiType::UInt64,
            Type::Int64 => FfiType::Int64,
            Type::Float32 => FfiType::Float32,
            Type::Float64 => FfiType::Float64,
            // Booleans lower into an Int8, to work around a bug in JNA.
            Type::Boolean => FfiType::Int8,
            // Strings are always owned rust values.
            // We might add a separate type for borrowed strings in future.
            Type::String => FfiType::RustBuffer(None),
            // Byte strings are also always owned rust values.
            // We might add a separate type for borrowed byte strings in future as well.
            Type::Bytes => FfiType::RustBuffer(None),
            // Objects are pointers to an Arc<>
            Type::Object { name, .. } => FfiType::RustArcPtr(name.to_owned()),
            // Callback interfaces are passed as opaque integer handles.
            Type::CallbackInterface(_) => FfiType::UInt64,
            Type::ForeignExecutor => FfiType::ForeignExecutorHandle,
            // Other types are serialized into a bytebuffer and deserialized on the other side.
            Type::Enum(_)
            | Type::Record(_)
            | Type::Optional(_)
            | Type::Sequence(_)
            | Type::Map(_, _)
            | Type::Timestamp
            | Type::Duration => FfiType::RustBuffer(None),
            Type::External {
                name,
                kind: ExternalKind::Interface,
                ..
            } => FfiType::RustArcPtr(name.clone()),
            Type::External {
                name,
                kind: ExternalKind::DataClass,
                ..
            } => FfiType::RustBuffer(Some(name.clone())),
            Type::Custom { builtin, .. } => FfiType::from(builtin.as_ref()),
        }
    }
}

// Needed for rust scaffolding askama template
impl From<&&Type> for FfiType {
    fn from(ty: &&Type) -> Self {
        (*ty).into()
    }
}

// A trait so various things can turn into a type.
pub trait AsType: core::fmt::Debug {
    fn as_type(&self) -> Type;
}

impl AsType for Type {
    fn as_type(&self) -> Type {
        self.clone()
    }
}

// Needed to handle &&Type and &&&Type values, which we sometimes end up with in the template code
impl<T, C> AsType for T
where
    T: std::ops::Deref<Target = C> + std::fmt::Debug,
    C: AsType,
{
    fn as_type(&self) -> Type {
        self.deref().as_type()
    }
}

impl From<uniffi_meta::Type> for Type {
    fn from(ty: uniffi_meta::Type) -> Self {
        use uniffi_meta::Type as Ty;

        match ty {
            Ty::U8 => Type::UInt8,
            Ty::U16 => Type::UInt16,
            Ty::U32 => Type::UInt32,
            Ty::U64 => Type::UInt64,
            Ty::I8 => Type::Int8,
            Ty::I16 => Type::Int16,
            Ty::I32 => Type::Int32,
            Ty::I64 => Type::Int64,
            Ty::F32 => Type::Float32,
            Ty::F64 => Type::Float64,
            Ty::Bool => Type::Boolean,
            Ty::String => Type::String,
            Ty::SystemTime => Type::Timestamp,
            Ty::Duration => Type::Duration,
            Ty::ForeignExecutor => Type::ForeignExecutor,
            Ty::Record { name } => Type::Record(name),
            Ty::Enum { name, .. } => Type::Enum(name),
            Ty::ArcObject {
                object_name,
                is_trait,
            } => Type::Object {
                name: object_name,
                imp: ObjectImpl::from_is_trait(is_trait),
            },
            Ty::CallbackInterface { name } => Type::CallbackInterface(name),
            Ty::Custom { name, builtin } => Type::Custom {
                name,
                builtin: builtin.into(),
            },
            Ty::Option { inner_type } => Type::Optional(inner_type.into()),
            Ty::Vec { inner_type } => Type::Sequence(inner_type.into()),
            Ty::HashMap {
                key_type,
                value_type,
            } => Type::Map(key_type.into(), value_type.into()),
        }
    }
}

impl From<uniffi_meta::Type> for Box<Type> {
    fn from(ty: uniffi_meta::Type) -> Self {
        Box::new(ty.into())
    }
}

impl From<Box<uniffi_meta::Type>> for Box<Type> {
    fn from(ty: Box<uniffi_meta::Type>) -> Self {
        Box::new((*ty).into())
    }
}

/// The set of all possible types used in a particular component interface.
///
/// Every component API uses a finite number of types, including primitive types, API-defined
/// types like records and enums, and recursive types such as sequences of the above. Our
/// component API doesn't support fancy generics so this is a finitely-enumerable set, which
/// is useful to be able to operate on explicitly.
///
/// You could imagine this struct doing some clever interning of names and so-on in future,
/// to reduce the overhead of passing around [Type] instances. For now we just do a whole
/// lot of cloning.
#[derive(Debug, Default)]
pub(crate) struct TypeUniverse {
    // Named type definitions (including aliases).
    type_definitions: HashMap<String, Type>,
    // All the types in the universe, by canonical type name, in a well-defined order.
    all_known_types: BTreeSet<Type>,
}

impl TypeUniverse {
    /// Add the definitions of all named [Type]s from a given WebIDL definition.
    ///
    /// This will fail if you try to add a name for which an existing type definition exists.
    pub(super) fn add_type_definitions_from<T: TypeFinder>(&mut self, defn: T) -> Result<()> {
        defn.add_type_definitions_to(self)
    }

    /// Add the definition of a named [Type].
    ///
    /// This will fail if you try to add a name for which an existing type definition exists.
    pub fn add_type_definition(&mut self, name: &str, type_: Type) -> Result<()> {
        if resolve_builtin_type(name).is_some() {
            bail!("please don't shadow builtin types ({name}, {:?})", type_,);
        }
        self.add_known_type(&type_);
        match self.type_definitions.entry(name.to_string()) {
            Entry::Occupied(o) => {
                let existing_def = o.get();
                if type_ == *existing_def && matches!(type_, Type::Record(_) | Type::Enum(_)) {
                    // UDL and proc-macro metadata are allowed to define the same record, enum and
                    // error types, if the definitions match (fields and variants are checked in
                    // add_{record,enum,error}_definition)
                    Ok(())
                } else {
                    bail!(
                        "Conflicting type definition for `{name}`! \
                         existing definition: {existing_def:?}, \
                         new definition: {type_:?}"
                    );
                }
            }
            Entry::Vacant(e) => {
                e.insert(type_);
                Ok(())
            }
        }
    }

    /// Get the [Type] corresponding to a given name, if any.
    pub(super) fn get_type_definition(&self, name: &str) -> Option<Type> {
        self.type_definitions.get(name).cloned()
    }

    /// Get the [Type] corresponding to a given WebIDL type node.
    ///
    /// If the node is a structural type (e.g. a sequence) then this will also add
    /// it to the set of all types seen in the component interface.
    pub(crate) fn resolve_type_expression<T: TypeResolver>(&mut self, expr: T) -> Result<Type> {
        expr.resolve_type_expression(self)
    }

    /// Add a [Type] to the set of all types seen in the component interface.
    pub fn add_known_type(&mut self, type_: &Type) {
        // Types are more likely to already be known than not, so avoid unnecessary cloning.
        if !self.all_known_types.contains(type_) {
            self.all_known_types.insert(type_.to_owned());

            // Add inner types. For UDL, this is actually pointless extra work (as is calling
            // add_known_type from add_function_definition), but for the proc-macro frontend
            // this is important if the inner type isn't ever mentioned outside one of these
            // generic builtin types.
            match type_ {
                Type::Optional(t) => self.add_known_type(t),
                Type::Sequence(t) => self.add_known_type(t),
                Type::Map(k, v) => {
                    self.add_known_type(k);
                    self.add_known_type(v);
                }
                _ => {}
            }
        }
    }

    /// Check if a [Type] is present
    pub fn contains(&self, type_: &Type) -> bool {
        self.all_known_types.contains(type_)
    }

    /// Iterator over all the known types in this universe.
    pub fn iter_known_types(&self) -> impl Iterator<Item = &Type> {
        self.all_known_types.iter()
    }
}

/// An abstract type for an iterator over &Type references.
///
/// Ideally we would not need to name this type explicitly, and could just
/// use an `impl Iterator<Item = &Type>` on any method that yields types.
pub type TypeIterator<'a> = Box<dyn Iterator<Item = &'a Type> + 'a>;

#[cfg(test)]
mod test_type_universe {
    // All the useful functionality of the `TypeUniverse` struct
    // is tested as part of the `TypeFinder` and `TypeResolver` test suites.
}
