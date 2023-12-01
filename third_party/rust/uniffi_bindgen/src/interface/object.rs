/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Object definitions for a `ComponentInterface`.
//!
//! This module converts "interface" definitions from UDL into [`Object`] structures
//! that can be added to a `ComponentInterface`, which are the main way we define stateful
//! objects with behaviour for a UniFFI Rust Component. An [`Object`] is an opaque handle
//! to some state on which methods can be invoked.
//!
//! (The terminology mismatch between "interface" and "object" is a historical artifact of
//! this tool prior to committing to WebIDL syntax).
//!
//! A declaration in the UDL like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! interface Example {
//!   constructor(string? name);
//!   string my_name();
//! };
//! # "##)?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in an [`Object`] member with one [`Constructor`] and one [`Method`] being added
//! to the resulting [`ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # interface Example {
//! #   constructor(string? name);
//! #   string my_name();
//! # };
//! # "##)?;
//! let obj = ci.get_object_definition("Example").unwrap();
//! assert_eq!(obj.name(), "Example");
//! assert_eq!(obj.constructors().len(), 1);
//! assert_eq!(obj.constructors()[0].arguments()[0].name(), "name");
//! assert_eq!(obj.methods().len(),1 );
//! assert_eq!(obj.methods()[0].name(), "my_name");
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! It's not necessary for all interfaces to have constructors.
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # interface Example {};
//! # "##)?;
//! let obj = ci.get_object_definition("Example").unwrap();
//! assert_eq!(obj.name(), "Example");
//! assert_eq!(obj.constructors().len(), 0);
//! # Ok::<(), anyhow::Error>(())
//! ```

use std::convert::TryFrom;
use std::{collections::HashSet, iter};

use anyhow::{bail, Result};
use uniffi_meta::Checksum;

use super::attributes::{ConstructorAttributes, InterfaceAttributes, MethodAttributes};
use super::ffi::{FfiArgument, FfiFunction, FfiType};
use super::function::{Argument, Callable};
use super::types::{ObjectImpl, Type, TypeIterator};
use super::{APIConverter, AsType, ComponentInterface};

/// An "object" is an opaque type that is passed around by reference, can
/// have methods called on it, and so on - basically your classic Object Oriented Programming
/// type of deal, except without elaborate inheritance hierarchies. Some can be instantiated.
///
/// In UDL these correspond to the `interface` keyword.
///
/// At the FFI layer, objects are represented by an opaque integer handle and a set of functions
/// a common prefix. The object's constructors are functions that return new objects by handle,
/// and its methods are functions that take a handle as first argument. The foreign language
/// binding code is expected to stitch these functions back together into an appropriate class
/// definition (or that language's equivalent thereof).
///
/// TODO:
///  - maybe "Class" would be a better name than "Object" here?
#[derive(Debug, Clone, Checksum)]
pub struct Object {
    pub(super) name: String,
    /// How this object is implemented in Rust
    pub(super) imp: ObjectImpl,
    pub(super) constructors: Vec<Constructor>,
    pub(super) methods: Vec<Method>,
    // The "trait" methods - they have a (presumably "well known") name, and
    // a regular method (albeit with a generated name)
    // XXX - this should really be a HashSet, but not enough transient types support hash to make it worthwhile now.
    pub(super) uniffi_traits: Vec<UniffiTrait>,
    // We don't include the FfiFunc in the hash calculation, because:
    //  - it is entirely determined by the other fields,
    //    so excluding it is safe.
    //  - its `name` property includes a checksum derived from  the very
    //    hash value we're trying to calculate here, so excluding it
    //    avoids a weird circular dependency in the calculation.
    #[checksum_ignore]
    pub(super) ffi_func_free: FfiFunction,
}

