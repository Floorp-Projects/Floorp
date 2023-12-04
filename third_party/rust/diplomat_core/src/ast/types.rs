use proc_macro2::Span;
use quote::ToTokens;
use serde::{Deserialize, Serialize};
use syn::{punctuated::Punctuated, *};

use lazy_static::lazy_static;
use std::collections::HashMap;
use std::fmt;
use std::ops::ControlFlow;

use super::{
    Attrs, Docs, Enum, Ident, Lifetime, LifetimeEnv, LifetimeTransitivity, Method, NamedLifetime,
    OpaqueStruct, Path, RustLink, Struct, ValidityError,
};
use crate::Env;

/// A type declared inside a Diplomat-annotated module.
#[derive(Clone, Serialize, Debug, Hash, PartialEq, Eq)]
#[non_exhaustive]
pub enum CustomType {
    /// A non-opaque struct whose fields will be visible across the FFI boundary.
    Struct(Struct),
    /// A struct annotated with [`diplomat::opaque`] whose fields are not visible.
    Opaque(OpaqueStruct),
    /// A fieldless enum.
    Enum(Enum),
}

impl CustomType {
    /// Get the name of the custom type, which is unique within a module.
    pub fn name(&self) -> &Ident {
        match self {
            CustomType::Struct(strct) => &strct.name,
            CustomType::Opaque(strct) => &strct.name,
            CustomType::Enum(enm) => &enm.name,
        }
    }

    /// Get the methods declared in impls of the custom type.
    pub fn methods(&self) -> &Vec<Method> {
        match self {
            CustomType::Struct(strct) => &strct.methods,
            CustomType::Opaque(strct) => &strct.methods,
            CustomType::Enum(enm) => &enm.methods,
        }
    }

    pub fn attrs(&self) -> &Attrs {
        match self {
            CustomType::Struct(strct) => &strct.attrs,
            CustomType::Opaque(strct) => &strct.attrs,
            CustomType::Enum(enm) => &enm.attrs,
        }
    }

    /// Get the doc lines of the custom type.
    pub fn docs(&self) -> &Docs {
        match self {
            CustomType::Struct(strct) => &strct.docs,
            CustomType::Opaque(strct) => &strct.docs,
            CustomType::Enum(enm) => &enm.docs,
        }
    }

    /// Get all rust links on this type and its methods
    pub fn all_rust_links(&self) -> impl Iterator<Item = &RustLink> + '_ {
        [self.docs()]
            .into_iter()
            .chain(self.methods().iter().map(|m| m.docs()))
            .flat_map(|d| d.rust_links().iter())
    }

    pub fn self_path(&self, in_path: &Path) -> Path {
        in_path.sub_path(self.name().clone())
    }

    /// Get the lifetimes of the custom type.
    pub fn lifetimes(&self) -> Option<&LifetimeEnv> {
        match self {
            CustomType::Struct(strct) => Some(&strct.lifetimes),
            CustomType::Opaque(strct) => Some(&strct.lifetimes),
            CustomType::Enum(_) => None,
        }
    }

    /// Performs various validity checks:
    ///
    /// - Checks that any references to opaque structs in parameters or return values
    ///   are always behind a box or reference, and that non-opaque custom types are *never* behind
    ///   references or boxes. The latter check is needed because non-opaque custom types typically get
    ///   *converted* at the FFI boundary.
    /// - Ensures that we are not exporting any non-opaque zero-sized types
    /// - Ensures that Options only contain boxes and references
    ///
    /// Errors are pushed into the `errors` vector.
    pub fn check_validity<'a>(
        &'a self,
        in_path: &Path,
        env: &Env,
        errors: &mut Vec<ValidityError>,
    ) {
        match self {
            CustomType::Struct(strct) => {
                for (_, field, _) in strct.fields.iter() {
                    field.check_validity(in_path, env, errors);
                }

                // check for ZSTs
                if !strct.fields.iter().any(|f| !f.1.is_zst()) {
                    errors.push(ValidityError::NonOpaqueZST(self.self_path(in_path)))
                }
            }
            CustomType::Opaque(_) => {}
            CustomType::Enum(e) => {
                // check for ZSTs
                if e.variants.is_empty() {
                    errors.push(ValidityError::NonOpaqueZST(self.self_path(in_path)))
                }
            }
        }

        for method in self.methods().iter() {
            method.check_validity(in_path, env, errors);
        }
    }
}

/// A symbol declared in a module, which can either be a pointer to another path,
/// or a custom type defined directly inside that module
#[derive(Clone, Serialize, Debug)]
#[non_exhaustive]
pub enum ModSymbol {
    /// A symbol that is a pointer to another path.
    Alias(Path),
    /// A symbol that is a submodule.
    SubModule(Ident),
    /// A symbol that is a custom type.
    CustomType(CustomType),
}

