//! This crate provides helper types for matching against enum variants, and
//! extracting bindings to each of the fields in the deriving Struct or Enum in
//! a generic way.
//!
//! If you are writing a `#[derive]` which needs to perform some operation on
//! every field, then you have come to the right place!
//!
//! # Example: `WalkFields`
//! ### Trait Implementation
//! ```
//! pub trait WalkFields: std::any::Any {
//!     fn walk_fields(&self, walk: &mut FnMut(&WalkFields));
//! }
//! impl WalkFields for i32 {
//!     fn walk_fields(&self, _walk: &mut FnMut(&WalkFields)) {}
//! }
//! ```
//!
//! ### Custom Derive
//! ```
//! #[macro_use]
//! extern crate synstructure;
//! #[macro_use]
//! extern crate quote;
//!
//! fn walkfields_derive(s: synstructure::Structure) -> quote::Tokens {
//!     let body = s.each(|bi| quote!{
//!         walk(#bi)
//!     });
//!
//!     s.bound_impl(quote!(synstructure_test_traits::WalkFields), quote!{
//!         fn walk_fields(&self, walk: &mut FnMut(&synstructure_test_traits::WalkFields)) {
//!             match *self { #body }
//!         }
//!     })
//! }
//! # const _IGNORE: &'static str = stringify!(
//! decl_derive!([WalkFields] => walkfields_derive);
//! # );
//!
//! /*
//!  * Test Case
//!  */
//! fn main() {
//!     test_derive! {
//!         walkfields_derive {
//!             enum A<T> {
//!                 B(i32, T),
//!                 C(i32),
//!             }
//!         }
//!         expands to {
//!             #[allow(non_upper_case_globals)]
//!             const _DERIVE_synstructure_test_traits_WalkFields_FOR_A: () = {
//!                 extern crate synstructure_test_traits;
//!                 impl<T> synstructure_test_traits::WalkFields for A<T>
//!                     where T: synstructure_test_traits::WalkFields
//!                 {
//!                     fn walk_fields(&self, walk: &mut FnMut(&synstructure_test_traits::WalkFields)) {
//!                         match *self {
//!                             A::B(ref __binding_0, ref __binding_1,) => {
//!                                 { walk(__binding_0) }
//!                                 { walk(__binding_1) }
//!                             }
//!                             A::C(ref __binding_0,) => {
//!                                 { walk(__binding_0) }
//!                             }
//!                         }
//!                     }
//!                 }
//!             };
//!         }
//!     }
//! }
//! ```
//!
//! # Example: `Interest`
//! ### Trait Implementation
//! ```
//! pub trait Interest {
//!     fn interesting(&self) -> bool;
//! }
//! impl Interest for i32 {
//!     fn interesting(&self) -> bool { *self > 0 }
//! }
//! ```
//!
//! ### Custom Derive
//! ```
//! #[macro_use]
//! extern crate synstructure;
//! #[macro_use]
//! extern crate quote;
//!
//! fn interest_derive(mut s: synstructure::Structure) -> quote::Tokens {
//!     let body = s.fold(false, |acc, bi| quote!{
//!         #acc || synstructure_test_traits::Interest::interesting(#bi)
//!     });
//!
//!     s.bound_impl(quote!(synstructure_test_traits::Interest), quote!{
//!         fn interesting(&self) -> bool {
//!             match *self {
//!                 #body
//!             }
//!         }
//!     })
//! }
//! # const _IGNORE: &'static str = stringify!(
//! decl_derive!([Interest] => interest_derive);
//! # );
//!
//! /*
//!  * Test Case
//!  */
//! fn main() {
//!     test_derive!{
//!         interest_derive {
//!             enum A<T> {
//!                 B(i32, T),
//!                 C(i32),
//!             }
//!         }
//!         expands to {
//!             #[allow(non_upper_case_globals)]
//!             const _DERIVE_synstructure_test_traits_Interest_FOR_A: () = {
//!                 extern crate synstructure_test_traits;
//!                 impl<T> synstructure_test_traits::Interest for A<T>
//!                     where T: synstructure_test_traits::Interest
//!                 {
//!                     fn interesting(&self) -> bool {
//!                         match *self {
//!                             A::B(ref __binding_0, ref __binding_1,) => {
//!                                 false ||
//!                                     synstructure_test_traits::Interest::interesting(__binding_0) ||
//!                                     synstructure_test_traits::Interest::interesting(__binding_1)
//!                             }
//!                             A::C(ref __binding_0,) => {
//!                                 false ||
//!                                     synstructure_test_traits::Interest::interesting(__binding_0)
//!                             }
//!                         }
//!                     }
//!                 }
//!             };
//!         }
//!     }
//! }
//! ```
//!
//! For more example usage, consider investigating the `abomonation_derive` crate,
//! which makes use of this crate, and is fairly simple.

extern crate proc_macro;
extern crate proc_macro2;
#[macro_use]
extern crate quote;
extern crate syn;
extern crate unicode_xid;

use std::collections::HashSet;

use syn::{
    Generics, Ident, Attribute, Field, Fields, Expr, DeriveInput,
    TraitBound, WhereClause, GenericParam, Data, WherePredicate,
    TypeParamBound, Type, TypeMacro, FieldsUnnamed, FieldsNamed,
    PredicateType, TypePath, token, punctuated,
};
use syn::visit::{self, Visit};

use quote::{ToTokens, Tokens};

use unicode_xid::UnicodeXID;

use proc_macro2::Span;

// NOTE: This module has documentation hidden, as it only exports macros (which
// always appear in the root of the crate) and helper methods / re-exports used
// in the implementation of those macros.
#[doc(hidden)]
pub mod macros;