impl Object {
    pub(super) fn new(name: String, imp: ObjectImpl) -> Self {
        Self {
            name,
            imp,
            constructors: Default::default(),
            methods: Default::default(),
            uniffi_traits: Default::default(),
            ffi_func_free: Default::default(),
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    /// Returns the fully qualified name that should be used by Rust code for this object.
    /// Includes `r#`, traits get a leading `dyn`. If we ever supported associated types, then
    /// this would also include them.
    pub fn rust_name(&self) -> String {
        self.imp.rust_name_for(&self.name)
    }

    pub fn imp(&self) -> &ObjectImpl {
        &self.imp
    }

    pub fn constructors(&self) -> Vec<&Constructor> {
        self.constructors.iter().collect()
    }

    pub fn primary_constructor(&self) -> Option<&Constructor> {
        self.constructors
            .iter()
            .find(|cons| cons.is_primary_constructor())
    }

    pub fn alternate_constructors(&self) -> Vec<&Constructor> {
        self.constructors
            .iter()
            .filter(|cons| !cons.is_primary_constructor())
            .collect()
    }

    pub fn methods(&self) -> Vec<&Method> {
        self.methods.iter().collect()
    }

    pub fn get_method(&self, name: &str) -> Method {
        let matches: Vec<_> = self.methods.iter().filter(|m| m.name() == name).collect();
        match matches.len() {
            1 => matches[0].clone(),
            n => panic!("{n} methods named {name}"),
        }
    }

    pub fn uniffi_traits(&self) -> Vec<&UniffiTrait> {
        self.uniffi_traits.iter().collect()
    }

    pub fn ffi_object_free(&self) -> &FfiFunction {
        &self.ffi_func_free
    }

    pub fn iter_ffi_function_definitions(&self) -> impl Iterator<Item = &FfiFunction> {
        iter::once(&self.ffi_func_free)
            .chain(self.constructors.iter().map(|f| &f.ffi_func))
            .chain(self.methods.iter().map(|f| &f.ffi_func))
            .chain(
                self.uniffi_traits
                    .iter()
                    .flat_map(|ut| match ut {
                        UniffiTrait::Display { fmt: m }
                        | UniffiTrait::Debug { fmt: m }
                        | UniffiTrait::Hash { hash: m } => vec![m],
                        UniffiTrait::Eq { eq, ne } => vec![eq, ne],
                    })
                    .map(|m| &m.ffi_func),
            )
    }

    pub fn derive_ffi_funcs(&mut self, ci_namespace: &str) -> Result<()> {
        // The name is already set if the function is defined through a proc-macro invocation
        // rather than in UDL. Don't overwrite it in that case.
        if self.ffi_func_free.name().is_empty() {
            self.ffi_func_free.name = uniffi_meta::free_fn_symbol_name(ci_namespace, &self.name);
        }
        self.ffi_func_free.arguments = vec![FfiArgument {
            name: "ptr".to_string(),
            type_: FfiType::RustArcPtr(self.name.to_string()),
        }];
        self.ffi_func_free.return_type = None;
        self.ffi_func_free.is_object_free_function = true;

        for cons in self.constructors.iter_mut() {
            cons.derive_ffi_func(ci_namespace, &self.name);
        }
        for meth in self.methods.iter_mut() {
            meth.derive_ffi_func(ci_namespace, &self.name)?;
        }
        for ut in self.uniffi_traits.iter_mut() {
            ut.derive_ffi_func(ci_namespace, &self.name)?;
        }

        Ok(())
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(
            self.methods
                .iter()
                .map(Method::iter_types)
                .chain(self.uniffi_traits.iter().map(UniffiTrait::iter_types))
                .chain(self.constructors.iter().map(Constructor::iter_types))
                .flatten(),
        )
    }
}

impl APIConverter<Object> for weedle::InterfaceDefinition<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Object> {
        if self.inheritance.is_some() {
            bail!("interface inheritance is not supported");
        }
        let attributes = match &self.attributes {
            Some(attrs) => InterfaceAttributes::try_from(attrs)?,
            None => Default::default(),
        };

        let name = self.identifier.0;
        let object_impl = attributes.object_impl();

        let mut object = Object::new(name.to_string(), object_impl);
        // Convert each member into a constructor or method, guarding against duplicate names.
        let mut member_names = HashSet::new();
        for member in &self.members.body {
            match member {
                weedle::interface::InterfaceMember::Constructor(t) => {
                    let mut cons: Constructor = t.convert(ci)?;
                    if object_impl == ObjectImpl::Trait {
                        bail!(
                            "Trait interfaces can not have constructors: \"{}\"",
                            cons.name()
                        )
                    }
                    if !member_names.insert(cons.name.clone()) {
                        bail!("Duplicate interface member name: \"{}\"", cons.name())
                    }
                    cons.set_object_name(ci.namespace(), object.name.clone());
                    object.constructors.push(cons);
                }
                weedle::interface::InterfaceMember::Operation(t) => {
                    let mut method: Method = t.convert(ci)?;
                    if !member_names.insert(method.name.clone()) {
                        bail!("Duplicate interface member name: \"{}\"", method.name())
                    }
                    method.set_object_info(ci.namespace(), &object);
                    object.methods.push(method);
                }
                _ => bail!("no support for interface member type {:?} yet", member),
            }
        }
        // A helper for our trait methods
        let mut make_trait_method =
            |name: &str, arguments: Vec<Argument>, return_type: Option<Type>| -> Result<Method> {
                // need to add known types as they aren't explicitly referenced in
                // the UDL
                if let Some(ref return_type) = return_type {
                    ci.types.add_known_type(return_type);
                }
                for arg in &arguments {
                    ci.types.add_known_type(&arg.type_);
                }
                Ok(Method {
                    // The name is used to create the ffi function for the method.
                    name: name.to_string(),
                    object_name: object.name.clone(),
                    checksum_fn_name: Default::default(), // gets filled in later.
                    is_async: false,
                    object_impl,
                    arguments,
                    return_type,
                    ffi_func: Default::default(),
                    throws: None,
                    takes_self_by_arc: false,
                    checksum_override: None,
                })
            };
        // synthesize the trait methods.
        for trait_name in attributes.get_traits() {
            let trait_method = match trait_name.as_str() {
                "Debug" => UniffiTrait::Debug {
                    fmt: make_trait_method("uniffi_trait_debug", vec![], Some(Type::String))?,
                },
                "Display" => UniffiTrait::Display {
                    fmt: make_trait_method("uniffi_trait_display", vec![], Some(Type::String))?,
                },
                "Eq" => UniffiTrait::Eq {
                    eq: make_trait_method(
                        "uniffi_trait_eq_eq",
                        vec![Argument {
                            name: "other".to_string(),
                            type_: Type::Object {
                                name: object.name().to_string(),
                                imp: object_impl,
                            },
                            by_ref: true,
                            default: None,
                            optional: false,
                        }],
                        Some(Type::Boolean),
                    )?,
                    ne: make_trait_method(
                        "uniffi_trait_eq_ne",
                        vec![Argument {
                            name: "other".to_string(),
                            type_: Type::Object {
                                name: object.name().to_string(),
                                imp: object_impl,
                            },
                            by_ref: true,
                            default: None,
                            optional: false,
                        }],
                        Some(Type::Boolean),
                    )?,
                },
                "Hash" => UniffiTrait::Hash {
                    hash: make_trait_method("uniffi_trait_hash", vec![], Some(Type::UInt64))?,
                },
                _ => bail!("Invalid trait name: {}", trait_name),
            };
            object.uniffi_traits.push(trait_method);
        }
        Ok(object)
    }
}

impl AsType for Object {
    fn as_type(&self) -> Type {
        Type::Object {
            name: self.name.clone(),
            imp: self.imp,
        }
    }
}

// Represents a constructor for an object type.
//
// In the FFI, this will be a function that returns a pointer to an instance
// of the corresponding object type.
#[derive(Debug, Clone, Checksum)]
pub struct Constructor {
    pub(super) name: String,
    pub(super) object_name: String,
    pub(super) arguments: Vec<Argument>,
    // We don't include the FFIFunc in the hash calculation, because:
    //  - it is entirely determined by the other fields,
    //    so excluding it is safe.
    //  - its `name` property includes a checksum derived from  the very
    //    hash value we're trying to calculate here, so excluding it
    //    avoids a weird circular dependency in the calculation.
    #[checksum_ignore]
    pub(super) ffi_func: FfiFunction,
    pub(super) throws: Option<Type>,
    pub(super) checksum_fn_name: String,
    // Force a checksum value.  This is used for functions from the proc-macro code, which uses a
    // different checksum method.
    #[checksum_ignore]
    pub(super) checksum_override: Option<u16>,
}

impl Constructor {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn arguments(&self) -> Vec<&Argument> {
        self.arguments.iter().collect()
    }

