// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::*;
use punctuated::{Iter, IterMut, Punctuated};

ast_struct! {
    /// Lifetimes and type parameters attached to a declaration of a function,
    /// enum, trait, etc.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    #[derive(Default)]
    pub struct Generics {
        pub lt_token: Option<Token![<]>,
        pub params: Punctuated<GenericParam, Token![,]>,
        pub gt_token: Option<Token![>]>,
        pub where_clause: Option<WhereClause>,
    }
}

ast_enum_of_structs! {
    /// A generic type parameter, lifetime, or const generic: `T: Into<String>`,
    /// `'a: 'b`, `const LEN: usize`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum GenericParam {
        /// A generic type parameter: `T: Into<String>`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Type(TypeParam {
            pub attrs: Vec<Attribute>,
            pub ident: Ident,
            pub colon_token: Option<Token![:]>,
            pub bounds: Punctuated<TypeParamBound, Token![+]>,
            pub eq_token: Option<Token![=]>,
            pub default: Option<Type>,
        }),

        /// A lifetime definition: `'a: 'b + 'c + 'd`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Lifetime(LifetimeDef {
            pub attrs: Vec<Attribute>,
            pub lifetime: Lifetime,
            pub colon_token: Option<Token![:]>,
            pub bounds: Punctuated<Lifetime, Token![+]>,
        }),

        /// A const generic parameter: `const LENGTH: usize`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Const(ConstParam {
            pub attrs: Vec<Attribute>,
            pub const_token: Token![const],
            pub ident: Ident,
            pub colon_token: Token![:],
            pub ty: Type,
            pub eq_token: Option<Token![=]>,
            pub default: Option<Expr>,
        }),
    }
}

impl Generics {
    /// Returns an
    /// <code
    ///   style="padding-right:0;">Iterator&lt;Item = &amp;</code><a
    ///   href="struct.TypeParam.html"><code
    ///   style="padding-left:0;padding-right:0;">TypeParam</code></a><code
    ///   style="padding-left:0;">&gt;</code>
    /// over the type parameters in `self.params`.
    pub fn type_params(&self) -> TypeParams {
        TypeParams(self.params.iter())
    }

    /// Returns an
    /// <code
    ///   style="padding-right:0;">Iterator&lt;Item = &amp;mut </code><a
    ///   href="struct.TypeParam.html"><code
    ///   style="padding-left:0;padding-right:0;">TypeParam</code></a><code
    ///   style="padding-left:0;">&gt;</code>
    /// over the type parameters in `self.params`.
    pub fn type_params_mut(&mut self) -> TypeParamsMut {
        TypeParamsMut(self.params.iter_mut())
    }

    /// Returns an
    /// <code
    ///   style="padding-right:0;">Iterator&lt;Item = &amp;</code><a
    ///   href="struct.LifetimeDef.html"><code
    ///   style="padding-left:0;padding-right:0;">LifetimeDef</code></a><code
    ///   style="padding-left:0;">&gt;</code>
    /// over the lifetime parameters in `self.params`.
    pub fn lifetimes(&self) -> Lifetimes {
        Lifetimes(self.params.iter())
    }

    /// Returns an
    /// <code
    ///   style="padding-right:0;">Iterator&lt;Item = &amp;mut </code><a
    ///   href="struct.LifetimeDef.html"><code
    ///   style="padding-left:0;padding-right:0;">LifetimeDef</code></a><code
    ///   style="padding-left:0;">&gt;</code>
    /// over the lifetime parameters in `self.params`.
    pub fn lifetimes_mut(&mut self) -> LifetimesMut {
        LifetimesMut(self.params.iter_mut())
    }

    /// Returns an
    /// <code
    ///   style="padding-right:0;">Iterator&lt;Item = &amp;</code><a
    ///   href="struct.ConstParam.html"><code
    ///   style="padding-left:0;padding-right:0;">ConstParam</code></a><code
    ///   style="padding-left:0;">&gt;</code>
    /// over the constant parameters in `self.params`.
    pub fn const_params(&self) -> ConstParams {
        ConstParams(self.params.iter())
    }

