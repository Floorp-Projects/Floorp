/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Low-level typesystem for the FFI layer of a component interface.
//!
//! This module provides the "FFI-level" typesystem of a UniFFI Rust Component, that is,
//! the C-style functions and structs and primitive datatypes that are used to interface
//! between the Rust component code and the foreign-language bindings.
//!
//! These types are purely an implementation detail of UniFFI, so consumers shouldn't
//! need to know about them. But as a developer working on UniFFI itself, you're likely
//! to spend a lot of time thinking about how these low-level types are used to represent
//! the higher-level "interface types" from the [`super::types::Type`] enum.
/// Represents the restricted set of low-level types that can be used to construct
/// the C-style FFI layer between a rust component and its foreign language bindings.
///
/// For the types that involve memory allocation, we make a distinction between
/// "owned" types (the recipient must free it, or pass it to someone else) and
/// "borrowed" types (the sender must keep it alive for the duration of the call).
#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub enum FfiType {
    // N.B. there are no booleans at this layer, since they cause problems for JNA.
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
    /// A `*const c_void` pointer to a rust-owned `Arc<T>`.
    /// If you've got one of these, you must call the appropriate rust function to free it.
    /// The templates will generate a unique `free` function for each T.
    /// The inner string references the name of the `T` type.
    RustArcPtr(String),
    /// A byte buffer allocated by rust, and owned by whoever currently holds it.
    /// If you've got one of these, you must either call the appropriate rust function to free it
    /// or pass it to someone that will.
    /// If the inner option is Some, it is the name of the external type. The bindings may need
    /// to use this name to import the correct RustBuffer for that type.
    RustBuffer(Option<String>),
    /// A borrowed reference to some raw bytes owned by foreign language code.
    /// The provider of this reference must keep it alive for the duration of the receiving call.
    ForeignBytes,
    /// A pointer to a single function in to the foreign language.
    /// This function contains all the machinery to make callbacks work on the foreign language side.
    ForeignCallback,
    // TODO: you can imagine a richer structural typesystem here, e.g. `Ref<String>` or something.
    // We don't need that yet and it's possible we never will, so it isn't here for now.
}

/// Represents an "extern C"-style function that will be part of the FFI.
///
/// These can't be declared explicitly in the UDL, but rather, are derived automatically
/// from the high-level interface. Each callable thing in the component API will have a
/// corresponding `FfiFunction` through which it can be invoked, and UniFFI also provides
/// some built-in `FfiFunction` helpers for use in the foreign language bindings.
#[derive(Debug, Default, Clone)]
pub struct FfiFunction {
    pub(super) name: String,
    pub(super) arguments: Vec<FfiArgument>,
    pub(super) return_type: Option<FfiType>,
}

impl FfiFunction {
    pub fn name(&self) -> &str {
        &self.name
    }
    pub fn arguments(&self) -> Vec<&FfiArgument> {
        self.arguments.iter().collect()
    }
    pub fn return_type(&self) -> Option<&FfiType> {
        self.return_type.as_ref()
    }
}

/// Represents an argument to an FFI function.
///
/// Each argument has a name and a type.
#[derive(Debug, Clone)]
pub struct FfiArgument {
    pub(super) name: String,
    pub(super) type_: FfiType,
}

impl FfiArgument {
    pub fn name(&self) -> &str {
        &self.name
    }
    pub fn type_(&self) -> FfiType {
        self.type_.clone()
    }
}

#[cfg(test)]
mod test {
    // There's not really much to test here to be honest,
    // it's mostly type declarations.
}
