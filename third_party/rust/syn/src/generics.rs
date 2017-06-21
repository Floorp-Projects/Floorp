use super::*;

/// Represents lifetimes and type parameters attached to a declaration
/// of a function, enum, trait, etc.
#[derive(Debug, Clone, Eq, PartialEq, Default, Hash)]
pub struct Generics {
    pub lifetimes: Vec<LifetimeDef>,
    pub ty_params: Vec<TyParam>,
    pub where_clause: WhereClause,
}

#[cfg(feature = "printing")]
/// Returned by `Generics::split_for_impl`.
#[derive(Debug)]
pub struct ImplGenerics<'a>(&'a Generics);

#[cfg(feature = "printing")]
/// Returned by `Generics::split_for_impl`.
#[derive(Debug)]
pub struct TyGenerics<'a>(&'a Generics);

#[cfg(feature = "printing")]
/// Returned by `TyGenerics::as_turbofish`.
#[derive(Debug)]
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
    /// # let name = syn::Ident::new("MyType");
    /// let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();
    /// quote! {
    ///     impl #impl_generics MyTrait for #name #ty_generics #where_clause {
    ///         // ...
    ///     }
    /// }
    /// # ;
    /// # }
    /// ```
    pub fn split_for_impl(&self) -> (ImplGenerics, TyGenerics, &WhereClause) {
        (ImplGenerics(self), TyGenerics(self), &self.where_clause)
    }
}

#[cfg(feature = "printing")]
impl<'a> TyGenerics<'a> {
    /// Turn a type's generics like `<X, Y>` into a turbofish like `::<X, Y>`.
    pub fn as_turbofish(&self) -> Turbofish {
        Turbofish(self.0)
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
pub struct Lifetime {
    pub ident: Ident,
}

impl Lifetime {
    pub fn new<T: Into<Ident>>(t: T) -> Self {
        let id = Ident::new(t);
        if !id.as_ref().starts_with('\'') {
            panic!("lifetime name must start with apostrophe as in \"'a\", \
                   got {:?}",
                   id.as_ref());
        }
        Lifetime { ident: id }
    }
}

/// A lifetime definition, e.g. `'a: 'b+'c+'d`
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct LifetimeDef {
    pub attrs: Vec<Attribute>,
    pub lifetime: Lifetime,
    pub bounds: Vec<Lifetime>,
}

impl LifetimeDef {
    pub fn new<T: Into<Ident>>(t: T) -> Self {
        LifetimeDef {
            attrs: Vec::new(),
            lifetime: Lifetime::new(t),
            bounds: Vec::new(),
        }
    }
}

/// A generic type parameter, e.g. `T: Into<String>`.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct TyParam {
    pub attrs: Vec<Attribute>,
    pub ident: Ident,
    pub bounds: Vec<TyParamBound>,
    pub default: Option<Ty>,
}

impl From<Ident> for TyParam {
    fn from(ident: Ident) -> Self {
        TyParam {
            attrs: vec![],
            ident: ident,
            bounds: vec![],
            default: None,
        }
    }
}

/// The AST represents all type param bounds as types.
/// `typeck::collect::compute_bounds` matches these against
/// the "special" built-in traits (see `middle::lang_items`) and
/// detects Copy, Send and Sync.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub enum TyParamBound {
    Trait(PolyTraitRef, TraitBoundModifier),
    Region(Lifetime),
}

/// A modifier on a bound, currently this is only used for `?Sized`, where the
/// modifier is `Maybe`. Negative bounds should also be handled here.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum TraitBoundModifier {
    None,
    Maybe,
}

/// A `where` clause in a definition
#[derive(Debug, Clone, Eq, PartialEq, Default, Hash)]
pub struct WhereClause {
    pub predicates: Vec<WherePredicate>,
}

impl WhereClause {
    pub fn none() -> Self {
        WhereClause { predicates: Vec::new() }
    }
}

/// A single predicate in a `where` clause
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub enum WherePredicate {
    /// A type binding, e.g. `for<'c> Foo: Send+Clone+'c`
    BoundPredicate(WhereBoundPredicate),
    /// A lifetime predicate, e.g. `'a: 'b+'c`
    RegionPredicate(WhereRegionPredicate),
    /// An equality predicate (unsupported)
    EqPredicate(WhereEqPredicate),
}

/// A type bound.
///
/// E.g. `for<'c> Foo: Send+Clone+'c`
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct WhereBoundPredicate {
    /// Any lifetimes from a `for` binding
    pub bound_lifetimes: Vec<LifetimeDef>,
    /// The type being bounded
    pub bounded_ty: Ty,
    /// Trait and lifetime bounds (`Clone+Send+'static`)
    pub bounds: Vec<TyParamBound>,
}

