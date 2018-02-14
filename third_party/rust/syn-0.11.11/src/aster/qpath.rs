use {Path, PathSegment, QSelf, Ty};
use aster::ident::ToIdent;
use aster::invoke::{Invoke, Identity};
use aster::path::{PathBuilder, PathSegmentBuilder};
use aster::ty::TyBuilder;

// ////////////////////////////////////////////////////////////////////////////

pub struct QPathBuilder<F = Identity> {
    callback: F,
}

impl QPathBuilder {
    pub fn new() -> Self {
        QPathBuilder::with_callback(Identity)
    }
}

impl<F> QPathBuilder<F>
    where F: Invoke<(QSelf, Path)>
{
    /// Construct a `QPathBuilder` that will call the `callback` with a constructed `QSelf`
    /// and `Path`.
    pub fn with_callback(callback: F) -> Self {
        QPathBuilder { callback: callback }
    }

    /// Build a qualified path first by starting with a type builder.
    pub fn with_ty(self, ty: Ty) -> QPathTyBuilder<F> {
        QPathTyBuilder {
            builder: self,
            ty: ty,
        }
    }

    /// Build a qualified path first by starting with a type builder.
    pub fn ty(self) -> TyBuilder<Self> {
        TyBuilder::with_callback(self)
    }

    /// Build a qualified path with a concrete type and path.
    pub fn build(self, qself: QSelf, path: Path) -> F::Result {
        self.callback.invoke((qself, path))
    }
}

impl<F> Invoke<Ty> for QPathBuilder<F>
    where F: Invoke<(QSelf, Path)>
{
    type Result = QPathTyBuilder<F>;

    fn invoke(self, ty: Ty) -> QPathTyBuilder<F> {
        self.with_ty(ty)
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct QPathTyBuilder<F> {
    builder: QPathBuilder<F>,
    ty: Ty,
}

impl<F> QPathTyBuilder<F>
    where F: Invoke<(QSelf, Path)>
{
    /// Build a qualified path with a path builder.
    pub fn as_(self) -> PathBuilder<Self> {
        PathBuilder::with_callback(self)
    }

    pub fn id<T>(self, id: T) -> F::Result
        where T: ToIdent
    {
        let path = Path {
            global: false,
            segments: vec![],
        };
        self.as_().build(path).id(id)
    }

    pub fn segment<T>(self, id: T) -> PathSegmentBuilder<QPathQSelfBuilder<F>>
        where T: ToIdent
    {
        let path = Path {
            global: false,
            segments: vec![],
        };
        self.as_().build(path).segment(id)
    }
}

impl<F> Invoke<Path> for QPathTyBuilder<F>
    where F: Invoke<(QSelf, Path)>
{
    type Result = QPathQSelfBuilder<F>;

    fn invoke(self, path: Path) -> QPathQSelfBuilder<F> {
        QPathQSelfBuilder {
            builder: self.builder,
            qself: QSelf {
                ty: Box::new(self.ty),
                position: path.segments.len(),
            },
            path: path,
        }
    }
}

// ////////////////////////////////////////////////////////////////////////////

pub struct QPathQSelfBuilder<F> {
    builder: QPathBuilder<F>,
    qself: QSelf,
    path: Path,
}

impl<F> QPathQSelfBuilder<F>
    where F: Invoke<(QSelf, Path)>
{
    pub fn id<T>(self, id: T) -> F::Result
        where T: ToIdent
    {
        self.segment(id).build()
    }

    pub fn segment<T>(self, id: T) -> PathSegmentBuilder<QPathQSelfBuilder<F>>
        where T: ToIdent
    {
        PathSegmentBuilder::with_callback(id, self)
    }
}

impl<F> Invoke<PathSegment> for QPathQSelfBuilder<F>
    where F: Invoke<(QSelf, Path)>
{
    type Result = F::Result;

    fn invoke(mut self, segment: PathSegment) -> F::Result {
        self.path.segments.push(segment);
        self.builder.build(self.qself, self.path)
    }
}
