use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;
use syntax::util::ThinVec;

use attr::AttrBuilder;
use expr::ExprBuilder;
use ident::ToIdent;
use invoke::{Invoke, Identity};
use item::ItemBuilder;
use mac::MacBuilder;
use pat::PatBuilder;
use ty::TyBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct StmtBuilder<F=Identity> {
    callback: F,
    span: Span,
}

impl StmtBuilder {
    pub fn new() -> StmtBuilder {
        StmtBuilder::with_callback(Identity)
    }
}

impl<F> StmtBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    pub fn with_callback(callback: F) -> Self {
        StmtBuilder {
            callback: callback,
            span: DUMMY_SP,
        }
    }

    pub fn build(self, stmt: ast::Stmt) -> F::Result {
        self.callback.invoke(stmt)
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn build_stmt_kind(self, stmt_: ast::StmtKind) -> F::Result {
        let stmt = ast::Stmt {
            id: ast::DUMMY_NODE_ID,
            node: stmt_,
            span: self.span,
        };
        self.build(stmt)
    }

    pub fn build_let(self,
                     pat: P<ast::Pat>,
                     ty: Option<P<ast::Ty>>,
                     init: Option<P<ast::Expr>>,
                     attrs: Vec<ast::Attribute>) -> F::Result {
        let local = ast::Local {
            pat: pat,
            ty: ty,
            init: init,
            id: ast::DUMMY_NODE_ID,
            span: self.span,
            attrs: ThinVec::from(attrs),
        };

        self.build_stmt_kind(ast::StmtKind::Local(P(local)))
    }

    pub fn let_(self) -> PatBuilder<Self> {
        let span = self.span;
        PatBuilder::with_callback(self).span(span)
    }

    pub fn let_id<I>(self, id: I) -> StmtLetBuilder<F>
        where I: ToIdent,
    {
        self.let_().id(id)
    }

    pub fn build_expr(self, expr: P<ast::Expr>) -> F::Result {
        self.build_stmt_kind(ast::StmtKind::Expr(expr))
    }

    pub fn expr(self) -> ExprBuilder<StmtExprBuilder<F>> {
        let span = self.span;
        ExprBuilder::with_callback(StmtExprBuilder(self)).span(span)
    }

    pub fn semi(self) -> ExprBuilder<StmtSemiBuilder<F>> {
        let span = self.span;
        ExprBuilder::with_callback(StmtSemiBuilder(self)).span(span)
    }

    pub fn build_item(self, item: P<ast::Item>) -> F::Result {
        self.build_stmt_kind(ast::StmtKind::Item(item))
    }

    pub fn item(self) -> ItemBuilder<StmtItemBuilder<F>> {
        let span = self.span;
        ItemBuilder::with_callback(StmtItemBuilder(self)).span(span)
    }

    pub fn build_mac(self,
                     mac: ast::Mac,
                     style: ast::MacStmtStyle,
                     attrs: Vec<ast::Attribute>) -> F::Result {
        let attrs = ThinVec::from(attrs);
        self.build_stmt_kind(ast::StmtKind::Mac(P((mac, style, attrs))))
    }

    pub fn mac(self) -> StmtMacBuilder<F> {
        StmtMacBuilder {
            builder: self,
            attrs: vec![],
        }
    }
}

