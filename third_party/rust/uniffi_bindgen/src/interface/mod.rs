/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Component Interface Definition.
//!
//! This module provides an abstract representation of the interface provided by a UniFFI Rust Component,
//! in high-level terms suitable for translation into target consumer languages such as Kotlin
//! and Swift. It also provides facilities for parsing a WebIDL interface definition file into such a
//! representation.
//!
//! The entrypoint to this crate is the `ComponentInterface` struct, which holds a complete definition
//! of the interface provided by a component, in two parts:
//!
//!    * The high-level consumer API, in terms of objects and records and methods and so-on
//!    * The low-level FFI contract through which the foreign language code can call into Rust.
//!
//! That's really the key concept of this crate so it's worth repeating: a `ComponentInterface` completely
//! defines the shape and semantics of an interface between the Rust-based implementation of a component
//! and its foreign language consumers, including details like:
//!
//!    * The names of all symbols in the compiled object file
//!    * The type and arity of all exported functions
//!    * The layout and conventions used for all arguments and return types
//!
//! If you have a dynamic library compiled from a Rust Component using this crate, and a foreign
//! language binding generated from the same `ComponentInterface` using the same version of this
//! module, then there should be no opportunities for them to disagree on how the two sides should
//! interact.
//!
//! General and incomplete TODO list for this thing:
//!
//!   * It should prevent user error and the possibility of generating bad code by doing (at least)
//!     the following checks:
//!       * No duplicate names (types, methods, args, etc)
//!       * No shadowing of builtin names, or names we use in code generation
//!     We expect that if the user actually does one of these things, then they *should* get a compile
//!     error when trying to build the component, because the codegen will be invalid. But we can't
//!     guarantee that there's not some edge-case where it produces valid-but-incorrect code.
//!
//!   * There is a *lot* of cloning going on, in the spirit of "first make it work". There's probably
//!     a good opportunity here for e.g. interned strings, but we're nowhere near the point were we need
//!     that kind of optimization just yet.
//!
//!   * Error messages and general developer experience leave a lot to be desired.

use std::{
    collections::{btree_map::Entry, BTreeMap, BTreeSet, HashSet},
    iter,
};

use anyhow::{anyhow, bail, ensure, Result};

pub mod universe;
pub use uniffi_meta::{AsType, ExternalKind, ObjectImpl, Type};
use universe::{TypeIterator, TypeUniverse};

mod callbacks;
pub use callbacks::CallbackInterface;
mod enum_;
pub use enum_::{Enum, Variant};
mod function;
pub use function::{Argument, Callable, Function, ResultType};
mod object;
pub use object::{Constructor, Method, Object, UniffiTrait};
mod record;
pub use record::{Field, Record};

pub mod ffi;
pub use ffi::{
    FfiArgument, FfiCallbackFunction, FfiDefinition, FfiField, FfiFunction, FfiStruct, FfiType,
};
pub use uniffi_meta::Radix;
use uniffi_meta::{
    ConstructorMetadata, LiteralMetadata, NamespaceMetadata, ObjectMetadata, TraitMethodMetadata,
    UniffiTraitMetadata, UNIFFI_CONTRACT_VERSION,
};
pub type Literal = LiteralMetadata;

/// The main public interface for this module, representing the complete details of an interface exposed
/// by a rust component and the details of consuming it via an extern-C FFI layer.
#[derive(Debug, Default)]
pub struct ComponentInterface {
    /// All of the types used in the interface.
    // We can't checksum `self.types`, but its contents are implied by the other fields
    // anyway, so it's safe to ignore it.
    pub(super) types: TypeUniverse,
    /// The high-level API provided by the component.
    enums: BTreeMap<String, Enum>,
    records: BTreeMap<String, Record>,
    functions: Vec<Function>,
    objects: Vec<Object>,
    callback_interfaces: Vec<CallbackInterface>,
    // Type names which were seen used as an error.
    errors: HashSet<String>,
    // Types which were seen used as callback interface error.
    callback_interface_throws_types: BTreeSet<Type>,
}

impl ComponentInterface {
    pub fn new(crate_name: &str) -> Self {
        assert!(!crate_name.is_empty());
        Self {
            types: TypeUniverse::new(NamespaceMetadata {
                crate_name: crate_name.to_string(),
                ..Default::default()
            }),
            ..Default::default()
        }
    }

    /// Parse a `ComponentInterface` from a string containing a WebIDL definition.
    pub fn from_webidl(idl: &str, module_path: &str) -> Result<Self> {
        ensure!(
            !module_path.is_empty(),
            "you must specify a valid crate name"
        );
        let group = uniffi_udl::parse_udl(idl, module_path)?;
        Self::from_metadata(group)
    }

    /// Create a `ComponentInterface` from a `MetadataGroup`
    /// Public so that external binding generators can use it.
    pub fn from_metadata(group: uniffi_meta::MetadataGroup) -> Result<Self> {
        let mut ci = Self {
            types: TypeUniverse::new(group.namespace.clone()),
            ..Default::default()
        };
        ci.add_metadata(group)?;
        Ok(ci)
    }

    /// Add a metadata group to a `ComponentInterface`.
    pub fn add_metadata(&mut self, group: uniffi_meta::MetadataGroup) -> Result<()> {
        if self.types.namespace.name.is_empty() {
            self.types.namespace = group.namespace.clone();
        } else if self.types.namespace != group.namespace {
            bail!(
                "Namespace mismatch: {:?} - {:?}",
                group.namespace,
                self.types.namespace
            );
        }

        if group.namespace_docstring.is_some() {
            self.types.namespace_docstring = group.namespace_docstring.clone();
        }

        // Unconditionally add the String type, which is used by the panic handling
        self.types.add_known_type(&uniffi_meta::Type::String)?;
        crate::macro_metadata::add_group_to_ci(self, group)?;
        Ok(())
    }

