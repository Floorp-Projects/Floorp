use super::*;

/// Represents lifetimes and type parameters attached to a declaration
/// of a function, enum, trait, etc.
#[derive(Debug, Clone, Eq, PartialEq, Default)]
pub struct Generics {
    pub lifetimes: Vec<LifetimeDef>,
    pub ty_params: Vec<TyParam>,
    pub where_clause: WhereClause,
}

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
    pub fn split_for_impl(&self) -> (Generics, Generics, WhereClause) {
        // Remove where clause and type parameter defaults.
        let impl_generics = Generics {
            lifetimes: self.lifetimes.clone(),
            ty_params: self.ty_params
                .iter()
                .map(|ty_param| {
                    TyParam {
                        ident: ty_param.ident.clone(),
                        bounds: ty_param.bounds.clone(),
                        default: None,
                    }
                })
                .collect(),
            where_clause: WhereClause::none(),
        };

        // Remove where clause, type parameter defaults, and bounds.
        let ty_generics = Generics {
            lifetimes: self.lifetimes
                .iter()
                .map(|lifetime_def| {
                    LifetimeDef {
                        lifetime: lifetime_def.lifetime.clone(),
                        bounds: Vec::new(),
                    }
                })
                .collect(),
            ty_params: self.ty_params
                .iter()
                .map(|ty_param| {
                    TyParam {
                        ident: ty_param.ident.clone(),
                        bounds: Vec::new(),
                        default: None,
                    }
                })
                .collect(),
            where_clause: WhereClause::none(),
        };

        let where_clause = self.where_clause.clone();

        (impl_generics, ty_generics, where_clause)
    }
}

#[derive(Debug, Clone, Eq, PartialEq)]
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
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct LifetimeDef {
    pub lifetime: Lifetime,
    pub bounds: Vec<Lifetime>,
}

impl LifetimeDef {
    pub fn new<T: Into<Ident>>(t: T) -> Self {
        LifetimeDef {
            lifetime: Lifetime::new(t),
            bounds: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct TyParam {
    pub ident: Ident,
    pub bounds: Vec<TyParamBound>,
    pub default: Option<Ty>,
}

/// The AST represents all type param bounds as types.
/// `typeck::collect::compute_bounds` matches these against
/// the "special" built-in traits (see `middle::lang_items`) and
/// detects Copy, Send and Sync.
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum TyParamBound {
    Trait(PolyTraitRef, TraitBoundModifier),
    Region(Lifetime),
}

/// A modifier on a bound, currently this is only used for `?Sized`, where the
/// modifier is `Maybe`. Negative bounds should also be handled here.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum TraitBoundModifier {
    None,
    Maybe,
}

/// A `where` clause in a definition
#[derive(Debug, Clone, Eq, PartialEq, Default)]
pub struct WhereClause {
    pub predicates: Vec<WherePredicate>,
}

impl WhereClause {
    pub fn none() -> Self {
        WhereClause { predicates: Vec::new() }
    }
}

/// A single predicate in a `where` clause
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum WherePredicate {
    /// A type binding, e.g. `for<'c> Foo: Send+Clone+'c`
    BoundPredicate(WhereBoundPredicate),
    /// A lifetime predicate, e.g. `'a: 'b+'c`
    RegionPredicate(WhereRegionPredicate),
}

/// A type bound.
///
/// E.g. `for<'c> Foo: Send+Clone+'c`
#[derive(Debug, Clone, Eq, PartialEq)]
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
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct WhereRegionPredicate {
    pub lifetime: Lifetime,
    pub bounds: Vec<Lifetime>,
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
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
        map!(ident, |id| Lifetime {
            ident: format!("'{}", id).into(),
        })
    ));

    named!(pub lifetime_def -> LifetimeDef, do_parse!(
        life: lifetime >>
        bounds: opt_vec!(preceded!(
            punct!(":"),
            separated_nonempty_list!(punct!("+"), lifetime)
        )) >>
        (LifetimeDef {
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
            punct!(":") >>
            bounds: separated_nonempty_list!(punct!("+"), lifetime) >>
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

    impl ToTokens for Lifetime {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
        }
    }

    impl ToTokens for LifetimeDef {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.lifetime.to_tokens(tokens);
            if !self.bounds.is_empty() {
                tokens.append(":");
                tokens.append_separated(&self.bounds, "+");
            }
        }
    }

    impl ToTokens for TyParam {
        fn to_tokens(&self, tokens: &mut Tokens) {
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
}