    pub fn full_arguments(&self) -> Vec<Argument> {
        self.arguments.to_vec()
    }

    pub fn ffi_func(&self) -> &FfiFunction {
        &self.ffi_func
    }

    pub fn checksum_fn_name(&self) -> &str {
        &self.checksum_fn_name
    }

    pub fn checksum(&self) -> u16 {
        self.checksum_override
            .unwrap_or_else(|| uniffi_meta::checksum(self))
    }

    pub fn throws(&self) -> bool {
        self.throws.is_some()
    }

    pub fn throws_name(&self) -> Option<&str> {
        super::throws_name(&self.throws)
    }

    pub fn throws_type(&self) -> Option<&Type> {
        self.throws.as_ref()
    }

    pub fn is_primary_constructor(&self) -> bool {
        self.name == "new"
    }

    fn derive_ffi_func(&mut self, ci_namespace: &str, obj_name: &str) {
        // The name is already set if the function is defined through a proc-macro invocation
        // rather than in UDL. Don't overwrite it in that case.
        if self.ffi_func.name.is_empty() {
            self.ffi_func.name =
                uniffi_meta::constructor_symbol_name(ci_namespace, obj_name, &self.name);
        }

        self.ffi_func.arguments = self.arguments.iter().map(Into::into).collect();
        self.ffi_func.return_type = Some(FfiType::RustArcPtr(obj_name.to_string()));
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.arguments.iter().flat_map(Argument::iter_types))
    }