    /// Returns an
    /// <code
    ///   style="padding-right:0;">Iterator&lt;Item = &amp;mut </code><a
    ///   href="struct.ConstParam.html"><code
    ///   style="padding-left:0;padding-right:0;">ConstParam</code></a><code
    ///   style="padding-left:0;">&gt;</code>
    /// over the constant parameters in `self.params`.
    pub fn const_params_mut(&mut self) -> ConstParamsMut {
        ConstParamsMut(self.params.iter_mut())
    }

    /// Initializes an empty `where`-clause if there is not one present already.
    pub fn make_where_clause(&mut self) -> &mut WhereClause {
        // This is Option::get_or_insert_with in Rust 1.20.
        if self.where_clause.is_none() {
            self.where_clause = Some(WhereClause {
                where_token: Default::default(),
                predicates: Punctuated::new(),
            });
        }
        match self.where_clause {
            Some(ref mut where_clause) => where_clause,
            None => unreachable!(),
        }
    }
}

pub struct TypeParams<'a>(Iter<'a, GenericParam>);

impl<'a> Iterator for TypeParams<'a> {
    type Item = &'a TypeParam;

    fn next(&mut self) -> Option<Self::Item> {
        // FIXME: Remove this when ? on Option is stable
        let next = match self.0.next() {
            Some(item) => item,
            None => return None,
        };
        if let GenericParam::Type(ref type_param) = *next {
            Some(type_param)
        } else {
            self.next()
        }
    }
}

pub struct TypeParamsMut<'a>(IterMut<'a, GenericParam>);

impl<'a> Iterator for TypeParamsMut<'a> {
    type Item = &'a mut TypeParam;

    fn next(&mut self) -> Option<Self::Item> {
        // FIXME: Remove this when ? on Option is stable
        let next = match self.0.next() {
            Some(item) => item,
            None => return None,
        };
        if let GenericParam::Type(ref mut type_param) = *next {
            Some(type_param)
        } else {
            self.next()
        }
    }
}

pub struct Lifetimes<'a>(Iter<'a, GenericParam>);

impl<'a> Iterator for Lifetimes<'a> {
    type Item = &'a LifetimeDef;

    fn next(&mut self) -> Option<Self::Item> {
        // FIXME: Remove this when ? on Option is stable
        let next = match self.0.next() {
            Some(item) => item,
            None => return None,
        };
        if let GenericParam::Lifetime(ref lifetime) = *next {
            Some(lifetime)
        } else {
            self.next()
        }
    }
}

pub struct LifetimesMut<'a>(IterMut<'a, GenericParam>);

impl<'a> Iterator for LifetimesMut<'a> {
    type Item = &'a mut LifetimeDef;

    fn next(&mut self) -> Option<Self::Item> {
        // FIXME: Remove this when ? on Option is stable
        let next = match self.0.next() {
            Some(item) => item,
            None => return None,
        };
        if let GenericParam::Lifetime(ref mut lifetime) = *next {
            Some(lifetime)
        } else {
            self.next()
        }
    }
}

pub struct ConstParams<'a>(Iter<'a, GenericParam>);

impl<'a> Iterator for ConstParams<'a> {
    type Item = &'a ConstParam;

    fn next(&mut self) -> Option<Self::Item> {
        // FIXME: Remove this when ? on Option is stable
        let next = match self.0.next() {
            Some(item) => item,
            None => return None,
        };
        if let GenericParam::Const(ref const_param) = *next {
            Some(const_param)
        } else {
            self.next()
        }
    }
}

pub struct ConstParamsMut<'a>(IterMut<'a, GenericParam>);

impl<'a> Iterator for ConstParamsMut<'a> {
    type Item = &'a mut ConstParam;

    fn next(&mut self) -> Option<Self::Item> {
        // FIXME: Remove this when ? on Option is stable
        let next = match self.0.next() {
            Some(item) => item,
            None => return None,
        };
        if let GenericParam::Const(ref mut const_param) = *next {
            Some(const_param)
        } else {
            self.next()
        }
    }
}

