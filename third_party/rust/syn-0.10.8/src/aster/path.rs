use {AngleBracketedParameterData, Generics, Ident, Lifetime, ParenthesizedParameterData, Path,
     PathParameters, PathSegment, Ty, TypeBinding};
use aster::ident::ToIdent;
use aster::invoke::{Invoke, Identity};
use aster::lifetime::IntoLifetime;
use aster::ty::TyBuilder;

// ////////////////////////////////////////////////////////////////////////////

pub trait IntoPath {
    fn into_path(self) -> Path;
}

impl IntoPath for Path {
    fn into_path(self) -> Path {
        self
    }
}

impl IntoPath for Ident {
    fn into_path(self) -> Path {
        PathBuilder::new().id(self).build()
    }
}

impl<'a> IntoPath for &'a str {
    fn into_path(self) -> Path {
        PathBuilder::new().id(self).build()
    }
}

impl IntoPath for String {
    fn into_path(self) -> Path {
        (&*self).into_path()
    }
}

impl<'a, T> IntoPath for &'a [T]
    where T: ToIdent
{
    fn into_path(self) -> Path {
        PathBuilder::new().ids(self).build()
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct PathBuilder<F = Identity> {
    callback: F,
    global: bool,
}

impl PathBuilder {
    pub fn new() -> Self {
        PathBuilder::with_callback(Identity)
    }
}

impl<F> PathBuilder<F>
    where F: Invoke<Path>
{
    pub fn with_callback(callback: F) -> Self {
        PathBuilder {
            callback: callback,
            global: false,
        }
    }

    pub fn build(self, path: Path) -> F::Result {
        self.callback.invoke(path)
    }

    pub fn global(mut self) -> Self {
        self.global = true;
        self
    }

    pub fn ids<I, T>(self, ids: I) -> PathSegmentsBuilder<F>
        where I: IntoIterator<Item = T>,
              T: ToIdent
    {
        let mut ids = ids.into_iter();
        let id = ids.next().expect("passed path with no id");

        self.id(id).ids(ids)
    }

    pub fn id<I>(self, id: I) -> PathSegmentsBuilder<F>
        where I: ToIdent
    {
        self.segment(id).build()
    }

    pub fn segment<I>(self, id: I) -> PathSegmentBuilder<PathSegmentsBuilder<F>>
        where I: ToIdent
    {
        PathSegmentBuilder::with_callback(id,
                                          PathSegmentsBuilder {
                                              callback: self.callback,
                                              global: self.global,
                                              segments: Vec::new(),
                                          })
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct PathSegmentsBuilder<F = Identity> {
    callback: F,
    global: bool,
    segments: Vec<PathSegment>,
}

impl<F> PathSegmentsBuilder<F>
    where F: Invoke<Path>
{
    pub fn ids<I, T>(mut self, ids: I) -> PathSegmentsBuilder<F>
        where I: IntoIterator<Item = T>,
              T: ToIdent
    {
        for id in ids {
            self = self.id(id);
        }

        self
    }

    pub fn id<T>(self, id: T) -> PathSegmentsBuilder<F>
        where T: ToIdent
    {
        self.segment(id).build()
    }

    pub fn segment<T>(self, id: T) -> PathSegmentBuilder<Self>
        where T: ToIdent
    {
        PathSegmentBuilder::with_callback(id, self)
    }

    pub fn build(self) -> F::Result {
        self.callback.invoke(Path {
            global: self.global,
            segments: self.segments,
        })
    }
}

impl<F> Invoke<PathSegment> for PathSegmentsBuilder<F> {
    type Result = Self;

    fn invoke(mut self, segment: PathSegment) -> Self {
        self.segments.push(segment);
        self
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct PathSegmentBuilder<F = Identity> {
    callback: F,
    id: Ident,
    lifetimes: Vec<Lifetime>,
    tys: Vec<Ty>,
    bindings: Vec<TypeBinding>,
}

impl<F> PathSegmentBuilder<F>
    where F: Invoke<PathSegment>
{
    pub fn with_callback<I>(id: I, callback: F) -> Self
        where I: ToIdent
    {
        PathSegmentBuilder {
            callback: callback,
            id: id.to_ident(),
            lifetimes: Vec::new(),
            tys: Vec::new(),
            bindings: Vec::new(),
        }
    }

    pub fn with_generics(self, generics: Generics) -> Self {
        // Strip off the bounds.
        let lifetimes = generics.lifetimes
            .iter()
            .map(|lifetime_def| lifetime_def.lifetime.clone());

        let tys = generics.ty_params
            .iter()
            .map(|ty_param| TyBuilder::new().id(ty_param.ident.clone()));

        self.with_lifetimes(lifetimes)
            .with_tys(tys)
    }

    pub fn with_lifetimes<I, L>(mut self, iter: I) -> Self
        where I: IntoIterator<Item = L>,
              L: IntoLifetime
    {
        let iter = iter.into_iter().map(|lifetime| lifetime.into_lifetime());
        self.lifetimes.extend(iter);
        self
    }

    pub fn with_lifetime<L>(mut self, lifetime: L) -> Self
        where L: IntoLifetime
    {
        self.lifetimes.push(lifetime.into_lifetime());
        self
    }

    pub fn lifetime<N>(self, name: N) -> Self
        where N: ToIdent
    {
        let lifetime = Lifetime { ident: name.to_ident() };
        self.with_lifetime(lifetime)
    }

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

    pub fn with_binding(mut self, binding: TypeBinding) -> Self {
        self.bindings.push(binding);
        self
    }

    pub fn binding<T>(self, id: T) -> TyBuilder<TypeBindingBuilder<F>>
        where T: ToIdent
    {
        TyBuilder::with_callback(TypeBindingBuilder {
            id: id.to_ident(),
            builder: self,
        })
    }

    pub fn no_return(self) -> F::Result {
        self.build_return(None)
    }

    pub fn return_(self) -> TyBuilder<PathSegmentReturnBuilder<F>> {
        TyBuilder::with_callback(PathSegmentReturnBuilder(self))
    }

    pub fn build_return(self, output: Option<Ty>) -> F::Result {
        let data = ParenthesizedParameterData {
            inputs: self.tys,
            output: output,
        };

        let parameters = PathParameters::Parenthesized(data);

        self.callback.invoke(PathSegment {
            ident: self.id,
            parameters: parameters,
        })
    }

    pub fn build(self) -> F::Result {
        let data = AngleBracketedParameterData {
            lifetimes: self.lifetimes,
            types: self.tys,
            bindings: self.bindings,
        };

        let parameters = PathParameters::AngleBracketed(data);

        self.callback.invoke(PathSegment {
            ident: self.id,
            parameters: parameters,
        })
    }
}

impl<F> Invoke<Ty> for PathSegmentBuilder<F>
    where F: Invoke<PathSegment>
{
    type Result = Self;

    fn invoke(self, ty: Ty) -> Self {
        self.with_ty(ty)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct TypeBindingBuilder<F> {
    id: Ident,
    builder: PathSegmentBuilder<F>,
}

impl<F> Invoke<Ty> for TypeBindingBuilder<F>
    where F: Invoke<PathSegment>
{
    type Result = PathSegmentBuilder<F>;

    fn invoke(self, ty: Ty) -> Self::Result {
        let id = self.id;

        self.builder.with_binding(TypeBinding {
            ident: id,
            ty: ty,
        })
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct PathSegmentReturnBuilder<F>(PathSegmentBuilder<F>);

impl<F> Invoke<Ty> for PathSegmentReturnBuilder<F>
    where F: Invoke<PathSegment>
{
    type Result = F::Result;

    fn invoke(self, ty: Ty) -> Self::Result {
        self.0.build_return(Some(ty))
    }
}