    fn set_object_name(&mut self, ci_namespace: &str, object_name: String) {
        // This is when we setup checksum_fn_name for objects defined in the UDL.  However, don't
        // overwrite checksum_fn_name if we got it from the proc-macro metadata.
        if self.checksum_fn_name.is_empty() {
            self.checksum_fn_name = uniffi_meta::constructor_checksum_symbol_name(
                ci_namespace,
                &object_name,
                &self.name,
            );
        }
        self.object_name = object_name;
    }
}

impl From<uniffi_meta::ConstructorMetadata> for Constructor {
    fn from(meta: uniffi_meta::ConstructorMetadata) -> Self {
        let ffi_name = meta.ffi_symbol_name();
        let checksum_fn_name = meta.checksum_symbol_name();
        let arguments = meta.inputs.into_iter().map(Into::into).collect();

        let ffi_func = FfiFunction {
            name: ffi_name,
            ..FfiFunction::default()
        };
        Self {
            name: meta.name,
            object_name: meta.self_name,
            arguments,
            ffi_func,
            throws: meta.throws.map(Into::into),
            checksum_fn_name,
            checksum_override: Some(meta.checksum),
        }
    }
}

impl APIConverter<Constructor> for weedle::interface::ConstructorInterfaceMember<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Constructor> {
        let attributes = match &self.attributes {
            Some(attr) => ConstructorAttributes::try_from(attr)?,
            None => Default::default(),
        };
        let throws = attributes
            .get_throws_err()
            .map(|name| ci.get_type(name).expect("invalid throws type"));
        Ok(Constructor {
            name: String::from(attributes.get_name().unwrap_or("new")),
            // We don't know the name of the containing `Object` at this point, fill it in later.
            object_name: Default::default(),
            // Also fill in checksum_fn_name later, since it depends on object_name
            checksum_fn_name: Default::default(),
            arguments: self.args.body.list.convert(ci)?,
            ffi_func: Default::default(),
            throws,
            checksum_override: None,
        })
    }
}

// Represents an instance method for an object type.
//
// The FFI will represent this as a function whose first/self argument is a
// `FfiType::RustArcPtr` to the instance.
#[derive(Debug, Clone, Checksum)]
pub struct Method {
    pub(super) name: String,
    pub(super) object_name: String,
    pub(super) is_async: bool,
    pub(super) object_impl: ObjectImpl,
    pub(super) arguments: Vec<Argument>,
    pub(super) return_type: Option<Type>,
    // We don't include the FFIFunc in the hash calculation, because:
    //  - it is entirely determined by the other fields,
    //    so excluding it is safe.
    //  - its `name` property includes a checksum derived from  the very
    //    hash value we're trying to calculate here, so excluding it
    //    avoids a weird circular dependency in the calculation.
    #[checksum_ignore]
    pub(super) ffi_func: FfiFunction,
    pub(super) throws: Option<Type>,
    pub(super) takes_self_by_arc: bool,
    pub(super) checksum_fn_name: String,
    // Force a checksum value.  This is used for functions from the proc-macro code, which uses a
    // different checksum method.
    #[checksum_ignore]
    pub(super) checksum_override: Option<u16>,
}

