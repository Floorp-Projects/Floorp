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
//! # "##, "crate_name")?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in an [`Object`] member with one [`Constructor`] and one [`Method`] being added
//! to the resulting [`crate::ComponentInterface`]:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {};
//! # interface Example {
//! #   constructor(string? name);
//! #   string my_name();
//! # };
//! # "##, "crate_name")?;
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
//! # "##, "crate_name")?;
//! let obj = ci.get_object_definition("Example").unwrap();
//! assert_eq!(obj.name(), "Example");
//! assert_eq!(obj.constructors().len(), 0);
//! # Ok::<(), anyhow::Error>(())
//! ```

use anyhow::Result;
use uniffi_meta::Checksum;

use super::callbacks;
use super::ffi::{FfiArgument, FfiCallbackFunction, FfiFunction, FfiStruct, FfiType};
use super::function::{Argument, Callable};
use super::{AsType, ObjectImpl, Type, TypeIterator};

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
    pub(super) module_path: String,
    pub(super) constructors: Vec<Constructor>,
    pub(super) methods: Vec<Method>,
    // The "trait" methods - they have a (presumably "well known") name, and
    // a regular method (albeit with a generated name)
    // XXX - this should really be a HashSet, but not enough transient types support hash to make it worthwhile now.
    pub(super) uniffi_traits: Vec<UniffiTrait>,
    // We don't include the FfiFuncs in the hash calculation, because:
    //  - it is entirely determined by the other fields,
    //    so excluding it is safe.
    //  - its `name` property includes a checksum derived from  the very
    //    hash value we're trying to calculate here, so excluding it
    //    avoids a weird circular dependency in the calculation.

    // FFI function to clone a pointer for this object
    #[checksum_ignore]
    pub(super) ffi_func_clone: FfiFunction,
    // FFI function to free a pointer for this object
    #[checksum_ignore]
    pub(super) ffi_func_free: FfiFunction,
    // Ffi function to initialize the foreign callback for trait interfaces
    #[checksum_ignore]
    pub(super) ffi_init_callback: Option<FfiFunction>,
    #[checksum_ignore]
    pub(super) docstring: Option<String>,
}

impl Object {
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

    pub fn is_trait_interface(&self) -> bool {
        self.imp.is_trait_interface()
    }

    pub fn has_callback_interface(&self) -> bool {
        self.imp.has_callback_interface()
    }

