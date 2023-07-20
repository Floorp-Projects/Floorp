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
    collections::{btree_map::Entry, BTreeMap, HashSet},
    convert::TryFrom,
    iter,
};

use anyhow::{bail, ensure, Result};

pub mod types;
pub use types::Type;
use types::{TypeIterator, TypeUniverse};

mod attributes;
mod callbacks;
pub use callbacks::CallbackInterface;
mod enum_;
pub use enum_::Enum;
mod error;
pub use error::Error;
mod function;
pub use function::{Argument, Function};
mod literal;
pub use literal::{Literal, Radix};
mod namespace;
pub use namespace::Namespace;
mod object;
pub use object::{Constructor, Method, Object};
mod record;
pub use record::{Field, Record};

pub mod ffi;
pub use ffi::{FfiArgument, FfiFunction, FfiType};
use uniffi_meta::{Checksum, FnMetadata, MethodMetadata, ObjectMetadata};

/// This needs to match the major/minor version of the `uniffi` crate.  See
/// `docs/uniffi-versioning.md` for details.
///
/// Once we get to 1.0, then we should reformat this to only include the major version number.
const UNIFFI_CONTRACT_VERSION: &str = "0.22";

/// The main public interface for this module, representing the complete details of an interface exposed
/// by a rust component and the details of consuming it via an extern-C FFI layer.
///
#[derive(Debug, Default, Checksum)]
pub struct ComponentInterface {
    /// This always points to `UNIFFI_CONTRACT_VERSION`.  By including it in the checksum, we
    /// prevent consumers from combining scaffolding and bindings that were created with different
    /// `uniffi` versions.
    uniffi_version: &'static str,
    /// All of the types used in the interface.
    // We can't checksum `self.types`, but its contents are implied by the other fields
    // anyway, so it's safe to ignore it.
    #[checksum_ignore]
    pub(super) types: TypeUniverse,
    /// The unique prefix that we'll use for namespacing when exposing this component's API.
    namespace: String,
    /// The internal unique prefix used to namespace FFI symbols
    #[checksum_ignore]
    ffi_namespace: String,
    /// The high-level API provided by the component.
    enums: BTreeMap<String, Enum>,
    records: BTreeMap<String, Record>,
    functions: Vec<Function>,
    objects: Vec<Object>,
    callback_interfaces: Vec<CallbackInterface>,
    errors: BTreeMap<String, Error>,
}