impl Method {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn is_async(&self) -> bool {
        self.is_async
    }

    pub fn arguments(&self) -> Vec<&Argument> {
        self.arguments.iter().collect()
    }

    // Methods have a special implicit first argument for the object instance,
    // hence `arguments` and `full_arguments` are different.
    pub fn full_arguments(&self) -> Vec<Argument> {
        vec![Argument {
            name: "ptr".to_string(),
            // TODO: ideally we'd get this via `ci.resolve_type_expression` so that it
            // is contained in the proper `TypeUniverse`, but this works for now.
            type_: Type::Object {
                name: self.object_name.clone(),
                imp: self.object_impl,
            },
            by_ref: !self.takes_self_by_arc,
            optional: false,
            default: None,
        }]
        .into_iter()
        .chain(self.arguments.iter().cloned())
        .collect()
    }

    pub fn return_type(&self) -> Option<&Type> {
        self.return_type.as_ref()
    }

    pub fn ffi_func(&self) -> &FfiFunction {
        &self.ffi_func
    }

    pub fn checksum_fn_name(&self) -> &str {
        &self.checksum_fn_name
    }

    pub fn checksum(&self) -> u16 {
        self.checksum_override
            .unwrap_or_else(|| uniffi_meta::checksum(self))
    }

    pub fn throws(&self) -> bool {
        self.throws.is_some()
    }

    pub fn throws_name(&self) -> Option<&str> {
        super::throws_name(&self.throws)
    }

    pub fn throws_type(&self) -> Option<&Type> {
        self.throws.as_ref()
    }

    pub fn takes_self_by_arc(&self) -> bool {
        self.takes_self_by_arc
    }

    pub fn derive_ffi_func(&mut self, ci_namespace: &str, obj_name: &str) -> Result<()> {
        // The name is already set if the function is defined through a proc-macro invocation
        // rather than in UDL. Don't overwrite it in that case.
        if self.ffi_func.name.is_empty() {
            self.ffi_func.name =
                uniffi_meta::method_symbol_name(ci_namespace, obj_name, &self.name);
        }
        self.ffi_func.init(
            self.return_type.as_ref().map(Into::into),
            self.full_arguments().iter().map(Into::into),
        );
        Ok(())
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(
            self.arguments
                .iter()
                .flat_map(Argument::iter_types)
                .chain(self.return_type.iter().flat_map(Type::iter_types)),
        )
    }

    fn set_object_info(&mut self, ci_namespace: &str, object: &Object) {
        // This is when we setup checksum_fn_name for objects defined in the UDL.  However, don't
        // overwrite checksum_fn_name if we got it from the proc-macro metadata.
        if self.checksum_fn_name.is_empty() {
            self.checksum_fn_name =
                uniffi_meta::method_checksum_symbol_name(ci_namespace, &object.name, &self.name);
        }
        self.object_name = object.name.clone();
        self.object_impl = object.imp;
    }
}

impl From<uniffi_meta::MethodMetadata> for Method {
    fn from(meta: uniffi_meta::MethodMetadata) -> Self {
        let ffi_name = meta.ffi_symbol_name();
        let checksum_fn_name = meta.checksum_symbol_name();
        let is_async = meta.is_async;
        let return_type = meta.return_type.map(Into::into);
        let arguments = meta.inputs.into_iter().map(Into::into).collect();

        let ffi_func = FfiFunction {
            name: ffi_name,
            is_async,
            ..FfiFunction::default()
        };

        Self {
            name: meta.name,
            object_name: meta.self_name,
            is_async,
            object_impl: ObjectImpl::Struct,
            arguments,
            return_type,
            ffi_func,
            throws: meta.throws.map(Into::into),
            takes_self_by_arc: false, // not yet supported by procmacros?
            checksum_fn_name,
            checksum_override: Some(meta.checksum),
        }
    }
}

