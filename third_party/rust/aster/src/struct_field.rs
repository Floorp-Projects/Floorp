use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use attr::AttrBuilder;
use ident::ToIdent;
use invoke::{Invoke, Identity};
use ty::TyBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct StructFieldBuilder<F=Identity> {
    callback: F,
    span: Span,
    ident: Option<ast::Ident>,
    vis: ast::Visibility,
    attrs: Vec<ast::Attribute>,
}

impl StructFieldBuilder {
    pub fn named<T>(name: T) -> Self
        where T: ToIdent,
    {
        StructFieldBuilder::named_with_callback(name, Identity)
    }

    pub fn unnamed() -> Self {
        StructFieldBuilder::unnamed_with_callback(Identity)
    }
}

impl<F> StructFieldBuilder<F>
    where F: Invoke<ast::StructField>,
{
    pub fn named_with_callback<T>(id: T, callback: F) -> Self
        where T: ToIdent,
    {
        let id = id.to_ident();
        StructFieldBuilder {
            callback: callback,
            span: DUMMY_SP,
            ident: Some(id),
            vis: ast::Visibility::Inherited,
            attrs: vec![],
        }
    }

    pub fn unnamed_with_callback(callback: F) -> Self {
        StructFieldBuilder {
            callback: callback,
            span: DUMMY_SP,
            ident: None,
            vis: ast::Visibility::Inherited,
            attrs: vec![],
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn pub_(mut self) -> Self {
        self.vis = ast::Visibility::Public;
        self
    }

    pub fn with_attrs<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Attribute>,
    {
        self.attrs.extend(iter);
        self
    }

    pub fn attr(self) -> AttrBuilder<Self> {
        let span = self.span;
        AttrBuilder::with_callback(self).span(span)
    }

    pub fn build_ty(self, ty: P<ast::Ty>) -> F::Result {
        let field = ast::StructField {
            id: ast::DUMMY_NODE_ID,
            span: self.span,
            ident: self.ident,
            vis: self.vis,
            ty: ty,
            attrs: self.attrs,
        };
        self.callback.invoke(field)
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }
}

impl<F> Invoke<ast::Attribute> for StructFieldBuilder<F> {
    type Result = Self;

    fn invoke(mut self, attr: ast::Attribute) -> Self {
        self.attrs.push(attr);
        self
    }
}

impl<F> Invoke<P<ast::Ty>> for StructFieldBuilder<F>
    where F: Invoke<ast::StructField>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.build_ty(ty)
    }
}