    /// The string namespace within which this API should be presented to the caller.
    ///
    /// This string would typically be used to prefix function names in the FFI, to build
    /// a package or module name for the foreign language, etc.
    pub fn namespace(&self) -> &str {
        &self.types.namespace.name
    }

    pub fn namespace_docstring(&self) -> Option<&str> {
        self.types.namespace_docstring.as_deref()
    }

    pub fn uniffi_contract_version(&self) -> u32 {
        // This is set by the scripts in the version-mismatch fixture
        let force_version = std::env::var("UNIFFI_FORCE_CONTRACT_VERSION");
        match force_version {
            Ok(v) if !v.is_empty() => v.parse().unwrap(),
            _ => UNIFFI_CONTRACT_VERSION,
        }
    }

    /// Get the definitions for every Enum type in the interface.
    pub fn enum_definitions(&self) -> impl Iterator<Item = &Enum> {
        self.enums.values()
    }

    /// Get an Enum definition by name, or None if no such Enum is defined.
    pub fn get_enum_definition(&self, name: &str) -> Option<&Enum> {
        self.enums.get(name)
    }

    /// Get the definitions for every Record type in the interface.
    pub fn record_definitions(&self) -> impl Iterator<Item = &Record> {
        self.records.values()
    }

    /// Get a Record definition by name, or None if no such Record is defined.
    pub fn get_record_definition(&self, name: &str) -> Option<&Record> {
        self.records.get(name)
    }

    /// Get the definitions for every Function in the interface.
    pub fn function_definitions(&self) -> &[Function] {
        &self.functions
    }

    /// Get a Function definition by name, or None if no such Function is defined.
    pub fn get_function_definition(&self, name: &str) -> Option<&Function> {
        // TODO: probably we could store these internally in a HashMap to make this easier?
        self.functions.iter().find(|f| f.name == name)
    }

    /// Get the definitions for every Object type in the interface.
    pub fn object_definitions(&self) -> &[Object] {
        &self.objects
    }

    /// Get an Object definition by name, or None if no such Object is defined.
    pub fn get_object_definition(&self, name: &str) -> Option<&Object> {
        // TODO: probably we could store these internally in a HashMap to make this easier?
        self.objects.iter().find(|o| o.name == name)
    }