impl From<uniffi_meta::TraitMethodMetadata> for Method {
    fn from(meta: uniffi_meta::TraitMethodMetadata) -> Self {
        let checksum_fn_name = meta.checksum_symbol_name();
        let return_type = meta.return_type.map(Into::into);
        let arguments = meta.inputs.into_iter().map(Into::into).collect();
        Self {
            name: meta.name,
            object_name: meta.trait_name,
            is_async: false,
            arguments,
            return_type,
            throws: meta.throws.map(Into::into),
            takes_self_by_arc: false, // not yet supported by procmacros?
            checksum_fn_name,
            checksum_override: Some(meta.checksum),
            // These are placeholder values that don't affect any behavior since we don't create
            // scaffolding functions for callback interface methods
            ffi_func: FfiFunction::default(),
            object_impl: ObjectImpl::Struct,
        }
    }
}

impl APIConverter<Method> for weedle::interface::OperationInterfaceMember<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Method> {
        if self.special.is_some() {
            bail!("special operations not supported");
        }
        if self.modifier.is_some() {
            bail!("method modifiers are not supported")
        }
        let return_type = ci.resolve_return_type_expression(&self.return_type)?;
        let attributes = MethodAttributes::try_from(self.attributes.as_ref())?;

        let throws = match attributes.get_throws_err() {
            Some(name) => match ci.get_type(name) {
                Some(t) => {
                    ci.note_name_used_as_error(name);
                    Some(t)
                }
                None => bail!("unknown type for error: {name}"),
            },
            None => None,
        };

        let takes_self_by_arc = attributes.get_self_by_arc();
        Ok(Method {
            name: match self.identifier {
                None => bail!("anonymous methods are not supported {:?}", self),
                Some(id) => {
                    let name = id.0.to_string();
                    if name == "new" {
                        bail!("the method name \"new\" is reserved for the default constructor");
                    }
                    name
                }
            },
            // We don't know the name of the containing `Object` at this point, fill it in later.
            object_name: Default::default(),
            // Also fill in checksum_fn_name later, since it depends on the object name
            checksum_fn_name: Default::default(),
            is_async: false,
            object_impl: ObjectImpl::Struct, // We'll fill this in later too.
            arguments: self.args.body.list.convert(ci)?,
            return_type,
            ffi_func: Default::default(),
            throws,
            takes_self_by_arc,
            checksum_override: None,
        })
    }
}

/// The list of traits we support generating helper methods for.
#[derive(Clone, Debug, Checksum)]
pub enum UniffiTrait {
    Debug { fmt: Method },
    Display { fmt: Method },
    Eq { eq: Method, ne: Method },
    Hash { hash: Method },
}

impl UniffiTrait {
    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(
            match self {
                UniffiTrait::Display { fmt: m }
                | UniffiTrait::Debug { fmt: m }
                | UniffiTrait::Hash { hash: m } => vec![m.iter_types()],
                UniffiTrait::Eq { eq, ne } => vec![eq.iter_types(), ne.iter_types()],
            }
            .into_iter()
            .flatten(),
        )
    }

    pub fn derive_ffi_func(&mut self, ci_namespace: &str, obj_name: &str) -> Result<()> {
        match self {
            UniffiTrait::Display { fmt: m }
            | UniffiTrait::Debug { fmt: m }
            | UniffiTrait::Hash { hash: m } => {
                m.derive_ffi_func(ci_namespace, obj_name)?;
            }
            UniffiTrait::Eq { eq, ne } => {
                eq.derive_ffi_func(ci_namespace, obj_name)?;
                ne.derive_ffi_func(ci_namespace, obj_name)?;
            }
        }
        Ok(())
    }
}

impl Callable for Constructor {
    fn arguments(&self) -> Vec<&Argument> {
        self.arguments()
    }

    fn return_type(&self) -> Option<Type> {
        Some(Type::Object {
            name: self.object_name.clone(),
            imp: ObjectImpl::Struct,
        })
    }

    fn throws_type(&self) -> Option<Type> {
        self.throws_type().cloned()
    }
}

impl Callable for Method {
    fn arguments(&self) -> Vec<&Argument> {
        self.arguments()
    }

    fn return_type(&self) -> Option<Type> {
        self.return_type().cloned()
    }

    fn throws_type(&self) -> Option<Type> {
        self.throws_type().cloned()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_that_all_argument_and_return_types_become_known() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                constructor(string? name, u16 age);
                sequence<u32> code_points_of_name();
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.object_definitions().len(), 1);
        ci.get_object_definition("Testing").unwrap();