/// The type of binding to use when generating a pattern.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum BindStyle {
    /// `x`
    Move,
    /// `mut x`
    MoveMut,
    /// `ref x`
    Ref,
    /// `ref mut x`
    RefMut,
}

impl ToTokens for BindStyle {
    fn to_tokens(&self, tokens: &mut Tokens) {
        match *self {
            BindStyle::Move => {}
            BindStyle::MoveMut => quote_spanned!(Span::call_site() => mut).to_tokens(tokens),
            BindStyle::Ref => quote_spanned!(Span::call_site() => ref).to_tokens(tokens),
            BindStyle::RefMut => quote_spanned!(Span::call_site() => ref mut).to_tokens(tokens),
        }
    }
}

// Internal method for merging seen_generics arrays together.
fn generics_fuse(res: &mut Vec<bool>, new: &[bool]) {
    for (i, &flag) in new.iter().enumerate() {
        if i == res.len() {
            res.push(false);
        }
        if flag {
            res[i] = true;
        }
    }
}

// Internal method for extracting the set of generics which have been matched
fn fetch_generics<'a>(set: &[bool], generics: &'a Generics) -> Vec<&'a Ident> {
    let mut tys = vec![];
    for (&seen, param) in set.iter().zip(generics.params.iter()) {
        if seen {
            match *param {
                GenericParam::Type(ref tparam) => tys.push(&tparam.ident),
                _ => {}
            }
        }
    }
    tys
}

// Internal method for sanitizing an identifier for hygiene purposes.
fn sanitize_ident(s: &str) -> Ident {
    let mut res = String::with_capacity(s.len());
    for mut c in s.chars() {
        if ! UnicodeXID::is_xid_continue(c) { c = '_' }
        // Deduplicate consecutive _ characters.
        if res.ends_with('_') && c == '_' { continue }
        res.push(c);
    }
    Ident::new(&res, Span::def_site())
}

/// Information about a specific binding. This contains both an `Ident`
/// reference to the given field, and the syn `&'a Field` descriptor for that
/// field.
///
/// This type supports `quote::ToTokens`, so can be directly used within the
/// `quote!` macro. It expands to a reference to the matched field.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct BindingInfo<'a> {
    /// The name which this BindingInfo will bind to.
    pub binding: Ident,

    /// The type of binding which this BindingInfo will create.
    pub style: BindStyle,

    field: &'a Field,

    // These are used to determine which type parameters are avaliable.
    generics: &'a Generics,
    seen_generics: Vec<bool>,
}

impl<'a> ToTokens for BindingInfo<'a> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        self.binding.to_tokens(tokens);
    }
}

impl<'a> BindingInfo<'a> {
    /// Returns a reference to the underlying `syn` AST node which this
    /// `BindingInfo` references
    pub fn ast(&self) -> &'a Field {
        self.field
    }

    /// Generates the pattern fragment for this field binding.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B{ a: i32, b: i32 },
    ///         C(u32),
    ///     }
    /// };
    /// let s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.variants()[0].bindings()[0].pat(),
    ///     quote! {
    ///         ref __binding_0
    ///     }
    /// );
    /// # }
    /// ```
    pub fn pat(&self) -> Tokens {
        let BindingInfo {
            ref binding,
            ref style,
            ..
        } = *self;
        quote!(#style #binding)
    }

    /// Returns a list of the type parameters which are referenced in this
    /// field's type.
    ///
    /// # Caveat
    ///
    /// If the field contains any macros in type position, all parameters will
    /// be considered bound. This is because we cannot determine which type
    /// parameters are bound by type macros.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     struct A<T, U> {
    ///         a: Option<T>,
    ///         b: U,
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.variants()[0].bindings()[0].referenced_ty_params(),
    ///     &[&(syn::Ident::from("T"))]
    /// );
    /// # }
    /// ```
    pub fn referenced_ty_params(&self) -> Vec<&'a Ident> {
        fetch_generics(&self.seen_generics, self.generics)
    }
}

/// This type is similar to `syn`'s `Variant` type, however each of the fields
/// are references rather than owned. When this is used as the AST for a real
/// variant, this struct simply borrows the fields of the `syn::Variant`,
/// however this type may also be used as the sole variant for a struct.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct VariantAst<'a> {
    pub attrs: &'a [Attribute],
    pub ident: &'a Ident,
    pub fields: &'a Fields,
    pub discriminant: &'a Option<(token::Eq, Expr)>,
}

/// A wrapper around a `syn::DeriveInput`'s variant which provides utilities
/// for destructuring `Variant`s with `match` expressions.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct VariantInfo<'a> {
    pub prefix: Option<&'a Ident>,
    bindings: Vec<BindingInfo<'a>>,
    omitted_fields: bool,
    ast: VariantAst<'a>,
    generics: &'a Generics,
}

/// Helper function used by the VariantInfo constructor. Walks all of the types
/// in `field` and returns a list of the type parameters from `ty_params` which
/// are referenced in the field.
fn get_ty_params<'a>(field: &Field, generics: &Generics) -> Vec<bool> {
    // Helper type. Discovers all identifiers inside of the visited type,
    // and calls a callback with them.
    struct BoundTypeLocator<'a> {
        result: Vec<bool>,
        generics: &'a Generics,
    }

    impl<'a> Visit<'a> for BoundTypeLocator<'a> {
        // XXX: This also (intentionally) captures paths like T::SomeType. Is
        // this desirable?
        fn visit_ident(&mut self, id: &Ident) {
            for (idx, i) in self.generics.params.iter().enumerate() {
                if let GenericParam::Type(ref tparam) = *i {
                    if tparam.ident == id {
                        self.result[idx] = true;
                    }
                }
            }
        }

        fn visit_type_macro(&mut self, x: &'a TypeMacro) {
            // If we see a type_mac declaration, then we can't know what type parameters
            // it might be binding, so we presume it binds all of them.
            for r in &mut self.result {
                *r = true;
            }
            visit::visit_type_macro(self, x)
        }
    }

    let mut btl = BoundTypeLocator {
        result: vec![false; generics.params.len()],
        generics: generics,
    };

    btl.visit_type(&field.ty);

    btl.result
}

