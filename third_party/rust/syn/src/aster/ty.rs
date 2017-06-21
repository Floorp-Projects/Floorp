use {Generics, Lifetime, MutTy, Mutability, Path, QSelf, Ty, TyParamBound};
use aster::ident::ToIdent;
use aster::invoke::{Invoke, Identity};
use aster::lifetime::IntoLifetime;
use aster::path::PathBuilder;
use aster::qpath::QPathBuilder;
use aster::ty_param::TyParamBoundBuilder;

// ////////////////////////////////////////////////////////////////////////////

pub struct TyBuilder<F = Identity> {
    callback: F,
}

impl TyBuilder {
    pub fn new() -> Self {
        TyBuilder::with_callback(Identity)
    }
}

impl<F> TyBuilder<F>
    where F: Invoke<Ty>
{
    pub fn with_callback(callback: F) -> Self {
        TyBuilder { callback: callback }
    }

    pub fn build(self, ty: Ty) -> F::Result {
        self.callback.invoke(ty)
    }

    pub fn id<I>(self, id: I) -> F::Result
        where I: ToIdent
    {
        self.path().id(id).build()
    }

    pub fn build_path(self, path: Path) -> F::Result {
        self.build(Ty::Path(None, path))
    }

    pub fn build_qpath(self, qself: QSelf, path: Path) -> F::Result {
        self.build(Ty::Path(Some(qself), path))
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

    pub fn build_slice(self, ty: Ty) -> F::Result {
        self.build(Ty::Slice(Box::new(ty)))
    }

    pub fn slice(self) -> TyBuilder<TySliceBuilder<F>> {
        TyBuilder::with_callback(TySliceBuilder(self))
    }

    pub fn ref_(self) -> TyRefBuilder<F> {
        TyRefBuilder {
            builder: self,
            lifetime: None,
            mutability: Mutability::Immutable,
        }
    }

    pub fn never(self) -> F::Result {
        self.build(Ty::Never)
    }

    pub fn infer(self) -> F::Result {
        self.build(Ty::Infer)
    }

    pub fn option(self) -> TyBuilder<TyOptionBuilder<F>> {
        TyBuilder::with_callback(TyOptionBuilder(self))
    }

    pub fn result(self) -> TyBuilder<TyResultOkBuilder<F>> {
        TyBuilder::with_callback(TyResultOkBuilder(self))
    }

    pub fn phantom_data(self) -> TyBuilder<TyPhantomDataBuilder<F>> {
        TyBuilder::with_callback(TyPhantomDataBuilder(self))
    }

    pub fn box_(self) -> TyBuilder<TyBoxBuilder<F>> {
        TyBuilder::with_callback(TyBoxBuilder(self))
    }

    pub fn iterator(self) -> TyBuilder<TyIteratorBuilder<F>> {
        TyBuilder::with_callback(TyIteratorBuilder(self))
    }

    pub fn impl_trait(self) -> TyImplTraitTyBuilder<F> {
        TyImplTraitTyBuilder {
            builder: self,
            bounds: Vec::new(),
        }
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TyPathBuilder<F>(TyBuilder<F>);

impl<F> Invoke<Path> for TyPathBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, path: Path) -> F::Result {
        self.0.build_path(path)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TyQPathBuilder<F>(TyBuilder<F>);

impl<F> Invoke<(QSelf, Path)> for TyQPathBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, (qself, path): (QSelf, Path)) -> F::Result {
        self.0.build_qpath(qself, path)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TySliceBuilder<F>(TyBuilder<F>);

impl<F> Invoke<Ty> for TySliceBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, ty: Ty) -> F::Result {
        self.0.build_slice(ty)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TyRefBuilder<F> {
    builder: TyBuilder<F>,
    lifetime: Option<Lifetime>,
    mutability: Mutability,
}

impl<F> TyRefBuilder<F>
    where F: Invoke<Ty>
{
    pub fn mut_(mut self) -> Self {
        self.mutability = Mutability::Mutable;
        self
    }

    pub fn lifetime<N>(mut self, name: N) -> Self
        where N: ToIdent
    {
        self.lifetime = Some(Lifetime { ident: name.to_ident() });
        self
    }

    pub fn build_ty(self, ty: Ty) -> F::Result {
        let ty = MutTy {
            ty: ty,
            mutability: self.mutability,
        };
        self.builder.build(Ty::Rptr(self.lifetime, Box::new(ty)))
    }

    pub fn ty(self) -> TyBuilder<Self> {
        TyBuilder::with_callback(self)
    }
}

impl<F> Invoke<Ty> for TyRefBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, ty: Ty) -> F::Result {
        self.build_ty(ty)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TyOptionBuilder<F>(TyBuilder<F>);

impl<F> Invoke<Ty> for TyOptionBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, ty: Ty) -> F::Result {
        let path = PathBuilder::new()
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

// ////////////////////////////////////////////////////////////////////////////

pub struct TyResultOkBuilder<F>(TyBuilder<F>);

impl<F> Invoke<Ty> for TyResultOkBuilder<F>
    where F: Invoke<Ty>
{
    type Result = TyBuilder<TyResultErrBuilder<F>>;

    fn invoke(self, ty: Ty) -> TyBuilder<TyResultErrBuilder<F>> {
        TyBuilder::with_callback(TyResultErrBuilder(self.0, ty))
    }
}

pub struct TyResultErrBuilder<F>(TyBuilder<F>, Ty);

impl<F> Invoke<Ty> for TyResultErrBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, ty: Ty) -> F::Result {
        let path = PathBuilder::new()
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

// ////////////////////////////////////////////////////////////////////////////

pub struct TyPhantomDataBuilder<F>(TyBuilder<F>);

impl<F> Invoke<Ty> for TyPhantomDataBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, ty: Ty) -> F::Result {
        let path = PathBuilder::new()
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

// ////////////////////////////////////////////////////////////////////////////

pub struct TyBoxBuilder<F>(TyBuilder<F>);

impl<F> Invoke<Ty> for TyBoxBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, ty: Ty) -> F::Result {
        let path = PathBuilder::new()
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

// ////////////////////////////////////////////////////////////////////////////

pub struct TyIteratorBuilder<F>(TyBuilder<F>);

impl<F> Invoke<Ty> for TyIteratorBuilder<F>
    where F: Invoke<Ty>
{
    type Result = F::Result;

    fn invoke(self, ty: Ty) -> F::Result {
        let path = PathBuilder::new()
            .global()
            .id("std")
            .id("iter")
            .segment("Iterator")
            .binding("Item")
            .build(ty.clone())
            .build()
            .build();

        self.0.build_path(path)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TyImplTraitTyBuilder<F> {
    builder: TyBuilder<F>,
    bounds: Vec<TyParamBound>,
}

impl<F> TyImplTraitTyBuilder<F>
    where F: Invoke<Ty>
{
    pub fn with_bounds<I>(mut self, iter: I) -> Self
        where I: Iterator<Item = TyParamBound>
    {
        self.bounds.extend(iter);
        self
    }

    pub fn with_bound(mut self, bound: TyParamBound) -> Self {
        self.bounds.push(bound);
        self
    }

    pub fn bound(self) -> TyParamBoundBuilder<Self> {
        TyParamBoundBuilder::with_callback(self)
    }

    pub fn with_generics(self, generics: Generics) -> Self {
        self.with_lifetimes(generics.lifetimes.into_iter().map(|def| def.lifetime))
    }

    pub fn with_lifetimes<I, L>(mut self, lifetimes: I) -> Self
        where I: Iterator<Item = L>,
              L: IntoLifetime
    {
        for lifetime in lifetimes {
            self = self.lifetime(lifetime);
        }

        self
    }

    pub fn lifetime<L>(self, lifetime: L) -> Self
        where L: IntoLifetime
    {
        self.bound().lifetime(lifetime)
    }

    pub fn build(self) -> F::Result {
        let bounds = self.bounds;
        self.builder.build(Ty::ImplTrait(bounds))
    }
}

impl<F> Invoke<TyParamBound> for TyImplTraitTyBuilder<F>
    where F: Invoke<Ty>
{
    type Result = Self;

    fn invoke(self, bound: TyParamBound) -> Self {
        self.with_bound(bound)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TyTupleBuilder<F> {
    builder: TyBuilder<F>,
    tys: Vec<Ty>,
}

impl<F> TyTupleBuilder<F>
    where F: Invoke<Ty>
{
    pub fn with_tys<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item = Ty>
    {
        self.tys.extend(iter);
        self
    }

    pub fn with_ty(mut self, ty: Ty) -> Self {
        self.tys.push(ty);
        self
    }

    pub fn ty(self) -> TyBuilder<Self> {
        TyBuilder::with_callback(self)
    }

    pub fn build(self) -> F::Result {
        self.builder.build(Ty::Tup(self.tys))
    }
}

impl<F> Invoke<Ty> for TyTupleBuilder<F>
    where F: Invoke<Ty>
{
    type Result = Self;

    fn invoke(self, ty: Ty) -> Self {
        self.with_ty(ty)
    }
}