    fn callback_interface_callback_definitions(
        &self,
    ) -> impl IntoIterator<Item = FfiCallbackFunction> + '_ {
        self.callback_interfaces
            .iter()
            .flat_map(|cbi| cbi.ffi_callbacks())
            .chain(self.objects.iter().flat_map(|o| o.ffi_callbacks()))
    }

    /// Get the definitions for callback FFI functions
    ///
    /// These are defined by the foreign code and invoked by Rust.
    fn callback_interface_vtable_definitions(&self) -> impl IntoIterator<Item = FfiStruct> + '_ {
        self.callback_interface_definitions()
            .iter()
            .map(|cbi| cbi.vtable_definition())
            .chain(
                self.object_definitions()
                    .iter()
                    .flat_map(|o| o.vtable_definition()),
            )
    }

    /// Get the definitions for every Callback Interface type in the interface.
    pub fn callback_interface_definitions(&self) -> &[CallbackInterface] {
        &self.callback_interfaces
    }

    /// Get a Callback interface definition by name, or None if no such interface is defined.
    pub fn get_callback_interface_definition(&self, name: &str) -> Option<&CallbackInterface> {
        // TODO: probably we could store these internally in a HashMap to make this easier?
        self.callback_interfaces.iter().find(|o| o.name == name)
    }

    /// Get the definitions for every Callback Interface type in the interface.
    pub fn has_async_callback_interface_definition(&self) -> bool {
        self.callback_interfaces
            .iter()
            .any(|cbi| cbi.has_async_method())
            || self
                .objects
                .iter()
                .any(|o| o.has_callback_interface() && o.has_async_method())
    }

    /// Get the definitions for every Method type in the interface.
    pub fn iter_callables(&self) -> impl Iterator<Item = &dyn Callable> {
        // Each of the `as &dyn Callable` casts is a trivial cast, but it seems like the clearest
        // way to express the logic in the current Rust
        #[allow(trivial_casts)]
        self.function_definitions()
            .iter()
            .map(|f| f as &dyn Callable)
            .chain(self.objects.iter().flat_map(|o| {
                o.constructors()
                    .into_iter()
                    .map(|c| c as &dyn Callable)
                    .chain(o.methods().into_iter().map(|m| m as &dyn Callable))
            }))
    }

    /// Should we generate read (and lift) functions for errors?
    ///
    /// This is a workaround for the fact that lower/write can't be generated for some errors,
    /// specifically errors that are defined as flat in the UDL, but actually have fields in the
    /// Rust source.
    pub fn should_generate_error_read(&self, e: &Enum) -> bool {
        // We can and should always generate read() methods for fielded errors
        let fielded = !e.is_flat();
        // For flat errors, we should only generate read() methods if we need them to support
        // callback interface errors
        let used_in_foreign_interface = self
            .callback_interface_definitions()
            .iter()
            .flat_map(|cb| cb.methods())
            .chain(
                self.object_definitions()
                    .iter()
                    .filter(|o| o.has_callback_interface())
                    .flat_map(|o| o.methods()),
            )
            .any(|m| m.throws_type() == Some(&e.as_type()));

        self.is_name_used_as_error(&e.name) && (fielded || used_in_foreign_interface)
    }

    /// Get details about all `Type::External` types.
    /// Returns an iterator of (name, crate_name, kind)
    pub fn iter_external_types(
        &self,
    ) -> impl Iterator<Item = (&String, String, ExternalKind, bool)> {
        self.types.iter_known_types().filter_map(|t| match t {
            Type::External {
                name,
                module_path,
                kind,
                tagged,
                ..
            } => Some((
                name,
                module_path.split("::").next().unwrap().to_string(),
                *kind,
                *tagged,
            )),
            _ => None,
        })
    }

    /// Get details about all `Type::Custom` types
    pub fn iter_custom_types(&self) -> impl Iterator<Item = (&String, &Type)> {
        self.types.iter_known_types().filter_map(|t| match t {
            Type::Custom { name, builtin, .. } => Some((name, &**builtin)),
            _ => None,
        })
    }

    /// Iterate over all known types in the interface.
    pub fn iter_types(&self) -> impl Iterator<Item = &Type> {
        self.types.iter_known_types()
    }

    /// Get a specific type
    pub fn get_type(&self, name: &str) -> Option<Type> {
        self.types.get_type_definition(name)
    }

    /// Iterate over all types contained in the given item.
    ///
    /// This method uses `iter_types` to iterate over the types contained within the given type,
    /// but additionally recurses into the definition of user-defined types like records and enums
    /// to yield the types that *they* contain.
    fn iter_types_in_item<'a>(&'a self, item: &'a Type) -> impl Iterator<Item = &'a Type> + 'a {
        RecursiveTypeIterator::new(self, item)
    }

    /// Check whether the given item contains any (possibly nested) Type::Object references.
    ///
    /// This is important to know in language bindings that cannot integrate object types
    /// tightly with the host GC, and hence need to perform manual destruction of objects.
    pub fn item_contains_object_references(&self, item: &Type) -> bool {
        // this is surely broken for external records with object refs?
        self.iter_types_in_item(item).any(|t| {
            matches!(
                t,
                Type::Object { .. }
                    | Type::External {
                        kind: ExternalKind::Interface,
                        ..
                    }
            )
        })
    }

    /// Check whether the given item contains any (possibly nested) unsigned types
    pub fn item_contains_unsigned_types(&self, item: &Type) -> bool {
        self.iter_types_in_item(item)
            .any(|t| matches!(t, Type::UInt8 | Type::UInt16 | Type::UInt32 | Type::UInt64))
    }

    /// Check whether the interface contains any optional types
    pub fn contains_optional_types(&self) -> bool {
        self.types
            .iter_known_types()
            .any(|t| matches!(t, Type::Optional { .. }))
    }

    /// Check whether the interface contains any sequence types
    pub fn contains_sequence_types(&self) -> bool {
        self.types
            .iter_known_types()
            .any(|t| matches!(t, Type::Sequence { .. }))
    }

    /// Check whether the interface contains any map types
    pub fn contains_map_types(&self) -> bool {
        self.types
            .iter_known_types()
            .any(|t| matches!(t, Type::Map { .. }))
    }

    /// Check whether the interface contains any object types
    pub fn contains_object_types(&self) -> bool {
        self.types
            .iter_known_types()
            .any(|t| matches!(t, Type::Object { .. }))
    }

    // The namespace to use in crate-level FFI function definitions. Not used as the ffi
    // namespace for types - each type has its own `module_path` which is used for them.
    fn ffi_namespace(&self) -> &str {
        &self.types.namespace.crate_name
    }

    /// Builtin FFI function to get the current contract version
    /// This is needed so that the foreign language bindings can check that they are using the same
    /// ABI as the scaffolding
    pub fn ffi_uniffi_contract_version(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_uniffi_contract_version", self.ffi_namespace()),
            is_async: false,
            arguments: vec![],
            return_type: Some(FfiType::UInt32),
            has_rust_call_status_arg: false,
            is_object_free_function: false,
        }
    }

    /// Builtin FFI function for allocating a new `RustBuffer`.
    /// This is needed so that the foreign language bindings can create buffers in which to pass
    /// complex data types across the FFI.
    pub fn ffi_rustbuffer_alloc(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_rustbuffer_alloc", self.ffi_namespace()),
            is_async: false,
            arguments: vec![FfiArgument {
                name: "size".to_string(),
                type_: FfiType::UInt64,
            }],
            return_type: Some(FfiType::RustBuffer(None)),
            has_rust_call_status_arg: true,
            is_object_free_function: false,
        }
    }

    /// Builtin FFI function for copying foreign-owned bytes
    /// This is needed so that the foreign language bindings can create buffers in which to pass
    /// complex data types across the FFI.
    pub fn ffi_rustbuffer_from_bytes(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_rustbuffer_from_bytes", self.ffi_namespace()),
            is_async: false,
            arguments: vec![FfiArgument {
                name: "bytes".to_string(),
                type_: FfiType::ForeignBytes,
            }],
            return_type: Some(FfiType::RustBuffer(None)),
            has_rust_call_status_arg: true,
            is_object_free_function: false,
        }
    }

    /// Builtin FFI function for freeing a `RustBuffer`.
    /// This is needed so that the foreign language bindings can free buffers in which they received
    /// complex data types returned across the FFI.
    pub fn ffi_rustbuffer_free(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_rustbuffer_free", self.ffi_namespace()),
            is_async: false,
            arguments: vec![FfiArgument {
                name: "buf".to_string(),
                type_: FfiType::RustBuffer(None),
            }],
            return_type: None,
            has_rust_call_status_arg: true,
            is_object_free_function: false,
        }
    }

    /// Builtin FFI function for reserving extra space in a `RustBuffer`.
    /// This is needed so that the foreign language bindings can grow buffers used for passing
    /// complex data types across the FFI.
    pub fn ffi_rustbuffer_reserve(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_rustbuffer_reserve", self.ffi_namespace()),
            is_async: false,
            arguments: vec![
                FfiArgument {
                    name: "buf".to_string(),
                    type_: FfiType::RustBuffer(None),
                },
                FfiArgument {
                    name: "additional".to_string(),
                    type_: FfiType::UInt64,
                },
            ],
            return_type: Some(FfiType::RustBuffer(None)),
            has_rust_call_status_arg: true,
            is_object_free_function: false,
        }
    }

    /// Builtin FFI function to poll a Rust future.
    pub fn ffi_rust_future_poll(&self, return_ffi_type: Option<FfiType>) -> FfiFunction {
        FfiFunction {
            name: self.rust_future_ffi_fn_name("rust_future_poll", return_ffi_type),
            is_async: false,
            arguments: vec![
                FfiArgument {
                    name: "handle".to_owned(),
                    type_: FfiType::Handle,
                },
                FfiArgument {
                    name: "callback".to_owned(),
                    type_: FfiType::Callback("RustFutureContinuationCallback".to_owned()),
                },
                FfiArgument {
                    name: "callback_data".to_owned(),
                    type_: FfiType::Handle,
                },
            ],
            return_type: None,
            has_rust_call_status_arg: false,
            is_object_free_function: false,
        }
    }

    /// Builtin FFI function to complete a Rust future and get it's result.
    ///
    /// We generate one of these for each FFI return type.
    pub fn ffi_rust_future_complete(&self, return_ffi_type: Option<FfiType>) -> FfiFunction {
        FfiFunction {
            name: self.rust_future_ffi_fn_name("rust_future_complete", return_ffi_type.clone()),
            is_async: false,
            arguments: vec![FfiArgument {
                name: "handle".to_owned(),
                type_: FfiType::Handle,
            }],
            return_type: return_ffi_type,
            has_rust_call_status_arg: true,
            is_object_free_function: false,
        }
    }

    /// Builtin FFI function for cancelling a Rust Future
    pub fn ffi_rust_future_cancel(&self, return_ffi_type: Option<FfiType>) -> FfiFunction {
        FfiFunction {
            name: self.rust_future_ffi_fn_name("rust_future_cancel", return_ffi_type),
            is_async: false,
            arguments: vec![FfiArgument {
                name: "handle".to_owned(),
                type_: FfiType::Handle,
            }],
            return_type: None,
            has_rust_call_status_arg: false,
            is_object_free_function: false,
        }
    }

    /// Builtin FFI function for freeing a Rust Future
    pub fn ffi_rust_future_free(&self, return_ffi_type: Option<FfiType>) -> FfiFunction {
        FfiFunction {
            name: self.rust_future_ffi_fn_name("rust_future_free", return_ffi_type),
            is_async: false,
            arguments: vec![FfiArgument {
                name: "handle".to_owned(),
                type_: FfiType::Handle,
            }],
            return_type: None,
            has_rust_call_status_arg: false,
            is_object_free_function: false,
        }
    }

    fn rust_future_ffi_fn_name(&self, base_name: &str, return_ffi_type: Option<FfiType>) -> String {
        let namespace = self.ffi_namespace();
        let return_type_name = FfiType::return_type_name(return_ffi_type.as_ref());
        format!("ffi_{namespace}_{base_name}_{return_type_name}")
    }

    /// Does this interface contain async functions?
    pub fn has_async_fns(&self) -> bool {
        self.iter_ffi_function_definitions().any(|f| f.is_async())
            || self
                .callback_interfaces
                .iter()
                .any(CallbackInterface::has_async_method)
    }

    /// Iterate over `T` parameters of the `FutureCallback<T>` callbacks in this interface
    pub fn iter_future_callback_params(&self) -> impl Iterator<Item = FfiType> {
        let unique_results = self
            .iter_callables()
            .map(|c| c.result_type().future_callback_param())
            .collect::<BTreeSet<_>>();
        unique_results.into_iter()
    }

    /// Iterate over return/throws types for async functions
    pub fn iter_async_result_types(&self) -> impl Iterator<Item = ResultType> {
        let unique_results = self
            .iter_callables()
            .map(|c| c.result_type())
            .collect::<BTreeSet<_>>();
        unique_results.into_iter()
    }

    /// Iterate over all Ffi definitions
    pub fn ffi_definitions(&self) -> impl Iterator<Item = FfiDefinition> + '_ {
        // Note: for languages like Python it's important to keep things in dependency order.
        // For example some FFI function definitions depend on FFI struct definitions, so the
        // function definitions come last.
        self.builtin_ffi_definitions()
            .into_iter()
            .chain(
                self.callback_interface_callback_definitions()
                    .into_iter()
                    .map(Into::into),
            )
            .chain(
                self.callback_interface_vtable_definitions()
                    .into_iter()
                    .map(Into::into),
            )
            .chain(self.iter_ffi_function_definitions().map(Into::into))
    }

    fn builtin_ffi_definitions(&self) -> impl IntoIterator<Item = FfiDefinition> + '_ {
        [
            FfiCallbackFunction {
                name: "RustFutureContinuationCallback".to_owned(),
                arguments: vec![
                    FfiArgument::new("data", FfiType::UInt64),
                    FfiArgument::new("poll_result", FfiType::Int8),
                ],
                return_type: None,
                has_rust_call_status_arg: false,
            }
            .into(),
            FfiCallbackFunction {
                name: "ForeignFutureFree".to_owned(),
                arguments: vec![FfiArgument::new("handle", FfiType::UInt64)],
                return_type: None,
                has_rust_call_status_arg: false,
            }
            .into(),
            FfiCallbackFunction {
                name: "CallbackInterfaceFree".to_owned(),
                arguments: vec![FfiArgument::new("handle", FfiType::UInt64)],
                return_type: None,
                has_rust_call_status_arg: false,
            }
            .into(),
            FfiStruct {
                name: "ForeignFuture".to_owned(),
                fields: vec![
                    FfiField::new("handle", FfiType::UInt64),
                    FfiField::new("free", FfiType::Callback("ForeignFutureFree".to_owned())),
                ],
            }
            .into(),
        ]
        .into_iter()
        .chain(
            self.all_possible_return_ffi_types()
                .flat_map(|return_type| {
                    [
                        callbacks::foreign_future_ffi_result_struct(return_type.clone()).into(),
                        callbacks::ffi_foreign_future_complete(return_type).into(),
                    ]
                }),
        )
    }

    /// List the definitions of all FFI functions in the interface.
    ///
    /// The set of FFI functions is derived automatically from the set of higher-level types
    /// along with the builtin FFI helper functions.
    pub fn iter_ffi_function_definitions(&self) -> impl Iterator<Item = FfiFunction> + '_ {
        self.iter_user_ffi_function_definitions()
            .cloned()
            .chain(self.iter_rust_buffer_ffi_function_definitions())
            .chain(self.iter_futures_ffi_function_definitions())
            .chain(self.iter_checksum_ffi_functions())
            .chain([self.ffi_uniffi_contract_version()])
    }

    /// Alternate version of iter_ffi_function_definitions for languages that don't support async
    pub fn iter_ffi_function_definitions_non_async(
        &self,
    ) -> impl Iterator<Item = FfiFunction> + '_ {
        self.iter_user_ffi_function_definitions()
            .cloned()
            .chain(self.iter_rust_buffer_ffi_function_definitions())
            .chain(self.iter_checksum_ffi_functions())
            .chain([self.ffi_uniffi_contract_version()])
    }

    /// List all FFI functions definitions for user-defined interfaces
    ///
    /// This includes FFI functions for:
    ///   - Top-level functions
    ///   - Object methods
    ///   - Callback interfaces
    pub fn iter_user_ffi_function_definitions(&self) -> impl Iterator<Item = &FfiFunction> + '_ {
        iter::empty()
            .chain(
                self.objects
                    .iter()
                    .flat_map(|obj| obj.iter_ffi_function_definitions()),
            )
            .chain(
                self.callback_interfaces
                    .iter()
                    .map(|cb| cb.ffi_init_callback()),
            )
            .chain(self.functions.iter().map(|f| &f.ffi_func))
    }

    /// List all FFI functions definitions for RustBuffer functionality.
    pub fn iter_rust_buffer_ffi_function_definitions(&self) -> impl Iterator<Item = FfiFunction> {
        [
            self.ffi_rustbuffer_alloc(),
            self.ffi_rustbuffer_from_bytes(),
            self.ffi_rustbuffer_free(),
            self.ffi_rustbuffer_reserve(),
        ]
        .into_iter()
    }

    fn all_possible_return_ffi_types(&self) -> impl Iterator<Item = Option<FfiType>> {
        [
            Some(FfiType::UInt8),
            Some(FfiType::Int8),
            Some(FfiType::UInt16),
            Some(FfiType::Int16),
            Some(FfiType::UInt32),
            Some(FfiType::Int32),
            Some(FfiType::UInt64),
            Some(FfiType::Int64),
            Some(FfiType::Float32),
            Some(FfiType::Float64),
            // RustBuffer and RustArcPtr have an inner field which we have to fill in with a
            // placeholder value.
            Some(FfiType::RustArcPtr("".to_owned())),
            Some(FfiType::RustBuffer(None)),
            None,
        ]
        .into_iter()
    }

    /// List all FFI functions definitions for async functionality.
    pub fn iter_futures_ffi_function_definitions(&self) -> impl Iterator<Item = FfiFunction> + '_ {
        self.all_possible_return_ffi_types()
            .flat_map(|return_type| {
                [
                    self.ffi_rust_future_poll(return_type.clone()),
                    self.ffi_rust_future_cancel(return_type.clone()),
                    self.ffi_rust_future_free(return_type.clone()),
                    self.ffi_rust_future_complete(return_type),
                ]
            })
    }

    /// List all API checksums to check
    ///
    /// Returns a list of (export_symbol_name, checksum) items
    pub fn iter_checksums(&self) -> impl Iterator<Item = (String, u16)> + '_ {
        let func_checksums = self
            .functions
            .iter()
            .map(|f| (f.checksum_fn_name(), f.checksum()));
        let method_checksums = self.objects.iter().flat_map(|o| {
            o.methods()
                .into_iter()
                .map(|m| (m.checksum_fn_name(), m.checksum()))
        });
        let constructor_checksums = self.objects.iter().flat_map(|o| {
            o.constructors()
                .into_iter()
                .map(|c| (c.checksum_fn_name(), c.checksum()))
        });
        let callback_method_checksums = self.callback_interfaces.iter().flat_map(|cbi| {
            cbi.methods().into_iter().filter_map(|m| {
                if m.checksum_fn_name().is_empty() {
                    // UDL-based callbacks don't have checksum functions, skip these
                    None
                } else {
                    Some((m.checksum_fn_name(), m.checksum()))
                }
            })
        });
        func_checksums
            .chain(method_checksums)
            .chain(constructor_checksums)
            .chain(callback_method_checksums)
            .map(|(fn_name, checksum)| (fn_name.to_string(), checksum))
    }

    pub fn iter_checksum_ffi_functions(&self) -> impl Iterator<Item = FfiFunction> + '_ {
        self.iter_checksums().map(|(name, _)| FfiFunction {
            name,
            is_async: false,
            arguments: vec![],
            return_type: Some(FfiType::UInt16),
            has_rust_call_status_arg: false,
            is_object_free_function: false,
        })
    }

    // Private methods for building a ComponentInterface.
    //
    /// Called by `APIBuilder` impls to add a newly-parsed enum definition to the `ComponentInterface`.
    pub(super) fn add_enum_definition(&mut self, defn: Enum) -> Result<()> {
        match self.enums.entry(defn.name().to_owned()) {
            Entry::Vacant(v) => {
                self.types.add_known_types(defn.iter_types())?;
                v.insert(defn);
            }
            Entry::Occupied(o) => {
                let existing_def = o.get();
                if defn != *existing_def {
                    bail!(
                        "Mismatching definition for enum `{}`!\n\
                        existing definition: {existing_def:#?},\n\
                        new definition: {defn:#?}",
                        defn.name(),
                    );
                }
            }
        }

        Ok(())
    }

    /// Adds a newly-parsed record definition to the `ComponentInterface`.
    pub(super) fn add_record_definition(&mut self, defn: Record) -> Result<()> {
        match self.records.entry(defn.name().to_owned()) {
            Entry::Vacant(v) => {
                self.types.add_known_types(defn.iter_types())?;
                v.insert(defn);
            }
            Entry::Occupied(o) => {
                let existing_def = o.get();
                if defn != *existing_def {
                    bail!(
                        "Mismatching definition for record `{}`!\n\
                         existing definition: {existing_def:#?},\n\
                         new definition: {defn:#?}",
                        defn.name(),
                    );
                }
            }
        }

        Ok(())
    }

    /// Called by `APIBuilder` impls to add a newly-parsed function definition to the `ComponentInterface`.
    pub(super) fn add_function_definition(&mut self, defn: Function) -> Result<()> {
        // Since functions are not a first-class type, we have to check for duplicates here
        // rather than relying on the type-finding pass to catch them.
        if self.functions.iter().any(|f| f.name == defn.name) {
            bail!("duplicate function definition: \"{}\"", defn.name);
        }
        if self.types.get_type_definition(defn.name()).is_some() {
            bail!("Conflicting type definition for \"{}\"", defn.name());
        }
        self.types.add_known_types(defn.iter_types())?;
        defn.throws_name()
            .map(|n| self.errors.insert(n.to_string()));
        self.functions.push(defn);

        Ok(())
    }

    pub(super) fn add_constructor_meta(&mut self, meta: ConstructorMetadata) -> Result<()> {
        let object = get_object(&mut self.objects, &meta.self_name)
            .ok_or_else(|| anyhow!("add_constructor_meta: object {} not found", &meta.self_name))?;
        let defn: Constructor = meta.into();

        self.types.add_known_types(defn.iter_types())?;
        defn.throws_name()
            .map(|n| self.errors.insert(n.to_string()));
        object.constructors.push(defn);

        Ok(())
    }

    pub(super) fn add_method_meta(&mut self, meta: impl Into<Method>) -> Result<()> {
        let mut method: Method = meta.into();
        let object = get_object(&mut self.objects, &method.object_name)
            .ok_or_else(|| anyhow!("add_method_meta: object {} not found", &method.object_name))?;

        self.types.add_known_types(method.iter_types())?;
        method
            .throws_name()
            .map(|n| self.errors.insert(n.to_string()));
        method.object_impl = object.imp;
        object.methods.push(method);
        Ok(())
    }

    pub(super) fn add_uniffitrait_meta(&mut self, meta: UniffiTraitMetadata) -> Result<()> {
        let object = get_object(&mut self.objects, meta.self_name())
            .ok_or_else(|| anyhow!("add_uniffitrait_meta: object not found"))?;
        let ut: UniffiTrait = meta.into();
        self.types.add_known_types(ut.iter_types())?;
        object.uniffi_traits.push(ut);
        Ok(())
    }

    pub(super) fn add_object_meta(&mut self, meta: ObjectMetadata) -> Result<()> {
        self.add_object_definition(meta.into())
    }

    /// Called by `APIBuilder` impls to add a newly-parsed object definition to the `ComponentInterface`.
    fn add_object_definition(&mut self, defn: Object) -> Result<()> {
        self.types.add_known_types(defn.iter_types())?;
        self.objects.push(defn);
        Ok(())
    }

    pub fn is_name_used_as_error(&self, name: &str) -> bool {
        self.errors.contains(name)
    }

    /// Called by `APIBuilder` impls to add a newly-parsed callback interface definition to the `ComponentInterface`.
    pub(super) fn add_callback_interface_definition(&mut self, defn: CallbackInterface) {
        self.callback_interfaces.push(defn);
    }

    pub(super) fn add_trait_method_meta(&mut self, meta: TraitMethodMetadata) -> Result<()> {
        if let Some(cbi) = get_callback_interface(&mut self.callback_interfaces, &meta.trait_name) {
            // uniffi_meta should ensure that we process callback interface methods in order, double
            // check that here
            if cbi.methods.len() != meta.index as usize {
                bail!(
                    "UniFFI internal error: callback interface method index mismatch for {}::{} (expected {}, saw {})",
                    meta.trait_name,
                    meta.name,
                    cbi.methods.len(),
                    meta.index,
                );
            }
            let method: Method = meta.into();
            if let Some(error) = method.throws_type() {
                self.callback_interface_throws_types.insert(error.clone());
            }
            self.types.add_known_types(method.iter_types())?;
            method
                .throws_name()
                .map(|n| self.errors.insert(n.to_string()));
            cbi.methods.push(method);
        } else {
            self.add_method_meta(meta)?;
        }
        Ok(())
    }

    /// Perform global consistency checks on the declared interface.
    ///
    /// This method checks for consistency problems in the declared interface
    /// as a whole, and which can only be detected after we've finished defining
    /// the entire interface.
    pub fn check_consistency(&self) -> Result<()> {
        if self.namespace().is_empty() {
            bail!("missing namespace definition");
        }

        // Because functions aren't first class types, we need to check here that
        // a function name hasn't already been used as a type name.
        for f in self.functions.iter() {
            if self.types.get_type_definition(f.name()).is_some() {
                bail!("Conflicting type definition for \"{}\"", f.name());
            }
        }
        Ok(())
    }

    /// Automatically derive the low-level FFI functions from the high-level types in the interface.
    ///
    /// This should only be called after the high-level types have been completed defined, otherwise
    /// the resulting set will be missing some entries.
    pub fn derive_ffi_funcs(&mut self) -> Result<()> {
        for func in self.functions.iter_mut() {
            func.derive_ffi_func()?;
        }
        for obj in self.objects.iter_mut() {
            obj.derive_ffi_funcs()?;
        }
        for callback in self.callback_interfaces.iter_mut() {
            callback.derive_ffi_funcs();
        }
        Ok(())
    }
}

