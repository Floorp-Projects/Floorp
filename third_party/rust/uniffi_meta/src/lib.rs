/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{collections::BTreeMap, hash::Hasher};
pub use uniffi_checksum_derive::Checksum;

use serde::Serialize;

mod ffi_names;
pub use ffi_names::*;

mod group;
pub use group::{group_metadata, MetadataGroup};

mod reader;
pub use reader::{read_metadata, read_metadata_type};

mod metadata;

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

impl<K: Checksum, V: Checksum> Checksum for BTreeMap<K, V> {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        state.write(&(self.len() as u64).to_le_bytes());
        for (key, value) in self {
            Checksum::checksum(key, state);
            Checksum::checksum(value, state);
        }
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

impl Checksum for &str {
    fn checksum<H: Hasher>(&self, state: &mut H) {
        (**self).checksum(state)
    }
}

// The namespace of a Component interface.
//
// This is used to match up the macro metadata with the UDL items.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct NamespaceMetadata {
    pub crate_name: String,
    pub name: String,
}

// UDL file included with `include_scaffolding!()`
//
// This is to find the UDL files in library mode generation
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct UdlFile {
    pub module_path: String,
    pub name: String,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct FnMetadata {
    pub module_path: String,
    pub name: String,
    pub is_async: bool,
    pub inputs: Vec<FnParamMetadata>,
    pub return_type: Option<Type>,
    pub throws: Option<Type>,
    pub checksum: u16,
}

impl FnMetadata {
    pub fn ffi_symbol_name(&self) -> String {
        fn_symbol_name(&self.module_path, &self.name)
    }

    pub fn checksum_symbol_name(&self) -> String {
        fn_checksum_symbol_name(&self.module_path, &self.name)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct ConstructorMetadata {
    pub module_path: String,
    pub self_name: String,
    pub name: String,
    pub inputs: Vec<FnParamMetadata>,
    pub throws: Option<Type>,
    pub checksum: u16,
}

impl ConstructorMetadata {
    pub fn ffi_symbol_name(&self) -> String {
        constructor_symbol_name(&self.module_path, &self.self_name, &self.name)
    }

    pub fn checksum_symbol_name(&self) -> String {
        constructor_checksum_symbol_name(&self.module_path, &self.self_name, &self.name)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct MethodMetadata {
    pub module_path: String,
    pub self_name: String,
    pub name: String,
    pub is_async: bool,
    pub inputs: Vec<FnParamMetadata>,
    pub return_type: Option<Type>,
    pub throws: Option<Type>,
    pub checksum: u16,
}

impl MethodMetadata {
    pub fn ffi_symbol_name(&self) -> String {
        method_symbol_name(&self.module_path, &self.self_name, &self.name)
    }

    pub fn checksum_symbol_name(&self) -> String {
        method_checksum_symbol_name(&self.module_path, &self.self_name, &self.name)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct TraitMethodMetadata {
    pub module_path: String,
    pub trait_name: String,
    // Note: the position of `index` is important since it causes callback interface methods to be
    // ordered correctly in MetadataGroup.items
    pub index: u32,
    pub name: String,
    pub is_async: bool,
    pub inputs: Vec<FnParamMetadata>,
    pub return_type: Option<Type>,
    pub throws: Option<Type>,
    pub checksum: u16,
}

impl TraitMethodMetadata {
    pub fn ffi_symbol_name(&self) -> String {
        method_symbol_name(&self.module_path, &self.trait_name, &self.name)
    }

    pub fn checksum_symbol_name(&self) -> String {
        method_checksum_symbol_name(&self.module_path, &self.trait_name, &self.name)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct FnParamMetadata {
    pub name: String,
    #[serde(rename = "type")]
    pub ty: Type,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
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
    Duration,
    ForeignExecutor,
    SystemTime,
    Enum {
        name: String,
    },
    Record {
        name: String,
    },
    ArcObject {
        object_name: String,
        is_trait: bool,
    },
    CallbackInterface {
        name: String,
    },
    Custom {
        name: String,
        builtin: Box<Type>,
    },
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
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub enum Literal {
    Str { value: String },
    Int { base10_digits: String },
    Float { base10_digits: String },
    Bool { value: bool },
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct RecordMetadata {
    pub module_path: String,
    pub name: String,
    pub fields: Vec<FieldMetadata>,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct FieldMetadata {
    pub name: String,
    #[serde(rename = "type")]
    pub ty: Type,
    pub default: Option<Literal>,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct EnumMetadata {
    pub module_path: String,
    pub name: String,
    pub variants: Vec<VariantMetadata>,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct VariantMetadata {
    pub name: String,
    pub fields: Vec<FieldMetadata>,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct ObjectMetadata {
    pub module_path: String,
    pub name: String,
    pub is_trait: bool,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub struct CallbackInterfaceMetadata {
    pub module_path: String,
    pub name: String,
}

impl ObjectMetadata {
    /// FFI symbol name for the `free` function for this object.
    ///
    /// This function is used to free the memory used by this object.
    pub fn free_ffi_symbol_name(&self) -> String {
        free_fn_symbol_name(&self.module_path, &self.name)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub enum ErrorMetadata {
    Enum { enum_: EnumMetadata, is_flat: bool },
}

impl ErrorMetadata {
    pub fn name(&self) -> &String {
        match self {
            Self::Enum { enum_, .. } => &enum_.name,
        }
    }

    pub fn module_path(&self) -> &String {
        match self {
            Self::Enum { enum_, .. } => &enum_.module_path,
        }
    }
}

/// Returns the last 16 bits of the value's hash as computed with [`SipHasher13`].
///
/// This is used as a safeguard against different UniFFI versions being used for scaffolding and
/// bindings generation.
pub fn checksum<T: Checksum>(val: &T) -> u16 {
    let mut hasher = siphasher::sip::SipHasher13::new();
    val.checksum(&mut hasher);
    (hasher.finish() & 0x000000000000FFFF) as u16
}

/// Enum covering all the possible metadata types
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize)]
pub enum Metadata {
    Namespace(NamespaceMetadata),
    UdlFile(UdlFile),
    Func(FnMetadata),
    Object(ObjectMetadata),
    CallbackInterface(CallbackInterfaceMetadata),
    Record(RecordMetadata),
    Enum(EnumMetadata),
    Error(ErrorMetadata),
    Constructor(ConstructorMetadata),
    Method(MethodMetadata),
    TraitMethod(TraitMethodMetadata),
}

impl Metadata {
    pub fn read(data: &[u8]) -> anyhow::Result<Self> {
        read_metadata(data)
    }
}

impl From<NamespaceMetadata> for Metadata {
    fn from(value: NamespaceMetadata) -> Metadata {
        Self::Namespace(value)
    }
}

impl From<UdlFile> for Metadata {
    fn from(value: UdlFile) -> Metadata {
        Self::UdlFile(value)
    }
}

impl From<FnMetadata> for Metadata {
    fn from(value: FnMetadata) -> Metadata {
        Self::Func(value)
    }
}

impl From<ConstructorMetadata> for Metadata {
    fn from(c: ConstructorMetadata) -> Self {
        Self::Constructor(c)
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

impl From<EnumMetadata> for Metadata {
    fn from(e: EnumMetadata) -> Self {
        Self::Enum(e)
    }
}

impl From<ErrorMetadata> for Metadata {
    fn from(e: ErrorMetadata) -> Self {
        Self::Error(e)
    }
}

impl From<ObjectMetadata> for Metadata {
    fn from(v: ObjectMetadata) -> Self {
        Self::Object(v)
    }
}

impl From<CallbackInterfaceMetadata> for Metadata {
    fn from(v: CallbackInterfaceMetadata) -> Self {
        Self::CallbackInterface(v)
    }
}

impl From<TraitMethodMetadata> for Metadata {
    fn from(v: TraitMethodMetadata) -> Self {
        Self::TraitMethod(v)
    }
}
