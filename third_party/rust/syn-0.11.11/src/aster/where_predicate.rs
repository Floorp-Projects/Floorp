use {Ident, Lifetime, LifetimeDef, Ty, TyParamBound, WhereBoundPredicate, WherePredicate,
     WhereRegionPredicate};
use aster::invoke::{Invoke, Identity};
use aster::lifetime::{IntoLifetime, IntoLifetimeDef, LifetimeDefBuilder};
use aster::path::IntoPath;
use aster::ty::TyBuilder;
use aster::ty_param::{TyParamBoundBuilder, PolyTraitRefBuilder, TraitTyParamBoundBuilder};

// ////////////////////////////////////////////////////////////////////////////

pub struct WherePredicateBuilder<F = Identity> {
    callback: F,
}

impl WherePredicateBuilder {
    pub fn new() -> Self {
        WherePredicateBuilder::with_callback(Identity)
    }
}

impl<F> WherePredicateBuilder<F>
    where F: Invoke<WherePredicate>
{
    pub fn with_callback(callback: F) -> Self {
        WherePredicateBuilder { callback: callback }
    }

    pub fn bound(self) -> TyBuilder<Self> {
        TyBuilder::with_callback(self)
    }

    pub fn lifetime<L>(self, lifetime: L) -> WhereRegionPredicateBuilder<F>
        where L: IntoLifetime
    {
        WhereRegionPredicateBuilder {
            callback: self.callback,
            lifetime: lifetime.into_lifetime(),
            bounds: Vec::new(),
        }
    }
}

impl<F> Invoke<Ty> for WherePredicateBuilder<F>
    where F: Invoke<WherePredicate>
{
    type Result = WhereBoundPredicateTyBuilder<F>;

    fn invoke(self, ty: Ty) -> Self::Result {
        WhereBoundPredicateTyBuilder {
            callback: self.callback,
            ty: ty,
            bound_lifetimes: Vec::new(),
        }
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct WhereBoundPredicateBuilder<F> {
    callback: F,
}

impl<F> Invoke<Ty> for WhereBoundPredicateBuilder<F>
    where F: Invoke<WherePredicate>
{
    type Result = WhereBoundPredicateTyBuilder<F>;

    fn invoke(self, ty: Ty) -> Self::Result {
        WhereBoundPredicateTyBuilder {
            callback: self.callback,
            ty: ty,
            bound_lifetimes: Vec::new(),
        }
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct WhereBoundPredicateTyBuilder<F> {
    callback: F,
    ty: Ty,
    bound_lifetimes: Vec<LifetimeDef>,
}

impl<F> WhereBoundPredicateTyBuilder<F>
    where F: Invoke<WherePredicate>
{
    pub fn with_for_lifetime<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetimeDef
    {
        self.bound_lifetimes.push(lifetime.into_lifetime_def());
        self
    }

    pub fn for_lifetime<N>(self, name: N) -> LifetimeDefBuilder<Self>
        where N: Into<Ident>
    {
        LifetimeDefBuilder::with_callback(name, self)
    }

    pub fn with_bound(self, bound: TyParamBound) -> WhereBoundPredicateTyBoundsBuilder<F> {
        WhereBoundPredicateTyBoundsBuilder {
            callback: self.callback,
            ty: self.ty,
            bound_lifetimes: self.bound_lifetimes,
            bounds: vec![bound],
        }
    }

    pub fn bound(self) -> TyParamBoundBuilder<WhereBoundPredicateTyBoundsBuilder<F>> {
        let builder = WhereBoundPredicateTyBoundsBuilder {
            callback: self.callback,
            ty: self.ty,
            bound_lifetimes: self.bound_lifetimes,
            bounds: vec![],
        };
        TyParamBoundBuilder::with_callback(builder)
    }

    pub fn trait_<P>
        (self,
         path: P)
         -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<WhereBoundPredicateTyBoundsBuilder<F>>>
        where P: IntoPath
    {
        self.bound().trait_(path)
    }

    pub fn lifetime<L>(self, lifetime: L) -> WhereBoundPredicateTyBoundsBuilder<F>
        where L: IntoLifetime
    {
        self.bound().lifetime(lifetime)
    }
}

impl<F> Invoke<LifetimeDef> for WhereBoundPredicateTyBuilder<F>
    where F: Invoke<WherePredicate>
{
    type Result = Self;

    fn invoke(self, lifetime: LifetimeDef) -> Self {
        self.with_for_lifetime(lifetime)
    }
}

impl<F> Invoke<TyParamBound> for WhereBoundPredicateTyBuilder<F>
    where F: Invoke<WherePredicate>
{
    type Result = WhereBoundPredicateTyBoundsBuilder<F>;

    fn invoke(self, bound: TyParamBound) -> Self::Result {
        self.with_bound(bound)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct WhereBoundPredicateTyBoundsBuilder<F> {
    callback: F,
    ty: Ty,
    bound_lifetimes: Vec<LifetimeDef>,
    bounds: Vec<TyParamBound>,
}

impl<F> WhereBoundPredicateTyBoundsBuilder<F>
    where F: Invoke<WherePredicate>
{
    pub fn with_for_lifetime<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetimeDef
    {
        self.bound_lifetimes.push(lifetime.into_lifetime_def());
        self
    }

    pub fn for_lifetime<N>(self, name: N) -> LifetimeDefBuilder<Self>
        where N: Into<Ident>
    {
        LifetimeDefBuilder::with_callback(name, self)
    }

    pub fn with_bound(mut self, bound: TyParamBound) -> Self {
        self.bounds.push(bound);
        self
    }

    pub fn bound(self) -> TyParamBoundBuilder<Self> {
        TyParamBoundBuilder::with_callback(self)
    }

    pub fn trait_<P>(self, path: P) -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<Self>>
        where P: IntoPath
    {
        self.bound().trait_(path)
    }

    pub fn lifetime<L>(self, lifetime: L) -> Self
        where L: IntoLifetime
    {
        self.bound().lifetime(lifetime)
    }

    pub fn build(self) -> F::Result {
        let predicate = WhereBoundPredicate {
            bound_lifetimes: self.bound_lifetimes,
            bounded_ty: self.ty,
            bounds: self.bounds,
        };

        self.callback.invoke(WherePredicate::BoundPredicate(predicate))
    }
}

impl<F> Invoke<LifetimeDef> for WhereBoundPredicateTyBoundsBuilder<F>
    where F: Invoke<WherePredicate>
{
    type Result = Self;

    fn invoke(self, lifetime: LifetimeDef) -> Self {
        self.with_for_lifetime(lifetime)
    }
}

impl<F> Invoke<TyParamBound> for WhereBoundPredicateTyBoundsBuilder<F>
    where F: Invoke<WherePredicate>
{
    type Result = Self;

    fn invoke(self, bound: TyParamBound) -> Self {
        self.with_bound(bound)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct WhereRegionPredicateBuilder<F> {
    callback: F,
    lifetime: Lifetime,
    bounds: Vec<Lifetime>,
}

impl<F> WhereRegionPredicateBuilder<F>
    where F: Invoke<WherePredicate>
{
    pub fn bound<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetime
    {
        self.bounds.push(lifetime.into_lifetime());
        self
    }

    pub fn build(self) -> F::Result {
        let predicate = WhereRegionPredicate {
            lifetime: self.lifetime,
            bounds: self.bounds,
        };

        self.callback.invoke(WherePredicate::RegionPredicate(predicate))
    }
}