/// A named type that is just a path, e.g. `std::borrow::Cow<'a, T>`.
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[non_exhaustive]
pub struct PathType {
    pub path: Path,
    pub lifetimes: Vec<Lifetime>,
}

impl PathType {
    pub fn to_syn(&self) -> syn::TypePath {
        let mut path = self.path.to_syn();

        if !self.lifetimes.is_empty() {
            if let Some(seg) = path.segments.last_mut() {
                let lifetimes = &self.lifetimes;
                seg.arguments =
                    syn::PathArguments::AngleBracketed(syn::parse_quote! { <#(#lifetimes),*> });
            }
        }

        syn::TypePath { qself: None, path }
    }

    pub fn new(path: Path) -> Self {
        Self {
            path,
            lifetimes: vec![],
        }
    }

    /// Get the `Self` type from a struct declaration.
    ///
    /// Consider the following struct declaration:
    /// ```
    /// struct RefList<'a> {
    ///     data: &'a i32,
    ///     next: Option<Box<Self>>,
    /// }
    /// ```
    /// When determining what type `Self` is in the `next` field, we would have to call
    /// this method on the `syn::ItemStruct` that represents this struct declaration.
    /// This method would then return a `PathType` representing `RefList<'a>`, so we
    /// know that's what `Self` should refer to.
    ///
    /// The reason this function exists though is so when we convert the fields' types
    /// to `PathType`s, we don't panic. We don't actually need to write the struct's
    /// field types expanded in the macro, so this function is more for correctness,
    pub fn extract_self_type(strct: &syn::ItemStruct) -> Self {
        let self_name = (&strct.ident).into();

        PathType {
            path: Path {
                elements: vec![self_name],
            },
            lifetimes: strct
                .generics
                .lifetimes()
                .map(|lt_def| (&lt_def.lifetime).into())
                .collect(),
        }
    }

    /// If this is a [`TypeName::Named`], grab the [`CustomType`] it points to from
    /// the `env`, which contains all [`CustomType`]s across all FFI modules.
    ///
    /// Also returns the path the CustomType is in (useful for resolving fields)
    pub fn resolve_with_path<'a>(&self, in_path: &Path, env: &'a Env) -> (Path, &'a CustomType) {
        let local_path = &self.path;
        let mut cur_path = in_path.clone();
        for (i, elem) in local_path.elements.iter().enumerate() {
            match elem.as_str() {
                "crate" => {
                    // TODO(#34): get the name of enclosing crate from env when we support multiple crates
                    cur_path = Path::empty()
                }

                "super" => cur_path = cur_path.get_super(),

                o => match env.get(&cur_path, o) {
                    Some(ModSymbol::Alias(p)) => {
                        let mut remaining_elements: Vec<Ident> =
                            local_path.elements.iter().skip(i + 1).cloned().collect();
                        let mut new_path = p.elements.clone();
                        new_path.append(&mut remaining_elements);
                        return PathType::new(Path { elements: new_path })
                            .resolve_with_path(&cur_path.clone(), env);
                    }
                    Some(ModSymbol::SubModule(name)) => {
                        cur_path.elements.push(name.clone());
                    }
                    Some(ModSymbol::CustomType(t)) => {
                        if i == local_path.elements.len() - 1 {
                            return (cur_path, t);
                        } else {
                            panic!(
                                "Unexpected custom type when resolving symbol {} in {}",
                                o,
                                cur_path.elements.join("::")
                            )
                        }
                    }
                    None => panic!(
                        "Could not resolve symbol {} in {}",
                        o,
                        cur_path.elements.join("::")
                    ),
                },
            }
        }

        panic!(
            "Path {} does not point to a custom type",
            in_path.elements.join("::")
        )
    }

    /// If this is a [`TypeName::Named`], grab the [`CustomType`] it points to from
    /// the `env`, which contains all [`CustomType`]s across all FFI modules.
    ///
    /// If you need to resolve struct fields later, call [`Self::resolve_with_path()`] instead
    /// to get the path to resolve the fields in.
    pub fn resolve<'a>(&self, in_path: &Path, env: &'a Env) -> &'a CustomType {
        self.resolve_with_path(in_path, env).1
    }
}

impl From<&syn::TypePath> for PathType {
    fn from(other: &syn::TypePath) -> Self {
        let lifetimes = other
            .path
            .segments
            .last()
            .and_then(|last| {
                if let PathArguments::AngleBracketed(angle_generics) = &last.arguments {
                    Some(
                        angle_generics
                            .args
                            .iter()
                            .map(|generic_arg| match generic_arg {
                                GenericArgument::Lifetime(lifetime) => lifetime.into(),
                                _ => panic!("generic type arguments are unsupported"),
                            })
                            .collect(),
                    )
                } else {
                    None
                }
            })
            .unwrap_or_default();

        Self {
            path: Path::from_syn(&other.path),
            lifetimes,
        }
    }
}