fn get_object<'a>(objects: &'a mut [Object], name: &str) -> Option<&'a mut Object> {
    objects.iter_mut().find(|o| o.name == name)
}

fn get_callback_interface<'a>(
    callback_interfaces: &'a mut [CallbackInterface],
    name: &str,
) -> Option<&'a mut CallbackInterface> {
    callback_interfaces.iter_mut().find(|o| o.name == name)
}

/// Stateful iterator for yielding all types contained in a given type.
///
/// This struct is the implementation of [`ComponentInterface::iter_types_in_item`] and should be
/// considered an opaque implementation detail. It's a separate struct because I couldn't
/// figure out a way to implement it using iterators and closures that would make the lifetimes
/// work out correctly.
///
/// The idea here is that we want to yield all the types from `iter_types` on a given type, and
/// additionally we want to recurse into the definition of any user-provided types like records,
/// enums, etc so we can also yield the types contained therein.
///
/// To guard against infinite recursion, we maintain a list of previously-seen user-defined
/// types, ensuring that we recurse into the definition of those types only once. To simplify
/// the implementation, we maintain a queue of pending user-defined types that we have seen
/// but not yet recursed into. (Ironically, the use of an explicit queue means our implementation
/// is not actually recursive...)
struct RecursiveTypeIterator<'a> {
    /// The [`ComponentInterface`] from which this iterator was created.
    ci: &'a ComponentInterface,
    /// The currently-active iterator from which we're yielding.
    current: TypeIterator<'a>,
    /// A set of names of user-defined types that we have already seen.
    seen: HashSet<&'a str>,
    /// A queue of user-defined types that we need to recurse into.
    pending: Vec<&'a Type>,
}

