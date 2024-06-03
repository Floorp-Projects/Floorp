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
//! the higher-level "interface types" from the [`Type`] enum.
/// Represents the restricted set of low-level types that can be used to construct
/// the C-style FFI layer between a rust component and its foreign language bindings.
///
/// For the types that involve memory allocation, we make a distinction between
/// "owned" types (the recipient must free it, or pass it to someone else) and
/// "borrowed" types (the sender must keep it alive for the duration of the call).
use uniffi_meta::{ExternalKind, Type};

#[derive(Debug, Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
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
    /// Pointer to a callback function.  The inner value which matches one of the callback
    /// definitions in [crate::ComponentInterface::ffi_definitions].
    Callback(String),
    /// Pointer to a FFI struct (e.g. a VTable).  The inner value matches one of the struct
    /// definitions in [crate::ComponentInterface::ffi_definitions].
    Struct(String),
    /// Opaque 64-bit handle
    ///
    /// These are used to pass objects across the FFI.
    Handle,
    RustCallStatus,
    /// Pointer to an FfiType.
    Reference(Box<FfiType>),
    /// Opaque pointer
    VoidPointer,
}

impl FfiType {
    pub fn reference(self) -> FfiType {
        FfiType::Reference(Box::new(self))
    }

    /// Unique name for an FFI return type
    pub fn return_type_name(return_type: Option<&FfiType>) -> String {
        match return_type {
            Some(t) => match t {
                FfiType::UInt8 => "u8".to_owned(),
                FfiType::Int8 => "i8".to_owned(),
                FfiType::UInt16 => "u16".to_owned(),
                FfiType::Int16 => "i16".to_owned(),
                FfiType::UInt32 => "u32".to_owned(),
                FfiType::Int32 => "i32".to_owned(),
                FfiType::UInt64 => "u64".to_owned(),
                FfiType::Int64 => "i64".to_owned(),
                FfiType::Float32 => "f32".to_owned(),
                FfiType::Float64 => "f64".to_owned(),
                FfiType::RustArcPtr(_) => "pointer".to_owned(),
                FfiType::RustBuffer(_) => "rust_buffer".to_owned(),
                _ => unimplemented!("FFI return type: {t:?}"),
            },
            None => "void".to_owned(),
        }
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
            Type::CallbackInterface { .. } => FfiType::UInt64,
            // Other types are serialized into a bytebuffer and deserialized on the other side.
            Type::Enum { .. }
            | Type::Record { .. }
            | Type::Optional { .. }
            | Type::Sequence { .. }
            | Type::Map { .. }
            | Type::Timestamp
            | Type::Duration => FfiType::RustBuffer(None),
            Type::External {
                name,
                kind: ExternalKind::Interface,
                ..
            }
            | Type::External {
                name,
                kind: ExternalKind::Trait,
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
impl From<Type> for FfiType {
    fn from(ty: Type) -> Self {
        (&ty).into()
    }
}

impl From<&&Type> for FfiType {
    fn from(ty: &&Type) -> Self {
        (*ty).into()
    }
}

/// An Ffi definition
#[derive(Debug, Clone)]
pub enum FfiDefinition {
    Function(FfiFunction),
    CallbackFunction(FfiCallbackFunction),
    Struct(FfiStruct),
}

impl FfiDefinition {
    pub fn name(&self) -> &str {
        match self {
            Self::Function(f) => f.name(),
            Self::CallbackFunction(f) => f.name(),
            Self::Struct(s) => s.name(),
        }
    }
}

/// Represents an "extern C"-style function that will be part of the FFI.
///
/// These can't be declared explicitly in the UDL, but rather, are derived automatically
/// from the high-level interface. Each callable thing in the component API will have a
/// corresponding `FfiFunction` through which it can be invoked, and UniFFI also provides
/// some built-in `FfiFunction` helpers for use in the foreign language bindings.
#[derive(Debug, Clone)]
pub struct FfiFunction {
    pub(super) name: String,
    pub(super) is_async: bool,
    pub(super) arguments: Vec<FfiArgument>,
    pub(super) return_type: Option<FfiType>,
    pub(super) has_rust_call_status_arg: bool,
    /// Used by C# generator to differentiate the free function and call it with void*
    /// instead of C# `SafeHandle` type. See <https://github.com/mozilla/uniffi-rs/pull/1488>.
    pub(super) is_object_free_function: bool,
}

impl FfiFunction {
    pub fn callback_init(module_path: &str, trait_name: &str, vtable_name: String) -> Self {
        Self {
            name: uniffi_meta::init_callback_vtable_fn_symbol_name(module_path, trait_name),
            arguments: vec![FfiArgument {
                name: "vtable".to_string(),
                type_: FfiType::Struct(vtable_name).reference(),
            }],
            return_type: None,
            has_rust_call_status_arg: false,
            ..Self::default()
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    /// Name of the FFI buffer version of this function that's generated when the
    /// `scaffolding-ffi-buffer-fns` feature is enabled.
    pub fn ffi_buffer_fn_name(&self) -> String {
        uniffi_meta::ffi_buffer_symbol_name(&self.name)
    }

    pub fn is_async(&self) -> bool {
        self.is_async
    }

    pub fn arguments(&self) -> Vec<&FfiArgument> {
        self.arguments.iter().collect()
    }

    pub fn return_type(&self) -> Option<&FfiType> {
        self.return_type.as_ref()
    }

    pub fn has_rust_call_status_arg(&self) -> bool {
        self.has_rust_call_status_arg
    }

    pub fn is_object_free_function(&self) -> bool {
        self.is_object_free_function
    }

    pub fn init(
        &mut self,
        return_type: Option<FfiType>,
        args: impl IntoIterator<Item = FfiArgument>,
    ) {
        self.arguments = args.into_iter().collect();
        if self.is_async() {
            self.return_type = Some(FfiType::Handle);
            self.has_rust_call_status_arg = false;
        } else {
            self.return_type = return_type;
        }
    }
}

impl Default for FfiFunction {
    fn default() -> Self {
        Self {
            name: "".into(),
            is_async: false,
            arguments: Vec::new(),
            return_type: None,
            has_rust_call_status_arg: true,
            is_object_free_function: false,
        }
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
    pub fn new(name: impl Into<String>, type_: FfiType) -> Self {
        Self {
            name: name.into(),
            type_,
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn type_(&self) -> FfiType {
        self.type_.clone()
    }
}

/// Represents an "extern C"-style callback function
///
/// These are defined in the foreign code and passed to Rust as a function pointer.
#[derive(Debug, Default, Clone)]
pub struct FfiCallbackFunction {
    // Name for this function type. This matches the value inside `FfiType::Callback`
    pub(super) name: String,
    pub(super) arguments: Vec<FfiArgument>,
    pub(super) return_type: Option<FfiType>,
    pub(super) has_rust_call_status_arg: bool,
}

impl FfiCallbackFunction {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn arguments(&self) -> Vec<&FfiArgument> {
        self.arguments.iter().collect()
    }

    pub fn return_type(&self) -> Option<&FfiType> {
        self.return_type.as_ref()
    }

    pub fn has_rust_call_status_arg(&self) -> bool {
        self.has_rust_call_status_arg
    }
}

/// Represents a repr(C) struct used in the FFI
#[derive(Debug, Default, Clone)]
pub struct FfiStruct {
    pub(super) name: String,
    pub(super) fields: Vec<FfiField>,
}

impl FfiStruct {
    /// Get the name of this struct
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get the fields for this struct
    pub fn fields(&self) -> &[FfiField] {
        &self.fields
    }
}

/// Represents a field of an [FfiStruct]
#[derive(Debug, Clone)]
pub struct FfiField {
    pub(super) name: String,
    pub(super) type_: FfiType,
}

impl FfiField {
    pub fn new(name: impl Into<String>, type_: FfiType) -> Self {
        Self {
            name: name.into(),
            type_,
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn type_(&self) -> FfiType {
        self.type_.clone()
    }
}

impl From<FfiFunction> for FfiDefinition {
    fn from(value: FfiFunction) -> FfiDefinition {
        FfiDefinition::Function(value)
    }
}

impl From<FfiStruct> for FfiDefinition {
    fn from(value: FfiStruct) -> FfiDefinition {
        FfiDefinition::Struct(value)
    }
}

impl From<FfiCallbackFunction> for FfiDefinition {
    fn from(value: FfiCallbackFunction) -> FfiDefinition {
        FfiDefinition::CallbackFunction(value)
    }
}

#[cfg(test)]
mod test {
    // There's not really much to test here to be honest,
    // it's mostly type declarations.
}
