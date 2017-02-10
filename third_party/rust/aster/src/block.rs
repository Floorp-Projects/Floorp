use std::iter::IntoIterator;

use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use expr::ExprBuilder;
use invoke::{Invoke, Identity};
use stmt::StmtBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct BlockBuilder<F=Identity> {
    callback: F,
    span: Span,
    stmts: Vec<ast::Stmt>,
    block_check_mode: ast::BlockCheckMode,
}

impl BlockBuilder {
    pub fn new() -> Self {
        BlockBuilder::with_callback(Identity)
    }
}

impl<F> BlockBuilder<F>
    where F: Invoke<P<ast::Block>>,
{
    pub fn with_callback(callback: F) -> Self {
        BlockBuilder {
            callback: callback,
            span: DUMMY_SP,
            stmts: Vec::new(),
            block_check_mode: ast::BlockCheckMode::Default,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn unsafe_(mut self) -> Self {
        let source = ast::UnsafeSource::CompilerGenerated;
        self.block_check_mode = ast::BlockCheckMode::Unsafe(source);
        self
    }

    pub fn with_stmts<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Stmt>
    {
        self.stmts.extend(iter);
        self
    }

    pub fn with_stmt(mut self, stmt: ast::Stmt) -> Self {
        self.stmts.push(stmt);
        self
    }

    pub fn stmt(self) -> StmtBuilder<Self> {
        let span = self.span;
        StmtBuilder::with_callback(self).span(span)
    }

    pub fn build_expr(self, expr: P<ast::Expr>) -> F::Result {
        self.build_(Some(expr))
    }

    pub fn expr(self) -> ExprBuilder<Self> {
        let span = self.span;
        ExprBuilder::with_callback(self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.build_(None)
    }

    fn build_(mut self, expr: Option<P<ast::Expr>>) -> F::Result {
        self.stmts.extend(expr.map(|expr| {
            StmtBuilder::new().span(expr.span).build_expr(expr)
        }));
        self.callback.invoke(P(ast::Block {
            stmts: self.stmts,
            id: ast::DUMMY_NODE_ID,
            rules: self.block_check_mode,
            span: self.span,
        }))
    }
}

impl<F> Invoke<ast::Stmt> for BlockBuilder<F>
    where F: Invoke<P<ast::Block>>,
{
    type Result = Self;

    fn invoke(self, stmt: ast::Stmt) -> Self {
        self.with_stmt(stmt)
    }
}

impl<F> Invoke<P<ast::Expr>> for BlockBuilder<F>
    where F: Invoke<P<ast::Block>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.build_expr(expr)
    }
}
