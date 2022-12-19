/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::hash::Hasher;
pub use uniffi_checksum_derive::Checksum;

use serde::{Deserialize, Serialize};

/// Similar to std::hash::Hash.
///
/// Implementations of this trait are expected to update the hasher state in
/// the same way across platforms. #[derive(Checksum)] will do the right thing.
pub trait Checksum {
    fn checksum<H: Hasher>(&self, state: &mut H);
}

impl Checksum for bool {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        state.write_u8(*self as u8);
    }
}

impl Checksum for u64 {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        state.write(&self.to_le_bytes());
    }
}

impl Checksum for i64 {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        state.write(&self.to_le_bytes());
    }
}

impl<T: Checksum> Checksum for Box<T> {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        (**self).checksum(state)
    }
}

impl<T: Checksum> Checksum for [T] {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        state.write(&(self.len() as u64).to_le_bytes());
        for item in self {
            Checksum::checksum(item, state);
        }
    }
}

impl<T: Checksum> Checksum for Vec<T> {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        Checksum::checksum(&**self, state);
    }
}

impl<T: Checksum> Checksum for Option<T> {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        match self {
            None => state.write(&0u64.to_le_bytes()),
            Some(value) => {
                state.write(&1u64.to_le_bytes());
                Checksum::checksum(value, state)
            }
        }
    }
}

impl Checksum for str {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        state.write(self.as_bytes());
        state.write_u8(0xff);
    }
}

impl Checksum for String {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        (**self).checksum(state)
    }
}

#[derive(Clone, Debug, Checksum, Deserialize, Serialize)]
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

#[derive(Clone, Debug, Checksum, Deserialize, Serialize)]
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

#[derive(Clone, Debug, Checksum, Deserialize, Serialize)]
pub struct FnParamMetadata {
    pub name: String,
    #[serde(rename = "type")]
    pub ty: Type,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Checksum, Deserialize, Serialize)]
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

#[derive(Clone, Debug, Checksum, Deserialize, Serialize)]
pub struct RecordMetadata {
    pub module_path: Vec<String>,
    pub name: String,
    pub fields: Vec<FieldMetadata>,
}

#[derive(Clone, Debug, Checksum, Deserialize, Serialize)]
pub struct FieldMetadata {
    pub name: String,
    #[serde(rename = "type")]
    pub ty: Type,
}

#[derive(Clone, Debug, Checksum, Deserialize, Serialize)]
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

/// Returns the last 16 bits of the value's hash as computed with [`SipHasher13`].
///
/// To be used as a checksum of FFI symbols, as a safeguard against different UniFFI versions being
/// used for scaffolding and bindings generation.
pub fn checksum<T: Checksum>(val: &T) -> u16 {
    let mut hasher = siphasher::sip::SipHasher13::new();
    val.checksum(&mut hasher);
    (hasher.finish() & 0x000000000000FFFF) as u16
}

pub fn fn_ffi_symbol_name(mod_path: &[String], name: &str, checksum: u16) -> String {
    let mod_path = mod_path.join("__");
    format!("_uniffi_{mod_path}_{name}_{checksum:x}")
}

/// Enum covering all the possible metadata types
#[derive(Clone, Debug, Checksum, Deserialize, Serialize)]
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
