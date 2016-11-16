use {Ident, LifetimeDef, Path, PolyTraitRef, TraitBoundModifier, Ty, TyParam, TyParamBound};
use aster::invoke::{Invoke, Identity};
use aster::lifetime::{IntoLifetime, IntoLifetimeDef, LifetimeDefBuilder};
use aster::path::{IntoPath, PathBuilder};
use aster::ty::TyBuilder;

// ////////////////////////////////////////////////////////////////////////////

pub struct TyParamBuilder<F = Identity> {
    callback: F,
    id: Ident,
    bounds: Vec<TyParamBound>,
    default: Option<Ty>,
}

impl TyParamBuilder {
    pub fn new<I>(id: I) -> Self
        where I: Into<Ident>
    {
        TyParamBuilder::with_callback(id, Identity)
    }

    pub fn from_ty_param(ty_param: TyParam) -> Self {
        TyParamBuilder::from_ty_param_with_callback(Identity, ty_param)
    }
}

impl<F> TyParamBuilder<F>
    where F: Invoke<TyParam>
{
    pub fn with_callback<I>(id: I, callback: F) -> Self
        where I: Into<Ident>
    {
        TyParamBuilder {
            callback: callback,
            id: id.into(),
            bounds: Vec::new(),
            default: None,
        }
    }

    pub fn from_ty_param_with_callback(callback: F, ty_param: TyParam) -> Self {
        TyParamBuilder {
            callback: callback,
            id: ty_param.ident,
            bounds: ty_param.bounds,
            default: ty_param.default,
        }
    }

    pub fn with_default(mut self, ty: Ty) -> Self {
        self.default = Some(ty);
        self
    }

    pub fn default(self) -> TyBuilder<Self> {
        TyBuilder::with_callback(self)
    }

    pub fn with_bound(mut self, bound: TyParamBound) -> Self {
        self.bounds.push(bound);
        self
    }

    pub fn bound(self) -> TyParamBoundBuilder<Self> {
        TyParamBoundBuilder::with_callback(self)
    }

    pub fn with_trait_bound(self, trait_ref: PolyTraitRef) -> Self {
        self.bound().build_trait(trait_ref, TraitBoundModifier::None)
    }

    pub fn trait_bound<P>(self, path: P) -> PolyTraitRefBuilder<Self>
        where P: IntoPath
    {
        PolyTraitRefBuilder::with_callback(path, self)
    }

    pub fn lifetime_bound<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetime
    {
        let lifetime = lifetime.into_lifetime();

        self.bounds.push(TyParamBound::Region(lifetime));
        self
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(TyParam {
            ident: self.id,
            bounds: self.bounds,
            default: self.default,
        })
    }
}

impl<F> Invoke<Ty> for TyParamBuilder<F>
    where F: Invoke<TyParam>
{
    type Result = Self;

    fn invoke(self, ty: Ty) -> Self {
        self.with_default(ty)
    }
}

impl<F> Invoke<TyParamBound> for TyParamBuilder<F>
    where F: Invoke<TyParam>
{
    type Result = Self;

    fn invoke(self, bound: TyParamBound) -> Self {
        self.with_bound(bound)
    }
}

impl<F> Invoke<PolyTraitRef> for TyParamBuilder<F>
    where F: Invoke<TyParam>
{
    type Result = Self;

    fn invoke(self, trait_ref: PolyTraitRef) -> Self {
        self.with_trait_bound(trait_ref)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TyParamBoundBuilder<F = Identity> {
    callback: F,
}

impl TyParamBoundBuilder {
    pub fn new() -> Self {
        TyParamBoundBuilder::with_callback(Identity)
    }
}

impl<F> TyParamBoundBuilder<F>
    where F: Invoke<TyParamBound>
{
    pub fn with_callback(callback: F) -> Self {
        TyParamBoundBuilder { callback: callback }
    }

    pub fn build_trait(self, poly_trait: PolyTraitRef, modifier: TraitBoundModifier) -> F::Result {
        let bound = TyParamBound::Trait(poly_trait, modifier);
        self.callback.invoke(bound)
    }

    pub fn trait_<P>(self, path: P) -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<F>>
        where P: IntoPath
    {
        let builder = TraitTyParamBoundBuilder {
            builder: self,
            modifier: TraitBoundModifier::None,
        };

        PolyTraitRefBuilder::with_callback(path, builder)
    }

    pub fn maybe_trait<P>(self, path: P) -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<F>>
        where P: IntoPath
    {
        let builder = TraitTyParamBoundBuilder {
            builder: self,
            modifier: TraitBoundModifier::Maybe,
        };

        PolyTraitRefBuilder::with_callback(path, builder)
    }

    pub fn iterator(self, ty: Ty) -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<F>> {
        let path = PathBuilder::new()
            .global()
            .id("std")
            .id("iter")
            .segment("Iterator")
            .binding("Item")
            .build(ty)
            .build()
            .build();
        self.trait_(path)
    }

    pub fn lifetime<L>(self, lifetime: L) -> F::Result
        where L: IntoLifetime
    {
        let lifetime = lifetime.into_lifetime();
        self.callback.invoke(TyParamBound::Region(lifetime))
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TraitTyParamBoundBuilder<F> {
    builder: TyParamBoundBuilder<F>,
    modifier: TraitBoundModifier,
}

impl<F> Invoke<PolyTraitRef> for TraitTyParamBoundBuilder<F>
    where F: Invoke<TyParamBound>
{
    type Result = F::Result;

    fn invoke(self, poly_trait: PolyTraitRef) -> Self::Result {
        self.builder.build_trait(poly_trait, self.modifier)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct PolyTraitRefBuilder<F> {
    callback: F,
    trait_ref: Path,
    lifetimes: Vec<LifetimeDef>,
}

impl<F> PolyTraitRefBuilder<F>
    where F: Invoke<PolyTraitRef>
{
    pub fn with_callback<P>(path: P, callback: F) -> Self
        where P: IntoPath
    {
        PolyTraitRefBuilder {
            callback: callback,
            trait_ref: path.into_path(),
            lifetimes: Vec::new(),
        }
    }

    pub fn with_lifetime<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetimeDef
    {
        self.lifetimes.push(lifetime.into_lifetime_def());
        self
    }

    pub fn lifetime<N>(self, name: N) -> LifetimeDefBuilder<Self>
        where N: Into<Ident>
    {
        LifetimeDefBuilder::with_callback(name, self)
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(PolyTraitRef {
            bound_lifetimes: self.lifetimes,
            trait_ref: self.trait_ref,
        })
    }
}

impl<F> Invoke<LifetimeDef> for PolyTraitRefBuilder<F>
    where F: Invoke<PolyTraitRef>
{
    type Result = Self;

    fn invoke(self, lifetime: LifetimeDef) -> Self {
        self.with_lifetime(lifetime)
    }
}