impl From<Path> for PathType {
    fn from(other: Path) -> Self {
        PathType::new(other)
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[allow(clippy::exhaustive_enums)] // there are only two kinds of mutability we care about
pub enum Mutability {
    Mutable,
    Immutable,
}

impl Mutability {
    pub fn to_syn(&self) -> Option<Token![mut]> {
        match self {
            Mutability::Mutable => Some(syn::token::Mut(Span::call_site())),
            Mutability::Immutable => None,
        }
    }

    pub fn from_syn(t: &Option<Token![mut]>) -> Self {
        match t {
            Some(_) => Mutability::Mutable,
            None => Mutability::Immutable,
        }
    }

    /// Returns `true` if `&self` is the mutable variant, otherwise `false`.
    pub fn is_mutable(&self) -> bool {
        matches!(self, Mutability::Mutable)
    }

    /// Returns `true` if `&self` is the immutable variant, otherwise `false`.
    pub fn is_immutable(&self) -> bool {
        matches!(self, Mutability::Immutable)
    }

    /// Shorthand ternary operator for choosing a value based on whether
    /// a `Mutability` is mutable or immutable.
    ///
    /// The following pattern (with very slight variations) shows up often in code gen:
    /// ```ignore
    /// if mutability.is_mutable() {
    ///     ""
    /// } else {
    ///     "const "
    /// }
    /// ```
    /// This is particularly annoying in `write!(...)` statements, where `cargo fmt`
    /// expands it to take up 5 lines.
    ///
    /// This method offers a 1-line alternative:
    /// ```ignore
    /// mutability.if_mut_else("", "const ")
    /// ```
    /// For cases where lazy evaluation is desired, consider using a conditional
    /// or a `match` statement.
    pub fn if_mut_else<T>(&self, if_mut: T, if_immut: T) -> T {
        match self {
            Mutability::Mutable => if_mut,
            Mutability::Immutable => if_immut,
        }
    }
}

/// A local type reference, such as the type of a field, parameter, or return value.
/// Unlike [`CustomType`], which represents a type declaration, [`TypeName`]s can compose
/// types through references and boxing, and can also capture unresolved paths.
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[non_exhaustive]
pub enum TypeName {
    /// A built-in Rust scalar primitive.
    Primitive(PrimitiveType),
    /// An unresolved path to a custom type, which can be resolved after all types
    /// are collected with [`TypeName::resolve()`].
    Named(PathType),
    /// An optionally mutable reference to another type.
    Reference(Lifetime, Mutability, Box<TypeName>),
    /// A `Box<T>` type.
    Box(Box<TypeName>),
    /// A `Option<T>` type.
    Option(Box<TypeName>),
    /// A `Result<T, E>` or `diplomat_runtime::DiplomatWriteable` type. If the bool is true, it's `Result`
    Result(Box<TypeName>, Box<TypeName>, bool),
    Writeable,
    /// A `&str` type.
    StrReference(Lifetime),
    /// A `&[T]` type, where `T` is a primitive.
    PrimitiveSlice(Lifetime, Mutability, PrimitiveType),
    /// The `()` type.
    Unit,
    /// The `Self` type.
    SelfType(PathType),
}

impl TypeName {
    /// Converts the [`TypeName`] back into an AST node that can be spliced into a program.
    pub fn to_syn(&self) -> syn::Type {
        match self {
            TypeName::Primitive(name) => {
                syn::Type::Path(syn::parse_str(PRIMITIVE_TO_STRING.get(name).unwrap()).unwrap())
            }
            TypeName::Named(name) | TypeName::SelfType(name) => {
                // Self also gets expanded instead of turning into `Self` because
                // this code is used to generate the `extern "C"` functions, which
                // aren't in an impl block.
                syn::Type::Path(name.to_syn())
            }
            TypeName::Reference(lifetime, mutability, underlying) => {
                syn::Type::Reference(TypeReference {
                    and_token: syn::token::And(Span::call_site()),
                    lifetime: lifetime.to_syn(),
                    mutability: mutability.to_syn(),
                    elem: Box::new(underlying.to_syn()),
                })
            }
            TypeName::Box(underlying) => syn::Type::Path(TypePath {
                qself: None,
                path: syn::Path {
                    leading_colon: None,
                    segments: Punctuated::from_iter(vec![PathSegment {
                        ident: syn::Ident::new("Box", Span::call_site()),
                        arguments: PathArguments::AngleBracketed(AngleBracketedGenericArguments {
                            colon2_token: None,
                            lt_token: syn::token::Lt(Span::call_site()),
                            args: Punctuated::from_iter(vec![GenericArgument::Type(
                                underlying.to_syn(),
                            )]),
                            gt_token: syn::token::Gt(Span::call_site()),
                        }),
                    }]),
                },
            }),
            TypeName::Option(underlying) => syn::Type::Path(TypePath {
                qself: None,
                path: syn::Path {
                    leading_colon: None,
                    segments: Punctuated::from_iter(vec![PathSegment {
                        ident: syn::Ident::new("Option", Span::call_site()),
                        arguments: PathArguments::AngleBracketed(AngleBracketedGenericArguments {
                            colon2_token: None,
                            lt_token: syn::token::Lt(Span::call_site()),
                            args: Punctuated::from_iter(vec![GenericArgument::Type(
                                underlying.to_syn(),
                            )]),
                            gt_token: syn::token::Gt(Span::call_site()),
                        }),
                    }]),
                },
            }),
            TypeName::Result(ok, err, true) => syn::Type::Path(TypePath {
                qself: None,
                path: syn::Path {
                    leading_colon: None,
                    segments: Punctuated::from_iter(vec![PathSegment {
                        ident: syn::Ident::new("Result", Span::call_site()),
                        arguments: PathArguments::AngleBracketed(AngleBracketedGenericArguments {
                            colon2_token: None,
                            lt_token: syn::token::Lt(Span::call_site()),
                            args: Punctuated::from_iter(vec![
                                GenericArgument::Type(ok.to_syn()),
                                GenericArgument::Type(err.to_syn()),
                            ]),
                            gt_token: syn::token::Gt(Span::call_site()),
                        }),
                    }]),
                },
            }),
            TypeName::Result(ok, err, false) => syn::Type::Path(TypePath {
                qself: None,
                path: syn::Path {
                    leading_colon: None,
                    segments: Punctuated::from_iter(vec![
                        PathSegment {
                            ident: syn::Ident::new("diplomat_runtime", Span::call_site()),
                            arguments: PathArguments::None,
                        },
                        PathSegment {
                            ident: syn::Ident::new("DiplomatResult", Span::call_site()),
                            arguments: PathArguments::AngleBracketed(
                                AngleBracketedGenericArguments {
                                    colon2_token: None,
                                    lt_token: syn::token::Lt(Span::call_site()),
                                    args: Punctuated::from_iter(vec![
                                        GenericArgument::Type(ok.to_syn()),
                                        GenericArgument::Type(err.to_syn()),
                                    ]),
                                    gt_token: syn::token::Gt(Span::call_site()),
                                },
                            ),
                        },
                    ]),
                },
            }),
            TypeName::Writeable => syn::parse_quote! {
                diplomat_runtime::DiplomatWriteable
            },
            TypeName::StrReference(lifetime) => syn::parse_str(&format!(
                "{}str",
                ReferenceDisplay(lifetime, &Mutability::Immutable)
            ))
            .unwrap(),
            TypeName::PrimitiveSlice(lifetime, mutability, name) => {
                let primitive_name = PRIMITIVE_TO_STRING.get(name).unwrap();
                let formatted_str = format!(
                    "{}[{}]",
                    ReferenceDisplay(lifetime, mutability),
                    primitive_name
                );
                syn::parse_str(&formatted_str).unwrap()
            }
            TypeName::Unit => syn::parse_quote! {
                ()
            },
        }
    }

    /// Extract a [`TypeName`] from a [`syn::Type`] AST node.
    /// The following rules are used to infer [`TypeName`] variants:
    /// - If the type is a path with a single element that is the name of a Rust primitive, returns a [`TypeName::Primitive`]
    /// - If the type is a path with a single element [`Box`], returns a [`TypeName::Box`] with the type parameter recursively converted
    /// - If the type is a path with a single element [`Option`], returns a [`TypeName::Option`] with the type parameter recursively converted
    /// - If the type is a path with a single element `Self` and `self_path_type` is provided, returns a [`TypeName::Named`]
    /// - If the type is a path with a single element [`Result`], returns a [`TypeName::Result`] with the type parameters recursively converted
    /// - If the type is a path equal to [`diplomat_runtime::DiplomatResult`], returns a [`TypeName::DiplomatResult`] with the type parameters recursively converted
    /// - If the type is a path equal to [`diplomat_runtime::DiplomatWriteable`], returns a [`TypeName::Writeable`]
    /// - If the type is a reference to `str`, returns a [`TypeName::StrReference`]
    /// - If the type is a reference to a slice of a Rust primitive, returns a [`TypeName::PrimitiveSlice`]
    /// - If the type is a reference (`&` or `&mut`), returns a [`TypeName::Reference`] with the referenced type recursively converted
    /// - Otherwise, assume that the reference is to a [`CustomType`] in either the current module or another one, returns a [`TypeName::Named`]
    pub fn from_syn(ty: &syn::Type, self_path_type: Option<PathType>) -> TypeName {
        match ty {
            syn::Type::Reference(r) => {
                let lifetime = Lifetime::from(&r.lifetime);
                let mutability = Mutability::from_syn(&r.mutability);

                if r.elem.to_token_stream().to_string() == "str" {
                    if mutability.is_mutable() {
                        panic!("mutable `str` references are disallowed");
                    }
                    return TypeName::StrReference(lifetime);
                }
                if let syn::Type::Slice(slice) = &*r.elem {
                    if let syn::Type::Path(p) = &*slice.elem {
                        if let Some(primitive) = p
                            .path
                            .get_ident()
                            .and_then(|i| STRING_TO_PRIMITIVE.get(i.to_string().as_str()))
                        {
                            return TypeName::PrimitiveSlice(lifetime, mutability, *primitive);
                        }
                    }
                }
                TypeName::Reference(
                    lifetime,
                    mutability,
                    Box::new(TypeName::from_syn(r.elem.as_ref(), self_path_type)),
                )
            }
            syn::Type::Path(p) => {
                if let Some(primitive) = p
                    .path
                    .get_ident()
                    .and_then(|i| STRING_TO_PRIMITIVE.get(i.to_string().as_str()))
                {
                    TypeName::Primitive(*primitive)
                } else if p.path.segments.len() == 1 && p.path.segments[0].ident == "Box" {
                    if let PathArguments::AngleBracketed(type_args) = &p.path.segments[0].arguments
                    {
                        if let GenericArgument::Type(tpe) = &type_args.args[0] {
                            TypeName::Box(Box::new(TypeName::from_syn(tpe, self_path_type)))
                        } else {
                            panic!("Expected first type argument for Box to be a type")
                        }
                    } else {
                        panic!("Expected angle brackets for Box type")
                    }
                } else if p.path.segments.len() == 1 && p.path.segments[0].ident == "Option" {
                    if let PathArguments::AngleBracketed(type_args) = &p.path.segments[0].arguments
                    {
                        if let GenericArgument::Type(tpe) = &type_args.args[0] {
                            TypeName::Option(Box::new(TypeName::from_syn(tpe, self_path_type)))
                        } else {
                            panic!("Expected first type argument for Option to be a type")
                        }
                    } else {
                        panic!("Expected angle brackets for Option type")
                    }
                } else if p.path.segments.len() == 1 && p.path.segments[0].ident == "Self" {
                    if let Some(self_path_type) = self_path_type {
                        TypeName::SelfType(self_path_type)
                    } else {
                        panic!("Cannot have `Self` type outside of a method");
                    }
                } else if p.path.segments.len() == 1 && p.path.segments[0].ident == "Result"
                    || is_runtime_type(p, "DiplomatResult")
                {
                    if let PathArguments::AngleBracketed(type_args) =
                        &p.path.segments.last().unwrap().arguments
                    {
                        if let (GenericArgument::Type(ok), GenericArgument::Type(err)) =
                            (&type_args.args[0], &type_args.args[1])
                        {
                            let ok = TypeName::from_syn(ok, self_path_type.clone());
                            let err = TypeName::from_syn(err, self_path_type);
                            TypeName::Result(
                                Box::new(ok),
                                Box::new(err),
                                !is_runtime_type(p, "DiplomatResult"),
                            )
                        } else {
                            panic!("Expected both type arguments for Result to be a type")
                        }
                    } else {
                        panic!("Expected angle brackets for Result type")
                    }
                } else if is_runtime_type(p, "DiplomatWriteable") {
                    TypeName::Writeable
                } else {
                    TypeName::Named(PathType::from(p))
                }
            }
            syn::Type::Tuple(tup) => {
                if tup.elems.is_empty() {
                    TypeName::Unit
                } else {
                    todo!("Tuples are not currently supported")
                }
            }
            other => panic!("Unsupported type: {}", other.to_token_stream()),
        }
    }

    /// Returns `true` if `self` is the `TypeName::SelfType` variant, otherwise
    /// `false`.
    pub fn is_self(&self) -> bool {
        matches!(self, TypeName::SelfType(_))
    }

    /// Recurse down the type tree, visiting all lifetimes.
    ///
    /// Using this function, you can collect all the lifetimes into a collection,
    /// or examine each one without having to make any additional allocations.
    pub fn visit_lifetimes<'a, F, B>(&'a self, visit: &mut F) -> ControlFlow<B>
    where
        F: FnMut(&'a Lifetime, LifetimeOrigin) -> ControlFlow<B>,
    {
        match self {
            TypeName::Named(path_type) | TypeName::SelfType(path_type) => path_type
                .lifetimes
                .iter()
                .try_for_each(|lt| visit(lt, LifetimeOrigin::Named)),
            TypeName::Reference(lt, _, ty) => {
                ty.visit_lifetimes(visit)?;
                visit(lt, LifetimeOrigin::Reference)
            }
            TypeName::Box(ty) | TypeName::Option(ty) => ty.visit_lifetimes(visit),
            TypeName::Result(ok, err, _) => {
                ok.visit_lifetimes(visit)?;
                err.visit_lifetimes(visit)
            }
            TypeName::StrReference(lt) => visit(lt, LifetimeOrigin::StrReference),
            TypeName::PrimitiveSlice(lt, ..) => visit(lt, LifetimeOrigin::PrimitiveSlice),
            _ => ControlFlow::Continue(()),
        }
    }

    /// Returns `true` if any lifetime satisfies a predicate, otherwise `false`.
    ///
    /// This method is short-circuiting, meaning that if the predicate ever succeeds,
    /// it will return immediately.
    pub fn any_lifetime<'a, F>(&'a self, mut f: F) -> bool
    where
        F: FnMut(&'a Lifetime, LifetimeOrigin) -> bool,
    {
        self.visit_lifetimes(&mut |lifetime, origin| {
            if f(lifetime, origin) {
                ControlFlow::Break(())
            } else {
                ControlFlow::Continue(())
            }
        })
        .is_break()
    }

    /// Returns `true` if all lifetimes satisfy a predicate, otherwise `false`.
    ///
    /// This method is short-circuiting, meaning that if the predicate ever fails,
    /// it will return immediately.
    pub fn all_lifetimes<'a, F>(&'a self, mut f: F) -> bool
    where
        F: FnMut(&'a Lifetime, LifetimeOrigin) -> bool,
    {
        self.visit_lifetimes(&mut |lifetime, origin| {
            if f(lifetime, origin) {
                ControlFlow::Continue(())
            } else {
                ControlFlow::Break(())
            }
        })
        .is_continue()
    }

    /// Returns all lifetimes in a [`LifetimeEnv`] that must live at least as
    /// long as the type.
    pub fn longer_lifetimes<'env>(
        &self,
        lifetime_env: &'env LifetimeEnv,
    ) -> Vec<&'env NamedLifetime> {
        self.transitive_lifetime_bounds(LifetimeTransitivity::longer(lifetime_env))
    }