impl ComponentInterface {
    /// Parse a `ComponentInterface` from a string containing a WebIDL definition.
    pub fn from_webidl(idl: &str) -> Result<Self> {
        let mut ci = Self {
            uniffi_version: UNIFFI_CONTRACT_VERSION,
            ..Default::default()
        };
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
        ci.types.add_known_type(&Type::String)?;
        // We process the WebIDL definitions in two passes.
        // First, go through and look for all the named types.
        ci.types.add_type_definitions_from(defns.as_slice())?;
        // With those names resolved, we can build a complete representation of the API.
        APIBuilder::process(&defns, &mut ci)?;

        // The FFI namespace must not be computed on the fly because it could otherwise be
        // influenced by things added later from proc-macro metadata. Those have their own
        // namespacing mechanism.
        assert!(!ci.namespace.is_empty());
        ci.ffi_namespace = format!("{}_{:x}", ci.namespace, ci.checksum());

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

    /// Get the definitions for every Error type in the interface.
    pub fn error_definitions(&self) -> impl Iterator<Item = &Error> {
        self.errors.values()
    }

    /// Get an Error definition by name, or None if no such Error is defined.
    pub fn get_error_definition(&self, name: &str) -> Option<&Error> {
        self.errors.get(name)
    }

    /// Should we generate read (and lift) functions for errors?
    ///
    /// This is a workaround for the fact that lower/write can't be generated for some errors,
    /// specifically errors that are defined as flat in the UDL, but actually have fields in the
    /// Rust source.
    pub fn should_generate_error_read(&self, error: &Error) -> bool {
        // We can and should always generate read() methods for fielded errors
        let fielded = !error.is_flat();
        // For flat errors, we should only generate read() methods if we need them to support
        // callback interface errors
        let used_in_callback_interface = self
            .callback_interface_definitions()
            .iter()
            .flat_map(|cb| cb.methods())
            .any(|m| m.throws_type() == Some(error.type_()));

        fielded || used_in_callback_interface
    }

    /// Get details about all `Type::External` types
    pub fn iter_external_types(&self) -> impl Iterator<Item = (&String, &String)> {
        self.types.iter_known_types().filter_map(|t| match t {
            Type::External { name, crate_name } => Some((name, crate_name)),
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
            .any(|t| matches!(t, Type::Object(_)))
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

    /// Calculate a numeric checksum for this ComponentInterface.
    ///
    /// The checksum can be used to guard against accidentally using foreign-language bindings
    /// generated from one version of an interface with the compiled Rust code from a different
    /// version of that interface. It offers the following properties:
    ///
    ///   - Two ComponentIntefaces generated from the same WebIDL file, using the same version of uniffi
    ///     and the same version of Rust, will always have the same checksum value.
    ///   - Two ComponentInterfaces will, with high probability, have different checksum values if:
    ///         - They were generated from two different WebIDL files.
    ///         - They were generated by two different versions of uniffi
    ///
    /// Note that this is designed to prevent accidents, not attacks, so there is no need for the
    /// checksum to be cryptographically secure.
    pub fn checksum(&self) -> u16 {
        uniffi_meta::checksum(self)
    }

    /// The namespace to use in FFI-level function definitions.
    ///
    /// The value returned by this method is used as a prefix to namespace all UDL-defined FFI
    /// functions used in this ComponentInterface.
    ///
    /// Since these names are an internal implementation detail that is not typically visible to
    /// consumers, we take the opportunity to add an additional safety guard by including a 4-hex-char
    /// checksum in each name. If foreign-language bindings attempt to load and use a version of the
    /// Rust code compiled from a different UDL definition than the one used for the bindings themselves,
    /// then there is a high probability of checksum mismatch and they will fail to link against the
    /// compiled Rust code. The result will be an ugly inscrutable link-time error, but that is a lot
    /// better than triggering potentially arbitrary memory unsafety!
    pub fn ffi_namespace(&self) -> &str {
        assert!(!self.ffi_namespace.is_empty());
        &self.ffi_namespace
    }

    /// Builtin FFI function for allocating a new `RustBuffer`.
    /// This is needed so that the foreign language bindings can create buffers in which to pass
    /// complex data types across the FFI.
    pub fn ffi_rustbuffer_alloc(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_rustbuffer_alloc", self.ffi_namespace()),
            arguments: vec![FfiArgument {
                name: "size".to_string(),
                type_: FfiType::Int32,
            }],
            return_type: Some(FfiType::RustBuffer(None)),
        }
    }

    /// Builtin FFI function for copying foreign-owned bytes
    /// This is needed so that the foreign language bindings can create buffers in which to pass
    /// complex data types across the FFI.
    pub fn ffi_rustbuffer_from_bytes(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_rustbuffer_from_bytes", self.ffi_namespace()),
            arguments: vec![FfiArgument {
                name: "bytes".to_string(),
                type_: FfiType::ForeignBytes,
            }],
            return_type: Some(FfiType::RustBuffer(None)),
        }
    }

    /// Builtin FFI function for freeing a `RustBuffer`.
    /// This is needed so that the foreign language bindings can free buffers in which they received
    /// complex data types returned across the FFI.
    pub fn ffi_rustbuffer_free(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_rustbuffer_free", self.ffi_namespace()),
            arguments: vec![FfiArgument {
                name: "buf".to_string(),
                type_: FfiType::RustBuffer(None),
            }],
            return_type: None,
        }
    }

    /// Builtin FFI function for reserving extra space in a `RustBuffer`.
    /// This is needed so that the foreign language bindings can grow buffers used for passing
    /// complex data types across the FFI.
    pub fn ffi_rustbuffer_reserve(&self) -> FfiFunction {
        FfiFunction {
            name: format!("ffi_{}_rustbuffer_reserve", self.ffi_namespace()),
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
        }
    }

    /// List the definitions of all FFI functions in the interface.
    ///
    /// The set of FFI functions is derived automatically from the set of higher-level types
    /// along with the builtin FFI helper functions.
    pub fn iter_ffi_function_definitions(&self) -> impl Iterator<Item = FfiFunction> + '_ {
        self.iter_user_ffi_function_definitions()
            .cloned()
            .chain(self.iter_rust_buffer_ffi_function_definitions())
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

    /// List all FFI functions definitions for RustBuffer functionality
    pub fn iter_rust_buffer_ffi_function_definitions(&self) -> impl Iterator<Item = FfiFunction> {
        [
            self.ffi_rustbuffer_alloc(),
            self.ffi_rustbuffer_from_bytes(),
            self.ffi_rustbuffer_free(),
            self.ffi_rustbuffer_reserve(),
        ]
        .into_iter()
    }

    //
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

    fn add_function_impl(&mut self, defn: Function) -> Result<()> {
        // Since functions are not a first-class type, we have to check for duplicates here
        // rather than relying on the type-finding pass to catch them.
        if self.functions.iter().any(|f| f.name == defn.name) {
            bail!("duplicate function definition: \"{}\"", defn.name);
        }
        if !matches!(self.types.get_type_definition(defn.name()), None) {
            bail!("Conflicting type definition for \"{}\"", defn.name());
        }
        self.functions.push(defn);

        Ok(())
    }

    /// Called by `APIBuilder` impls to add a newly-parsed function definition to the `ComponentInterface`.
    fn add_function_definition(&mut self, defn: Function) -> Result<()> {
        for arg in &defn.arguments {
            self.types.add_known_type(&arg.type_)?;
        }
        if let Some(ty) = &defn.return_type {
            self.types.add_known_type(ty)?;
        }

        self.add_function_impl(defn)
    }

    pub(super) fn add_fn_meta(&mut self, meta: FnMetadata) -> Result<()> {
        self.add_function_impl(meta.into())
    }

    pub(super) fn add_method_meta(&mut self, meta: MethodMetadata) {
        let object = get_or_insert_object(&mut self.objects, &meta.self_name);
        let defn: Method = meta.into();
        object.methods.push(defn);
    }

    pub(super) fn add_object_free_fn(&mut self, meta: ObjectMetadata) {
        let object = get_or_insert_object(&mut self.objects, &meta.name);
        object.ffi_func_free.name = meta.free_ffi_symbol_name();
    }

    /// Called by `APIBuilder` impls to add a newly-parsed object definition to the `ComponentInterface`.
    fn add_object_definition(&mut self, defn: Object) {
        // Note that there will be no duplicates thanks to the previous type-finding pass.
        self.objects.push(defn);
    }

    /// Called by `APIBuilder` impls to add a newly-parsed callback interface definition to the `ComponentInterface`.
    fn add_callback_interface_definition(&mut self, defn: CallbackInterface) {
        // Note that there will be no duplicates thanks to the previous type-finding pass.
        self.callback_interfaces.push(defn);
    }

    /// Called by `APIBuilder` impls to add a newly-parsed error definition to the `ComponentInterface`.
    pub(super) fn add_error_definition(&mut self, defn: Error) -> Result<()> {
        match self.errors.entry(defn.name().to_owned()) {
            Entry::Vacant(v) => {
                v.insert(defn);
            }
            Entry::Occupied(o) => {
                let existing_def = o.get();
                if defn != *existing_def {
                    bail!(
                        "Mismatching definition for error `{}`!\n\
                         existing definition: {existing_def:#?},\n\
                         new definition: {defn:#?}",
                        defn.name(),
                    );
                }
            }
        }

        Ok(())
    }

    /// Resolve unresolved types within proc-macro function / method signatures.
    pub fn resolve_types(&mut self) -> Result<()> {
        fn handle_unresolved_in(
            ty: &mut Type,
            f: impl Fn(&str) -> Result<Type> + Clone,
        ) -> Result<()> {
            match ty {
                Type::Unresolved { name } => {
                    *ty = f(name)?;
                }
                Type::Optional(inner) => {
                    handle_unresolved_in(inner, f)?;
                }
                Type::Sequence(inner) => {
                    handle_unresolved_in(inner, f)?;
                }
                Type::Map(k, v) => {
                    handle_unresolved_in(k, f.clone())?;
                    handle_unresolved_in(v, f)?;
                }
                _ => {}
            }

            Ok(())
        }

        let fn_sig_types = self.functions.iter_mut().flat_map(|fun| {
            fun.arguments
                .iter_mut()
                .map(|arg| &mut arg.type_)
                .chain(&mut fun.return_type)
        });
        let method_sig_types = self.objects.iter_mut().flat_map(|obj| {
            obj.methods.iter_mut().flat_map(|m| {
                m.arguments
                    .iter_mut()
                    .map(|arg| &mut arg.type_)
                    .chain(&mut m.return_type)
            })
        });

        let record_fields_types = self
            .records
            .values_mut()
            .flat_map(|r| r.fields.iter_mut().map(|f| &mut f.type_));
        let enum_fields_types = self.enums.values_mut().flat_map(|e| {
            e.variants
                .iter_mut()
                .flat_map(|r| r.fields.iter_mut().map(|f| &mut f.type_))
        });

        let possibly_unresolved_types = fn_sig_types
            .chain(method_sig_types)
            .chain(record_fields_types)
            .chain(enum_fields_types);

        for ty in possibly_unresolved_types {
            handle_unresolved_in(ty, |unresolved_ty_name| {
                match self.types.get_type_definition(unresolved_ty_name) {
                    Some(def) => {
                        assert!(
                            !matches!(&def, Type::Unresolved { .. }),
                            "unresolved types must not be part of TypeUniverse"
                        );
                        Ok(def)
                    }
                    None => bail!("Failed to resolve type `{unresolved_ty_name}`"),
                }
            })?;

            // The proc-macro scanning metadata code doesn't add known types
            // when they could contain unresolved types, so we have to do it
            // here after replacing unresolveds.
            self.types.add_known_type(ty)?;
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
            for variant in &e.variants {
                if self.types.get_type_definition(variant.name()).is_some() {
                    bail!(
                        "Enum variant names must not shadow type names: \"{}\"",
                        variant.name()
                    )
                }
            }
        }

        for ty in self.iter_types() {
            match ty {
                Type::Object(name) => {
                    ensure!(
                        self.objects.iter().any(|o| o.name == *name),
                        "Object `{name}` has no definition",
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
                Type::Unresolved { name } => {
                    bail!("Type `{name}` should be resolved at this point");
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
        let ci_prefix = self.ffi_namespace().to_owned();
        for func in self.functions.iter_mut() {
            func.derive_ffi_func(&ci_prefix)?;
        }
        for obj in self.objects.iter_mut() {
            obj.derive_ffi_funcs(&ci_prefix)?;
        }
        for callback in self.callback_interfaces.iter_mut() {
            callback.derive_ffi_funcs(&ci_prefix);
        }
        Ok(())
    }
}

fn get_or_insert_object<'a>(objects: &'a mut Vec<Object>, name: &str) -> &'a mut Object {
    // The find-based way of writing this currently runs into a borrow checker
    // error, so we use position
    match objects.iter_mut().position(|o| o.name == name) {
        Some(idx) => &mut objects[idx],
        None => {
            objects.push(Object::new(name.to_owned()));
            objects.last_mut().unwrap()
        }
    }
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
            | Type::Error(nm)
            | Type::Object(nm)
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
                Type::Enum(nm) => self.ci.get_enum_definition(nm).map(Enum::iter_types),
                Type::Error(nm) => self.ci.get_error_definition(nm).map(Error::iter_types),
                Type::Object(nm) => self.ci.get_object_definition(nm).map(Object::iter_types),
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
                    let err = d.convert(ci)?;
                    ci.add_error_definition(err)?;
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
                    let e = d.convert(ci)?;
                    ci.add_error_definition(e)?;
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

fn convert_type(s: &uniffi_meta::Type) -> Type {
    use uniffi_meta::Type as Ty;

    match s {
        Ty::U8 => Type::UInt8,
        Ty::U16 => Type::UInt16,
        Ty::U32 => Type::UInt32,
        Ty::U64 => Type::UInt64,
        Ty::I8 => Type::Int8,
        Ty::I16 => Type::Int16,
        Ty::I32 => Type::Int32,
        Ty::I64 => Type::Int64,
        Ty::F32 => Type::Float32,
        Ty::F64 => Type::Float64,
        Ty::Bool => Type::Boolean,
        Ty::String => Type::String,
        Ty::Option { inner_type } => Type::Optional(convert_type(inner_type).into()),
        Ty::Vec { inner_type } => Type::Sequence(convert_type(inner_type).into()),
        Ty::HashMap {
            key_type,
            value_type,
        } => Type::Map(
            convert_type(key_type).into(),
            convert_type(value_type).into(),
        ),
        Ty::ArcObject { object_name } => Type::Object(object_name.clone()),
        Ty::Unresolved { name } => Type::Unresolved { name: name.clone() },
    }
}

#[cfg(test)]
mod test {
    use super::*;

    // Note that much of the functionality of `ComponentInterface` is tested via its interactions
    // with specific member types, in the sub-modules defining those member types.

    const UDL1: &str = r#"
        namespace foobar{};
        enum Test {
            "test_me",
        };
    "#;

    const UDL2: &str = r#"
        namespace hello {
            u64 world();
        };
        dictionary Test {
            boolean me;
        };
    "#;

    #[test]
    fn test_checksum_always_matches_for_same_webidl() {
        for udl in &[UDL1, UDL2] {
            let ci1 = ComponentInterface::from_webidl(udl).unwrap();
            let ci2 = ComponentInterface::from_webidl(udl).unwrap();
            assert_eq!(ci1.checksum(), ci2.checksum());
        }
    }

    #[test]
    fn test_checksum_differs_for_different_webidl() {
        // There is a small probability of this test spuriously failing due to hash collision.
        // If it happens often enough to be a problem, probably this whole "checksum" thing
        // is not working out as intended.
        let ci1 = ComponentInterface::from_webidl(UDL1).unwrap();
        let ci2 = ComponentInterface::from_webidl(UDL2).unwrap();
        assert_ne!(ci1.checksum(), ci2.checksum());
    }

    #[test]
    fn test_checksum_differs_for_different_uniffi_version() {
        // There is a small probability of this test spuriously failing due to hash collision.
        // If it happens often enough to be a problem, probably this whole "checksum" thing
        // is not working out as intended.
        for udl in &[UDL1, UDL2] {
            let ci1 = ComponentInterface::from_webidl(udl).unwrap();
            let mut ci2 = ComponentInterface::from_webidl(udl).unwrap();
            ci2.uniffi_version = "99.99";
            assert_ne!(ci1.checksum(), ci2.checksum());
        }
    }

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
             existing definition: Object(\"Testing\"), \
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
            "Conflicting type definition for `Testing`! \
             existing definition: Enum(\"Testing\"), \
             new definition: Error(\"Testing\")"
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
        assert!(!ci.item_contains_unsigned_types(&Type::Object("Testing".into())));
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
        assert!(ci.item_contains_unsigned_types(&Type::Object("TestObj".into())));
    }
}