impl<'a> RecursiveTypeIterator<'a> {
    /// Allocate a new `RecursiveTypeIterator` over the given item.
    fn new(ci: &'a ComponentInterface, item: &'a Type) -> RecursiveTypeIterator<'a> {
        RecursiveTypeIterator {
            ci,
            // We begin by iterating over the types from the item itself.
            current: item.iter_types(),
            seen: Default::default(),
            pending: Default::default(),
        }
    }

    /// Add a new type to the queue of pending types, if not previously seen.
    fn add_pending_type(&mut self, type_: &'a Type) {
        match type_ {
            Type::Record { name, .. }
            | Type::Enum { name, .. }
            | Type::Object { name, .. }
            | Type::CallbackInterface { name, .. } => {
                if !self.seen.contains(name.as_str()) {
                    self.pending.push(type_);
                    self.seen.insert(name.as_str());
                }
            }
            _ => (),
        }
    }

    /// Advance the iterator to recurse into the next pending type, if any.
    ///
    /// This method is called when the current iterator is empty, and it will select
    /// the next pending type from the queue and start iterating over its contained types.
    /// The return value will be the first item from the new iterator.
    fn advance_to_next_type(&mut self) -> Option<&'a Type> {
        if let Some(next_type) = self.pending.pop() {
            // This is a little awkward because the various definition lookup methods return an `Option<T>`.
            // In the unlikely event that one of them returns `None` then, rather than trying to advance
            // to a non-existent type, we just leave the existing iterator in place and allow the recursive
            // call to `next()` to try again with the next pending type.
            let next_iter = match next_type {
                Type::Record { name, .. } => {
                    self.ci.get_record_definition(name).map(Record::iter_types)
                }
                Type::Enum { name, .. } => self.ci.get_enum_definition(name).map(Enum::iter_types),
                Type::Object { name, .. } => {
                    self.ci.get_object_definition(name).map(Object::iter_types)
                }
                Type::CallbackInterface { name, .. } => self
                    .ci
                    .get_callback_interface_definition(name)
                    .map(CallbackInterface::iter_types),
                _ => None,
            };
            if let Some(next_iter) = next_iter {
                self.current = next_iter;
            }
            // Advance the new iterator to its first item. If the new iterator happens to be empty,
            // this will recurse back in to `advance_to_next_type` until we find one that isn't.
            self.next()
        } else {
            // We've completely finished the iteration over all pending types.
            None
        }
    }
}