        assert_eq!(ci.iter_types().count(), 6);
        assert!(ci.iter_types().any(|t| t.canonical_name() == "u16"));
        assert!(ci.iter_types().any(|t| t.canonical_name() == "u32"));
        assert!(ci.iter_types().any(|t| t.canonical_name() == "Sequenceu32"));
        assert!(ci.iter_types().any(|t| t.canonical_name() == "string"));
        assert!(ci
            .iter_types()
            .any(|t| t.canonical_name() == "Optionalstring"));
        assert!(ci.iter_types().any(|t| t.canonical_name() == "TypeTesting"));
    }

    #[test]
    fn test_alternate_constructors() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                constructor();
                [Name=new_with_u32]
                constructor(u32 v);
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.object_definitions().len(), 1);

        let obj = ci.get_object_definition("Testing").unwrap();
        assert!(obj.primary_constructor().is_some());
        assert_eq!(obj.alternate_constructors().len(), 1);
        assert_eq!(obj.methods().len(), 0);

        let cons = obj.primary_constructor().unwrap();
        assert_eq!(cons.name(), "new");
        assert_eq!(cons.arguments.len(), 0);
        assert_eq!(cons.ffi_func.arguments.len(), 0);

        let cons = obj.alternate_constructors()[0];
        assert_eq!(cons.name(), "new_with_u32");
        assert_eq!(cons.arguments.len(), 1);
        assert_eq!(cons.ffi_func.arguments.len(), 1);
    }

    #[test]
    fn test_the_name_new_identifies_the_primary_constructor() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                [Name=newish]
                constructor();
                [Name=new]
                constructor(u32 v);
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        assert_eq!(ci.object_definitions().len(), 1);

        let obj = ci.get_object_definition("Testing").unwrap();
        assert!(obj.primary_constructor().is_some());
        assert_eq!(obj.alternate_constructors().len(), 1);
        assert_eq!(obj.methods().len(), 0);

        let cons = obj.primary_constructor().unwrap();
        assert_eq!(cons.name(), "new");
        assert_eq!(cons.arguments.len(), 1);

        let cons = obj.alternate_constructors()[0];
        assert_eq!(cons.name(), "newish");
        assert_eq!(cons.arguments.len(), 0);
        assert_eq!(cons.ffi_func.arguments.len(), 0);
    }

    #[test]
    fn test_the_name_new_is_reserved_for_constructors() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                constructor();
                void new(u32 v);
            };
        "#;
        let err = ComponentInterface::from_webidl(UDL).unwrap_err();
        assert_eq!(
            err.to_string(),
            "the method name \"new\" is reserved for the default constructor"
        );
    }

    #[test]
    fn test_duplicate_primary_constructors_not_allowed() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                constructor();
                constructor(u32 v);
            };
        "#;
        let err = ComponentInterface::from_webidl(UDL).unwrap_err();
        assert_eq!(err.to_string(), "Duplicate interface member name: \"new\"");

        const UDL2: &str = r#"
            namespace test{};
            interface Testing {
                constructor();
                [Name=new]
                constructor(u32 v);
            };
        "#;
        let err = ComponentInterface::from_webidl(UDL2).unwrap_err();
        assert_eq!(err.to_string(), "Duplicate interface member name: \"new\"");
    }

    #[test]
    fn test_trait_attribute() {
        const UDL: &str = r#"
            namespace test{};
            interface NotATrait {
            };
            [Trait]
            interface ATrait {
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL).unwrap();
        let obj = ci.get_object_definition("NotATrait").unwrap();
        assert_eq!(obj.imp.rust_name_for(&obj.name), "r#NotATrait");
        let obj = ci.get_object_definition("ATrait").unwrap();
        assert_eq!(obj.imp.rust_name_for(&obj.name), "dyn r#ATrait");
    }

    #[test]
    fn test_trait_constructors_not_allowed() {
        const UDL: &str = r#"
            namespace test{};
            [Trait]
            interface Testing {
                constructor();
            };
        "#;
        let err = ComponentInterface::from_webidl(UDL).unwrap_err();
        assert_eq!(
            err.to_string(),
            "Trait interfaces can not have constructors: \"new\""
        );
    }
}