/// Returned by `Generics::split_for_impl`.
///
/// *This type is available if Syn is built with the `"derive"` or `"full"`
/// feature and the `"printing"` feature.*
#[cfg(feature = "printing")]
#[cfg_attr(feature = "extra-traits", derive(Debug, Eq, PartialEq, Hash))]
#[cfg_attr(feature = "clone-impls", derive(Clone))]
pub struct ImplGenerics<'a>(&'a Generics);

/// Returned by `Generics::split_for_impl`.
///
/// *This type is available if Syn is built with the `"derive"` or `"full"`
/// feature and the `"printing"` feature.*
#[cfg(feature = "printing")]
#[cfg_attr(feature = "extra-traits", derive(Debug, Eq, PartialEq, Hash))]
#[cfg_attr(feature = "clone-impls", derive(Clone))]
pub struct TypeGenerics<'a>(&'a Generics);

/// Returned by `TypeGenerics::as_turbofish`.
///
/// *This type is available if Syn is built with the `"derive"` or `"full"`
/// feature and the `"printing"` feature.*
#[cfg(feature = "printing")]
#[cfg_attr(feature = "extra-traits", derive(Debug, Eq, PartialEq, Hash))]
#[cfg_attr(feature = "clone-impls", derive(Clone))]
pub struct Turbofish<'a>(&'a Generics);

#[cfg(feature = "printing")]
impl Generics {
    /// Split a type's generics into the pieces required for impl'ing a trait
    /// for that type.
    ///
    /// ```
    /// # extern crate syn;
    /// # #[macro_use]
    /// # extern crate quote;
    /// # fn main() {
    /// # let generics: syn::Generics = Default::default();
    /// # let name = syn::Ident::from("MyType");
    /// let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();
    /// quote! {
    ///     impl #impl_generics MyTrait for #name #ty_generics #where_clause {
    ///         // ...
    ///     }
    /// }
    /// # ;
    /// # }
    /// ```
    ///
    /// *This method is available if Syn is built with the `"derive"` or
    /// `"full"` feature and the `"printing"` feature.*
    pub fn split_for_impl(&self) -> (ImplGenerics, TypeGenerics, Option<&WhereClause>) {
        (
            ImplGenerics(self),
            TypeGenerics(self),
            self.where_clause.as_ref(),
        )
    }
}

#[cfg(feature = "printing")]
impl<'a> TypeGenerics<'a> {
    /// Turn a type's generics like `<X, Y>` into a turbofish like `::<X, Y>`.
    ///
    /// *This method is available if Syn is built with the `"derive"` or
    /// `"full"` feature and the `"printing"` feature.*
    pub fn as_turbofish(&self) -> Turbofish {
        Turbofish(self.0)
    }
}

ast_struct! {
    /// A set of bound lifetimes: `for<'a, 'b, 'c>`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    #[derive(Default)]
    pub struct BoundLifetimes {
        pub for_token: Token![for],
        pub lt_token: Token![<],
        pub lifetimes: Punctuated<LifetimeDef, Token![,]>,
        pub gt_token: Token![>],
    }
}

impl LifetimeDef {
    pub fn new(lifetime: Lifetime) -> Self {
        LifetimeDef {
            attrs: Vec::new(),
            lifetime: lifetime,
            colon_token: None,
            bounds: Punctuated::new(),
        }
    }
}

impl From<Ident> for TypeParam {
    fn from(ident: Ident) -> Self {
        TypeParam {
            attrs: vec![],
            ident: ident,
            colon_token: None,
            bounds: Punctuated::new(),
            eq_token: None,
            default: None,
        }
    }
}

ast_enum_of_structs! {
    /// A trait or lifetime used as a bound on a type parameter.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub enum TypeParamBound {
        pub Trait(TraitBound),
        pub Lifetime(Lifetime),
    }
}