impl<F> Invoke<P<ast::Pat>> for StmtBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = StmtLetBuilder<F>;

    fn invoke(self, pat: P<ast::Pat>) -> StmtLetBuilder<F> {
        StmtLetBuilder {
            builder: self,
            attrs: vec![],
            pat: pat,
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct StmtMacBuilder<F> {
    builder: StmtBuilder<F>,
    attrs: Vec<ast::Attribute>,
}

impl<F> StmtMacBuilder<F>
    where F: Invoke<ast::Stmt>,
{
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

    pub fn style(self, style: ast::MacStmtStyle) -> MacBuilder<StmtMacStyleBuilder<F>> {
        let span = self.builder.span;
        MacBuilder::with_callback(StmtMacStyleBuilder {
            builder: self.builder,
            style: style,
            attrs: self.attrs,
        }).span(span)
    }
}

impl<F> Invoke<ast::Attribute> for StmtMacBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = Self;

    fn invoke(self, attr: ast::Attribute) -> Self {
        self.with_attr(attr)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct StmtMacStyleBuilder<F> {
    builder: StmtBuilder<F>,
    style: ast::MacStmtStyle,
    attrs: Vec<ast::Attribute>,
}

impl<F> Invoke<ast::Mac> for StmtMacStyleBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = F::Result;

    fn invoke(self, mac: ast::Mac) -> F::Result {
        self.builder.build_mac(mac, self.style, self.attrs)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct StmtExprBuilder<F>(StmtBuilder<F>);

impl<F> Invoke<P<ast::Expr>> for StmtExprBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.0.build_expr(expr)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct StmtSemiBuilder<F>(StmtBuilder<F>);

impl<F> Invoke<P<ast::Expr>> for StmtSemiBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.0.build_stmt_kind(ast::StmtKind::Semi(expr))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct StmtLetBuilder<F> {
    builder: StmtBuilder<F>,
    attrs: Vec<ast::Attribute>,
    pat: P<ast::Pat>,
}

impl<F> StmtLetBuilder<F>
    where F: Invoke<ast::Stmt>,
{
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

    pub fn build_option_ty(self, ty: Option<P<ast::Ty>>) -> StmtLetTyBuilder<F> {
        StmtLetTyBuilder {
            builder: self.builder,
            attrs: self.attrs,
            pat: self.pat,
            ty: ty,
        }
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.builder.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn build_expr(self, init: P<ast::Expr>) -> F::Result {
        self.builder.build_let(self.pat, None, Some(init), self.attrs)
    }

    pub fn expr(self) -> ExprBuilder<Self> {
        ExprBuilder::with_callback(self)
    }

    pub fn build(self) -> F::Result {
        self.builder.build_let(self.pat, None, None, self.attrs)
    }
}

impl<F> Invoke<ast::Attribute> for StmtLetBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = Self;

    fn invoke(self, attr: ast::Attribute) -> Self {
        self.with_attr(attr)
    }
}

impl<F> Invoke<P<ast::Ty>> for StmtLetBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = StmtLetTyBuilder<F>;

    fn invoke(self, ty: P<ast::Ty>) -> StmtLetTyBuilder<F> {
        self.build_option_ty(Some(ty))
    }
}

impl<F> Invoke<P<ast::Expr>> for StmtLetBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.build_expr(expr)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct StmtLetTyBuilder<F> {
    builder: StmtBuilder<F>,
    attrs: Vec<ast::Attribute>,
    pat: P<ast::Pat>,
    ty: Option<P<ast::Ty>>,
}

impl<F> StmtLetTyBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    pub fn expr(self) -> ExprBuilder<Self> {
        ExprBuilder::with_callback(self)
    }

    pub fn build_option_expr(self, expr: Option<P<ast::Expr>>) -> F::Result {
        self.builder.build_let(self.pat, self.ty, expr, self.attrs)
    }

    pub fn build(self) -> F::Result {
        self.build_option_expr(None)
    }
}

impl<F> Invoke<P<ast::Expr>> for StmtLetTyBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = F::Result;

    fn invoke(self, init: P<ast::Expr>) -> F::Result {
        self.build_option_expr(Some(init))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct StmtItemBuilder<F>(StmtBuilder<F>);

impl<F> Invoke<P<ast::Item>> for StmtItemBuilder<F>
    where F: Invoke<ast::Stmt>,
{
    type Result = F::Result;

    fn invoke(self, item: P<ast::Item>) -> F::Result {
        self.0.build_item(item)
    }
}
