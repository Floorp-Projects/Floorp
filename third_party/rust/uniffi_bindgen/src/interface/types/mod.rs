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
//! The set of all [`Type`]s used in a component interface is represented by a [`TypeUniverse`],
//! which can be used by the bindings generator code to determine what type-related helper
//! functions to emit for a given component.
//!
//! As a developer working on UniFFI itself, you're likely to spend a fair bit of time thinking
//! about how these API-level types map into the lower-level types of the FFI layer as represented
//! by the [`ffi::FFIType`](super::ffi::FFIType) enum, but that's a detail that is invisible to end users.

use std::{collections::hash_map::Entry, collections::BTreeSet, collections::HashMap};

use anyhow::{bail, Result};
use heck::ToUpperCamelCase;

use super::ffi::FFIType;

mod finder;
pub(super) use finder::TypeFinder;
mod resolver;
pub(super) use resolver::{resolve_builtin_type, TypeResolver};

/// Represents all the different high-level types that can be used in a component interface.
/// At this level we identify user-defined types by name, without knowing any details
/// of their internal structure apart from what type of thing they are (record, enum, etc).
#[derive(Debug, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
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
    Timestamp,
    Duration,
    // Types defined in the component API, each of which has a string name.
    Object(String),
    Record(String),
    Enum(String),
    Error(String),
    CallbackInterface(String),
    // Structurally recursive types.
    Optional(Box<Type>),
    Sequence(Box<Type>),
    Map(Box<Type>, Box<Type>),
    // An FfiConverter we `use` from an external crate
    External { name: String, crate_name: String },
    // Custom type on the scaffolding side
    Custom { name: String, builtin: Box<Type> },
}

impl Type {
    /// Get the canonical, unique-within-this-component name for a type.
    ///
    /// When generating helper code for foreign language bindings, it's sometimes useful to be
    /// able to name a particular type in order to e.g. call a helper function that is specific
    /// to that type. We support this by defining a naming convention where each type gets a
    /// unique canonical name, constructed recursively from the names of its component types (if any).
    pub fn canonical_name(&self) -> String {
        match self {
            // Builtin primitive types, with plain old names.
            Type::Int8 => "i8".into(),
            Type::UInt8 => "u8".into(),
            Type::Int16 => "i16".into(),
            Type::UInt16 => "u16".into(),
            Type::Int32 => "i32".into(),
            Type::UInt32 => "u32".into(),
            Type::Int64 => "i64".into(),
            Type::UInt64 => "u64".into(),
            Type::Float32 => "f32".into(),
            Type::Float64 => "f64".into(),
            Type::String => "string".into(),
            Type::Boolean => "bool".into(),
            // API defined types.
            // Note that these all get unique names, and the parser ensures that the names do not
            // conflict with a builtin type. We add a prefix to the name to guard against pathological
            // cases like a record named `SequenceRecord` interfering with `sequence<Record>`.
            // However, types that support importing all end up with the same prefix of "Type", so
            // that the import handling code knows how to find the remote reference.
            Type::Object(nm) => format!("Type{}", nm),
            Type::Error(nm) => format!("Type{}", nm),
            Type::Enum(nm) => format!("Type{}", nm),
            Type::Record(nm) => format!("Type{}", nm),
            Type::CallbackInterface(nm) => format!("CallbackInterface{}", nm),
            Type::Timestamp => "Timestamp".into(),
            Type::Duration => "Duration".into(),
            // Recursive types.
            // These add a prefix to the name of the underlying type.
            // The component API definition cannot give names to recursive types, so as long as the
            // prefixes we add here are all unique amongst themselves, then we have no chance of
            // acccidentally generating name collisions.
            Type::Optional(t) => format!("Optional{}", t.canonical_name()),
            Type::Sequence(t) => format!("Sequence{}", t.canonical_name()),
            Type::Map(k, v) => format!(
                "Map{}{}",
                k.canonical_name().to_upper_camel_case(),
                v.canonical_name().to_upper_camel_case()
            ),
            // A type that exists externally.
            Type::External { name, .. } | Type::Custom { name, .. } => format!("Type{}", name),
        }
    }

    pub fn ffi_type(&self) -> FFIType {
        self.into()
    }
}