impl<'a> VariantInfo<'a> {
    fn new(ast: VariantAst<'a>, prefix: Option<&'a Ident>, generics: &'a Generics) -> Self {
        let bindings = match *ast.fields {
            Fields::Unit => vec![],
            Fields::Unnamed(FieldsUnnamed { unnamed: ref fields, .. }) |
            Fields::Named(FieldsNamed { named: ref fields, .. }) => {
                fields.into_iter()
                    .enumerate()
                    .map(|(i, field)| {
                        BindingInfo {
                            // XXX: This has to be call_site to avoid privacy
                            // when deriving on private fields.
                            binding: Ident::new(
                                &format!("__binding_{}", i),
                                Span::call_site(),
                            ),
                            style: BindStyle::Ref,
                            field: field,
                            generics: generics,
                            seen_generics: get_ty_params(field, generics),
                        }
                    })
                    .collect::<Vec<_>>()
            }
        };

        VariantInfo {
            prefix: prefix,
            bindings: bindings,
            omitted_fields: false,
            ast: ast,
            generics: generics,
        }
    }

    /// Returns a slice of the bindings in this Variant.
    pub fn bindings(&self) -> &[BindingInfo<'a>] {
        &self.bindings
    }

    /// Returns a mut slice of the bindings in this Variant.
    pub fn bindings_mut(&mut self) -> &mut [BindingInfo<'a>] {
        &mut self.bindings
    }

