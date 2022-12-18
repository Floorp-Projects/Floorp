/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    collections::hash_map::DefaultHasher,
    hash::{Hash, Hasher},
};

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Hash, Deserialize, Serialize)]
pub struct FnMetadata {
    pub module_path: Vec<String>,
    pub name: String,
    pub inputs: Vec<FnParamMetadata>,
    pub return_type: Option<Type>,
}

impl FnMetadata {
    pub fn ffi_symbol_name(&self) -> String {
        fn_ffi_symbol_name(&self.module_path, &self.name, checksum(self))
    }
}

#[derive(Clone, Debug, Hash, Deserialize, Serialize)]
pub struct MethodMetadata {
    pub module_path: Vec<String>,
    pub self_name: String,
    pub name: String,
    pub inputs: Vec<FnParamMetadata>,
    pub return_type: Option<Type>,
}

impl MethodMetadata {
    pub fn ffi_symbol_name(&self) -> String {
        let full_name = format!("impl_{}_{}", self.self_name, self.name);
        fn_ffi_symbol_name(&self.module_path, &full_name, checksum(self))
    }
}

#[derive(Clone, Debug, Hash, Deserialize, Serialize)]
pub struct FnParamMetadata {
    pub name: String,
    #[serde(rename = "type")]
    pub ty: Type,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Deserialize, Serialize)]
pub enum Type {
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    F32,
    F64,
    Bool,
    String,
    Option {
        inner_type: Box<Type>,
    },
    Vec {
        inner_type: Box<Type>,
    },
    HashMap {
        key_type: Box<Type>,
        value_type: Box<Type>,
    },
    ArcObject {
        object_name: String,
    },
    Unresolved {
        name: String,
    },
}

#[derive(Clone, Debug, Hash, Deserialize, Serialize)]
pub struct RecordMetadata {
    pub module_path: Vec<String>,
    pub name: String,
    pub fields: Vec<FieldMetadata>,
}

#[derive(Clone, Debug, Hash, Deserialize, Serialize)]
pub struct FieldMetadata {
    pub name: String,
    #[serde(rename = "type")]
    pub ty: Type,
}

#[derive(Clone, Debug, Hash, Deserialize, Serialize)]
pub struct ObjectMetadata {
    pub module_path: Vec<String>,
    pub name: String,
}

impl ObjectMetadata {
    /// FFI symbol name for the `free` function for this object.
    ///
    /// This function is used to free the memory used by this object.
    pub fn free_ffi_symbol_name(&self) -> String {
        let free_name = format!("object_free_{}", self.name);
        fn_ffi_symbol_name(&self.module_path, &free_name, checksum(self))
    }
}

/// Returns the last 16 bits of the value's hash as computed with [`DefaultHasher`].
///
/// To be used as a checksum of FFI symbols, as a safeguard against different UniFFI versions being
/// used for scaffolding and bindings generation.
pub fn checksum<T: Hash>(val: &T) -> u16 {
    let mut hasher = DefaultHasher::new();
    val.hash(&mut hasher);
    (hasher.finish() & 0x000000000000FFFF) as u16
}

pub fn fn_ffi_symbol_name(mod_path: &[String], name: &str, checksum: u16) -> String {
    let mod_path = mod_path.join("__");
    format!("_uniffi_{mod_path}_{name}_{checksum:x}")
}

/// Enum covering all the possible metadata types
#[derive(Clone, Debug, Hash, Deserialize, Serialize)]
pub enum Metadata {
    Func(FnMetadata),
    Method(MethodMetadata),
    Record(RecordMetadata),
    Object(ObjectMetadata),
}

impl From<FnMetadata> for Metadata {
    fn from(value: FnMetadata) -> Metadata {
        Self::Func(value)
    }
}

impl From<MethodMetadata> for Metadata {
    fn from(m: MethodMetadata) -> Self {
        Self::Method(m)
    }
}

impl From<RecordMetadata> for Metadata {
    fn from(r: RecordMetadata) -> Self {
        Self::Record(r)
    }
}

impl From<ObjectMetadata> for Metadata {
    fn from(v: ObjectMetadata) -> Self {
        Self::Object(v)
    }
}