/// A lifetime predicate.
///
/// E.g. `'a: 'b+'c`
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct WhereRegionPredicate {
    pub lifetime: Lifetime,
    pub bounds: Vec<Lifetime>,
}

/// An equality predicate (unsupported).
///
/// E.g. `T=int`
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct WhereEqPredicate {
    pub lhs_ty: Ty,
    pub rhs_ty: Ty,
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use attr::parsing::outer_attr;
    use ident::parsing::ident;
    use ty::parsing::{ty, poly_trait_ref};

    named!(pub generics -> Generics, map!(
        alt!(
            do_parse!(
                punct!("<") >>
                lifetimes: separated_list!(punct!(","), lifetime_def) >>
                ty_params: opt_vec!(preceded!(
                    cond!(!lifetimes.is_empty(), punct!(",")),
                    separated_nonempty_list!(punct!(","), ty_param)
                )) >>
                cond!(!lifetimes.is_empty() || !ty_params.is_empty(), option!(punct!(","))) >>
                punct!(">") >>
                (lifetimes, ty_params)
            )
            |
            epsilon!() => { |_| (Vec::new(), Vec::new()) }
        ),
        |(lifetimes, ty_params)| Generics {
            lifetimes: lifetimes,
            ty_params: ty_params,
            where_clause: Default::default(),
        }
    ));

    named!(pub lifetime -> Lifetime, preceded!(
        punct!("'"),
        alt!(
            map!(ident, |id| Lifetime {
                ident: format!("'{}", id).into(),
            })
            |
            map!(keyword!("static"), |_| Lifetime {
                ident: "'static".into(),
            })
        )
    ));

    named!(pub lifetime_def -> LifetimeDef, do_parse!(
        attrs: many0!(outer_attr) >>
        life: lifetime >>
        bounds: opt_vec!(preceded!(
            punct!(":"),
            separated_list!(punct!("+"), lifetime)
        )) >>
        (LifetimeDef {
            attrs: attrs,
            lifetime: life,
            bounds: bounds,
        })
    ));

    named!(pub bound_lifetimes -> Vec<LifetimeDef>, opt_vec!(do_parse!(
        keyword!("for") >>
        punct!("<") >>
        lifetimes: terminated_list!(punct!(","), lifetime_def) >>
        punct!(">") >>
        (lifetimes)
    )));

    named!(ty_param -> TyParam, do_parse!(
        attrs: many0!(outer_attr) >>
        id: ident >>
        bounds: opt_vec!(preceded!(
            punct!(":"),
            separated_nonempty_list!(punct!("+"), ty_param_bound)
        )) >>
        default: option!(preceded!(
            punct!("="),
            ty
        )) >>
        (TyParam {
            attrs: attrs,
            ident: id,
            bounds: bounds,
            default: default,
        })
    ));

    named!(pub ty_param_bound -> TyParamBound, alt!(
        preceded!(punct!("?"), poly_trait_ref) => {
            |poly| TyParamBound::Trait(poly, TraitBoundModifier::Maybe)
        }
        |
        lifetime => { TyParamBound::Region }
        |
        poly_trait_ref => {
            |poly| TyParamBound::Trait(poly, TraitBoundModifier::None)
        }
    ));

    named!(pub where_clause -> WhereClause, alt!(
        do_parse!(
            keyword!("where") >>
            predicates: separated_nonempty_list!(punct!(","), where_predicate) >>
            option!(punct!(",")) >>
            (WhereClause { predicates: predicates })
        )
        |
        epsilon!() => { |_| Default::default() }
    ));

    named!(where_predicate -> WherePredicate, alt!(
        do_parse!(
            ident: lifetime >>
            bounds: opt_vec!(preceded!(
                punct!(":"),
                separated_list!(punct!("+"), lifetime)
            )) >>
            (WherePredicate::RegionPredicate(WhereRegionPredicate {
                lifetime: ident,
                bounds: bounds,
            }))
        )
        |
        do_parse!(
            bound_lifetimes: bound_lifetimes >>
            bounded_ty: ty >>
            punct!(":") >>
            bounds: separated_nonempty_list!(punct!("+"), ty_param_bound) >>
            (WherePredicate::BoundPredicate(WhereBoundPredicate {
                bound_lifetimes: bound_lifetimes,
                bounded_ty: bounded_ty,
                bounds: bounds,
            }))
        )
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use attr::FilterAttrs;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Generics {
        fn to_tokens(&self, tokens: &mut Tokens) {
            let has_lifetimes = !self.lifetimes.is_empty();
            let has_ty_params = !self.ty_params.is_empty();
            if has_lifetimes || has_ty_params {
                tokens.append("<");
                tokens.append_separated(&self.lifetimes, ",");
                if has_lifetimes && has_ty_params {
                    tokens.append(",");
                }
                tokens.append_separated(&self.ty_params, ",");
                tokens.append(">");
            }
        }
    }

    impl<'a> ToTokens for ImplGenerics<'a> {
        fn to_tokens(&self, tokens: &mut Tokens) {
            let has_lifetimes = !self.0.lifetimes.is_empty();
            let has_ty_params = !self.0.ty_params.is_empty();
            if has_lifetimes || has_ty_params {
                tokens.append("<");
                tokens.append_separated(&self.0.lifetimes, ",");
                // Leave off the type parameter defaults
                for (i, ty_param) in self.0
                        .ty_params
                        .iter()
                        .enumerate() {
                    if i > 0 || has_lifetimes {
                        tokens.append(",");
                    }
                    tokens.append_all(ty_param.attrs.outer());
                    ty_param.ident.to_tokens(tokens);
                    if !ty_param.bounds.is_empty() {
                        tokens.append(":");
                        tokens.append_separated(&ty_param.bounds, "+");
                    }
                }
                tokens.append(">");
            }
        }
    }

    impl<'a> ToTokens for TyGenerics<'a> {
        fn to_tokens(&self, tokens: &mut Tokens) {
            let has_lifetimes = !self.0.lifetimes.is_empty();
            let has_ty_params = !self.0.ty_params.is_empty();
            if has_lifetimes || has_ty_params {
                tokens.append("<");
                // Leave off the lifetime bounds and attributes
                let lifetimes = self.0
                    .lifetimes
                    .iter()
                    .map(|ld| &ld.lifetime);
                tokens.append_separated(lifetimes, ",");
                if has_lifetimes && has_ty_params {
                    tokens.append(",");
                }
                // Leave off the type parameter bounds, defaults, and attributes
                let ty_params = self.0
                    .ty_params
                    .iter()
                    .map(|tp| &tp.ident);
                tokens.append_separated(ty_params, ",");
                tokens.append(">");
            }
        }
    }

    impl<'a> ToTokens for Turbofish<'a> {
        fn to_tokens(&self, tokens: &mut Tokens) {
            let has_lifetimes = !self.0.lifetimes.is_empty();
            let has_ty_params = !self.0.ty_params.is_empty();
            if has_lifetimes || has_ty_params {
                tokens.append("::");
                TyGenerics(self.0).to_tokens(tokens);
            }
        }
    }

    impl ToTokens for Lifetime {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
        }
    }

    impl ToTokens for LifetimeDef {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.lifetime.to_tokens(tokens);
            if !self.bounds.is_empty() {
                tokens.append(":");
                tokens.append_separated(&self.bounds, "+");
            }
        }
    }

    impl ToTokens for TyParam {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.ident.to_tokens(tokens);
            if !self.bounds.is_empty() {
                tokens.append(":");
                tokens.append_separated(&self.bounds, "+");
            }
            if let Some(ref default) = self.default {
                tokens.append("=");
                default.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for TyParamBound {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                TyParamBound::Region(ref lifetime) => lifetime.to_tokens(tokens),
                TyParamBound::Trait(ref trait_ref, modifier) => {
                    match modifier {
                        TraitBoundModifier::None => {}
                        TraitBoundModifier::Maybe => tokens.append("?"),
                    }
                    trait_ref.to_tokens(tokens);
                }
            }
        }
    }

    impl ToTokens for WhereClause {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if !self.predicates.is_empty() {
                tokens.append("where");
                tokens.append_separated(&self.predicates, ",");
            }
        }
    }

    impl ToTokens for WherePredicate {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                WherePredicate::BoundPredicate(ref predicate) => {
                    predicate.to_tokens(tokens);
                }
                WherePredicate::RegionPredicate(ref predicate) => {
                    predicate.to_tokens(tokens);
                }
                WherePredicate::EqPredicate(ref predicate) => {
                    predicate.to_tokens(tokens);
                }
            }
        }
    }

    impl ToTokens for WhereBoundPredicate {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if !self.bound_lifetimes.is_empty() {
                tokens.append("for");
                tokens.append("<");
                tokens.append_separated(&self.bound_lifetimes, ",");
                tokens.append(">");
            }
            self.bounded_ty.to_tokens(tokens);
            if !self.bounds.is_empty() {
                tokens.append(":");
                tokens.append_separated(&self.bounds, "+");
            }
        }
    }

    impl ToTokens for WhereRegionPredicate {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.lifetime.to_tokens(tokens);
            if !self.bounds.is_empty() {
                tokens.append(":");
                tokens.append_separated(&self.bounds, "+");
            }
        }
    }

    impl ToTokens for WhereEqPredicate {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.lhs_ty.to_tokens(tokens);
            tokens.append("=");
            self.rhs_ty.to_tokens(tokens);
        }
    }
}