    /// Returns a `VariantAst` object which contains references to the
    /// underlying `syn` AST node which this `Variant` was created from.
    pub fn ast(&self) -> VariantAst<'a> {
        self.ast
    }

    /// True if any bindings were omitted due to a `filter` call.
    pub fn omitted_bindings(&self) -> bool {
        self.omitted_fields
    }

    /// Generates the match-arm pattern which could be used to match against this Variant.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    /// let s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.variants()[0].pat(),
    ///     quote!{
    ///         A::B(ref __binding_0, ref __binding_1,)
    ///     }
    /// );
    /// # }
    /// ```
    pub fn pat(&self) -> Tokens {
        let mut t = Tokens::new();
        if let Some(prefix) = self.prefix {
            prefix.to_tokens(&mut t);
            quote!(::).to_tokens(&mut t);
        }
        self.ast.ident.to_tokens(&mut t);
        match *self.ast.fields {
            Fields::Unit => {
                assert!(self.bindings.len() == 0);
            }
            Fields::Unnamed(..) => {
                token::Paren(Span::call_site()).surround(&mut t, |t| {
                    for binding in &self.bindings {
                        binding.pat().to_tokens(t);
                        quote!(,).to_tokens(t);
                    }
                    if self.omitted_fields {
                        quote!(..).to_tokens(t);
                    }
                })
            }
            Fields::Named(..) => {
                token::Brace(Span::call_site()).surround(&mut t, |t| {
                    for binding in &self.bindings {
                        binding.field.ident.to_tokens(t);
                        quote!(:).to_tokens(t);
                        binding.pat().to_tokens(t);
                        quote!(,).to_tokens(t);
                    }
                    if self.omitted_fields {
                        quote!(..).to_tokens(t);
                    }
                })
            }
        }
        t
    }

    /// Generates the token stream required to construct the current variant.
    ///
    /// The init array initializes each of the fields in the order they are
    /// written in `variant.ast().fields`.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(usize, usize),
    ///         C{ v: usize },
    ///     }
    /// };
    /// let s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.variants()[0].construct(|_, i| quote!(#i)),
    ///
    ///     quote!{
    ///         A::B(0usize, 1usize,)
    ///     }
    /// );
    ///
    /// assert_eq!(
    ///     s.variants()[1].construct(|_, i| quote!(#i)),
    ///
    ///     quote!{
    ///         A::C{ v: 0usize, }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn construct<F, T>(&self, mut func: F) -> Tokens
    where
        F: FnMut(&Field, usize) -> T,
        T: ToTokens,
    {
        let mut t = Tokens::new();
        if let Some(prefix) = self.prefix {
            quote!(#prefix ::).to_tokens(&mut t);
        }
        self.ast.ident.to_tokens(&mut t);

        match *self.ast.fields {
            Fields::Unit => (),
            Fields::Unnamed(FieldsUnnamed { ref unnamed, .. }) => {
                token::Paren::default().surround(&mut t, |t| {
                    for (i, field) in unnamed.into_iter().enumerate() {
                        func(field, i).to_tokens(t);
                        quote!(,).to_tokens(t);
                    }
                })
            }
            Fields::Named(FieldsNamed { ref named, .. }) => {
                token::Brace::default().surround(&mut t, |t| {
                    for (i, field) in named.into_iter().enumerate() {
                        field.ident.to_tokens(t);
                        quote!(:).to_tokens(t);
                        func(field, i).to_tokens(t);
                        quote!(,).to_tokens(t);
                    }
                })
            }
        }
        t
    }

    /// Runs the passed-in function once for each bound field, passing in a `BindingInfo`.
    /// and generating a `match` arm which evaluates the returned tokens.
    ///
    /// This method will ignore fields which are ignored through the `filter`
    /// method.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    /// let s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.variants()[0].each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::B(ref __binding_0, ref __binding_1,) => {
    ///             { println!("{:?}", __binding_0) }
    ///             { println!("{:?}", __binding_1) }
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn each<F, R>(&self, mut f: F) -> Tokens
    where
        F: FnMut(&BindingInfo) -> R,
        R: ToTokens,
    {
        let pat = self.pat();
        let mut body = Tokens::new();
        for binding in &self.bindings {
            token::Brace::default().surround(&mut body, |body| {
                f(binding).to_tokens(body);
            });
        }
        quote!(#pat => { #body })
    }

    /// Runs the passed-in function once for each bound field, passing in the
    /// result of the previous call, and a `BindingInfo`. generating a `match`
    /// arm which evaluates to the resulting tokens.
    ///
    /// This method will ignore fields which are ignored through the `filter`
    /// method.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    /// let s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.variants()[0].fold(quote!(0), |acc, bi| quote!(#acc + #bi)),
    ///
    ///     quote!{
    ///         A::B(ref __binding_0, ref __binding_1,) => {
    ///             0 + __binding_0 + __binding_1
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn fold<F, I, R>(&self, init: I, mut f: F) -> Tokens
    where
        F: FnMut(Tokens, &BindingInfo) -> R,
        I: ToTokens,
        R: ToTokens,
    {
        let pat = self.pat();
        let body = self.bindings.iter().fold(quote!(#init), |i, bi| {
            let r = f(i, bi);
            quote!(#r)
        });
        quote!(#pat => { #body })
    }

    /// Filter the bindings created by this `Variant` object. This has 2 effects:
    ///
    /// * The bindings will no longer appear in match arms generated by methods
    ///   on this `Variant` or its subobjects.
    ///
    /// * Impl blocks created with the `bound_impl` or `unsafe_bound_impl`
    ///   method only consider type parameters referenced in the types of
    ///   non-filtered fields.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B{ a: i32, b: i32 },
    ///         C{ a: u32 },
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.variants_mut()[0].filter(|bi| {
    ///     bi.ast().ident == Some("b".into())
    /// });
    ///
    /// assert_eq!(
    ///     s.each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::B{ b: ref __binding_1, .. } => {
    ///             { println!("{:?}", __binding_1) }
    ///         }
    ///         A::C{ a: ref __binding_0, } => {
    ///             { println!("{:?}", __binding_0) }
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn filter<F>(&mut self, f: F) -> &mut Self
    where
        F: FnMut(&BindingInfo) -> bool,
    {
        let before_len = self.bindings.len();
        self.bindings.retain(f);
        if self.bindings.len() != before_len {
            self.omitted_fields = true;
        }
        self
    }

    /// Remove the binding at the given index.
    ///
    /// # Panics
    ///
    /// Panics if the index is out of range.
    pub fn remove_binding(&mut self, idx: usize) -> &mut Self {
        self.bindings.remove(idx);
        self.omitted_fields = true;
        self
    }

    /// Updates the `BindStyle` for each of the passed-in fields by calling the
    /// passed-in function for each `BindingInfo`.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.variants_mut()[0].bind_with(|bi| BindStyle::RefMut);
    ///
    /// assert_eq!(
    ///     s.each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::B(ref mut __binding_0, ref mut __binding_1,) => {
    ///             { println!("{:?}", __binding_0) }
    ///             { println!("{:?}", __binding_1) }
    ///         }
    ///         A::C(ref __binding_0,) => {
    ///             { println!("{:?}", __binding_0) }
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn bind_with<F>(&mut self, mut f: F) -> &mut Self
    where
        F: FnMut(&BindingInfo) -> BindStyle,
    {
        for binding in &mut self.bindings {
            binding.style = f(&binding);
        }
        self
    }

    /// Updates the binding name for each fo the passed-in fields by calling the
    /// passed-in function for each `BindingInfo`.
    ///
    /// The function will be called with the `BindingInfo` and its index in the
    /// enclosing variant.
    ///
    /// The default name is `__binding_{}` where `{}` is replaced with an
    /// increasing number.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B{ a: i32, b: i32 },
    ///         C{ a: u32 },
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.variants_mut()[0].binding_name(|bi, i| bi.ident.clone().unwrap());
    ///
    /// assert_eq!(
    ///     s.each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::B{ a: ref a, b: ref b, } => {
    ///             { println!("{:?}", a) }
    ///             { println!("{:?}", b) }
    ///         }
    ///         A::C{ a: ref __binding_0, } => {
    ///             { println!("{:?}", __binding_0) }
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn binding_name<F>(&mut self, mut f: F) -> &mut Self
    where
        F: FnMut(&Field, usize) -> Ident,
    {
        for (it, binding) in self.bindings.iter_mut().enumerate() {
            binding.binding = f(binding.field, it);
        }
        self
    }

    /// Returns a list of the type parameters which are referenced in this
    /// field's type.
    ///
    /// # Caveat
    ///
    /// If the field contains any macros in type position, all parameters will
    /// be considered bound. This is because we cannot determine which type
    /// parameters are bound by type macros.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     struct A<T, U> {
    ///         a: Option<T>,
    ///         b: U,
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.variants()[0].bindings()[0].referenced_ty_params(),
    ///     &[&(syn::Ident::from("T"))]
    /// );
    /// # }
    /// ```
    pub fn referenced_ty_params(&self) -> Vec<&'a Ident> {
        let mut flags = Vec::new();
        for binding in &self.bindings {
            generics_fuse(&mut flags, &binding.seen_generics);
        }
        fetch_generics(&flags, self.generics)
    }
}

/// A wrapper around a `syn::DeriveInput` which provides utilities for creating
/// custom derive trait implementations.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Structure<'a> {
    variants: Vec<VariantInfo<'a>>,
    omitted_variants: bool,
    ast: &'a DeriveInput,
}

impl<'a> Structure<'a> {
    /// Create a new `Structure` with the variants and fields from the passed-in
    /// `DeriveInput`.
    pub fn new(ast: &'a DeriveInput) -> Self {
        let variants = match ast.data {
            Data::Enum(ref data) => {
                (&data.variants).into_iter()
                    .map(|v| {
                        VariantInfo::new(
                            VariantAst {
                                attrs: &v.attrs,
                                ident: &v.ident,
                                fields: &v.fields,
                                discriminant: &v.discriminant
                            },
                            Some(&ast.ident),
                            &ast.generics,
                        )
                    })
                    .collect::<Vec<_>>()
            }
            Data::Struct(ref data) => {
                // SAFETY NOTE: Normally putting an `Expr` in static storage
                // wouldn't be safe, because it could contain `Term` objects
                // which use thread-local interning. However, this static always
                // contains the value `None`. Thus, it will never contain any
                // unsafe values.
                struct UnsafeMakeSync(Option<(token::Eq, Expr)>);
                unsafe impl Sync for UnsafeMakeSync {}
                static NONE_DISCRIMINANT: UnsafeMakeSync = UnsafeMakeSync(None);

                vec![
                    VariantInfo::new(
                        VariantAst {
                            attrs: &ast.attrs,
                            ident: &ast.ident,
                            fields: &data.fields,
                            discriminant: &NONE_DISCRIMINANT.0,
                        },
                        None,
                        &ast.generics,
                    ),
                ]
            }
            Data::Union(_) => {
                panic!("synstructure does not handle untagged unions \
                    (https://github.com/mystor/synstructure/issues/6)");
            }
        };

        Structure {
            variants: variants,
            omitted_variants: false,
            ast: ast,
        }
    }

    /// Returns a slice of the variants in this Structure.
    pub fn variants(&self) -> &[VariantInfo<'a>] {
        &self.variants
    }

    /// Returns a mut slice of the variants in this Structure.
    pub fn variants_mut(&mut self) -> &mut [VariantInfo<'a>] {
        &mut self.variants
    }

    /// Returns a reference to the underlying `syn` AST node which this
    /// `Structure` was created from.
    pub fn ast(&self) -> &'a DeriveInput {
        self.ast
    }

    /// True if any variants were omitted due to a `filter_variants` call.
    pub fn omitted_variants(&self) -> bool {
        self.omitted_variants
    }

    /// Runs the passed-in function once for each bound field, passing in a `BindingInfo`.
    /// and generating `match` arms which evaluate the returned tokens.
    ///
    /// This method will ignore variants or fields which are ignored through the
    /// `filter` and `filter_variant` methods.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    /// let s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::B(ref __binding_0, ref __binding_1,) => {
    ///             { println!("{:?}", __binding_0) }
    ///             { println!("{:?}", __binding_1) }
    ///         }
    ///         A::C(ref __binding_0,) => {
    ///             { println!("{:?}", __binding_0) }
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn each<F, R>(&self, mut f: F) -> Tokens
    where
        F: FnMut(&BindingInfo) -> R,
        R: ToTokens,
    {
        let mut t = Tokens::new();
        for variant in &self.variants {
            variant.each(&mut f).to_tokens(&mut t);
        }
        if self.omitted_variants {
            quote!(_ => {}).to_tokens(&mut t);
        }
        t
    }

    /// Runs the passed-in function once for each bound field, passing in the
    /// result of the previous call, and a `BindingInfo`. generating `match`
    /// arms which evaluate to the resulting tokens.
    ///
    /// This method will ignore variants or fields which are ignored through the
    /// `filter` and `filter_variant` methods.
    ///
    /// If a variant has been ignored, it will return the `init` value.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    /// let s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.fold(quote!(0), |acc, bi| quote!(#acc + #bi)),
    ///
    ///     quote!{
    ///         A::B(ref __binding_0, ref __binding_1,) => {
    ///             0 + __binding_0 + __binding_1
    ///         }
    ///         A::C(ref __binding_0,) => {
    ///             0 + __binding_0
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn fold<F, I, R>(&self, init: I, mut f: F) -> Tokens
    where
        F: FnMut(Tokens, &BindingInfo) -> R,
        I: ToTokens,
        R: ToTokens,
    {
        let mut t = Tokens::new();
        for variant in &self.variants {
            variant.fold(&init, &mut f).to_tokens(&mut t);
        }
        if self.omitted_variants {
            quote!(_ => { #init }).to_tokens(&mut t);
        }
        t
    }

    /// Runs the passed-in function once for each variant, passing in a
    /// `VariantInfo`. and generating `match` arms which evaluate the returned
    /// tokens.
    ///
    /// This method will ignore variants and not bind fields which are ignored
    /// through the `filter` and `filter_variant` methods.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    /// let s = Structure::new(&di);
    ///
    /// assert_eq!(
    ///     s.each_variant(|v| {
    ///         let name = &v.ast().ident;
    ///         quote!(println!(stringify!(#name)))
    ///     }),
    ///
    ///     quote!{
    ///         A::B(ref __binding_0, ref __binding_1,) => {
    ///             println!(stringify!(B))
    ///         }
    ///         A::C(ref __binding_0,) => {
    ///             println!(stringify!(C))
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn each_variant<F, R>(&self, mut f: F) -> Tokens
    where
        F: FnMut(&VariantInfo) -> R,
        R: ToTokens,
    {
        let mut t = Tokens::new();
        for variant in &self.variants {
            let pat = variant.pat();
            let body = f(variant);
            quote!(#pat => { #body }).to_tokens(&mut t);
        }
        if self.omitted_variants {
            quote!(_ => {}).to_tokens(&mut t);
        }
        t
    }

    /// Filter the bindings created by this `Structure` object. This has 2 effects:
    ///
    /// * The bindings will no longer appear in match arms generated by methods
    ///   on this `Structure` or its subobjects.
    ///
    /// * Impl blocks created with the `bound_impl` or `unsafe_bound_impl`
    ///   method only consider type parameters referenced in the types of
    ///   non-filtered fields.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B{ a: i32, b: i32 },
    ///         C{ a: u32 },
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.filter(|bi| { bi.ast().ident == Some("a".into()) });
    ///
    /// assert_eq!(
    ///     s.each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::B{ a: ref __binding_0, .. } => {
    ///             { println!("{:?}", __binding_0) }
    ///         }
    ///         A::C{ a: ref __binding_0, } => {
    ///             { println!("{:?}", __binding_0) }
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn filter<F>(&mut self, mut f: F) -> &mut Self
    where
        F: FnMut(&BindingInfo) -> bool,
    {
        for variant in &mut self.variants {
            variant.filter(&mut f);
        }
        self
    }

    /// Filter the variants matched by this `Structure` object. This has 2 effects:
    ///
    /// * Match arms destructuring these variants will no longer be generated by
    ///   methods on this `Structure`
    ///
    /// * Impl blocks created with the `bound_impl` or `unsafe_bound_impl`
    ///   method only consider type parameters referenced in the types of
    ///   fields in non-fitered variants.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    ///
    /// let mut s = Structure::new(&di);
    ///
    /// s.filter_variants(|v| v.ast().ident != "B");
    ///
    /// assert_eq!(
    ///     s.each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::C(ref __binding_0,) => {
    ///             { println!("{:?}", __binding_0) }
    ///         }
    ///         _ => {}
    ///     }
    /// );
    /// # }
    /// ```
    pub fn filter_variants<F>(&mut self, f: F) -> &mut Self
    where
        F: FnMut(&VariantInfo) -> bool,
    {
        let before_len = self.variants.len();
        self.variants.retain(f);
        if self.variants.len() != before_len {
            self.omitted_variants = true;
        }
        self
    }

    /// Remove the variant at the given index.
    ///
    /// # Panics
    ///
    /// Panics if the index is out of range.
    pub fn remove_variant(&mut self, idx: usize) -> &mut Self {
        self.variants.remove(idx);
        self.omitted_variants = true;
        self
    }

    /// Updates the `BindStyle` for each of the passed-in fields by calling the
    /// passed-in function for each `BindingInfo`.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B(i32, i32),
    ///         C(u32),
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.bind_with(|bi| BindStyle::RefMut);
    ///
    /// assert_eq!(
    ///     s.each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::B(ref mut __binding_0, ref mut __binding_1,) => {
    ///             { println!("{:?}", __binding_0) }
    ///             { println!("{:?}", __binding_1) }
    ///         }
    ///         A::C(ref mut __binding_0,) => {
    ///             { println!("{:?}", __binding_0) }
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn bind_with<F>(&mut self, mut f: F) -> &mut Self
    where
        F: FnMut(&BindingInfo) -> BindStyle,
    {
        for variant in &mut self.variants {
            variant.bind_with(&mut f);
        }
        self
    }

    /// Updates the binding name for each fo the passed-in fields by calling the
    /// passed-in function for each `BindingInfo`.
    ///
    /// The function will be called with the `BindingInfo` and its index in the
    /// enclosing variant.
    ///
    /// The default name is `__binding_{}` where `{}` is replaced with an
    /// increasing number.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A {
    ///         B{ a: i32, b: i32 },
    ///         C{ a: u32 },
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.binding_name(|bi, i| bi.ident.clone().unwrap());
    ///
    /// assert_eq!(
    ///     s.each(|bi| quote!(println!("{:?}", #bi))),
    ///
    ///     quote!{
    ///         A::B{ a: ref a, b: ref b, } => {
    ///             { println!("{:?}", a) }
    ///             { println!("{:?}", b) }
    ///         }
    ///         A::C{ a: ref a, } => {
    ///             { println!("{:?}", a) }
    ///         }
    ///     }
    /// );
    /// # }
    /// ```
    pub fn binding_name<F>(&mut self, mut f: F) -> &mut Self
    where
        F: FnMut(&Field, usize) -> Ident,
    {
        for variant in &mut self.variants {
            variant.binding_name(&mut f);
        }
        self
    }

    /// Returns a list of the type parameters which are refrenced in the types
    /// of non-filtered fields / variants.
    ///
    /// # Caveat
    ///
    /// If the struct contains any macros in type position, all parameters will
    /// be considered bound. This is because we cannot determine which type
    /// parameters are bound by type macros.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A<T, U> {
    ///         B(T, i32),
    ///         C(Option<U>),
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.filter_variants(|v| v.ast().ident != "C");
    ///
    /// assert_eq!(
    ///     s.referenced_ty_params(),
    ///     &[&(syn::Ident::from("T"))]
    /// );
    /// # }
    /// ```
    pub fn referenced_ty_params(&self) -> Vec<&'a Ident> {
        let mut flags = Vec::new();
        for variant in &self.variants {
            for binding in &variant.bindings {
                generics_fuse(&mut flags, &binding.seen_generics);
            }
        }
        fetch_generics(&flags, &self.ast.generics)
    }

    /// Add trait bounds for a trait with the given path for each type parmaeter
    /// referenced in the types of non-filtered fields.
    ///
    /// # Caveat
    ///
    /// If the method contains any macros in type position, all parameters will
    /// be considered bound. This is because we cannot determine which type
    /// parameters are bound by type macros.
    pub fn add_trait_bounds(&self, bound: &TraitBound, where_clause: &mut Option<WhereClause>) {
        let mut seen = HashSet::new();
        let mut pred = |ty: Type| if !seen.contains(&ty) {
            seen.insert(ty.clone());

            // Ensure we have a where clause, because we need to use it. We
            // can't use `get_or_insert_with`, because it isn't supported on all
            // rustc versions we support.
            if where_clause.is_none() {
                *where_clause = Some(WhereClause {
                    where_token: Default::default(),
                    predicates: punctuated::Punctuated::new(),
                });
            }
            let clause = where_clause.as_mut().unwrap();

            // Add a predicate.
            clause.predicates.push(WherePredicate::Type(PredicateType {
                lifetimes: None,
                bounded_ty: ty,
                colon_token: Default::default(),
                bounds: Some(punctuated::Pair::End(TypeParamBound::Trait(bound.clone())))
                    .into_iter()
                    .collect(),
            }));
        };

        for variant in &self.variants {
            for binding in &variant.bindings {
                for &seen in &binding.seen_generics {
                    if seen {
                        pred(binding.ast().ty.clone());
                        break;
                    }
                }

                for param in binding.referenced_ty_params() {
                    pred(Type::Path(TypePath {
                        qself: None,
                        path: (*param).clone().into(),
                    }));
                }
            }
        }
    }

    /// Creates an `impl` block with the required generic type fields filled in
    /// to implement the trait `path`.
    ///
    /// This method also adds where clauses to the impl requiring that all
    /// referenced type parmaeters implement the trait `path`.
    ///
    /// # Hygiene and Paths
    ///
    /// This method wraps the impl block inside of a `const` (see the example
    /// below). In this scope, the first segment of the passed-in path is
    /// `extern crate`-ed in. If you don't want to generate that `extern crate`
    /// item, use a global path.
    ///
    /// This means that if you are implementing `my_crate::Trait`, you simply
    /// write `s.bound_impl(quote!(my_crate::Trait), quote!(...))`, and for the
    /// entirety of the definition, you can refer to your crate as `my_crate`.
    ///
    /// # Caveat
    ///
    /// If the method contains any macros in type position, all parameters will
    /// be considered bound. This is because we cannot determine which type
    /// parameters are bound by type macros.
    ///
    /// # Panics
    ///
    /// Panics if the path string parameter is not a valid `TraitBound`.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A<T, U> {
    ///         B(T),
    ///         C(Option<U>),
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.filter_variants(|v| v.ast().ident != "B");
    ///
    /// assert_eq!(
    ///     s.bound_impl(quote!(krate::Trait), quote!{
    ///         fn a() {}
    ///     }),
    ///     quote!{
    ///         #[allow(non_upper_case_globals)]
    ///         const _DERIVE_krate_Trait_FOR_A: () = {
    ///             extern crate krate;
    ///             impl<T, U> krate::Trait for A<T, U>
    ///                 where Option<U>: krate::Trait,
    ///                       U: krate::Trait
    ///             {
    ///                 fn a() {}
    ///             }
    ///         };
    ///     }
    /// );
    /// # }
    /// ```
    pub fn bound_impl<P: ToTokens,B: ToTokens>(&self, path: P, body: B) -> Tokens {
        self.impl_internal(
            path.into_tokens(),
            body.into_tokens(),
            quote!(),
            true,
        )
    }

    /// Creates an `impl` block with the required generic type fields filled in
    /// to implement the unsafe trait `path`.
    ///
    /// This method also adds where clauses to the impl requiring that all
    /// referenced type parmaeters implement the trait `path`.
    ///
    /// # Hygiene and Paths
    ///
    /// This method wraps the impl block inside of a `const` (see the example
    /// below). In this scope, the first segment of the passed-in path is
    /// `extern crate`-ed in. If you don't want to generate that `extern crate`
    /// item, use a global path.
    ///
    /// This means that if you are implementing `my_crate::Trait`, you simply
    /// write `s.bound_impl(quote!(my_crate::Trait), quote!(...))`, and for the
    /// entirety of the definition, you can refer to your crate as `my_crate`.
    ///
    /// # Caveat
    ///
    /// If the method contains any macros in type position, all parameters will
    /// be considered bound. This is because we cannot determine which type
    /// parameters are bound by type macros.
    ///
    /// # Panics
    ///
    /// Panics if the path string parameter is not a valid `TraitBound`.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A<T, U> {
    ///         B(T),
    ///         C(Option<U>),
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.filter_variants(|v| v.ast().ident != "B");
    ///
    /// assert_eq!(
    ///     s.unsafe_bound_impl(quote!(krate::Trait), quote!{
    ///         fn a() {}
    ///     }),
    ///     quote!{
    ///         #[allow(non_upper_case_globals)]
    ///         const _DERIVE_krate_Trait_FOR_A: () = {
    ///             extern crate krate;
    ///             unsafe impl<T, U> krate::Trait for A<T, U>
    ///                 where Option<U>: krate::Trait,
    ///                       U: krate::Trait
    ///             {
    ///                 fn a() {}
    ///             }
    ///         };
    ///     }
    /// );
    /// # }
    /// ```
    pub fn unsafe_bound_impl<P: ToTokens, B: ToTokens>(&self, path: P, body: B) -> Tokens {
        self.impl_internal(
            path.into_tokens(),
            body.into_tokens(),
            quote!(unsafe),
            true,
        )
    }

    /// Creates an `impl` block with the required generic type fields filled in
    /// to implement the trait `path`.
    ///
    /// This method will not add any where clauses to the impl.
    ///
    /// # Hygiene and Paths
    ///
    /// This method wraps the impl block inside of a `const` (see the example
    /// below). In this scope, the first segment of the passed-in path is
    /// `extern crate`-ed in. If you don't want to generate that `extern crate`
    /// item, use a global path.
    ///
    /// This means that if you are implementing `my_crate::Trait`, you simply
    /// write `s.bound_impl(quote!(my_crate::Trait), quote!(...))`, and for the
    /// entirety of the definition, you can refer to your crate as `my_crate`.
    ///
    /// # Panics
    ///
    /// Panics if the path string parameter is not a valid `TraitBound`.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A<T, U> {
    ///         B(T),
    ///         C(Option<U>),
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.filter_variants(|v| v.ast().ident != "B");
    ///
    /// assert_eq!(
    ///     s.unbound_impl(quote!(krate::Trait), quote!{
    ///         fn a() {}
    ///     }),
    ///     quote!{
    ///         #[allow(non_upper_case_globals)]
    ///         const _DERIVE_krate_Trait_FOR_A: () = {
    ///             extern crate krate;
    ///             impl<T, U> krate::Trait for A<T, U> {
    ///                 fn a() {}
    ///             }
    ///         };
    ///     }
    /// );
    /// # }
    /// ```
    pub fn unbound_impl<P: ToTokens, B: ToTokens>(&self, path: P, body: B) -> Tokens {
        self.impl_internal(
            path.into_tokens(),
            body.into_tokens(),
            quote!(),
            false,
        )
    }

    /// Creates an `impl` block with the required generic type fields filled in
    /// to implement the unsafe trait `path`.
    ///
    /// This method will not add any where clauses to the impl.
    ///
    /// # Hygiene and Paths
    ///
    /// This method wraps the impl block inside of a `const` (see the example
    /// below). In this scope, the first segment of the passed-in path is
    /// `extern crate`-ed in. If you don't want to generate that `extern crate`
    /// item, use a global path.
    ///
    /// This means that if you are implementing `my_crate::Trait`, you simply
    /// write `s.bound_impl(quote!(my_crate::Trait), quote!(...))`, and for the
    /// entirety of the definition, you can refer to your crate as `my_crate`.
    ///
    /// # Panics
    ///
    /// Panics if the path string parameter is not a valid `TraitBound`.
    ///
    /// # Example
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # extern crate synstructure;
    /// # #[macro_use] extern crate syn;
    /// # use synstructure::*;
    /// # fn main() {
    /// let di: syn::DeriveInput = parse_quote! {
    ///     enum A<T, U> {
    ///         B(T),
    ///         C(Option<U>),
    ///     }
    /// };
    /// let mut s = Structure::new(&di);
    ///
    /// s.filter_variants(|v| v.ast().ident != "B");
    ///
    /// assert_eq!(
    ///     s.unsafe_unbound_impl(quote!(krate::Trait), quote!{
    ///         fn a() {}
    ///     }),
    ///     quote!{
    ///         #[allow(non_upper_case_globals)]
    ///         const _DERIVE_krate_Trait_FOR_A: () = {
    ///             extern crate krate;
    ///             unsafe impl<T, U> krate::Trait for A<T, U> {
    ///                 fn a() {}
    ///             }
    ///         };
    ///     }
    /// );
    /// # }
    /// ```
    pub fn unsafe_unbound_impl<P: ToTokens, B: ToTokens>(&self, path: P, body: B) -> Tokens {
        self.impl_internal(
            path.into_tokens(),
            body.into_tokens(),
            quote!(unsafe),
            false,
        )
    }

    fn impl_internal(
        &self,
        path: Tokens,
        body: Tokens,
        safety: Tokens,
        add_bounds: bool,
    ) -> Tokens {
        let name = &self.ast.ident;
        let (impl_generics, ty_generics, where_clause) = self.ast.generics.split_for_impl();

        let bound = syn::parse2::<TraitBound>(path.into())
            .expect("`path` argument must be a valid rust trait bound");

        let mut where_clause = where_clause.cloned();
        if add_bounds {
            self.add_trait_bounds(&bound, &mut where_clause);
        }

        let dummy_const: Ident = sanitize_ident(&format!(
            "_DERIVE_{}_FOR_{}",
            (&bound).into_tokens(),
            name.into_tokens(),
        ));

        // This function is smart. If a global path is passed, no extern crate
        // statement will be generated, however, a relative path will cause the
        // crate which it is relative to to be imported within the current
        // scope.
        let mut extern_crate = quote!();
        if bound.path.leading_colon.is_none() {
            if let Some(ref seg) = bound.path.segments.first() {
                let seg = seg.value();
                extern_crate = quote! { extern crate #seg; };
            }
        }

        quote! {
            #[allow(non_upper_case_globals)]
            const #dummy_const: () = {
                #extern_crate
                #safety impl #impl_generics #bound for #name #ty_generics #where_clause {
                    #body
                }
            };
        }
    }
}
