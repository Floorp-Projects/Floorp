use std::iter::IntoIterator;

use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span, Spanned, respan};
use syntax::ptr::P;

use invoke::{Invoke, Identity};

use expr::ExprBuilder;
use ident::ToIdent;
use path::PathBuilder;
use qpath::QPathBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct PatBuilder<F=Identity> {
    callback: F,
    span: Span,
}

impl PatBuilder {
    pub fn new() -> Self {
        PatBuilder::with_callback(Identity)
    }
}

impl<F> PatBuilder<F>
    where F: Invoke<P<ast::Pat>>,
{
    pub fn with_callback(callback: F) -> Self {
        PatBuilder {
            callback: callback,
            span: DUMMY_SP,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn build(self, pat: P<ast::Pat>) -> F::Result {
        self.callback.invoke(pat)
    }

    pub fn build_pat_kind(self, pat_kind: ast::PatKind) -> F::Result {
        let span = self.span;
        self.build(P(ast::Pat {
            id: ast::DUMMY_NODE_ID,
            node: pat_kind,
            span: span,
        }))
    }

    pub fn wild(self) -> F::Result {
        self.build_pat_kind(ast::PatKind::Wild)
    }

    pub fn build_id<I>(self, mode: ast::BindingMode, id: I, sub: Option<P<ast::Pat>>) -> F::Result
        where I: ToIdent,
    {
        let id = respan(self.span, id.to_ident());

        self.build_pat_kind(ast::PatKind::Ident(mode, id, sub))
    }

    pub fn id<I>(self, id: I) -> F::Result
        where I: ToIdent
    {
        let mode = ast::BindingMode::ByValue(ast::Mutability::Immutable);
        self.build_id(mode, id, None)
    }

    pub fn mut_id<I>(self, id: I) -> F::Result
        where I: ToIdent
    {
        let mode = ast::BindingMode::ByValue(ast::Mutability::Mutable);
        self.build_id(mode, id, None)
    }

    pub fn ref_id<I>(self, id: I) -> F::Result
        where I: ToIdent
    {
        let mode = ast::BindingMode::ByRef(ast::Mutability::Immutable);
        self.build_id(mode, id, None)
    }

    pub fn ref_mut_id<I>(self, id: I) -> F::Result
        where I: ToIdent
    {
        let mode = ast::BindingMode::ByRef(ast::Mutability::Mutable);
        self.build_id(mode, id, None)
    }

    pub fn enum_(self) -> PathBuilder<PatEnumBuilder<F>> {
        PathBuilder::with_callback(PatEnumBuilder(self))
    }

    pub fn struct_(self) -> PathBuilder<PatStructBuilder<F>> {
        PathBuilder::with_callback(PatStructBuilder(self))
    }

    pub fn expr(self) -> ExprBuilder<PatExprBuilder<F>> {
        ExprBuilder::with_callback(PatExprBuilder(self))
    }

    pub fn build_path(self, path: ast::Path) -> F::Result {
        self.build_pat_kind(ast::PatKind::Path(None, path))
    }

    pub fn build_qpath(self, qself: ast::QSelf, path: ast::Path) -> F::Result {
        self.build_pat_kind(ast::PatKind::Path(Some(qself), path))
    }

    pub fn path(self) -> PathBuilder<Self> {
        PathBuilder::with_callback(self)
    }

    pub fn qpath(self) -> QPathBuilder<Self> {
        QPathBuilder::with_callback(self)
    }

    pub fn build_range(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_pat_kind(ast::PatKind::Range(lhs, rhs, ast::RangeEnd::Included))
    }

    pub fn range(self) -> ExprBuilder<PatRangeBuilder<F>> {
        ExprBuilder::with_callback(PatRangeBuilder(self))
    }

    pub fn tuple(self) -> PatTupleBuilder<F> {
        PatTupleBuilder {
            builder: self,
            pats: Vec::new(),
            wild: None,
        }
    }

    pub fn ref_(self) -> PatBuilder<PatRefBuilder<F>> {
        PatBuilder::with_callback(PatRefBuilder {
            builder: self,
            mutability: ast::Mutability::Immutable,
        })
    }

    pub fn ref_mut(self) -> PatBuilder<PatRefBuilder<F>> {
        PatBuilder::with_callback(PatRefBuilder {
            builder: self,
            mutability: ast::Mutability::Mutable,
        })
    }

    pub fn some(self) -> PatBuilder<PatEnumPathPatBuilder<F>> {
        let path = PathBuilder::new().span(self.span)
            .global()
            .ids(&["std", "option", "Option", "Some"])
            .build();

        let span = self.span;
        PatBuilder::with_callback(PatEnumPathPatBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    pub fn none(self) -> F::Result {
        let path = PathBuilder::new().span(self.span)
            .global()
            .ids(&["std", "option", "Option", "None"])
            .build();

        self.enum_()
            .build(path)
            .build()
    }

    pub fn ok(self) -> PatBuilder<PatEnumPathPatBuilder<F>> {
        let path = PathBuilder::new().span(self.span)
            .global()
            .ids(&["std", "result", "Result", "Ok"])
            .build();

        let span = self.span;
        PatBuilder::with_callback(PatEnumPathPatBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    pub fn err(self) -> PatBuilder<PatEnumPathPatBuilder<F>> {
        let path = PathBuilder::new().span(self.span)
            .global()
            .ids(&["std", "result", "Result", "Err"])
            .build();

        let span = self.span;
        PatBuilder::with_callback(PatEnumPathPatBuilder {
            builder: self,
            path: path,
        }).span(span)
    }
}

impl<F> Invoke<ast::Path> for PatBuilder<F>
    where F: Invoke<P<ast::Pat>>,
{
    type Result = F::Result;

    fn invoke(self, path: ast::Path) -> F::Result {
        self.build_path(path)
    }
}

impl<F> Invoke<(ast::QSelf, ast::Path)> for PatBuilder<F>
    where F: Invoke<P<ast::Pat>>,
{
    type Result = F::Result;

    fn invoke(self, (qself, path): (ast::QSelf, ast::Path)) -> F::Result {
        self.build_qpath(qself, path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatRangeBuilder<F>(PatBuilder<F>);

impl<F> Invoke<P<ast::Expr>> for PatRangeBuilder<F>
    where F: Invoke<P<ast::Pat>>
{
    type Result = ExprBuilder<PatRangeExprBuilder<F>>;

    fn invoke(self, lhs: P<ast::Expr>) -> Self::Result {
        ExprBuilder::with_callback(PatRangeExprBuilder {
            builder: self.0,
            lhs: lhs,
        })
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatRangeExprBuilder<F> {
    builder: PatBuilder<F>,
    lhs: P<ast::Expr>,
}

impl<F> Invoke<P<ast::Expr>> for PatRangeExprBuilder<F>
    where F: Invoke<P<ast::Pat>>
{
    type Result = F::Result;

    fn invoke(self, rhs: P<ast::Expr>) -> Self::Result {
        self.builder.build_range(self.lhs, rhs)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatEnumBuilder<F>(PatBuilder<F>);

impl<F> Invoke<ast::Path> for PatEnumBuilder<F> {
    type Result = PatEnumPathBuilder<F>;

    fn invoke(self, path: ast::Path) -> PatEnumPathBuilder<F> {
        PatEnumPathBuilder {
            builder: self.0,
            path: path,
            pats: Vec::new(),
            wild: None,
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatEnumPathBuilder<F> {
    builder: PatBuilder<F>,
    path: ast::Path,
    pats: Vec<P<ast::Pat>>,
    wild: Option<usize>,
}

impl<F> PatEnumPathBuilder<F>
    where F: Invoke<P<ast::Pat>>,
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
        PatBuilder::with_callback(self)
    }

    pub fn with_ids<I, T>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=T>,
              T: ToIdent,
    {
        for id in iter {
            self = self.id(id);
        }
        self
    }

    pub fn id<I>(self, id: I) -> Self
        where I: ToIdent
    {
        self.pat().id(id)
    }

    pub fn wild(mut self) -> Self {
        self.wild = Some(self.pats.len());
        self
    }

    pub fn build(self) -> F::Result {
        self.builder.build_pat_kind(ast::PatKind::TupleStruct(
            self.path,
            self.pats,
            self.wild))
    }
}

impl<F> Invoke<P<ast::Pat>> for PatEnumPathBuilder<F>
    where F: Invoke<P<ast::Pat>>,
{
    type Result = Self;

    fn invoke(mut self, pat: P<ast::Pat>) -> Self {
        self.pats.push(pat);
        self
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatEnumPathPatBuilder<F> {
    builder: PatBuilder<F>,
    path: ast::Path,
}

impl<F> Invoke<P<ast::Pat>> for PatEnumPathPatBuilder<F>
    where F: Invoke<P<ast::Pat>>
{
    type Result = F::Result;

    fn invoke(self, pat: P<ast::Pat>) -> F::Result {
        self.builder.enum_()
            .build(self.path)
            .with_pat(pat)
            .build()
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatStructBuilder<F>(PatBuilder<F>);

impl<F> Invoke<ast::Path> for PatStructBuilder<F> {
    type Result = PatStructPathBuilder<F>;

    fn invoke(self, path: ast::Path) -> PatStructPathBuilder<F> {
        PatStructPathBuilder {
            builder: self.0,
            path: path,
            pats: Vec::new(),
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatStructPathBuilder<F> {
    builder: PatBuilder<F>,
    path: ast::Path,
    pats: Vec<Spanned<ast::FieldPat>>,
}

impl<F> PatStructPathBuilder<F>
    where F: Invoke<P<ast::Pat>>,
{
    pub fn with_field_pat(mut self, pat: ast::FieldPat) -> Self {
        self.pats.push(respan(self.builder.span, pat));
        self
    }

    pub fn with_pats<I, T>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=(T, P<ast::Pat>)>,
              T: ToIdent,
    {
        for (id, pat) in iter {
            self = self.pat(id).build(pat);
        }
        self
    }

    pub fn pat<I>(self, id: I) -> PatBuilder<PatStructFieldBuilder<F>>
        where I: ToIdent,
    {
        PatBuilder::with_callback(PatStructFieldBuilder {
            builder: self,
            id: id.to_ident(),
        })
    }

    pub fn with_ids<I, T>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=T>,
              T: ToIdent,
    {
        for id in iter {
            self = self.id(id);
        }
        self
    }

    pub fn mut_id<I>(self, id: I) -> Self
        where I: ToIdent,
    {
        let id = id.to_ident();
        let span = self.builder.span;
        let pat = PatBuilder::new().span(span).mut_id(id);

        self.with_field_pat(ast::FieldPat {
            ident: id,
            pat: pat,
            is_shorthand: true,
            attrs: Vec::new().into(),
        })
    }

    pub fn id<I>(self, id: I) -> Self
        where I: ToIdent,
    {
        let id = id.to_ident();
        let span = self.builder.span;
        let pat = PatBuilder::new().span(span).id(id);

        self.with_field_pat(ast::FieldPat {
            ident: id,
            pat: pat,
            is_shorthand: true,
            attrs: Vec::new().into(),
        })
    }

    pub fn etc(self) -> F::Result {
        self.builder.build_pat_kind(ast::PatKind::Struct(self.path, self.pats, true))
    }

    pub fn build(self) -> F::Result {
        self.builder.build_pat_kind(ast::PatKind::Struct(self.path, self.pats, false))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatStructFieldBuilder<F> {
    builder: PatStructPathBuilder<F>,
    id: ast::Ident,
}

impl<F> Invoke<P<ast::Pat>> for PatStructFieldBuilder<F>
    where F: Invoke<P<ast::Pat>>,
{
    type Result = PatStructPathBuilder<F>;

    fn invoke(self, pat: P<ast::Pat>) -> PatStructPathBuilder<F> {
        self.builder.with_field_pat(ast::FieldPat {
            ident: self.id,
            pat: pat,
            is_shorthand: false,
            attrs: Vec::new().into(),
        })
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatExprBuilder<F>(PatBuilder<F>);

impl<F> Invoke<P<ast::Expr>> for PatExprBuilder<F>
    where F: Invoke<P<ast::Pat>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.0.build_pat_kind(ast::PatKind::Lit(expr))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatTupleBuilder<F> {
    builder: PatBuilder<F>,
    pats: Vec<P<ast::Pat>>,
    wild: Option<usize>,
}

impl<F: Invoke<P<ast::Pat>>> PatTupleBuilder<F>
    where F: Invoke<P<ast::Pat>>
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
        PatBuilder::with_callback(self)
    }

    pub fn wild(mut self) -> Self {
        self.wild = Some(self.pats.len());
        self
    }

    pub fn build(self) -> F::Result {
        self.builder.build_pat_kind(ast::PatKind::Tuple(self.pats, self.wild))
    }
}

impl<F> Invoke<P<ast::Pat>> for PatTupleBuilder<F>
    where F: Invoke<P<ast::Pat>>
{
    type Result = PatTupleBuilder<F>;

    fn invoke(self, pat: P<ast::Pat>) -> Self {
        self.with_pat(pat)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct PatRefBuilder<F> {
    builder: PatBuilder<F>,
    mutability: ast::Mutability,
}

impl<F> Invoke<P<ast::Pat>> for PatRefBuilder<F>
    where F: Invoke<P<ast::Pat>>
{
    type Result = F::Result;

    fn invoke(self, pat: P<ast::Pat>) -> F::Result {
        self.builder.build_pat_kind(ast::PatKind::Ref(pat, self.mutability))
    }
}