    pub fn has_async_method(&self) -> bool {
        self.methods.iter().any(Method::is_async)
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

    pub fn ffi_object_clone(&self) -> &FfiFunction {
        &self.ffi_func_clone
    }

    pub fn ffi_object_free(&self) -> &FfiFunction {
        &self.ffi_func_free
    }

    pub fn ffi_init_callback(&self) -> &FfiFunction {
        self.ffi_init_callback
            .as_ref()
            .unwrap_or_else(|| panic!("No ffi_init_callback set for {}", &self.name))
    }

    pub fn docstring(&self) -> Option<&str> {
        self.docstring.as_deref()
    }

    pub fn iter_ffi_function_definitions(&self) -> impl Iterator<Item = &FfiFunction> {
        [&self.ffi_func_clone, &self.ffi_func_free]
            .into_iter()
            .chain(&self.ffi_init_callback)
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

    pub fn derive_ffi_funcs(&mut self) -> Result<()> {
        assert!(!self.ffi_func_clone.name().is_empty());
        assert!(!self.ffi_func_free.name().is_empty());
        self.ffi_func_clone.arguments = vec![FfiArgument {
            name: "ptr".to_string(),
            type_: FfiType::RustArcPtr(self.name.to_string()),
        }];
        self.ffi_func_clone.return_type = Some(FfiType::RustArcPtr(self.name.to_string()));
        self.ffi_func_free.arguments = vec![FfiArgument {
            name: "ptr".to_string(),
            type_: FfiType::RustArcPtr(self.name.to_string()),
        }];
        self.ffi_func_free.return_type = None;
        self.ffi_func_free.is_object_free_function = true;
        if self.has_callback_interface() {
            self.ffi_init_callback = Some(FfiFunction::callback_init(
                &self.module_path,
                &self.name,
                callbacks::vtable_name(&self.name),
            ));
        }

        for cons in self.constructors.iter_mut() {
            cons.derive_ffi_func();
        }
        for meth in self.methods.iter_mut() {
            meth.derive_ffi_func()?;
        }
        for ut in self.uniffi_traits.iter_mut() {
            ut.derive_ffi_func()?;
        }

        Ok(())
    }

    /// For trait interfaces, FfiCallbacks to define for our methods, otherwise an empty vec.
    pub fn ffi_callbacks(&self) -> Vec<FfiCallbackFunction> {
        if self.is_trait_interface() {
            callbacks::ffi_callbacks(&self.name, &self.methods)
        } else {
            vec![]
        }
    }

    /// For trait interfaces, the VTable FFI type
    pub fn vtable(&self) -> Option<FfiType> {
        self.is_trait_interface()
            .then(|| FfiType::Struct(callbacks::vtable_name(&self.name)))
    }

    /// For trait interfaces, the VTable struct to define.  Otherwise None.
    pub fn vtable_definition(&self) -> Option<FfiStruct> {
        self.is_trait_interface()
            .then(|| callbacks::vtable_struct(&self.name, &self.methods))
    }

    /// Vec of (ffi_callback_name, method) pairs
    pub fn vtable_methods(&self) -> Vec<(FfiCallbackFunction, &Method)> {
        self.methods
            .iter()
            .enumerate()
            .map(|(i, method)| {
                (
                    callbacks::method_ffi_callback(&self.name, method, i),
                    method,
                )
            })
            .collect()
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

impl AsType for Object {
    fn as_type(&self) -> Type {
        Type::Object {
            name: self.name.clone(),
            module_path: self.module_path.clone(),
            imp: self.imp,
        }
    }
}

impl From<uniffi_meta::ObjectMetadata> for Object {
    fn from(meta: uniffi_meta::ObjectMetadata) -> Self {
        let ffi_clone_name = meta.clone_ffi_symbol_name();
        let ffi_free_name = meta.free_ffi_symbol_name();
        Object {
            module_path: meta.module_path,
            name: meta.name,
            imp: meta.imp,
            constructors: Default::default(),
            methods: Default::default(),
            uniffi_traits: Default::default(),
            ffi_func_clone: FfiFunction {
                name: ffi_clone_name,
                ..Default::default()
            },
            ffi_func_free: FfiFunction {
                name: ffi_free_name,
                ..Default::default()
            },
            ffi_init_callback: None,
            docstring: meta.docstring.clone(),
        }
    }
}

impl From<uniffi_meta::UniffiTraitMetadata> for UniffiTrait {
    fn from(meta: uniffi_meta::UniffiTraitMetadata) -> Self {
        match meta {
            uniffi_meta::UniffiTraitMetadata::Debug { fmt } => {
                UniffiTrait::Debug { fmt: fmt.into() }
            }
            uniffi_meta::UniffiTraitMetadata::Display { fmt } => {
                UniffiTrait::Display { fmt: fmt.into() }
            }
            uniffi_meta::UniffiTraitMetadata::Eq { eq, ne } => UniffiTrait::Eq {
                eq: eq.into(),
                ne: ne.into(),
            },
            uniffi_meta::UniffiTraitMetadata::Hash { hash } => {
                UniffiTrait::Hash { hash: hash.into() }
            }
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
    pub(super) object_module_path: String,
    pub(super) is_async: bool,
    pub(super) arguments: Vec<Argument>,
    // We don't include the FFIFunc in the hash calculation, because:
    //  - it is entirely determined by the other fields,
    //    so excluding it is safe.
    //  - its `name` property includes a checksum derived from  the very
    //    hash value we're trying to calculate here, so excluding it
    //    avoids a weird circular dependency in the calculation.
    #[checksum_ignore]
    pub(super) ffi_func: FfiFunction,
    #[checksum_ignore]
    pub(super) docstring: Option<String>,
    pub(super) throws: Option<Type>,
    pub(super) checksum_fn_name: String,
    // Force a checksum value, or we'll fallback to the trait.
    #[checksum_ignore]
    pub(super) checksum: Option<u16>,
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
        self.checksum.unwrap_or_else(|| uniffi_meta::checksum(self))
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

    pub fn docstring(&self) -> Option<&str> {
        self.docstring.as_deref()
    }

    pub fn is_primary_constructor(&self) -> bool {
        self.name == "new"
    }

    fn derive_ffi_func(&mut self) {
        assert!(!self.ffi_func.name().is_empty());
        self.ffi_func.init(
            Some(FfiType::RustArcPtr(self.object_name.clone())),
            self.arguments.iter().map(Into::into),
        );
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        Box::new(self.arguments.iter().flat_map(Argument::iter_types))
    }
}

impl From<uniffi_meta::ConstructorMetadata> for Constructor {
    fn from(meta: uniffi_meta::ConstructorMetadata) -> Self {
        let ffi_name = meta.ffi_symbol_name();
        let checksum_fn_name = meta.checksum_symbol_name();
        let arguments = meta.inputs.into_iter().map(Into::into).collect();

        let ffi_func = FfiFunction {
            name: ffi_name,
            is_async: meta.is_async,
            ..FfiFunction::default()
        };
        Self {
            name: meta.name,
            object_name: meta.self_name,
            is_async: meta.is_async,
            object_module_path: meta.module_path,
            arguments,
            ffi_func,
            docstring: meta.docstring.clone(),
            throws: meta.throws.map(Into::into),
            checksum_fn_name,
            checksum: meta.checksum,
        }
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
    pub(super) object_module_path: String,
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
    #[checksum_ignore]
    pub(super) docstring: Option<String>,
    pub(super) throws: Option<Type>,
    pub(super) takes_self_by_arc: bool,
    pub(super) checksum_fn_name: String,
    // Force a checksum value, or we'll fallback to the trait.
    #[checksum_ignore]
    pub(super) checksum: Option<u16>,
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
                module_path: self.object_module_path.clone(),
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
        self.checksum.unwrap_or_else(|| uniffi_meta::checksum(self))
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

    pub fn docstring(&self) -> Option<&str> {
        self.docstring.as_deref()
    }

    pub fn takes_self_by_arc(&self) -> bool {
        self.takes_self_by_arc
    }

    pub fn derive_ffi_func(&mut self) -> Result<()> {
        assert!(!self.ffi_func.name().is_empty());
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

    /// For async callback interface methods, the FFI struct to pass to the completion function.
    pub fn foreign_future_ffi_result_struct(&self) -> FfiStruct {
        callbacks::foreign_future_ffi_result_struct(self.return_type.as_ref().map(FfiType::from))
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
            object_module_path: meta.module_path,
            is_async,
            object_impl: ObjectImpl::Struct, // will be filled in later
            arguments,
            return_type,
            ffi_func,
            docstring: meta.docstring.clone(),
            throws: meta.throws.map(Into::into),
            takes_self_by_arc: meta.takes_self_by_arc,
            checksum_fn_name,
            checksum: meta.checksum,
        }
    }
}

impl From<uniffi_meta::TraitMethodMetadata> for Method {
    fn from(meta: uniffi_meta::TraitMethodMetadata) -> Self {
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
            object_name: meta.trait_name,
            object_module_path: meta.module_path,
            is_async,
            arguments,
            return_type,
            docstring: meta.docstring.clone(),
            throws: meta.throws.map(Into::into),
            takes_self_by_arc: meta.takes_self_by_arc,
            checksum_fn_name,
            checksum: meta.checksum,
            ffi_func,
            object_impl: ObjectImpl::Struct,
        }
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

    pub fn derive_ffi_func(&mut self) -> Result<()> {
        match self {
            UniffiTrait::Display { fmt: m }
            | UniffiTrait::Debug { fmt: m }
            | UniffiTrait::Hash { hash: m } => {
                m.derive_ffi_func()?;
            }
            UniffiTrait::Eq { eq, ne } => {
                eq.derive_ffi_func()?;
                ne.derive_ffi_func()?;
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
            module_path: self.object_module_path.clone(),
            imp: ObjectImpl::Struct,
        })
    }

    fn throws_type(&self) -> Option<Type> {
        self.throws_type().cloned()
    }

    fn is_async(&self) -> bool {
        self.is_async
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

    fn is_async(&self) -> bool {
        self.is_async
    }

    fn takes_self(&self) -> bool {
        true
    }
}

#[cfg(test)]
mod test {
    use super::super::ComponentInterface;
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
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(ci.object_definitions().len(), 1);
        ci.get_object_definition("Testing").unwrap();

        assert_eq!(ci.iter_types().count(), 6);
        assert!(ci.iter_types().any(|t| t == &Type::UInt16));
        assert!(ci.iter_types().any(|t| t == &Type::UInt32));
        assert!(ci.iter_types().any(|t| t
            == &Type::Sequence {
                inner_type: Box::new(Type::UInt32)
            }));
        assert!(ci.iter_types().any(|t| t == &Type::String));
        assert!(ci.iter_types().any(|t| t
            == &Type::Optional {
                inner_type: Box::new(Type::String)
            }));
        assert!(ci
            .iter_types()
            .any(|t| matches!(t, Type::Object { name, ..} if name == "Testing")));
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
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
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
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
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
        let err = ComponentInterface::from_webidl(UDL, "crate_name").unwrap_err();
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
        let err = ComponentInterface::from_webidl(UDL, "crate_name").unwrap_err();
        assert_eq!(err.to_string(), "Duplicate interface member name: \"new\"");

        const UDL2: &str = r#"
            namespace test{};
            interface Testing {
                constructor();
                [Name=new]
                constructor(u32 v);
            };
        "#;
        let err = ComponentInterface::from_webidl(UDL2, "crate_name").unwrap_err();
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
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
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
        let err = ComponentInterface::from_webidl(UDL, "crate_name").unwrap_err();
        assert_eq!(
            err.to_string(),
            "Trait interfaces can not have constructors: \"new\""
        );
    }

    #[test]
    fn test_docstring_object() {
        const UDL: &str = r#"
            namespace test{};
            /// informative docstring
            interface Testing { };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(
            ci.get_object_definition("Testing")
                .unwrap()
                .docstring()
                .unwrap(),
            "informative docstring"
        );
    }

    #[test]
    fn test_docstring_constructor() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                /// informative docstring
                constructor();
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(
            ci.get_object_definition("Testing")
                .unwrap()
                .primary_constructor()
                .unwrap()
                .docstring()
                .unwrap(),
            "informative docstring"
        );
    }

    #[test]
    fn test_docstring_method() {
        const UDL: &str = r#"
            namespace test{};
            interface Testing {
                /// informative docstring
                void testing();
            };
        "#;
        let ci = ComponentInterface::from_webidl(UDL, "crate_name").unwrap();
        assert_eq!(
            ci.get_object_definition("Testing")
                .unwrap()
                .get_method("testing")
                .docstring()
                .unwrap(),
            "informative docstring"
        );
    }
}