    /// Returns all lifetimes in a [`LifetimeEnv`] that are outlived by the type.
    pub fn shorter_lifetimes<'env>(
        &self,
        lifetime_env: &'env LifetimeEnv,
    ) -> Vec<&'env NamedLifetime> {
        self.transitive_lifetime_bounds(LifetimeTransitivity::shorter(lifetime_env))
    }

    /// Visits the provided [`LifetimeTransitivity`] value with all `NamedLifetime`s
    /// in the type tree, and returns the transitively reachable lifetimes.
    fn transitive_lifetime_bounds<'env>(
        &self,
        mut transitivity: LifetimeTransitivity<'env>,
    ) -> Vec<&'env NamedLifetime> {
        self.visit_lifetimes(&mut |lifetime, _| -> ControlFlow<()> {
            if let Lifetime::Named(named) = lifetime {
                transitivity.visit(named);
            }
            ControlFlow::Continue(())
        });
        transitivity.finish()
    }

    fn check_opaque<'a>(
        &'a self,
        in_path: &Path,
        env: &Env,
        behind_reference: bool,
        errors: &mut Vec<ValidityError>,
    ) {
        match self {
            TypeName::Reference(.., underlying) => {
                underlying.check_opaque(in_path, env, true, errors)
            }
            TypeName::Box(underlying) => underlying.check_opaque(in_path, env, true, errors),
            TypeName::Option(underlying) => underlying.check_opaque(in_path, env, false, errors),
            TypeName::Result(ok, err, _) => {
                ok.check_opaque(in_path, env, false, errors);
                err.check_opaque(in_path, env, false, errors);
            }
            TypeName::Named(path_type) => {
                if let CustomType::Opaque(_) = path_type.resolve(in_path, env) {
                    if !behind_reference {
                        errors.push(ValidityError::OpaqueAsValue(self.clone()))
                    }
                } else if behind_reference {
                    errors.push(ValidityError::NonOpaqueBehindRef(self.clone()))
                }
            }
            _ => {}
        }
    }

    // Disallow non-pointer containing Option<T> inside struct fields and Result
    fn check_option(&self, errors: &mut Vec<ValidityError>) {
        match self {
            TypeName::Reference(.., underlying) => underlying.check_option(errors),
            TypeName::Box(underlying) => underlying.check_option(errors),
            TypeName::Option(underlying) => {
                if !underlying.is_pointer() {
                    errors.push(ValidityError::OptionNotContainingPointer(self.clone()))
                }
            }
            TypeName::Result(ok, err, _) => {
                ok.check_option(errors);
                err.check_option(errors);
            }
            _ => {}
        }
    }

    /// Checks that any references to opaque structs in parameters or return values
    /// are always behind a box or reference, and that non-opaque custom types are *never* behind
    /// references or boxes.
    ///
    /// Also checks that there are no elided lifetimes in the return type.
    ///
    /// Errors are pushed into the `errors` vector.
    pub fn check_validity<'a>(
        &'a self,
        in_path: &Path,
        env: &Env,
        errors: &mut Vec<ValidityError>,
    ) {
        self.check_opaque(in_path, env, false, errors);
        self.check_option(errors);
    }

    /// Checks the validity of return types.
    ///
    /// This is equivalent to `TypeName::check_validity`, but it also ensures
    /// that the type doesn't elide any lifetimes.
    ///
    /// Once we decide to support lifetime elision in return types, this function
    /// will probably be removed.
    pub fn check_return_type_validity(
        &self,
        in_path: &Path,
        env: &Env,
        errors: &mut Vec<ValidityError>,
    ) {
        self.check_validity(in_path, env, errors);
        self.check_lifetime_elision(self, in_path, env, errors);
    }

    /// Checks that there aren't any elided lifetimes.
    fn check_lifetime_elision(
        &self,
        full_type: &Self,
        in_path: &Path,
        env: &Env,
        errors: &mut Vec<ValidityError>,
    ) {
        match self {
            TypeName::Named(path_type) | TypeName::SelfType(path_type) => {
                let (_path, custom) = path_type.resolve_with_path(in_path, env);
                if let Some(lifetimes) = custom.lifetimes() {
                    let lifetimes_provided = path_type
                        .lifetimes
                        .iter()
                        .filter(|lt| !matches!(lt, Lifetime::Anonymous))
                        .count();

                    if lifetimes_provided != lifetimes.len() {
                        // There's a discrepency between the number of declared
                        // lifetimes and the number of lifetimes provided in
                        // the return type, so there must have been elision.
                        errors.push(ValidityError::LifetimeElisionInReturn {
                            full_type: full_type.clone(),
                            sub_type: self.clone(),
                        });
                    } else {
                        // The struct was written with the number of lifetimes
                        // that it was declared with, so we're good.
                    }
                } else {
                    // `CustomType::Enum`, which doesn't have any lifetimes.
                    // We already checked that enums don't have generics in
                    // core.
                }
            }
            TypeName::Reference(lifetime, _, ty) => {
                if let Lifetime::Anonymous = lifetime {
                    errors.push(ValidityError::LifetimeElisionInReturn {
                        full_type: full_type.clone(),
                        sub_type: self.clone(),
                    });
                }

                ty.check_lifetime_elision(full_type, in_path, env, errors);
            }
            TypeName::Box(ty) | TypeName::Option(ty) => {
                ty.check_lifetime_elision(full_type, in_path, env, errors)
            }
            TypeName::Result(ok, err, _) => {
                ok.check_lifetime_elision(full_type, in_path, env, errors);
                err.check_lifetime_elision(full_type, in_path, env, errors);
            }
            TypeName::StrReference(Lifetime::Anonymous)
            | TypeName::PrimitiveSlice(Lifetime::Anonymous, ..) => {
                errors.push(ValidityError::LifetimeElisionInReturn {
                    full_type: full_type.clone(),
                    sub_type: self.clone(),
                });
            }
            _ => {}
        }
    }

    pub fn is_zst(&self) -> bool {
        // check_zst() prevents non-unit types from being ZSTs
        matches!(*self, TypeName::Unit)
    }

    pub fn is_pointer(&self) -> bool {
        matches!(*self, TypeName::Reference(..) | TypeName::Box(_))
    }
}