ast_struct! {
    /// A trait used as a bound on a type parameter.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct TraitBound {
        pub paren_token: Option<token::Paren>,
        pub modifier: TraitBoundModifier,
        /// The `for<'a>` in `for<'a> Foo<&'a T>`
        pub lifetimes: Option<BoundLifetimes>,
        /// The `Foo<&'a T>` in `for<'a> Foo<&'a T>`
        pub path: Path,
    }
}

ast_enum! {
    /// A modifier on a trait bound, currently only used for the `?` in
    /// `?Sized`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    #[cfg_attr(feature = "clone-impls", derive(Copy))]
    pub enum TraitBoundModifier {
        None,
        Maybe(Token![?]),
    }
}

ast_struct! {
    /// A `where` clause in a definition: `where T: Deserialize<'de>, D:
    /// 'static`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct WhereClause {
        pub where_token: Token![where],
        pub predicates: Punctuated<WherePredicate, Token![,]>,
    }
}

ast_enum_of_structs! {
    /// A single predicate in a `where` clause: `T: Deserialize<'de>`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum WherePredicate {
        /// A type predicate in a `where` clause: `for<'c> Foo<'c>: Trait<'c>`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Type(PredicateType {
            /// Any lifetimes from a `for` binding
            pub lifetimes: Option<BoundLifetimes>,
            /// The type being bounded
            pub bounded_ty: Type,
            pub colon_token: Token![:],
            /// Trait and lifetime bounds (`Clone+Send+'static`)
            pub bounds: Punctuated<TypeParamBound, Token![+]>,
        }),

        /// A lifetime predicate in a `where` clause: `'a: 'b + 'c`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Lifetime(PredicateLifetime {
            pub lifetime: Lifetime,
            pub colon_token: Option<Token![:]>,
            pub bounds: Punctuated<Lifetime, Token![+]>,
        }),

        /// An equality predicate in a `where` clause (unsupported).
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Eq(PredicateEq {
            pub lhs_ty: Type,
            pub eq_token: Token![=],
            pub rhs_ty: Type,
        }),
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;

    use synom::Synom;
    use punctuated::Pair;

    impl Synom for Generics {
        named!(parse -> Self, map!(
            alt!(
                do_parse!(
                    lt: punct!(<) >>
                    lifetimes: call!(Punctuated::<LifetimeDef, Token![,]>::parse_terminated) >>
                    ty_params: cond!(
                        lifetimes.empty_or_trailing(),
                        Punctuated::<TypeParam, Token![,]>::parse_terminated
                    ) >>
                    gt: punct!(>) >>
                    (lifetimes, ty_params, Some(lt), Some(gt))
                )
                |
                epsilon!() => { |_| (Punctuated::new(), None, None, None) }
            ),
            |(lifetimes, ty_params, lt, gt)| Generics {
                lt_token: lt,
                params: lifetimes.into_pairs()
                    .map(Pair::into_tuple)
                    .map(|(life, comma)| Pair::new(GenericParam::Lifetime(life), comma))
                    .chain(ty_params.unwrap_or_default()
                        .into_pairs()
                        .map(Pair::into_tuple)
                        .map(|(ty, comma)| Pair::new(GenericParam::Type(ty), comma)))
                    .collect(),
                gt_token: gt,
                where_clause: None,
            }
        ));

        fn description() -> Option<&'static str> {
            Some("generic parameters in declaration")
        }
    }

    impl Synom for GenericParam {
        named!(parse -> Self, alt!(
            syn!(TypeParam) => { GenericParam::Type }
            |
            syn!(LifetimeDef) => { GenericParam::Lifetime }
            |
            syn!(ConstParam) => { GenericParam::Const }
        ));

        fn description() -> Option<&'static str> {
            Some("generic parameter")
        }
    }

    impl Synom for LifetimeDef {
        named!(parse -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            life: syn!(Lifetime) >>
            colon: option!(punct!(:)) >>
            bounds: cond!(
                colon.is_some(),
                Punctuated::parse_separated_nonempty
            ) >>
            (LifetimeDef {
                attrs: attrs,
                lifetime: life,
                bounds: bounds.unwrap_or_default(),
                colon_token: colon,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("lifetime definition")
        }
    }

    impl Synom for BoundLifetimes {
        named!(parse -> Self, do_parse!(
            for_: keyword!(for) >>
            lt: punct!(<) >>
            lifetimes: call!(Punctuated::parse_terminated) >>
            gt: punct!(>) >>
            (BoundLifetimes {
                for_token: for_,
                lt_token: lt,
                gt_token: gt,
                lifetimes: lifetimes,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("bound lifetimes")
        }
    }

    impl Synom for TypeParam {
        named!(parse -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            id: syn!(Ident) >>
            colon: option!(punct!(:)) >>
            bounds: cond!(
                colon.is_some(),
                Punctuated::parse_separated_nonempty
            ) >>
            default: option!(do_parse!(
                eq: punct!(=) >>
                ty: syn!(Type) >>
                (eq, ty)
            )) >>
            (TypeParam {
                attrs: attrs,
                ident: id,
                bounds: bounds.unwrap_or_default(),
                colon_token: colon,
                eq_token: default.as_ref().map(|d| Token![=]((d.0).0)),
                default: default.map(|d| d.1),
            })
        ));

        fn description() -> Option<&'static str> {
            Some("type parameter")
        }
    }

    impl Synom for TypeParamBound {
        named!(parse -> Self, alt!(
            syn!(Lifetime) => { TypeParamBound::Lifetime }
            |
            syn!(TraitBound) => { TypeParamBound::Trait }
            |
            parens!(syn!(TraitBound)) => {|(parens, mut bound)| {
                bound.paren_token = Some(parens);
                TypeParamBound::Trait(bound)
            }}
        ));

        fn description() -> Option<&'static str> {
            Some("type parameter bound")
        }
    }

    impl Synom for TraitBound {
        named!(parse -> Self, do_parse!(
            modifier: syn!(TraitBoundModifier) >>
            lifetimes: option!(syn!(BoundLifetimes)) >>
            mut path: syn!(Path) >>
            parenthesized: option!(cond_reduce!(
                path.segments.last().unwrap().value().arguments.is_empty(),
                syn!(ParenthesizedGenericArguments)
            )) >>
            ({
                if let Some(parenthesized) = parenthesized {
                    let parenthesized = PathArguments::Parenthesized(parenthesized);
                    path.segments.last_mut().unwrap().value_mut().arguments = parenthesized;
                }
                TraitBound {
                    paren_token: None,
                    modifier: modifier,
                    lifetimes: lifetimes,
                    path: path,
                }
            })
        ));

        fn description() -> Option<&'static str> {
            Some("trait bound")
        }
    }

    impl Synom for TraitBoundModifier {
        named!(parse -> Self, alt!(
            punct!(?) => { TraitBoundModifier::Maybe }
            |
            epsilon!() => { |_| TraitBoundModifier::None }
        ));

        fn description() -> Option<&'static str> {
            Some("trait bound modifier")
        }
    }

    impl Synom for ConstParam {
        named!(parse -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            const_: keyword!(const) >>
            ident: syn!(Ident) >>
            colon: punct!(:) >>
            ty: syn!(Type) >>
            eq_def: option!(tuple!(punct!(=), syn!(Expr))) >>
            ({
                let (eq_token, default) = match eq_def {
                    Some((eq_token, default)) => (Some(eq_token), Some(default)),
                    None => (None, None),
                };
                ConstParam {
                    attrs: attrs,
                    const_token: const_,
                    ident: ident,
                    colon_token: colon,
                    ty: ty,
                    eq_token: eq_token,
                    default: default,
                }
            })
        ));

        fn description() -> Option<&'static str> {
            Some("generic `const` parameter")
        }
    }

    impl Synom for WhereClause {
        named!(parse -> Self, do_parse!(
            where_: keyword!(where) >>
            predicates: call!(Punctuated::parse_terminated) >>
            (WhereClause {
                predicates: predicates,
                where_token: where_,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("where clause")
        }
    }

    impl Synom for WherePredicate {
        named!(parse -> Self, alt!(
            do_parse!(
                ident: syn!(Lifetime) >>
                colon: option!(punct!(:)) >>
                bounds: cond!(
                    colon.is_some(),
                    Punctuated::parse_separated
                ) >>
                (WherePredicate::Lifetime(PredicateLifetime {
                    lifetime: ident,
                    bounds: bounds.unwrap_or_default(),
                    colon_token: colon,
                }))
            )
            |
            do_parse!(
                lifetimes: option!(syn!(BoundLifetimes)) >>
                bounded_ty: syn!(Type) >>
                colon: punct!(:) >>
                bounds: call!(Punctuated::parse_separated_nonempty) >>
                (WherePredicate::Type(PredicateType {
                    lifetimes: lifetimes,
                    bounded_ty: bounded_ty,
                    bounds: bounds,
                    colon_token: colon,
                }))
            )
        ));

        fn description() -> Option<&'static str> {
            Some("predicate in where clause")
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use attr::FilterAttrs;
    use quote::{ToTokens, Tokens};

    impl ToTokens for Generics {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if self.params.is_empty() {
                return;
            }

            TokensOrDefault(&self.lt_token).to_tokens(tokens);

            // Print lifetimes before types and consts, regardless of their
            // order in self.params.
            //
            // TODO: ordering rules for const parameters vs type parameters have
            // not been settled yet. https://github.com/rust-lang/rust/issues/44580
            let mut trailing_or_empty = true;
            for param in self.params.pairs() {
                if let GenericParam::Lifetime(_) = **param.value() {
                    param.to_tokens(tokens);
                    trailing_or_empty = param.punct().is_some();
                }
            }
            for param in self.params.pairs() {
                match **param.value() {
                    GenericParam::Type(_) | GenericParam::Const(_) => {
                        if !trailing_or_empty {
                            <Token![,]>::default().to_tokens(tokens);
                            trailing_or_empty = true;
                        }
                        param.to_tokens(tokens);
                    }
                    GenericParam::Lifetime(_) => {}
                }
            }

            TokensOrDefault(&self.gt_token).to_tokens(tokens);
        }
    }

    impl<'a> ToTokens for ImplGenerics<'a> {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if self.0.params.is_empty() {
                return;
            }

            TokensOrDefault(&self.0.lt_token).to_tokens(tokens);

            // Print lifetimes before types and consts, regardless of their
            // order in self.params.
            //
            // TODO: ordering rules for const parameters vs type parameters have
            // not been settled yet. https://github.com/rust-lang/rust/issues/44580
            let mut trailing_or_empty = true;
            for param in self.0.params.pairs() {
                if let GenericParam::Lifetime(_) = **param.value() {
                    param.to_tokens(tokens);
                    trailing_or_empty = param.punct().is_some();
                }
            }
            for param in self.0.params.pairs() {
                if let GenericParam::Lifetime(_) = **param.value() {
                    continue;
                }
                if !trailing_or_empty {
                    <Token![,]>::default().to_tokens(tokens);
                    trailing_or_empty = true;
                }
                match **param.value() {
                    GenericParam::Lifetime(_) => unreachable!(),
                    GenericParam::Type(ref param) => {
                        // Leave off the type parameter defaults
                        tokens.append_all(param.attrs.outer());
                        param.ident.to_tokens(tokens);
                        if !param.bounds.is_empty() {
                            TokensOrDefault(&param.colon_token).to_tokens(tokens);
                            param.bounds.to_tokens(tokens);
                        }
                    }
                    GenericParam::Const(ref param) => {
                        // Leave off the const parameter defaults
                        tokens.append_all(param.attrs.outer());
                        param.const_token.to_tokens(tokens);
                        param.ident.to_tokens(tokens);
                        param.colon_token.to_tokens(tokens);
                        param.ty.to_tokens(tokens);
                    }
                }
                param.punct().to_tokens(tokens);
            }

            TokensOrDefault(&self.0.gt_token).to_tokens(tokens);
        }
    }

    impl<'a> ToTokens for TypeGenerics<'a> {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if self.0.params.is_empty() {
                return;
            }

            TokensOrDefault(&self.0.lt_token).to_tokens(tokens);

            // Print lifetimes before types and consts, regardless of their
            // order in self.params.
            //
            // TODO: ordering rules for const parameters vs type parameters have
            // not been settled yet. https://github.com/rust-lang/rust/issues/44580
            let mut trailing_or_empty = true;
            for param in self.0.params.pairs() {
                if let GenericParam::Lifetime(ref def) = **param.value() {
                    // Leave off the lifetime bounds and attributes
                    def.lifetime.to_tokens(tokens);
                    param.punct().to_tokens(tokens);
                    trailing_or_empty = param.punct().is_some();
                }
            }
            for param in self.0.params.pairs() {
                if let GenericParam::Lifetime(_) = **param.value() {
                    continue;
                }
                if !trailing_or_empty {
                    <Token![,]>::default().to_tokens(tokens);
                    trailing_or_empty = true;
                }
                match **param.value() {
                    GenericParam::Lifetime(_) => unreachable!(),
                    GenericParam::Type(ref param) => {
                        // Leave off the type parameter defaults
                        param.ident.to_tokens(tokens);
                    }
                    GenericParam::Const(ref param) => {
                        // Leave off the const parameter defaults
                        param.ident.to_tokens(tokens);
                    }
                }
                param.punct().to_tokens(tokens);
            }

            TokensOrDefault(&self.0.gt_token).to_tokens(tokens);
        }
    }

    impl<'a> ToTokens for Turbofish<'a> {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if !self.0.params.is_empty() {
                <Token![::]>::default().to_tokens(tokens);
                TypeGenerics(self.0).to_tokens(tokens);
            }
        }
    }

    impl ToTokens for BoundLifetimes {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.for_token.to_tokens(tokens);
            self.lt_token.to_tokens(tokens);
            self.lifetimes.to_tokens(tokens);
            self.gt_token.to_tokens(tokens);
        }
    }

    impl ToTokens for LifetimeDef {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.lifetime.to_tokens(tokens);
            if !self.bounds.is_empty() {
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
                self.bounds.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for TypeParam {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.ident.to_tokens(tokens);
            if !self.bounds.is_empty() {
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
                self.bounds.to_tokens(tokens);
            }
            if self.default.is_some() {
                TokensOrDefault(&self.eq_token).to_tokens(tokens);
                self.default.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for TraitBound {
        fn to_tokens(&self, tokens: &mut Tokens) {
            let to_tokens = |tokens: &mut Tokens| {
                self.modifier.to_tokens(tokens);
                self.lifetimes.to_tokens(tokens);
                self.path.to_tokens(tokens);
            };
            match self.paren_token {
                Some(ref paren) => paren.surround(tokens, to_tokens),
                None => to_tokens(tokens),
            }
        }
    }

    impl ToTokens for TraitBoundModifier {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                TraitBoundModifier::None => {}
                TraitBoundModifier::Maybe(ref t) => t.to_tokens(tokens),
            }
        }
    }

    impl ToTokens for ConstParam {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.const_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            if self.default.is_some() {
                TokensOrDefault(&self.eq_token).to_tokens(tokens);
                self.default.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for WhereClause {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if !self.predicates.is_empty() {
                self.where_token.to_tokens(tokens);
                self.predicates.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for PredicateType {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.lifetimes.to_tokens(tokens);
            self.bounded_ty.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.bounds.to_tokens(tokens);
        }
    }

    impl ToTokens for PredicateLifetime {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.lifetime.to_tokens(tokens);
            if !self.bounds.is_empty() {
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
                self.bounds.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for PredicateEq {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.lhs_ty.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.rhs_ty.to_tokens(tokens);
        }
    }
}
