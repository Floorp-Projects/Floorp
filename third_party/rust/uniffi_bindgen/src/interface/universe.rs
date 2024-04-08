/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The set of all [`Type`]s used in a component interface is represented by a `TypeUniverse`,
//! which can be used by the bindings generator code to determine what type-related helper
//! functions to emit for a given component.
//!
use anyhow::Result;
use std::{collections::hash_map::Entry, collections::BTreeSet, collections::HashMap};

pub use uniffi_meta::{AsType, ExternalKind, NamespaceMetadata, ObjectImpl, Type, TypeIterator};

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
    /// The unique prefixes that we'll use for namespacing when exposing this component's API.
    pub namespace: NamespaceMetadata,

    // Named type definitions (including aliases).
    type_definitions: HashMap<String, Type>,
    // All the types in the universe, by canonical type name, in a well-defined order.
    all_known_types: BTreeSet<Type>,
}

impl TypeUniverse {
    pub fn new(namespace: NamespaceMetadata) -> Self {
        Self {
            namespace,
            ..Default::default()
        }
    }

    /// Add the definition of a named [Type].
    fn add_type_definition(&mut self, name: &str, type_: &Type) -> Result<()> {
        match self.type_definitions.entry(name.to_string()) {
            Entry::Occupied(o) => {
                // all conflicts have been resolved by now in udl.
                // I doubt procmacros could cause this?
                assert_eq!(type_, o.get());
                Ok(())
            }
            Entry::Vacant(e) => {
                e.insert(type_.clone());
                Ok(())
            }
        }
    }

    /// Get the [Type] corresponding to a given name, if any.
    pub(super) fn get_type_definition(&self, name: &str) -> Option<Type> {
        self.type_definitions.get(name).cloned()
    }

    /// Add a [Type] to the set of all types seen in the component interface.
    pub fn add_known_type(&mut self, type_: &Type) -> Result<()> {
        // Types are more likely to already be known than not, so avoid unnecessary cloning.
        if !self.all_known_types.contains(type_) {
            self.all_known_types.insert(type_.to_owned());
        }
        match type_ {
            Type::UInt8 => self.add_type_definition("u8", type_)?,
            Type::Int8 => self.add_type_definition("i8", type_)?,
            Type::UInt16 => self.add_type_definition("u16", type_)?,
            Type::Int16 => self.add_type_definition("i16", type_)?,
            Type::UInt32 => self.add_type_definition("i32", type_)?,
            Type::Int32 => self.add_type_definition("u32", type_)?,
            Type::UInt64 => self.add_type_definition("u64", type_)?,
            Type::Int64 => self.add_type_definition("i64", type_)?,
            Type::Float32 => self.add_type_definition("f32", type_)?,
            Type::Float64 => self.add_type_definition("f64", type_)?,
            Type::Boolean => self.add_type_definition("bool", type_)?,
            Type::String => self.add_type_definition("string", type_)?,
            Type::Bytes => self.add_type_definition("bytes", type_)?,
            Type::Timestamp => self.add_type_definition("timestamp", type_)?,
            Type::Duration => self.add_type_definition("duration", type_)?,
            Type::ForeignExecutor => {
                self.add_type_definition("ForeignExecutor", type_)?;
            }
            Type::Object { name, .. }
            | Type::Record { name, .. }
            | Type::Enum { name, .. }
            | Type::CallbackInterface { name, .. }
            | Type::External { name, .. } => self.add_type_definition(name, type_)?,
            Type::Custom { name, builtin, .. } => {
                self.add_type_definition(name, type_)?;
                self.add_known_type(builtin)?;
            }
            // Structurally recursive types.
            Type::Optional { inner_type, .. } | Type::Sequence { inner_type, .. } => {
                self.add_known_type(inner_type)?;
            }
            Type::Map {
                key_type,
                value_type,
            } => {
                self.add_known_type(key_type)?;
                self.add_known_type(value_type)?;
            }
        }
        Ok(())
    }

    /// Add many [`Type`]s...
    pub fn add_known_types(&mut self, types: TypeIterator<'_>) -> Result<()> {
        for t in types {
            self.add_known_type(t)?
        }
        Ok(())
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

#[cfg(test)]
mod test_type_universe {
    // All the useful functionality of the `TypeUniverse` struct
    // is tested as part of the `TypeFinder` and `TypeResolver` test suites.
}
