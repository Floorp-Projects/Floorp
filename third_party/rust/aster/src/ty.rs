use std::iter::IntoIterator;

use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use expr::ExprBuilder;
use ident::ToIdent;
use invoke::{Invoke, Identity};
use lifetime::IntoLifetime;
use path::PathBuilder;
use qpath::QPathBuilder;
use symbol::ToSymbol;
use ty_param::TyParamBoundBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct TyBuilder<F=Identity> {
    callback: F,
    span: Span,
}

impl TyBuilder {
    pub fn new() -> Self {
        TyBuilder::with_callback(Identity)
    }
}

impl<F> TyBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    pub fn with_callback(callback: F) -> Self {
        TyBuilder {
            callback: callback,
            span: DUMMY_SP,
        }
    }

    pub fn build(self, ty: P<ast::Ty>) -> F::Result {
        self.callback.invoke(ty)
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn build_ty_kind(self, ty_: ast::TyKind) -> F::Result {
        let span = self.span;
        self.build(P(ast::Ty {
            id: ast::DUMMY_NODE_ID,
            node: ty_,
            span: span,
        }))
    }

    pub fn id<I>(self, id: I) -> F::Result
        where I: ToIdent,
    {
        self.path().id(id).build()
    }

    pub fn build_path(self, path: ast::Path) -> F::Result {
        self.build_ty_kind(ast::TyKind::Path(None, path))
    }

    pub fn build_qpath(self, qself: ast::QSelf, path: ast::Path) -> F::Result {
        self.build_ty_kind(ast::TyKind::Path(Some(qself), path))
    }

    pub fn path(self) -> PathBuilder<TyPathBuilder<F>> {
        PathBuilder::with_callback(TyPathBuilder(self))
    }

    pub fn qpath(self) -> QPathBuilder<TyQPathBuilder<F>> {
        QPathBuilder::with_callback(TyQPathBuilder(self))
    }

    pub fn isize(self) -> F::Result {
        self.id("isize")
    }

    pub fn i8(self) -> F::Result {
        self.id("i8")
    }

    pub fn i16(self) -> F::Result {
        self.id("i16")
    }

    pub fn i32(self) -> F::Result {
        self.id("i32")
    }

    pub fn i64(self) -> F::Result {
        self.id("i64")
    }

    pub fn usize(self) -> F::Result {
        self.id("usize")
    }

    pub fn u8(self) -> F::Result {
        self.id("u8")
    }

    pub fn u16(self) -> F::Result {
        self.id("u16")
    }

    pub fn u32(self) -> F::Result {
        self.id("u32")
    }

    pub fn u64(self) -> F::Result {
        self.id("u64")
    }

    pub fn f32(self) -> F::Result {
        self.id("f32")
    }

    pub fn f64(self) -> F::Result {
        self.id("f64")
    }

    pub fn bool(self) -> F::Result {
        self.id("bool")
    }

    pub fn unit(self) -> F::Result {
        self.tuple().build()
    }

    pub fn tuple(self) -> TyTupleBuilder<F> {
        TyTupleBuilder {
            builder: self,
            tys: vec![],
        }
    }

    pub fn array(self, len: usize) -> TyBuilder<TyArrayBuilder<F>> {
        let span = self.span;
        TyBuilder::with_callback(TyArrayBuilder(self, len)).span(span)
    }

    pub fn build_slice(self, ty: P<ast::Ty>) -> F::Result {
        self.build_ty_kind(ast::TyKind::Slice(ty))
    }

    pub fn build_array(self, ty: P<ast::Ty>, len: usize) -> F::Result {
        let len_expr = ExprBuilder::new().usize(len);
        self.build_ty_kind(ast::TyKind::Array(ty, len_expr))
    }

    pub fn slice(self) -> TyBuilder<TySliceBuilder<F>> {
        let span = self.span;
        TyBuilder::with_callback(TySliceBuilder(self)).span(span)
    }

    pub fn ref_(self) -> TyRefBuilder<F> {
        TyRefBuilder {
            builder: self,
            lifetime: None,
            mutability: ast::Mutability::Immutable,
        }
    }

    pub fn never(self) -> F::Result {
        self.build_ty_kind(ast::TyKind::Never)
    }

    pub fn infer(self) -> F::Result {
        self.build_ty_kind(ast::TyKind::Infer)
    }

    pub fn option(self) -> TyBuilder<TyOptionBuilder<F>> {
        let span = self.span;
        TyBuilder::with_callback(TyOptionBuilder(self)).span(span)
    }

    pub fn result(self) -> TyBuilder<TyResultOkBuilder<F>> {
        let span = self.span;
        TyBuilder::with_callback(TyResultOkBuilder(self)).span(span)
    }

    pub fn phantom_data(self) -> TyBuilder<TyPhantomDataBuilder<F>> {
        let span = self.span;
        TyBuilder::with_callback(TyPhantomDataBuilder(self)).span(span)
    }

    pub fn box_(self) -> TyBuilder<TyBoxBuilder<F>> {
        let span = self.span;
        TyBuilder::with_callback(TyBoxBuilder(self)).span(span)
    }

    pub fn iterator(self) -> TyBuilder<TyIteratorBuilder<F>> {
        let span = self.span;
        TyBuilder::with_callback(TyIteratorBuilder(self)).span(span)
    }

    pub fn impl_trait(self) -> TyImplTraitTyBuilder<F> {
        TyImplTraitTyBuilder { builder: self, bounds: Vec::new() }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyPathBuilder<F>(TyBuilder<F>);

impl<F> Invoke<ast::Path> for TyPathBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, path: ast::Path) -> F::Result {
        self.0.build_path(path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyQPathBuilder<F>(TyBuilder<F>);

impl<F> Invoke<(ast::QSelf, ast::Path)> for TyQPathBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, (qself, path): (ast::QSelf, ast::Path)) -> F::Result {
        self.0.build_qpath(qself, path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TySliceBuilder<F>(TyBuilder<F>);

impl<F> Invoke<P<ast::Ty>> for TySliceBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.0.build_slice(ty)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyArrayBuilder<F>(TyBuilder<F>, usize);

impl<F> Invoke<P<ast::Ty>> for TyArrayBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.0.build_array(ty, self.1)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyRefBuilder<F> {
    builder: TyBuilder<F>,
    lifetime: Option<ast::Lifetime>,
    mutability: ast::Mutability,
}

impl<F> TyRefBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    pub fn mut_(mut self) -> Self {
        self.mutability = ast::Mutability::Mutable;
        self
    }

    pub fn lifetime<N>(mut self, name: N) -> Self
        where N: ToSymbol,
    {
        self.lifetime = Some(ast::Lifetime {
            id: ast::DUMMY_NODE_ID,
            span: self.builder.span,
            name: name.to_symbol(),
        });
        self
    }

    pub fn build_ty(self, ty: P<ast::Ty>) -> F::Result {
        let ty = ast::MutTy {
            ty: ty,
            mutbl: self.mutability,
        };
        self.builder.build_ty_kind(ast::TyKind::Rptr(self.lifetime, ty))
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.builder.span;
        TyBuilder::with_callback(self).span(span)
    }
}

impl<F> Invoke<P<ast::Ty>> for TyRefBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.build_ty(ty)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyOptionBuilder<F>(TyBuilder<F>);

impl<F> Invoke<P<ast::Ty>> for TyOptionBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        let path = PathBuilder::new()
            .span(self.0.span)
            .global()
            .id("std")
            .id("option")
            .segment("Option")
                .with_ty(ty)
                .build()
            .build();

        self.0.build_path(path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyResultOkBuilder<F>(TyBuilder<F>);

impl<F> Invoke<P<ast::Ty>> for TyResultOkBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = TyBuilder<TyResultErrBuilder<F>>;

    fn invoke(self, ty: P<ast::Ty>) -> TyBuilder<TyResultErrBuilder<F>> {
        let span = self.0.span;
        TyBuilder::with_callback(TyResultErrBuilder(self.0, ty)).span(span)
    }
}

pub struct TyResultErrBuilder<F>(TyBuilder<F>, P<ast::Ty>);

impl<F> Invoke<P<ast::Ty>> for TyResultErrBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        let path = PathBuilder::new()
            .span(self.0.span)
            .global()
            .id("std")
            .id("result")
            .segment("Result")
                .with_ty(self.1)
                .with_ty(ty)
                .build()
            .build();

        self.0.build_path(path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyPhantomDataBuilder<F>(TyBuilder<F>);

impl<F> Invoke<P<ast::Ty>> for TyPhantomDataBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        let path = PathBuilder::new()
            .span(self.0.span)
            .global()
            .id("std")
            .id("marker")
            .segment("PhantomData")
                .with_ty(ty)
                .build()
            .build();

        self.0.build_path(path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyBoxBuilder<F>(TyBuilder<F>);

impl<F> Invoke<P<ast::Ty>> for TyBoxBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        let path = PathBuilder::new()
            .span(self.0.span)
            .global()
            .id("std")
            .id("boxed")
            .segment("Box")
                .with_ty(ty)
                .build()
            .build();

        self.0.build_path(path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyIteratorBuilder<F>(TyBuilder<F>);

impl<F> Invoke<P<ast::Ty>> for TyIteratorBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        let path = PathBuilder::new()
            .span(self.0.span)
            .global()
            .id("std")
            .id("iter")
            .segment("Iterator")
                .binding("Item").build(ty.clone())
                .build()
            .build();

        self.0.build_path(path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyImplTraitTyBuilder<F> {
    builder: TyBuilder<F>,
    bounds: Vec<ast::TyParamBound>,
}

impl<F> TyImplTraitTyBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    pub fn with_bounds<I>(mut self, iter: I) -> Self
        where I: Iterator<Item=ast::TyParamBound>,
    {
        self.bounds.extend(iter);
        self
    }

    pub fn with_bound(mut self, bound: ast::TyParamBound) -> Self {
        self.bounds.push(bound);
        self
    }

    pub fn bound(self) -> TyParamBoundBuilder<Self> {
        TyParamBoundBuilder::with_callback(self)
    }

    pub fn with_generics(self, generics: ast::Generics) -> Self {
        self.with_lifetimes(
            generics.lifetimes.into_iter()
                .map(|def| def.lifetime)
        )
    }

    pub fn with_lifetimes<I, L>(mut self, lifetimes: I) -> Self
        where I: Iterator<Item=L>,
              L: IntoLifetime,
    {
        for lifetime in lifetimes {
            self = self.lifetime(lifetime);
        }

        self
    }

    pub fn lifetime<L>(self, lifetime: L) -> Self
        where L: IntoLifetime,
    {
        self.bound().lifetime(lifetime)
    }

    pub fn build(self) -> F::Result {
        self.builder.build_ty_kind(ast::TyKind::ImplTrait(self.bounds))
    }
}

impl<F> Invoke<ast::TyParamBound> for TyImplTraitTyBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = Self;

    fn invoke(self, bound: ast::TyParamBound) -> Self {
        self.with_bound(bound)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct TyTupleBuilder<F> {
    builder: TyBuilder<F>,
    tys: Vec<P<ast::Ty>>,
}

impl<F> TyTupleBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
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
        let span = self.builder.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.builder.build_ty_kind(ast::TyKind::Tup(self.tys))
    }
}

impl<F> Invoke<P<ast::Ty>> for TyTupleBuilder<F>
    where F: Invoke<P<ast::Ty>>,
{
    type Result = Self;

    fn invoke(self, ty: P<ast::Ty>) -> Self {
        self.with_ty(ty)
    }
}