#[non_exhaustive]
pub enum LifetimeOrigin {
    Named,
    Reference,
    StrReference,
    PrimitiveSlice,
}

fn is_runtime_type(p: &TypePath, name: &str) -> bool {
    (p.path.segments.len() == 1 && p.path.segments[0].ident == name)
        || (p.path.segments.len() == 2
            && p.path.segments[0].ident == "diplomat_runtime"
            && p.path.segments[1].ident == name)
}

impl fmt::Display for TypeName {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            TypeName::Primitive(p) => p.fmt(f),
            TypeName::Named(p) | TypeName::SelfType(p) => p.fmt(f),
            TypeName::Reference(lifetime, mutability, typ) => {
                write!(f, "{}{typ}", ReferenceDisplay(lifetime, mutability))
            }
            TypeName::Box(typ) => write!(f, "Box<{typ}>"),
            TypeName::Option(typ) => write!(f, "Option<{typ}>"),
            TypeName::Result(ok, err, _) => {
                write!(f, "Result<{ok}, {err}>")
            }
            TypeName::Writeable => "DiplomatWriteable".fmt(f),
            TypeName::StrReference(lifetime) => {
                write!(
                    f,
                    "{}str",
                    ReferenceDisplay(lifetime, &Mutability::Immutable)
                )
            }
            TypeName::PrimitiveSlice(lifetime, mutability, typ) => {
                write!(f, "{}[{typ}]", ReferenceDisplay(lifetime, mutability))
            }
            TypeName::Unit => "()".fmt(f),
        }
    }
}

