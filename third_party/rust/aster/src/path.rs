use std::iter::IntoIterator;

use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use invoke::{Invoke, Identity};

use ident::ToIdent;
use symbol::ToSymbol;
use ty::TyBuilder;

use lifetime::IntoLifetime;

//////////////////////////////////////////////////////////////////////////////

pub trait IntoPath {
    fn into_path(self) -> ast::Path;
}

impl IntoPath for ast::Path {
    fn into_path(self) -> ast::Path {
        self
    }
}

impl IntoPath for ast::Ident {
    fn into_path(self) -> ast::Path {
        PathBuilder::new().id(self).build()
    }
}

impl<'a> IntoPath for &'a str {
    fn into_path(self) -> ast::Path {
        PathBuilder::new().id(self).build()
    }
}

impl IntoPath for String {
    fn into_path(self) -> ast::Path {
        (&*self).into_path()
    }
}

impl<'a, T> IntoPath for &'a [T] where T: ToIdent {
    fn into_path(self) -> ast::Path {
        PathBuilder::new().ids(self).build()
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PathBuilder<F=Identity> {
    callback: F,
    span: Span,
    global: bool,
}

impl PathBuilder {
    pub fn new() -> Self {
        PathBuilder::with_callback(Identity)
    }
}

impl<F> PathBuilder<F>
    where F: Invoke<ast::Path>,
{
    pub fn with_callback(callback: F) -> Self {
        PathBuilder {
            callback: callback,
            span: DUMMY_SP,
            global: false,
        }
    }

    pub fn build(self, path: ast::Path) -> F::Result {
        self.callback.invoke(path)
    }

    /// Update the span to start from this location.
    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn global(mut self) -> Self {
        self.global = true;
        self
    }

    pub fn ids<I, T>(self, ids: I) -> PathSegmentsBuilder<F>
        where I: IntoIterator<Item=T>,
              T: ToIdent,
    {
        let mut ids = ids.into_iter();
        let id = ids.next().expect("passed path with no id");

        self.id(id).ids(ids)
    }

    pub fn id<I>(self, id: I) -> PathSegmentsBuilder<F>
        where I: ToIdent,
    {
        self.segment(id).build()
    }

    pub fn segment<I>(self, id: I)
        -> PathSegmentBuilder<PathSegmentsBuilder<F>>
        where I: ToIdent,
    {
        let mut segments = vec![];

        if self.global {
            segments.push(ast::PathSegment::crate_root());
        }

        PathSegmentBuilder::with_callback(id, PathSegmentsBuilder {
            callback: self.callback,
            span: self.span,
            segments: segments,
        })
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PathSegmentsBuilder<F=Identity> {
    callback: F,
    span: Span,
    segments: Vec<ast::PathSegment>,
}

impl<F> PathSegmentsBuilder<F>
    where F: Invoke<ast::Path>,
{
    pub fn ids<I, T>(mut self, ids: I) -> PathSegmentsBuilder<F>
        where I: IntoIterator<Item=T>,
              T: ToIdent,
    {
        for id in ids {
            self = self.id(id);
        }

        self
    }

    pub fn id<T>(self, id: T) -> PathSegmentsBuilder<F>
        where T: ToIdent,
    {
        self.segment(id).build()
    }

    pub fn segment<T>(self, id: T) -> PathSegmentBuilder<Self>
        where T: ToIdent,
    {
        let span = self.span;
        PathSegmentBuilder::with_callback(id, self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(ast::Path {
            span: self.span,
            segments: self.segments,
        })
    }
}

impl<F> Invoke<ast::PathSegment> for PathSegmentsBuilder<F> {
    type Result = Self;

    fn invoke(mut self, segment: ast::PathSegment) -> Self {
        self.segments.push(segment);
        self
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PathSegmentBuilder<F=Identity> {
    callback: F,
    span: Span,
    id: ast::Ident,
    lifetimes: Vec<ast::Lifetime>,
    tys: Vec<P<ast::Ty>>,
    bindings: Vec<ast::TypeBinding>,
}

impl<F> PathSegmentBuilder<F>
    where F: Invoke<ast::PathSegment>,
{
    pub fn with_callback<I>(id: I, callback: F) -> Self
        where I: ToIdent,
    {
        PathSegmentBuilder {
            callback: callback,
            span: DUMMY_SP,
            id: id.to_ident(),
            lifetimes: Vec::new(),
            tys: Vec::new(),
            bindings: Vec::new(),
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_generics(self, generics: ast::Generics) -> Self {
        // Strip off the bounds.
        let lifetimes = generics.lifetimes.iter()
            .map(|lifetime_def| lifetime_def.lifetime);

        let span = self.span;

        let tys = generics.ty_params.iter()
            .map(|ty_param| TyBuilder::new().span(span).id(ty_param.ident));

        self.with_lifetimes(lifetimes)
            .with_tys(tys)
    }

    pub fn with_lifetimes<I, L>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=L>,
              L: IntoLifetime,
    {
        let iter = iter.into_iter().map(|lifetime| lifetime.into_lifetime());
        self.lifetimes.extend(iter);
        self
    }

    pub fn with_lifetime<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetime,
    {
        self.lifetimes.push(lifetime.into_lifetime());
        self
    }

    pub fn lifetime<N>(self, name: N) -> Self
        where N: ToSymbol,
    {
        let lifetime = ast::Lifetime {
            id: ast::DUMMY_NODE_ID,
            span: self.span,
            name: name.to_symbol(),
        };
        self.with_lifetime(lifetime)
    }

    pub fn with_tys<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=P<ast::Ty>>,
    {
        self.tys.extend(iter);
        self
    }

    pub fn with_ty(mut self, ty: P<ast::Ty>) -> Self {
        self.tys.push(ty);
        self
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn with_binding(mut self, binding: ast::TypeBinding) -> Self {
        self.bindings.push(binding);
        self
    }

    pub fn binding<T>(self, id: T) -> TyBuilder<TypeBindingBuilder<F>>
        where T: ToIdent,
    {
        let span = self.span;
        TyBuilder::with_callback(TypeBindingBuilder {
            id: id.to_ident(),
            builder: self,
        }).span(span)
    }

    pub fn no_return(self) -> F::Result {
        self.build_return(None)
    }

    pub fn return_(self) -> TyBuilder<PathSegmentReturnBuilder<F>> {
        TyBuilder::with_callback(PathSegmentReturnBuilder(self))
    }

    pub fn build_return(self, output: Option<P<ast::Ty>>) -> F::Result {
        let parameters = if self.tys.is_empty() {
            None
        } else {
            let data = ast::ParenthesizedParameterData {
                span: self.span,
                inputs: self.tys,
                output: output,
            };

            Some(P(ast::PathParameters::Parenthesized(data)))
        };

        self.callback.invoke(ast::PathSegment {
            identifier: self.id,
            parameters: parameters,
        })
    }

    pub fn build(self) -> F::Result {
        let parameters = if self.lifetimes.is_empty() &&
                            self.tys.is_empty() &&
                            self.bindings.is_empty() {
            None
        } else {
            let data = ast::AngleBracketedParameterData {
                lifetimes: self.lifetimes,
                types: self.tys,
                bindings: self.bindings,
            };

            Some(P(ast::PathParameters::AngleBracketed(data)))
        };

        self.callback.invoke(ast::PathSegment {
            identifier: self.id,
            parameters: parameters,
        })
    }
}

impl<F> Invoke<P<ast::Ty>> for PathSegmentBuilder<F>
    where F: Invoke<ast::PathSegment>
{
    type Result = Self;

    fn invoke(self, ty: P<ast::Ty>) -> Self {
        self.with_ty(ty)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TypeBindingBuilder<F> {
    id: ast::Ident,
    builder: PathSegmentBuilder<F>,
}

impl<F> Invoke<P<ast::Ty>> for TypeBindingBuilder<F>
    where F: Invoke<ast::PathSegment>
{
    type Result = PathSegmentBuilder<F>;

    fn invoke(self, ty: P<ast::Ty>) -> Self::Result {
        let id = self.id;
        let span = self.builder.span;

        self.builder.with_binding(ast::TypeBinding {
            id: ast::DUMMY_NODE_ID,
            ident: id,
            ty: ty,
            span: span,
        })
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PathSegmentReturnBuilder<F>(PathSegmentBuilder<F>);

impl<F> Invoke<P<ast::Ty>> for PathSegmentReturnBuilder<F>
    where F: Invoke<ast::PathSegment>
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> Self::Result {
        self.0.build_return(Some(ty))
    }
}
