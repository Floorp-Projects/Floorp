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
//! As a developer working on UniFFI itself, you're likely to spend a fair bit of time thinking
//! about how these API-level types map into the lower-level types of the FFI layer as represented
//! by the [`ffi::FfiType`](super::ffi::FfiType) enum, but that's a detail that is invisible to end users.

use crate::Checksum;

#[derive(Debug, Copy, Clone, Eq, PartialEq, Checksum, Ord, PartialOrd)]
pub enum ObjectImpl {
    // A single Rust type
    Struct,
    // A trait that's can be implemented by Rust types
    Trait,
    // A trait + a callback interface -- can be implemented by both Rust and foreign types.
    CallbackTrait,
}

impl ObjectImpl {
    /// Return the fully qualified name which should be used by Rust code for
    /// an object with the given name.
    /// Includes `r#`, traits get a leading `dyn`. If we ever supported associated types, then
    /// this would also include them.
    pub fn rust_name_for(&self, name: &str) -> String {
        if self.is_trait_interface() {
            format!("dyn r#{name}")
        } else {
            format!("r#{name}")
        }
    }

    pub fn is_trait_interface(&self) -> bool {
        matches!(self, Self::Trait | Self::CallbackTrait)
    }

    pub fn has_callback_interface(&self) -> bool {
        matches!(self, Self::CallbackTrait)
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Checksum, Ord, PartialOrd)]
pub enum ExternalKind {
    Interface,
    Trait,
    // Either a record or enum
    DataClass,
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
        // The module path to the object
        module_path: String,
        // The name in the "type universe"
        name: String,
        // How the object is implemented.
        imp: ObjectImpl,
    },
    // Types defined in the component API, each of which has a string name.
    Record {
        module_path: String,
        name: String,
    },
    Enum {
        module_path: String,
        name: String,
    },
    CallbackInterface {
        module_path: String,
        name: String,
    },
    // Structurally recursive types.
    Optional {
        inner_type: Box<Type>,
    },
    Sequence {
        inner_type: Box<Type>,
    },
    Map {
        key_type: Box<Type>,
        value_type: Box<Type>,
    },
    // An FfiConverter we `use` from an external crate
    External {
        module_path: String,
        name: String,
        #[checksum_ignore] // The namespace is not known generating scaffolding.
        namespace: String,
        kind: ExternalKind,
        tagged: bool, // does its FfiConverter use <UniFFITag>?
    },
    // Custom type on the scaffolding side
    Custom {
        module_path: String,
        name: String,
        builtin: Box<Type>,
    },
}

impl Type {
    pub fn iter_types(&self) -> TypeIterator<'_> {
        let nested_types = match self {
            Type::Optional { inner_type } | Type::Sequence { inner_type } => {
                inner_type.iter_types()
            }
            Type::Map {
                key_type,
                value_type,
            } => Box::new(key_type.iter_types().chain(value_type.iter_types())),
            _ => Box::new(std::iter::empty()),
        };
        Box::new(std::iter::once(self).chain(nested_types))
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

/// An abstract type for an iterator over &Type references.
///
/// Ideally we would not need to name this type explicitly, and could just
/// use an `impl Iterator<Item = &Type>` on any method that yields types.
pub type TypeIterator<'a> = Box<dyn Iterator<Item = &'a Type> + 'a>;
