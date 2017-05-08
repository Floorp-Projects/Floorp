use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use invoke::{Invoke, Identity};
use lifetime::{IntoLifetime, IntoLifetimeDef, LifetimeDefBuilder};
use path::IntoPath;
use symbol::ToSymbol;
use ty::TyBuilder;
use ty_param::{TyParamBoundBuilder, PolyTraitRefBuilder, TraitTyParamBoundBuilder};

//////////////////////////////////////////////////////////////////////////////

pub struct WherePredicateBuilder<F=Identity> {
    callback: F,
    span: Span,
}

impl WherePredicateBuilder {
    pub fn new() -> Self {
        WherePredicateBuilder::with_callback(Identity)
    }
}

impl<F> WherePredicateBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    pub fn with_callback(callback: F) -> Self {
        WherePredicateBuilder {
            callback: callback,
            span: DUMMY_SP,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn bound(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn lifetime<L>(self, lifetime: L) -> WhereRegionPredicateBuilder<F>
        where L: IntoLifetime,
    {
        WhereRegionPredicateBuilder {
            callback: self.callback,
            span: self.span,
            lifetime: lifetime.into_lifetime(),
            bounds: Vec::new(),
        }
    }

    pub fn eq<P>(self, p: P) -> WhereEqPredicateBuilder<F>
        where P: IntoPath,
    {
        WhereEqPredicateBuilder {
            callback: self.callback,
            span: self.span,
            lhs: TyBuilder::new().build_path(p.into_path()),
        }
    }
}

impl<F> Invoke<P<ast::Ty>> for WherePredicateBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    type Result = WhereBoundPredicateTyBuilder<F>;

    fn invoke(self, ty: P<ast::Ty>) -> Self::Result {
        WhereBoundPredicateTyBuilder {
            callback: self.callback,
            span: self.span,
            ty: ty,
            bound_lifetimes: Vec::new(),
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct WhereBoundPredicateBuilder<F> {
    callback: F,
    span: Span,
}

impl<F> Invoke<P<ast::Ty>> for WhereBoundPredicateBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    type Result = WhereBoundPredicateTyBuilder<F>;

    fn invoke(self, ty: P<ast::Ty>) -> Self::Result {
        WhereBoundPredicateTyBuilder {
            callback: self.callback,
            span: self.span,
            ty: ty,
            bound_lifetimes: Vec::new(),
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct WhereBoundPredicateTyBuilder<F> {
    callback: F,
    span: Span,
    ty: P<ast::Ty>,
    bound_lifetimes: Vec<ast::LifetimeDef>,
}

impl<F> WhereBoundPredicateTyBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    pub fn with_for_lifetime<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetimeDef,
    {
        self.bound_lifetimes.push(lifetime.into_lifetime_def());
        self
    }

    pub fn for_lifetime<N>(self, name: N) -> LifetimeDefBuilder<Self>
        where N: ToSymbol,
    {
        LifetimeDefBuilder::with_callback(name, self)
    }

    pub fn with_bound(self, bound: ast::TyParamBound) -> WhereBoundPredicateTyBoundsBuilder<F> {
        WhereBoundPredicateTyBoundsBuilder {
            callback: self.callback,
            span: self.span,
            ty: self.ty,
            bound_lifetimes: self.bound_lifetimes,
            bounds: vec![bound],
        }
    }

    pub fn bound(self) -> TyParamBoundBuilder<WhereBoundPredicateTyBoundsBuilder<F>> {
        let span = self.span;
        let builder = WhereBoundPredicateTyBoundsBuilder {
            callback: self.callback,
            span: self.span,
            ty: self.ty,
            bound_lifetimes: self.bound_lifetimes,
            bounds: vec![],
        };
        TyParamBoundBuilder::with_callback(builder).span(span)
    }

    pub fn trait_<P>(self, path: P)
        -> PolyTraitRefBuilder<
            TraitTyParamBoundBuilder<
                WhereBoundPredicateTyBoundsBuilder<F>
            >
        >
        where P: IntoPath,
    {
        self.bound().trait_(path)
    }

    pub fn lifetime<L>(self, lifetime: L) -> WhereBoundPredicateTyBoundsBuilder<F>
        where L: IntoLifetime,
    {
        self.bound().lifetime(lifetime)
    }
}

impl<F> Invoke<ast::LifetimeDef> for WhereBoundPredicateTyBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    type Result = Self;

    fn invoke(self, lifetime: ast::LifetimeDef) -> Self {
        self.with_for_lifetime(lifetime)
    }
}

impl<F> Invoke<ast::TyParamBound> for WhereBoundPredicateTyBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    type Result = WhereBoundPredicateTyBoundsBuilder<F>;

    fn invoke(self, bound: ast::TyParamBound) -> Self::Result {
        self.with_bound(bound)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct WhereBoundPredicateTyBoundsBuilder<F> {
    callback: F,
    span: Span,
    ty: P<ast::Ty>,
    bound_lifetimes: Vec<ast::LifetimeDef>,
    bounds: Vec<ast::TyParamBound>,
}

impl<F> WhereBoundPredicateTyBoundsBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    pub fn with_for_lifetime<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetimeDef,
    {
        self.bound_lifetimes.push(lifetime.into_lifetime_def());
        self
    }

    pub fn for_lifetime<N>(self, name: N) -> LifetimeDefBuilder<Self>
        where N: ToSymbol,
    {
        LifetimeDefBuilder::with_callback(name, self)
    }

    pub fn with_bound(mut self, bound: ast::TyParamBound) -> Self {
        self.bounds.push(bound);
        self
    }

    pub fn bound(self) -> TyParamBoundBuilder<Self> {
        let span = self.span;
        TyParamBoundBuilder::with_callback(self).span(span)
    }

    pub fn trait_<P>(self, path: P)
        -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<Self>>
        where P: IntoPath,
    {
        self.bound().trait_(path)
    }

    pub fn lifetime<L>(self, lifetime: L) -> Self
        where L: IntoLifetime,
    {
        self.bound().lifetime(lifetime)
    }

    pub fn build(self) -> F::Result {
        let predicate = ast::WhereBoundPredicate {
            span: self.span,
            bound_lifetimes: self.bound_lifetimes,
            bounded_ty: self.ty,
            bounds: self.bounds,
        };

        self.callback.invoke(ast::WherePredicate::BoundPredicate(predicate))
    }
}

impl<F> Invoke<ast::LifetimeDef> for WhereBoundPredicateTyBoundsBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    type Result = Self;

    fn invoke(self, lifetime: ast::LifetimeDef) -> Self {
        self.with_for_lifetime(lifetime)
    }
}

impl<F> Invoke<ast::TyParamBound> for WhereBoundPredicateTyBoundsBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    type Result = Self;

    fn invoke(self, bound: ast::TyParamBound) -> Self {
        self.with_bound(bound)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct WhereRegionPredicateBuilder<F> {
    callback: F,
    span: Span,
    lifetime: ast::Lifetime,
    bounds: Vec<ast::Lifetime>,
}

impl<F> WhereRegionPredicateBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    pub fn bound<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetime,
    {
        self.bounds.push(lifetime.into_lifetime());
        self
    }

    pub fn build(self) -> F::Result {
        let predicate = ast::WhereRegionPredicate {
            span: self.span,
            lifetime: self.lifetime,
            bounds: self.bounds,
        };

        self.callback.invoke(ast::WherePredicate::RegionPredicate(predicate))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct WhereEqPredicateBuilder<F> {
    callback: F,
    span: Span,
    lhs: P<ast::Ty>,
}

impl<F> WhereEqPredicateBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn build_ty(self, ty: P<ast::Ty>) -> F::Result {
        let WhereEqPredicateBuilder { callback, span, lhs } = self;

        let predicate = ast::WhereEqPredicate {
            id: ast::DUMMY_NODE_ID,
            span: span,
            lhs_ty: lhs,
            rhs_ty: ty,
        };

        callback.invoke(ast::WherePredicate::EqPredicate(predicate))
    }
}

impl<F> Invoke<P<ast::Ty>> for WhereEqPredicateBuilder<F>
    where F: Invoke<ast::WherePredicate>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.build_ty(ty)
    }
}
