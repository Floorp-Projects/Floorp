use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use ident::ToIdent;
use invoke::{Invoke, Identity};
use lifetime::{IntoLifetime, IntoLifetimeDef, LifetimeDefBuilder};
use path::{IntoPath, PathBuilder};
use symbol::ToSymbol;
use ty::TyBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct TyParamBuilder<F=Identity> {
    callback: F,
    span: Span,
    id: ast::Ident,
    bounds: Vec<ast::TyParamBound>,
    default: Option<P<ast::Ty>>,
}

impl TyParamBuilder {
    pub fn new<I>(id: I) -> Self
        where I: ToIdent,
    {
        TyParamBuilder::with_callback(id, Identity)
    }

    pub fn from_ty_param(ty_param: ast::TyParam) -> Self {
        TyParamBuilder::from_ty_param_with_callback(Identity, ty_param)
    }
}

impl<F> TyParamBuilder<F>
    where F: Invoke<ast::TyParam>,
{
    pub fn with_callback<I>(id: I, callback: F) -> Self
        where I: ToIdent
    {
        TyParamBuilder {
            callback: callback,
            span: DUMMY_SP,
            id: id.to_ident(),
            bounds: Vec::new(),
            default: None,
        }
    }

    pub fn from_ty_param_with_callback(callback: F, ty_param: ast::TyParam) -> Self {
        TyParamBuilder {
            callback: callback,
            span: ty_param.span,
            id: ty_param.ident,
            bounds: ty_param.bounds,
            default: ty_param.default,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_default(mut self, ty: P<ast::Ty>) -> Self {
        self.default = Some(ty);
        self
    }

    pub fn default(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn with_bound(mut self, bound: ast::TyParamBound) -> Self {
        self.bounds.push(bound);
        self
    }

    pub fn bound(self) -> TyParamBoundBuilder<Self> {
        TyParamBoundBuilder::with_callback(self)
    }

    pub fn with_trait_bound(self, trait_ref: ast::PolyTraitRef) -> Self {
        self.bound().build_trait(trait_ref, ast::TraitBoundModifier::None)
    }

    pub fn trait_bound<P>(self, path: P) -> PolyTraitRefBuilder<Self>
        where P: IntoPath,
    {
        PolyTraitRefBuilder::with_callback(path, self)
    }

    pub fn lifetime_bound<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetime,
    {
        let lifetime = lifetime.into_lifetime();

        self.bounds.push(ast::TyParamBound::RegionTyParamBound(lifetime));
        self
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(ast::TyParam {
            attrs: ast::ThinVec::new(),
            ident: self.id,
            id: ast::DUMMY_NODE_ID,
            bounds: self.bounds,
            default: self.default,
            span: self.span,
        })
    }
}

impl<F> Invoke<P<ast::Ty>> for TyParamBuilder<F>
    where F: Invoke<ast::TyParam>,
{
    type Result = Self;

    fn invoke(self, ty: P<ast::Ty>) -> Self {
        self.with_default(ty)
    }
}

impl<F> Invoke<ast::TyParamBound> for TyParamBuilder<F>
    where F: Invoke<ast::TyParam>,
{
    type Result = Self;

    fn invoke(self, bound: ast::TyParamBound) -> Self {
        self.with_bound(bound)
    }
}

impl<F> Invoke<ast::PolyTraitRef> for TyParamBuilder<F>
    where F: Invoke<ast::TyParam>,
{
    type Result = Self;

    fn invoke(self, trait_ref: ast::PolyTraitRef) -> Self {
        self.with_trait_bound(trait_ref)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyParamBoundBuilder<F=Identity> {
    callback: F,
    span: Span,
}

impl TyParamBoundBuilder {
    pub fn new() -> Self {
        TyParamBoundBuilder::with_callback(Identity)
    }
}

impl<F> TyParamBoundBuilder<F>
    where F: Invoke<ast::TyParamBound>,
{
    pub fn with_callback(callback: F) -> Self {
        TyParamBoundBuilder {
            callback: callback,
            span: DUMMY_SP,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn build_trait(self,
                             poly_trait: ast::PolyTraitRef,
                             modifier: ast::TraitBoundModifier) -> F::Result {
        let bound = ast::TyParamBound::TraitTyParamBound(poly_trait, modifier);
        self.callback.invoke(bound)
    }

    pub fn trait_<P>(self, path: P) -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<F>>
        where P: IntoPath,
    {
        let span = self.span;
        let builder = TraitTyParamBoundBuilder {
            builder: self,
            modifier: ast::TraitBoundModifier::None,
        };

        PolyTraitRefBuilder::with_callback(path, builder).span(span)
    }

    pub fn maybe_trait<P>(self, path: P) -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<F>>
        where P: IntoPath,
    {
        let span = self.span;
        let builder = TraitTyParamBoundBuilder {
            builder: self,
            modifier: ast::TraitBoundModifier::Maybe,
        };

        PolyTraitRefBuilder::with_callback(path, builder).span(span)
    }

    pub fn iterator(self, ty: P<ast::Ty>) -> PolyTraitRefBuilder<TraitTyParamBoundBuilder<F>> {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .id("std")
            .id("iter")
            .segment("Iterator")
                .binding("Item").build(ty)
                .build()
                .build();
        self.trait_(path)
    }

    pub fn lifetime<L>(self, lifetime: L) -> F::Result
        where L: IntoLifetime,
    {
        let lifetime = lifetime.into_lifetime();
        self.callback.invoke(ast::TyParamBound::RegionTyParamBound(lifetime))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TraitTyParamBoundBuilder<F> {
    builder: TyParamBoundBuilder<F>,
    modifier: ast::TraitBoundModifier,
}

impl<F> Invoke<ast::PolyTraitRef> for TraitTyParamBoundBuilder<F>
    where F: Invoke<ast::TyParamBound>,
{
    type Result = F::Result;

    fn invoke(self, poly_trait: ast::PolyTraitRef) -> Self::Result {
        self.builder.build_trait(poly_trait, self.modifier)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PolyTraitRefBuilder<F> {
    callback: F,
    span: Span,
    trait_ref: ast::TraitRef,
    lifetimes: Vec<ast::LifetimeDef>,
}

impl<F> PolyTraitRefBuilder<F>
    where F: Invoke<ast::PolyTraitRef>,
{
    pub fn with_callback<P>(path: P, callback: F) -> Self
        where P: IntoPath,
    {
        let trait_ref = ast::TraitRef {
            path: path.into_path(),
            ref_id: ast::DUMMY_NODE_ID,
        };

        PolyTraitRefBuilder {
            callback: callback,
            span: DUMMY_SP,
            trait_ref: trait_ref,
            lifetimes: Vec::new(),
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_lifetime<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetimeDef,
    {
        self.lifetimes.push(lifetime.into_lifetime_def());
        self
    }

    pub fn lifetime<N>(self, name: N) -> LifetimeDefBuilder<Self>
        where N: ToSymbol,
    {
        LifetimeDefBuilder::with_callback(name, self)
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(ast::PolyTraitRef {
            bound_lifetimes: self.lifetimes,
            trait_ref: self.trait_ref,
            span: self.span,
        })
    }
}

impl<F> Invoke<ast::LifetimeDef> for PolyTraitRefBuilder<F>
    where F: Invoke<ast::PolyTraitRef>,
{
    type Result = Self;

    fn invoke(self, lifetime: ast::LifetimeDef) -> Self {
        self.with_lifetime(lifetime)
    }
}