/// An [`fmt::Display`] type for formatting Rust references.
///
/// # Examples
///
/// ```ignore
/// let lifetime = Lifetime::from(&syn::parse_str::<syn::Lifetime>("'a"));
/// let mutability = Mutability::Mutable;
/// // ...
/// let fmt = format!("{}[u8]", ReferenceDisplay(&lifetime, &mutability));
///
/// assert_eq!(fmt, "&'a mut [u8]");
/// ```
struct ReferenceDisplay<'a>(&'a Lifetime, &'a Mutability);

impl<'a> fmt::Display for ReferenceDisplay<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.0 {
            Lifetime::Static => "&'static ".fmt(f)?,
            Lifetime::Named(lifetime) => write!(f, "&{lifetime} ")?,
            Lifetime::Anonymous => '&'.fmt(f)?,
        }

        if self.1.is_mutable() {
            "mut ".fmt(f)?;
        }

        Ok(())
    }
}

impl fmt::Display for PathType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.path.fmt(f)?;

        if let Some((first, rest)) = self.lifetimes.split_first() {
            write!(f, "<{first}")?;
            for lifetime in rest {
                write!(f, ", {lifetime}")?;
            }
            '>'.fmt(f)?;
        }
        Ok(())
    }
}

/// A built-in Rust primitive scalar type.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[allow(non_camel_case_types)]
#[allow(clippy::exhaustive_enums)] // there are only these (scalar types)
pub enum PrimitiveType {
    i8,
    u8,
    i16,
    u16,
    i32,
    u32,
    i64,
    u64,
    i128,
    u128,
    isize,
    usize,
    f32,
    f64,
    bool,
    char,
}

