#![cfg_attr(feature = "unstable", allow(should_implement_trait))]

use std::iter::IntoIterator;

use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use attr::AttrBuilder;
use expr::ExprBuilder;
use invoke::{Invoke, Identity};
use pat::PatBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct ArmBuilder<F=Identity> {
    callback: F,
    span: Span,
    attrs: Vec<ast::Attribute>,
}

impl ArmBuilder {
    pub fn new() -> Self {
        ArmBuilder::with_callback(Identity)
    }
}

impl<F> ArmBuilder<F>
    where F: Invoke<ast::Arm>,
{
    pub fn with_callback(callback: F) -> Self {
        ArmBuilder {
            callback: callback,
            span: DUMMY_SP,
            attrs: Vec::new(),
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_attrs<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Attribute>,
    {
        self.attrs.extend(iter);
        self
    }

    pub fn with_attr(mut self, attr: ast::Attribute) -> Self {
        self.attrs.push(attr);
        self
    }

    pub fn attr(self) -> AttrBuilder<Self> {
        AttrBuilder::with_callback(self)
    }

    pub fn with_pats<I>(self, iter: I) -> ArmPatBuilder<F>
        where I: IntoIterator<Item=P<ast::Pat>>,
    {
        ArmPatBuilder {
            callback: self.callback,
            span: self.span,
            attrs: self.attrs,
            pats: iter.into_iter().collect(),
        }
    }

    pub fn with_pat(self, pat: P<ast::Pat>) -> ArmPatBuilder<F> {
        ArmPatBuilder {
            callback: self.callback,
            span: self.span,
            attrs: self.attrs,
            pats: vec![pat],
        }
    }

    pub fn pat(self) -> PatBuilder<Self> {
        PatBuilder::with_callback(self)
    }

    /*
    pub fn with_guard(self, guard: Option<P<ast::Expr>>) -> ExprBuilder<ArmBodyBuilder<F>> {
        ExprBuilder::with_callback(ArmBodyBuilder {
            builder: self,
            guard: guard,
        })
    }

    pub fn guard(self) -> ExprBuilder<Self> {
        ExprBuilder::with_callback(self)
    }

    pub fn body(self) -> ExprBuilder<ArmBodyBuilder<F>> {
        self.with_guard(None)
    }

    pub fn build_arm_(self,
                      guard: Option<P<ast::Expr>>,
                      body: P<ast::Expr>) -> F::Result {
        self.callback.invoke(ast::Arm {
            attrs: self.attrs,
            pats: self.pats,
            guard: guard,
            body: body,
        })
    }
    */
}

impl<F> Invoke<ast::Attribute> for ArmBuilder<F>
    where F: Invoke<ast::Arm>,
{
    type Result = Self;

    fn invoke(self, attr: ast::Attribute) -> Self {
        self.with_attr(attr)
    }
}

impl<F> Invoke<P<ast::Pat>> for ArmBuilder<F>
    where F: Invoke<ast::Arm>,
{
    type Result = ArmPatBuilder<F>;

    fn invoke(self, pat: P<ast::Pat>) -> ArmPatBuilder<F> {
        self.with_pat(pat)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ArmPatBuilder<F> {
    callback: F,
    span: Span,
    attrs: Vec<ast::Attribute>,
    pats: Vec<P<ast::Pat>>,
}

impl<F> ArmPatBuilder<F>
    where F: Invoke<ast::Arm>,
{
    pub fn with_pats<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=P<ast::Pat>>,
    {
        self.pats.extend(iter);
        self
    }

    pub fn with_pat(mut self, pat: P<ast::Pat>) -> Self {
        self.pats.push(pat);
        self
    }

    pub fn pat(self) -> PatBuilder<Self> {
        let span = self.span;
        PatBuilder::with_callback(self).span(span)
    }

    pub fn with_guard(self, guard: Option<P<ast::Expr>>) -> ArmBodyBuilder<F> {
        ArmBodyBuilder {
            builder: self,
            guard: guard,
        }
    }

    pub fn guard(self) -> ExprBuilder<Self> {
        let span = self.span;
        ExprBuilder::with_callback(self).span(span)
    }

    pub fn body(self) -> ExprBuilder<ArmBodyBuilder<F>> {
        ArmBodyBuilder {
            builder: self,
            guard: None,
        }.body()
    }

    pub fn build_arm_(self,
                      guard: Option<P<ast::Expr>>,
                      body: P<ast::Expr>) -> F::Result {
        self.callback.invoke(ast::Arm {
            attrs: self.attrs,
            pats: self.pats,
            guard: guard,
            body: body,
        })
    }
}

impl<F> Invoke<P<ast::Pat>> for ArmPatBuilder<F>
    where F: Invoke<ast::Arm>,
{
    type Result = Self;

    fn invoke(self, pat: P<ast::Pat>) -> Self {
        self.with_pat(pat)
    }
}

impl<F> Invoke<P<ast::Expr>> for ArmPatBuilder<F>
    where F: Invoke<ast::Arm>,
{
    type Result = ArmBodyBuilder<F>;

    fn invoke(self, guard: P<ast::Expr>) -> ArmBodyBuilder<F> {
        self.with_guard(Some(guard))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ArmBodyBuilder<F> {
    builder: ArmPatBuilder<F>,
    guard: Option<P<ast::Expr>>,
}

impl<F> ArmBodyBuilder<F>
    where F: Invoke<ast::Arm>,
{
    pub fn body(self) -> ExprBuilder<ArmBodyBuilder<F>> {
        let span = self.builder.span;
        ExprBuilder::with_callback(self).span(span)
    }

    pub fn build(self, body: P<ast::Expr>) -> F::Result {
        self.builder.build_arm_(self.guard, body)
    }
}

impl<F> Invoke<P<ast::Expr>> for ArmBodyBuilder<F>
    where F: Invoke<ast::Arm>,
{
    type Result = F::Result;

    fn invoke(self, body: P<ast::Expr>) -> F::Result {
        self.build(body)
    }
}