impl<'a> Iterator for RecursiveTypeIterator<'a> {
    type Item = &'a Type;
    fn next(&mut self) -> Option<Self::Item> {
        if let Some(type_) = self.current.next() {
            self.add_pending_type(type_);
            Some(type_)
        } else {
            self.advance_to_next_type()
        }
    }
}

// Helpers for functions/methods/constructors which all have the same "throws" semantics.
fn throws_name(throws: &Option<Type>) -> Option<&str> {
    // Type has no `name()` method, just `canonical_name()` which isn't what we want.
    match throws {
        None => None,
        Some(Type::Enum { name, .. }) | Some(Type::Object { name, .. }) => Some(name),
        _ => panic!("unknown throw type: {throws:?}"),
    }
}

#[cfg(test)]
mod test {
    use super::*;

    // Note that much of the functionality of `ComponentInterface` is tested via its interactions
    // with specific member types, in the sub-modules defining those member types.

    #[test]
    fn test_duplicate_type_names_are_an_error() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                constructor();
            };
            dictionary Testing {
                u32 field;
            };
        "#;
        let err = ComponentInterface::from_webidl(UDL, "crate_name").unwrap_err();
        assert_eq!(
            err.to_string(),
            "Conflicting type definition for `Testing`! \
             existing definition: Object { module_path: \"crate_name\", name: \"Testing\", imp: Struct }, \
             new definition: Record { module_path: \"crate_name\", name: \"Testing\" }"
        );

        const UDL2: &str = r#"
            namespace test{};
            enum Testing {
                "one", "two"
            };
            [Error]
            enum Testing { "three", "four" };
        "#;
        let err = ComponentInterface::from_webidl(UDL2, "crate_name").unwrap_err();
        assert_eq!(
            err.to_string(),
            "Mismatching definition for enum `Testing`!
existing definition: Enum {
    name: \"Testing\",
    module_path: \"crate_name\",
    discr_type: None,
    variants: [
        Variant {
            name: \"one\",
            discr: None,
            fields: [],
            docstring: None,
        },
        Variant {
            name: \"two\",
            discr: None,
            fields: [],
            docstring: None,
        },
    ],
    flat: true,
    non_exhaustive: false,
    docstring: None,
},
new definition: Enum {
    name: \"Testing\",
    module_path: \"crate_name\",
    discr_type: None,
    variants: [
        Variant {
            name: \"three\",
            discr: None,
            fields: [],
            docstring: None,
        },
        Variant {
            name: \"four\",
            discr: None,
            fields: [],
            docstring: None,
        },
    ],
    flat: true,
    non_exhaustive: false,
    docstring: None,
}",
        );

        const UDL3: &str = r#"
            namespace test{
                u32 Testing();
            };
            enum Testing {
                "one", "two"
            };
        "#;
        let err = ComponentInterface::from_webidl(UDL3, "crate_name").unwrap_err();
        assert!(format!("{err:#}").contains("Conflicting type definition for \"Testing\""));
    }

    #[test]
    fn test_contains_optional_types() {
        let mut ci = ComponentInterface {
            ..Default::default()
        };

        // check that `contains_optional_types` returns false when there is no Optional type in the interface
        assert!(!ci.contains_optional_types());

        // check that `contains_optional_types` returns true when there is an Optional type in the interface
        assert!(ci
            .types
            .add_known_type(&Type::Optional {
                inner_type: Box::new(Type::String)
            })
            .is_ok());
        assert!(ci.contains_optional_types());
    }

    #[test]
    fn test_contains_sequence_types() {
        let mut ci = ComponentInterface {
            ..Default::default()
        };

        // check that `contains_sequence_types` returns false when there is no Sequence type in the interface
        assert!(!ci.contains_sequence_types());

        // check that `contains_sequence_types` returns true when there is a Sequence type in the interface
        assert!(ci
            .types
            .add_known_type(&Type::Sequence {
                inner_type: Box::new(Type::UInt64)
            })
            .is_ok());
        assert!(ci.contains_sequence_types());
        assert!(ci.types.contains(&Type::UInt64));
    }

    #[test]
    fn test_contains_map_types() {
        let mut ci = ComponentInterface {
            ..Default::default()
        };

        // check that `contains_map_types` returns false when there is no Map type in the interface
        assert!(!ci.contains_map_types());

        // check that `contains_map_types` returns true when there is a Map type in the interface
        assert!(ci
            .types
            .add_known_type(&Type::Map {
                key_type: Box::new(Type::String),
                value_type: Box::new(Type::Boolean)
            })
            .is_ok());
        assert!(ci.contains_map_types());
        assert!(ci.types.contains(&Type::String));
        assert!(ci.types.contains(&Type::Boolean));
    }

    #[test]
    fn test_no_infinite_recursion_when_walking_types() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                void tester(Testing foo);
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert!(!ci.item_contains_unsigned_types(&Type::Object {
            name: "Testing".into(),
            module_path: "".into(),
            imp: ObjectImpl::Struct,
        }));
    }

    #[test]
    fn test_correct_recursion_when_walking_types() {
        const UDL: &str = r#"
            namespace test{};
            interface TestObj {
                void tester(TestRecord foo);
            };
            dictionary TestRecord {
                NestedRecord bar;
            };
            dictionary NestedRecord {
                u64 baz;
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert!(ci.item_contains_unsigned_types(&Type::Object {
            name: "TestObj".into(),
            module_path: "".into(),
            imp: ObjectImpl::Struct,
        }));
    }

    #[test]
    fn test_docstring_namespace() {
        const UDL: &str = r#"
            /// informative docstring
            namespace test{};
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.namespace_docstring().unwrap(), "informative docstring");
    }

    #[test]
    fn test_multiline_docstring() {
        const UDL: &str = r#"
            /// informative
            /// docstring
            namespace test{};
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.namespace_docstring().unwrap(), "informative\ndocstring");
    }
}
