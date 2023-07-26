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
    convert::TryFrom,
    iter,
};

use anyhow::{anyhow, bail, ensure, Result};

pub mod types;
pub use types::{AsType, ExternalKind, ObjectImpl, Type};
use types::{TypeIterator, TypeUniverse};

mod attributes;
mod callbacks;
pub use callbacks::CallbackInterface;
mod enum_;
pub use enum_::{Enum, Variant};
mod function;
pub use function::{Argument, Callable, Function, ResultType};
mod literal;
pub use literal::{Literal, Radix};
mod namespace;
pub use namespace::Namespace;
mod object;
pub use object::{Constructor, Method, Object, UniffiTrait};
mod record;
pub use record::{Field, Record};

pub mod ffi;
pub use ffi::{FfiArgument, FfiFunction, FfiType};
use uniffi_meta::{ConstructorMetadata, ObjectMetadata, TraitMethodMetadata};

// This needs to match the minor version of the `uniffi` crate.  See
// `docs/uniffi-versioning.md` for details.
//
// Once we get to 1.0, then we'll need to update the scheme to something like 100 + major_version
const UNIFFI_CONTRACT_VERSION: u32 = 22;

/// The main public interface for this module, representing the complete details of an interface exposed
/// by a rust component and the details of consuming it via an extern-C FFI layer.
#[derive(Debug, Default)]
pub struct ComponentInterface {
    /// All of the types used in the interface.
    // We can't checksum `self.types`, but its contents are implied by the other fields
    // anyway, so it's safe to ignore it.
    pub(super) types: TypeUniverse,
    /// The unique prefix that we'll use for namespacing when exposing this component's API.
    namespace: String,
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
    /// Parse a `ComponentInterface` from a string containing a WebIDL definition.
    pub fn from_webidl(idl: &str) -> Result<Self> {
        let mut ci = Self::default();
        // There's some lifetime thing with the errors returned from weedle::Definitions::parse
        // that my own lifetime is too short to worry about figuring out; unwrap and move on.

        // Note we use `weedle::Definitions::parse` instead of `weedle::parse` so
        // on parse errors we can see how far weedle got, which helps locate the problem.
        use weedle::Parse; // this trait must be in scope for parse to work.
        let (remaining, defns) = weedle::Definitions::parse(idl.trim()).unwrap();
        if !remaining.is_empty() {
            println!("Error parsing the IDL. Text remaining to be parsed is:");
            println!("{remaining}");
            bail!("parse error");
        }
        // Unconditionally add the String type, which is used by the panic handling
        ci.types.add_known_type(&Type::String);
        // We process the WebIDL definitions in two passes.
        // First, go through and look for all the named types.
        ci.types.add_type_definitions_from(defns.as_slice())?;
        // With those names resolved, we can build a complete representation of the API.
        APIBuilder::process(&defns, &mut ci)?;

        // The following two methods will be called later anyways, but we call them here because
        // it's convenient for UDL-only tests.
        ci.check_consistency()?;
        // Now that the high-level API is settled, we can derive the low-level FFI.
        ci.derive_ffi_funcs()?;

        Ok(ci)
    }