/// When passing data across the FFI, each `Type` value will be lowered into a corresponding
/// `FFIType` value. This conversion tells you which one.
///
/// Note that the conversion is one-way - given an FFIType, it is not in general possible to
/// tell what the corresponding Type is that it's being used to represent.
impl From<&Type> for FFIType {
    fn from(t: &Type) -> FFIType {
        match t {
            // Types that are the same map to themselves, naturally.
            Type::UInt8 => FFIType::UInt8,
            Type::Int8 => FFIType::Int8,
            Type::UInt16 => FFIType::UInt16,
            Type::Int16 => FFIType::Int16,
            Type::UInt32 => FFIType::UInt32,
            Type::Int32 => FFIType::Int32,
            Type::UInt64 => FFIType::UInt64,
            Type::Int64 => FFIType::Int64,
            Type::Float32 => FFIType::Float32,
            Type::Float64 => FFIType::Float64,
            // Booleans lower into an Int8, to work around a bug in JNA.
            Type::Boolean => FFIType::Int8,
            // Strings are always owned rust values.
            // We might add a separate type for borrowed strings in future.
            Type::String => FFIType::RustBuffer,
            // Objects are pointers to an Arc<>
            Type::Object(_) => FFIType::RustArcPtr,
            // Callback interfaces are passed as opaque integer handles.
            Type::CallbackInterface(_) => FFIType::UInt64,
            // Other types are serialized into a bytebuffer and deserialized on the other side.
            Type::Enum(_)
            | Type::Error(_)
            | Type::Record(_)
            | Type::Optional(_)
            | Type::Sequence(_)
            | Type::Map(_, _)
            | Type::Timestamp
            | Type::Duration
            | Type::External { .. } => FFIType::RustBuffer,
            Type::Custom { builtin, .. } => FFIType::from(builtin.as_ref()),
        }
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
            bail!(
                "please don't shadow builtin types ({}, {})",
                name,
                type_.canonical_name()
            );
        }
        let type_ = self.add_known_type(type_)?;
        match self.type_definitions.entry(name.to_string()) {
            Entry::Occupied(_) => bail!("Conflicting type definition for \"{}\"", name),
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
    ///
    /// This helpfully returns a `Result<Type>` so it can be chained in with other
    /// methods during the type resolution process.
    pub fn add_known_type(&mut self, type_: Type) -> Result<Type> {
        // Types are more likely to already be known than not, so avoid unnecessary cloning.
        if !self.all_known_types.contains(&type_) {
            self.all_known_types.insert(type_.clone());
        }
        Ok(type_)
    }

    /// Iterator over all the known types in this universe.
    pub fn iter_known_types(&self) -> impl Iterator<Item = Type> + '_ {
        self.all_known_types.iter().cloned()
    }
}

/// An abstract type for an iterator over &Type references.
///
/// Ideally we would not need to name this type explicitly, and could just
/// use an `impl Iterator<Item=&Type>` on any method that yields types.
/// Unfortunately existential types are not currently supported in trait method
/// signatures, so for now we hide the concrete type behind a box.
pub type TypeIterator<'a> = Box<dyn Iterator<Item = &'a Type> + 'a>;

/// A trait for objects that may contain references to types.
///
/// Various objects in our interface will contain (possibly nested) references to types -
/// for example a `Record` struct will contain one or more `Field` structs which will each
/// have an associated type. This trait provides a uniform interface for inspecting the
/// types references by an object.

pub trait IterTypes {
    /// Iterate over all types contained within on object.
    ///
    /// This method iterates over the types contained with in object, making
    /// no particular guarantees about ordering or handling of duplicates.
    ///
    /// The return type is a Box in order to hide the concrete implementation
    /// details of the iterator. Ideally we would return `impl Iterator` here
    /// but that's not currently supported for trait methods.
    fn iter_types(&self) -> TypeIterator<'_>;
}

impl<T: IterTypes> IterTypes for &T {
    fn iter_types(&self) -> TypeIterator<'_> {
        (*self).iter_types()
    }
}

impl<T: IterTypes> IterTypes for Box<T> {
    fn iter_types(&self) -> TypeIterator<'_> {
        self.as_ref().iter_types()
    }
}

impl<T: IterTypes> IterTypes for Option<T> {
    fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.iter().flat_map(IterTypes::iter_types))
    }
}

impl IterTypes for Type {
    fn iter_types(&self) -> TypeIterator<'_> {
        let nested_types = match self {
            Type::Optional(t) | Type::Sequence(t) => Some(t.iter_types()),
            Type::Map(k, v) => Some(Box::new(k.iter_types().chain(v.iter_types())) as _),
            _ => None,
        };
        Box::new(std::iter::once(self).chain(nested_types.into_iter().flatten()))
    }
}

impl IterTypes for TypeUniverse {
    fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.all_known_types.iter())
    }
}

#[cfg(test)]
mod test_type {
    use super::*;

    #[test]
    fn test_canonical_names() {
        // Non-exhaustive, but gives a bit of a flavour of what we want.
        assert_eq!(Type::UInt8.canonical_name(), "u8");
        assert_eq!(Type::String.canonical_name(), "string");
        assert_eq!(
            Type::Optional(Box::new(Type::Sequence(Box::new(Type::Object(
                "Example".into()
            )))))
            .canonical_name(),
            "OptionalSequenceTypeExample"
        );
    }
}

#[cfg(test)]
mod test_type_universe {
    // All the useful functionality of the `TypeUniverse` struct
    // is tested as part of the `TypeFinder` and `TypeResolver` test suites.
}
