use {Generics, Ident, LifetimeDef, TyParam, WhereClause, WherePredicate};
use aster::invoke::{Identity, Invoke};
use aster::lifetime::{IntoLifetime, LifetimeDefBuilder, IntoLifetimeDef};
use aster::path::IntoPath;
use aster::ty_param::TyParamBuilder;
use aster::where_predicate::WherePredicateBuilder;

pub struct GenericsBuilder<F = Identity> {
    callback: F,
    lifetimes: Vec<LifetimeDef>,
    ty_params: Vec<TyParam>,
    predicates: Vec<WherePredicate>,
}

impl GenericsBuilder {
    pub fn new() -> Self {
        GenericsBuilder::with_callback(Identity)
    }

    pub fn from_generics(generics: Generics) -> Self {
        GenericsBuilder::from_generics_with_callback(generics, Identity)
    }
}

impl<F> GenericsBuilder<F>
    where F: Invoke<Generics>
{
    pub fn with_callback(callback: F) -> Self {
        GenericsBuilder {
            callback: callback,
            lifetimes: Vec::new(),
            ty_params: Vec::new(),
            predicates: Vec::new(),
        }
    }

    pub fn from_generics_with_callback(generics: Generics, callback: F) -> Self {
        GenericsBuilder {
            callback: callback,
            lifetimes: generics.lifetimes,
            ty_params: generics.ty_params,
            predicates: generics.where_clause.predicates,
        }
    }

    pub fn with(self, generics: Generics) -> Self {
        self.with_lifetimes(generics.lifetimes.into_iter())
            .with_ty_params(generics.ty_params.into_iter())
            .with_predicates(generics.where_clause.predicates.into_iter())
    }

    pub fn with_lifetimes<I, L>(mut self, iter: I) -> Self
        where I: IntoIterator<Item = L>,
              L: IntoLifetimeDef
    {
        let iter = iter.into_iter().map(|lifetime_def| lifetime_def.into_lifetime_def());
        self.lifetimes.extend(iter);
        self
    }

    pub fn with_lifetime_names<I, N>(mut self, iter: I) -> Self
        where I: IntoIterator<Item = N>,
              N: Into<Ident>
    {
        for name in iter {
            self = self.lifetime_name(name);
        }
        self
    }

    pub fn with_lifetime(mut self, lifetime: LifetimeDef) -> Self {
        self.lifetimes.push(lifetime);
        self
    }

    pub fn lifetime_name<N>(self, name: N) -> Self
        where N: Into<Ident>
    {
        self.lifetime(name).build()
    }

    pub fn lifetime<N>(self, name: N) -> LifetimeDefBuilder<Self>
        where N: Into<Ident>
    {
        LifetimeDefBuilder::with_callback(name, self)
    }

    pub fn with_ty_params<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item = TyParam>
    {
        self.ty_params.extend(iter);
        self
    }

    pub fn with_ty_param_ids<I, T>(mut self, iter: I) -> Self
        where I: IntoIterator<Item = T>,
              T: Into<Ident>
    {
        for id in iter {
            self = self.ty_param_id(id);
        }
        self
    }

    pub fn with_ty_param(mut self, ty_param: TyParam) -> Self {
        self.ty_params.push(ty_param);
        self
    }

    pub fn ty_param_id<I>(self, id: I) -> Self
        where I: Into<Ident>
    {
        self.ty_param(id).build()
    }

    pub fn ty_param<I>(self, id: I) -> TyParamBuilder<Self>
        where I: Into<Ident>
    {
        TyParamBuilder::with_callback(id, self)
    }

    pub fn with_predicates<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item = WherePredicate>
    {
        self.predicates.extend(iter);
        self
    }

    pub fn with_predicate(mut self, predicate: WherePredicate) -> Self {
        self.predicates.push(predicate);
        self
    }

    pub fn predicate(self) -> WherePredicateBuilder<Self> {
        WherePredicateBuilder::with_callback(self)
    }

    pub fn add_lifetime_bound<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetime
    {
        let lifetime = lifetime.into_lifetime();

        for lifetime_def in &mut self.lifetimes {
            lifetime_def.bounds.push(lifetime.clone());
        }

        for ty_param in &mut self.ty_params {
            *ty_param = TyParamBuilder::from_ty_param(ty_param.clone())
                .lifetime_bound(lifetime.clone())
                .build();
        }

        self
    }

    pub fn add_ty_param_bound<P>(mut self, path: P) -> Self
        where P: IntoPath
    {
        let path = path.into_path();

        for ty_param in &mut self.ty_params {
            *ty_param = TyParamBuilder::from_ty_param(ty_param.clone())
                .trait_bound(path.clone())
                .build()
                .build();
        }

        self
    }

    pub fn strip_bounds(self) -> Self {
        self.strip_lifetimes().strip_ty_params().strip_predicates()
    }

    pub fn strip_lifetimes(mut self) -> Self {
        for lifetime in &mut self.lifetimes {
            lifetime.bounds = vec![];
        }
        self
    }

    pub fn strip_ty_params(mut self) -> Self {
        for ty_param in &mut self.ty_params {
            ty_param.bounds = vec![];
        }
        self
    }

    pub fn strip_predicates(mut self) -> Self {
        self.predicates = vec![];
        self
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(Generics {
                                 lifetimes: self.lifetimes,
                                 ty_params: self.ty_params,
                                 where_clause: WhereClause { predicates: self.predicates },
                             })
    }
}

impl<F> Invoke<LifetimeDef> for GenericsBuilder<F>
    where F: Invoke<Generics>
{
    type Result = Self;

    fn invoke(self, lifetime: LifetimeDef) -> Self {
        self.with_lifetime(lifetime)
    }
}

impl<F> Invoke<TyParam> for GenericsBuilder<F>
    where F: Invoke<Generics>
{
    type Result = Self;

    fn invoke(self, ty_param: TyParam) -> Self {
        self.with_ty_param(ty_param)
    }
}

impl<F> Invoke<WherePredicate> for GenericsBuilder<F>
    where F: Invoke<Generics>
{
    type Result = Self;

    fn invoke(self, predicate: WherePredicate) -> Self {
        self.with_predicate(predicate)
    }
}