    /// The string namespace within which this API should be presented to the caller.
    ///
    /// This string would typically be used to prefix function names in the FFI, to build
    /// a package or module name for the foreign language, etc.
    pub fn namespace(&self) -> &str {
        self.namespace.as_str()
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

    /// Get the definitions for every Callback Interface type in the interface.
    pub fn callback_interface_definitions(&self) -> &[CallbackInterface] {
        &self.callback_interfaces
    }

    /// Get a Callback interface definition by name, or None if no such interface is defined.
    pub fn get_callback_interface_definition(&self, name: &str) -> Option<&CallbackInterface> {
        // TODO: probably we could store these internally in a HashMap to make this easier?
        self.callback_interfaces.iter().find(|o| o.name == name)
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
        let used_in_callback_interface = self
            .callback_interface_definitions()
            .iter()
            .flat_map(|cb| cb.methods())
            .any(|m| m.throws_type() == Some(&e.as_type()));

        self.is_name_used_as_error(&e.name) && (fielded || used_in_callback_interface)
    }

    /// Get details about all `Type::External` types
    pub fn iter_external_types(&self) -> impl Iterator<Item = (&String, &String, ExternalKind)> {
        self.types.iter_known_types().filter_map(|t| match t {
            Type::External {
                name,
                crate_name,
                kind,
            } => Some((name, crate_name, *kind)),
            _ => None,
        })
    }

    /// Get details about all `Type::Custom` types
    pub fn iter_custom_types(&self) -> impl Iterator<Item = (&String, &Type)> {
        self.types.iter_known_types().filter_map(|t| match t {
            Type::Custom { name, builtin } => Some((name, &**builtin)),
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

    pub fn is_callback_interface_throws_type(&self, type_: Type) -> bool {
        self.callback_interface_throws_types.contains(&type_)
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
        self.iter_types_in_item(item)
            .any(|t| matches!(t, Type::Object { .. }))
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
            .any(|t| matches!(t, Type::Optional(_)))
    }

    /// Check whether the interface contains any sequence types
    pub fn contains_sequence_types(&self) -> bool {
        self.types
            .iter_known_types()
            .any(|t| matches!(t, Type::Sequence(_)))
    }

    /// Check whether the interface contains any map types
    pub fn contains_map_types(&self) -> bool {
        self.types
            .iter_known_types()
            .any(|t| matches!(t, Type::Map(_, _)))
    }

    /// The namespace to use in FFI-level function definitions.
    ///
    /// The value returned by this method is used as a prefix to namespace all UDL-defined FFI
    /// functions used in this ComponentInterface.
    pub fn ffi_namespace(&self) -> &str {
        &self.namespace
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
                type_: FfiType::Int32,
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
                    type_: FfiType::Int32,
                },
            ],
            return_type: Some(FfiType::RustBuffer(None)),
            has_rust_call_status_arg: true,
            is_object_free_function: false,
        }
    }

    /// Does this interface contain async functions?
    pub fn has_async_fns(&self) -> bool {
        self.iter_ffi_function_definitions().any(|f| f.is_async())
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

    /// List the definitions of all FFI functions in the interface.
    ///
    /// The set of FFI functions is derived automatically from the set of higher-level types
    /// along with the builtin FFI helper functions.
    pub fn iter_ffi_function_definitions(&self) -> impl Iterator<Item = FfiFunction> + '_ {
        self.iter_user_ffi_function_definitions()
            .cloned()
            .chain(self.iter_rust_buffer_ffi_function_definitions())
            .chain(self.iter_checksum_ffi_functions())
            .chain(self.ffi_foreign_executor_callback_set())
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

    /// The ffi_foreign_executor_callback_set FFI function
    ///
    /// We only include this in the FFI if the `ForeignExecutor` type is actually used
    pub fn ffi_foreign_executor_callback_set(&self) -> Option<FfiFunction> {
        if self.types.contains(&Type::ForeignExecutor) {
            Some(FfiFunction {
                name: "uniffi_foreign_executor_callback_set".into(),
                arguments: vec![FfiArgument {
                    name: "callback".into(),
                    type_: FfiType::ForeignExecutorCallback,
                }],
                return_type: None,
                is_async: false,
                has_rust_call_status_arg: false,
                is_object_free_function: false,
            })
        } else {
            None
        }
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

    /// Resolve a weedle type expression into a `Type`.
    ///
    /// This method uses the current state of our `TypeUniverse` to turn a weedle type expression
    /// into a concrete `Type` (or error if the type expression is not well defined). It abstracts
    /// away the complexity of walking weedle's type struct hierarchy by dispatching to the `TypeResolver`
    /// trait.
    fn resolve_type_expression<T: types::TypeResolver>(&mut self, expr: T) -> Result<Type> {
        self.types.resolve_type_expression(expr)
    }

    /// Resolve a weedle `ReturnType` expression into an optional `Type`.
    ///
    /// This method is similar to `resolve_type_expression`, but tailored specifically for return types.
    /// It can return `None` to represent a non-existent return value.
    fn resolve_return_type_expression(
        &mut self,
        expr: &weedle::types::ReturnType<'_>,
    ) -> Result<Option<Type>> {
        Ok(match expr {
            weedle::types::ReturnType::Undefined(_) => None,
            weedle::types::ReturnType::Type(t) => {
                // Older versions of WebIDL used `void` for functions that don't return a value,
                // while newer versions have replaced it with `undefined`. Special-case this for
                // backwards compatibility for our consumers.
                use weedle::types::{NonAnyType::Identifier, SingleType::NonAny, Type::Single};
                match t {
                    Single(NonAny(Identifier(id))) if id.type_.0 == "void" => None,
                    _ => Some(self.resolve_type_expression(t)?),
                }
            }
        })
    }

    /// Called by `APIBuilder` impls to add a newly-parsed namespace definition to the `ComponentInterface`.
    fn add_namespace_definition(&mut self, defn: Namespace) -> Result<()> {
        if !self.namespace.is_empty() {
            bail!("duplicate namespace definition");
        }
        self.namespace = defn.name;
        Ok(())
    }

    /// Called by `APIBuilder` impls to add a newly-parsed enum definition to the `ComponentInterface`.
    pub(super) fn add_enum_definition(&mut self, defn: Enum) -> Result<()> {
        match self.enums.entry(defn.name().to_owned()) {
            Entry::Vacant(v) => {
                for variant in defn.variants() {
                    for field in variant.fields() {
                        self.types.add_known_type(&field.as_type());
                    }
                }
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

    /// Called by `APIBuilder` impls to add a newly-parsed record definition to the `ComponentInterface`.
    pub(super) fn add_record_definition(&mut self, defn: Record) -> Result<()> {
        match self.records.entry(defn.name().to_owned()) {
            Entry::Vacant(v) => {
                for field in defn.fields() {
                    self.types.add_known_type(&field.as_type());
                }
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
        for arg in &defn.arguments {
            self.types.add_known_type(&arg.type_);
        }
        if let Some(ty) = &defn.return_type {
            self.types.add_known_type(ty);
        }

        // Since functions are not a first-class type, we have to check for duplicates here
        // rather than relying on the type-finding pass to catch them.
        if self.functions.iter().any(|f| f.name == defn.name) {
            bail!("duplicate function definition: \"{}\"", defn.name);
        }
        if !matches!(self.types.get_type_definition(defn.name()), None) {
            bail!("Conflicting type definition for \"{}\"", defn.name());
        }
        if defn.is_async() {
            // Async functions depend on the foreign executor
            self.types.add_known_type(&Type::ForeignExecutor);
        }
        self.functions.push(defn);

        Ok(())
    }

    pub(super) fn add_constructor_meta(&mut self, meta: ConstructorMetadata) -> Result<()> {
        let object = get_object(&mut self.objects, &meta.self_name)
            .ok_or_else(|| anyhow!("add_constructor_meta: object {} not found", &meta.self_name))?;
        let defn: Constructor = meta.into();

        for arg in &defn.arguments {
            self.types.add_known_type(&arg.type_);
        }
        object.constructors.push(defn);

        Ok(())
    }

    pub(super) fn add_method_meta(&mut self, meta: impl Into<Method>) -> Result<()> {
        let defn: Method = meta.into();
        let object = get_object(&mut self.objects, &defn.object_name)
            .ok_or_else(|| anyhow!("add_method_meta: object {} not found", &defn.object_name))?;

        for arg in &defn.arguments {
            self.types.add_known_type(&arg.type_);
        }
        if let Some(ty) = &defn.return_type {
            self.types.add_known_type(ty);
        }
        if defn.is_async() {
            // Async functions depend on the foreign executor
            self.types.add_known_type(&Type::ForeignExecutor);
        }
        object.methods.push(defn);

        Ok(())
    }

    pub(super) fn add_object_meta(&mut self, meta: ObjectMetadata) {
        let free_name = meta.free_ffi_symbol_name();
        let mut obj = Object::new(meta.name, ObjectImpl::from_is_trait(meta.is_trait));
        obj.ffi_func_free.name = free_name;
        self.types.add_known_type(&obj.as_type());
        self.add_object_definition(obj);
    }

    /// Called by `APIBuilder` impls to add a newly-parsed object definition to the `ComponentInterface`.
    fn add_object_definition(&mut self, defn: Object) {
        // Note that there will be no duplicates thanks to the previous type-finding pass.
        self.objects.push(defn);
    }

    pub(super) fn note_name_used_as_error(&mut self, name: &str) {
        self.errors.insert(name.to_string());
    }

    pub fn is_name_used_as_error(&self, name: &str) -> bool {
        self.errors.contains(name)
    }

    /// Called by `APIBuilder` impls to add a newly-parsed callback interface definition to the `ComponentInterface`.
    pub(super) fn add_callback_interface_definition(&mut self, defn: CallbackInterface) {
        // Note that there will be no duplicates thanks to the previous type-finding pass.
        for method in defn.methods() {
            if let Some(error) = method.throws_type() {
                self.callback_interface_throws_types.insert(error.clone());
            }
        }
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
            cbi.methods.push(meta.into());
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
        if self.namespace.is_empty() {
            bail!("missing namespace definition");
        }

        // To keep codegen tractable, enum variant names must not shadow type names.
        for e in self.enums.values() {
            // Not sure why this was deemed important for "normal" enums but
            // not those used as errors, but this is what we've always done.
            if !self.is_name_used_as_error(&e.name) {
                for variant in &e.variants {
                    if self.types.get_type_definition(variant.name()).is_some() {
                        bail!(
                            "Enum variant names must not shadow type names: \"{}\"",
                            variant.name()
                        )
                    }
                }
            }
        }

        for ty in self.iter_types() {
            match ty {
                Type::Object { name, .. } => {
                    ensure!(
                        self.objects.iter().any(|o| o.name == *name),
                        "Object `{name}` has no definition"
                    );
                }
                Type::Record(name) => {
                    ensure!(
                        self.records.contains_key(name),
                        "Record `{name}` has no definition",
                    );
                }
                Type::Enum(name) => {
                    ensure!(
                        self.enums.contains_key(name),
                        "Enum `{name}` has no definition",
                    );
                }
                _ => {}
            }
        }

        Ok(())
    }

    /// Automatically derive the low-level FFI functions from the high-level types in the interface.
    ///
    /// This should only be called after the high-level types have been completed defined, otherwise
    /// the resulting set will be missing some entries.
    pub fn derive_ffi_funcs(&mut self) -> Result<()> {
        let ci_namespace = self.ffi_namespace().to_owned();
        for func in self.functions.iter_mut() {
            func.derive_ffi_func(&ci_namespace)?;
        }
        for obj in self.objects.iter_mut() {
            obj.derive_ffi_funcs(&ci_namespace)?;
        }
        for callback in self.callback_interfaces.iter_mut() {
            callback.derive_ffi_funcs(&ci_namespace);
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
            Type::Record(nm)
            | Type::Enum(nm)
            | Type::Object { name: nm, .. }
            | Type::CallbackInterface(nm) => {
                if !self.seen.contains(nm.as_str()) {
                    self.pending.push(type_);
                    self.seen.insert(nm.as_str());
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
                Type::Record(nm) => self.ci.get_record_definition(nm).map(Record::iter_types),
                Type::Enum(name) => self.ci.get_enum_definition(name).map(Enum::iter_types),
                Type::Object { name: nm, .. } => {
                    self.ci.get_object_definition(nm).map(Object::iter_types)
                }
                Type::CallbackInterface(nm) => self
                    .ci
                    .get_callback_interface_definition(nm)
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

/// Trait to help build a `ComponentInterface` from WedIDL syntax nodes.
///
/// This trait does structural matching on the various weedle AST nodes and
/// uses them to build up the records, enums, objects etc in the provided
/// `ComponentInterface`.
trait APIBuilder {
    fn process(&self, ci: &mut ComponentInterface) -> Result<()>;
}

/// Add to a `ComponentInterface` from a list of weedle definitions,
/// by processing each in turn.
impl<T: APIBuilder> APIBuilder for Vec<T> {
    fn process(&self, ci: &mut ComponentInterface) -> Result<()> {
        for item in self {
            item.process(ci)?;
        }
        Ok(())
    }
}

/// Add to a `ComponentInterface` from a weedle definition.
/// This is conceptually the root of the parser, and dispatches to implementations
/// for the various specific WebIDL types that we support.
impl APIBuilder for weedle::Definition<'_> {
    fn process(&self, ci: &mut ComponentInterface) -> Result<()> {
        match self {
            weedle::Definition::Namespace(d) => d.process(ci)?,
            weedle::Definition::Enum(d) => {
                // We check if the enum represents an error...
                let attrs = attributes::EnumAttributes::try_from(d.attributes.as_ref())?;
                if attrs.contains_error_attr() {
                    let e = d.convert(ci)?;
                    ci.note_name_used_as_error(&e.name);
                    ci.add_enum_definition(e)?;
                } else {
                    let e = d.convert(ci)?;
                    ci.add_enum_definition(e)?;
                }
            }
            weedle::Definition::Dictionary(d) => {
                let rec = d.convert(ci)?;
                ci.add_record_definition(rec)?;
            }
            weedle::Definition::Interface(d) => {
                let attrs = attributes::InterfaceAttributes::try_from(d.attributes.as_ref())?;
                if attrs.contains_enum_attr() {
                    let e = d.convert(ci)?;
                    ci.add_enum_definition(e)?;
                } else if attrs.contains_error_attr() {
                    let e: Enum = d.convert(ci)?;
                    ci.note_name_used_as_error(&e.name);
                    ci.add_enum_definition(e)?;
                } else {
                    let obj = d.convert(ci)?;
                    ci.add_object_definition(obj);
                }
            }
            weedle::Definition::CallbackInterface(d) => {
                let obj = d.convert(ci)?;
                ci.add_callback_interface_definition(obj);
            }
            // everything needed for typedefs is done in finder.rs.
            weedle::Definition::Typedef(_) => {}
            _ => bail!("don't know how to deal with {:?}", self),
        }
        Ok(())
    }
}

/// Trait to help convert WedIDL syntax nodes into `ComponentInterface` objects.
///
/// This trait does structural matching on the various weedle AST nodes and converts
/// them into appropriate structs that we can use to build up the contents of a
/// `ComponentInterface`. It is basically the `TryFrom` trait except that the conversion
/// always happens in the context of a given `ComponentInterface`, which is used for
/// resolving e.g. type definitions.
///
/// The difference between this trait and `APIBuilder` is that `APIConverter` treats the
/// `ComponentInterface` as a read-only data source for resolving types, while `APIBuilder`
/// actually mutates the `ComponentInterface` to add new definitions.
trait APIConverter<T> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<T>;
}

/// Convert a list of weedle items into a list of `ComponentInterface` items,
/// by doing a direct item-by-item mapping.
impl<U, T: APIConverter<U>> APIConverter<Vec<U>> for Vec<T> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Vec<U>> {
        self.iter().map(|v| v.convert(ci)).collect::<Result<_>>()
    }
}

// Helpers for functions/methods/constructors which all have the same "throws" semantics.
fn throws_name(throws: &Option<Type>) -> Option<&str> {
    // Type has no `name()` method, just `canonical_name()` which isn't what we want.
    match throws {
        None => None,
        Some(Type::Enum(name)) => Some(name),
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
        let err = ComponentInterface::from_webidl(UDL).unwrap_err();
        assert_eq!(
            err.to_string(),
            "Conflicting type definition for `Testing`! \
             existing definition: Object { name: \"Testing\", imp: Struct }, \
             new definition: Record(\"Testing\")"
        );

        const UDL2: &str = r#"
            namespace test{};
            enum Testing {
                "one", "two"
            };
            [Error]
            enum Testing { "three", "four" };
        "#;
        let err = ComponentInterface::from_webidl(UDL2).unwrap_err();
        assert_eq!(
            err.to_string(),
            "Mismatching definition for enum `Testing`!\nexisting definition: Enum {
    name: \"Testing\",
    variants: [
        Variant {
            name: \"one\",
            fields: [],
        },
        Variant {
            name: \"two\",
            fields: [],
        },
    ],
    flat: true,
},
new definition: Enum {
    name: \"Testing\",
    variants: [
        Variant {
            name: \"three\",
            fields: [],
        },
        Variant {
            name: \"four\",
            fields: [],
        },
    ],
    flat: true,
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
        let err = ComponentInterface::from_webidl(UDL3).unwrap_err();
        assert_eq!(
            err.to_string(),
            "Conflicting type definition for \"Testing\""
        );
    }

    #[test]
    fn test_enum_variant_names_dont_shadow_types() {
        // There are some edge-cases during codegen where we don't know how to disambiguate
        // between an enum variant reference and a top-level type reference, so we
        // disallow it in order to give a more scrutable error to the consumer.
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                constructor();
            };
            [Enum]
            interface HardToCodegenFor {
                Testing();
                OtherVariant(u32 field);
            };
        "#;
        let err = ComponentInterface::from_webidl(UDL).unwrap_err();
        assert_eq!(
            err.to_string(),
            "Enum variant names must not shadow type names: \"Testing\""
        );
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
            .add_type_definition("TestOptional{}", Type::Optional(Box::new(Type::String)))
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
            .add_type_definition("TestSequence{}", Type::Sequence(Box::new(Type::UInt64)))
            .is_ok());
        assert!(ci.contains_sequence_types());
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
            .add_type_definition(
                "Map{}",
                Type::Map(Box::new(Type::String), Box::new(Type::Boolean))
            )
            .is_ok());
        assert!(ci.contains_map_types());
    }

    #[test]
    fn test_no_infinite_recursion_when_walking_types() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                void tester(Testing foo);
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert!(!ci.item_contains_unsigned_types(&Type::Object {
            name: "Testing".into(),
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
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert!(ci.item_contains_unsigned_types(&Type::Object {
            name: "TestObj".into(),
            imp: ObjectImpl::Struct,
        }));
    }
}
