use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span, respan};
use syntax::ptr::P;

use invoke::{Invoke, Identity};
use lifetime::IntoLifetime;
use ty::TyBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct SelfBuilder<F=Identity> {
    callback: F,
    span: Span,
    mutable: ast::Mutability,
}

impl SelfBuilder {
    pub fn new() -> Self {
        SelfBuilder::with_callback(Identity)
    }
}

impl<F> SelfBuilder<F>
    where F: Invoke<ast::ExplicitSelf>,
{
    pub fn with_callback(callback: F) -> Self {
        SelfBuilder {
            callback: callback,
            span: DUMMY_SP,
            mutable: ast::Mutability::Immutable,
        }
    }

    pub fn build(self, self_: ast::SelfKind) -> F::Result {
        let self_ = respan(self.span, self_);
        self.callback.invoke(self_)
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn mut_(mut self) -> Self {
        self.mutable = ast::Mutability::Mutable;
        self
    }

    pub fn value(self) -> F::Result {
        let self_ = ast::SelfKind::Value(self.mutable);
        self.build(self_)
    }

    pub fn ref_(self) -> F::Result {
        let self_ = ast::SelfKind::Region(None, self.mutable);
        self.build(self_)
    }

    pub fn ref_lifetime<L>(self, lifetime: L) -> F::Result
        where L: IntoLifetime,
    {
        let self_ = ast::SelfKind::Region(Some(lifetime.into_lifetime()), self.mutable);
        self.build(self_)
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }
}

impl<F> Invoke<P<ast::Ty>> for SelfBuilder<F>
    where F: Invoke<ast::ExplicitSelf>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        let explicit_self = ast::SelfKind::Explicit(ty, self.mutable);
        self.build(explicit_self)
    }
}