impl fmt::Display for PrimitiveType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            PrimitiveType::i8 => "i8",
            PrimitiveType::u8 => "u8",
            PrimitiveType::i16 => "i16",
            PrimitiveType::u16 => "u16",
            PrimitiveType::i32 => "i32",
            PrimitiveType::u32 => "u32",
            PrimitiveType::i64 => "i64",
            PrimitiveType::u64 => "u64",
            PrimitiveType::i128 => "i128",
            PrimitiveType::u128 => "u128",
            PrimitiveType::isize => "isize",
            PrimitiveType::usize => "usize",
            PrimitiveType::f32 => "f32",
            PrimitiveType::f64 => "f64",
            PrimitiveType::bool => "bool",
            PrimitiveType::char => "char",
        }
        .fmt(f)
    }
}

lazy_static! {
    static ref PRIMITIVES_MAPPING: [(&'static str, PrimitiveType); 16] = [
        ("i8", PrimitiveType::i8),
        ("u8", PrimitiveType::u8),
        ("i16", PrimitiveType::i16),
        ("u16", PrimitiveType::u16),
        ("i32", PrimitiveType::i32),
        ("u32", PrimitiveType::u32),
        ("i64", PrimitiveType::i64),
        ("u64", PrimitiveType::u64),
        ("i128", PrimitiveType::i128),
        ("u128", PrimitiveType::u128),
        ("isize", PrimitiveType::isize),
        ("usize", PrimitiveType::usize),
        ("f32", PrimitiveType::f32),
        ("f64", PrimitiveType::f64),
        ("bool", PrimitiveType::bool),
        ("char", PrimitiveType::char),
    ];
    static ref STRING_TO_PRIMITIVE: HashMap<&'static str, PrimitiveType> =
        PRIMITIVES_MAPPING.iter().cloned().collect();
    static ref PRIMITIVE_TO_STRING: HashMap<PrimitiveType, &'static str> =
        PRIMITIVES_MAPPING.iter().map(|t| (t.1, t.0)).collect();
}

#[cfg(test)]
mod tests {
    use insta;

    use syn;

    use super::TypeName;

    #[test]
    fn typename_primitives() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                i32
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                usize
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                bool
            },
            None
        ));
    }

    #[test]
    fn typename_named() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                MyLocalStruct
            },
            None
        ));
    }

    #[test]
    fn typename_references() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                &i32
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                &mut MyLocalStruct
            },
            None
        ));
    }

    #[test]
    fn typename_boxes() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Box<i32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Box<MyLocalStruct>
            },
            None
        ));
    }

    #[test]
    fn typename_option() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Option<i32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Option<MyLocalStruct>
            },
            None
        ));
    }

    #[test]
    fn typename_result() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatResult<MyLocalStruct, i32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatResult<(), MyLocalStruct>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Result<MyLocalStruct, i32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Result<(), MyLocalStruct>
            },
            None
        ));
    }

    #[test]
    fn lifetimes() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Foo<'a, 'b>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                ::core::my_type::Foo
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                ::core::my_type::Foo<'test>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Option<Ref<'object>>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Foo<'a, 'b, 'c, 'd>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                very::long::path::to::my::Type<'x, 'y, 'z>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Result<OkRef<'a, 'b>, ErrRef<'c>>
            },
            None
        ));
    }
}
