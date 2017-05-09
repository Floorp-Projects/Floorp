#![cfg_attr(feature = "clippy", allow(should_implement_trait))]

use std::iter::IntoIterator;

use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span, Spanned, respan};
use syntax::ptr::P;

use arm::ArmBuilder;
use attr::AttrBuilder;
use block::BlockBuilder;
use fn_decl::FnDeclBuilder;
use ident::ToIdent;
use invoke::{Invoke, Identity};
use lit::LitBuilder;
use mac::MacBuilder;
use pat::PatBuilder;
use path::{IntoPath, PathBuilder};
use qpath::QPathBuilder;
use symbol::ToSymbol;
use ty::TyBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct ExprBuilder<F=Identity> {
    callback: F,
    span: Span,
    attrs: Vec<ast::Attribute>,
}

impl ExprBuilder {
    pub fn new() -> Self {
        ExprBuilder::with_callback(Identity)
    }
}

macro_rules! signed_int_method {
    ($ty:ident, $unsigned:ident) => {
        pub fn $ty(self, val: $ty) -> F::Result {
            if val == ::std::$ty::MIN {
                self.neg().lit().$ty(val as $unsigned)
            } else if val < 0 {
                self.neg().lit().$ty(-val as $unsigned)
            } else {
                self.lit().$ty(val as $unsigned)
            }
        }
    };
}

impl<F> ExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn with_callback(callback: F) -> Self {
        ExprBuilder {
            callback: callback,
            span: DUMMY_SP,
            attrs: vec![],
        }
    }

    pub fn build(self, expr: P<ast::Expr>) -> F::Result {
        self.callback.invoke(expr)
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_attr(mut self, attr: ast::Attribute) -> Self {
        self.attrs.push(attr);
        self
    }

    pub fn attr(self) -> AttrBuilder<Self> {
        AttrBuilder::with_callback(self)
    }

    pub fn build_expr_kind(self, expr: ast::ExprKind) -> F::Result {
        let expr = P(ast::Expr {
            id: ast::DUMMY_NODE_ID,
            node: expr,
            span: self.span,
            attrs: self.attrs.clone().into(),
        });
        self.build(expr)
    }

    pub fn build_path(self, path: ast::Path) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Path(None, path))
    }

    pub fn build_qpath(self, qself: ast::QSelf, path: ast::Path) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Path(Some(qself), path))
    }

    pub fn path(self) -> PathBuilder<Self> {
        let span = self.span;
        PathBuilder::with_callback(self).span(span)
    }

    pub fn qpath(self) -> QPathBuilder<Self> {
        let span = self.span;
        QPathBuilder::with_callback(self).span(span)
    }

    pub fn id<I>(self, id: I) -> F::Result
        where I: ToIdent
    {
        self.path().id(id).build()
    }

    pub fn build_lit(self, lit: P<ast::Lit>) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Lit(lit))
    }

    pub fn lit(self) -> LitBuilder<Self> {
        LitBuilder::with_callback(self)
    }

    pub fn bool(self, value: bool) -> F::Result {
        self.lit().bool(value)
    }

    pub fn true_(self) -> F::Result {
        self.bool(true)
    }

    pub fn false_(self) -> F::Result {
        self.bool(false)
    }

    pub fn int(self, value: i64) -> F::Result {
        if value == ::std::i64::MIN {
            self.neg().lit().int(value as u64)
        } else if value < 0 {
            self.neg().lit().int(-value as u64)
        } else {
            self.lit().int(value as u64)
        }
    }

    pub fn uint(self, value: u64) -> F::Result {
        self.lit().uint(value as u64)
    }

    signed_int_method!(i8, u8);
    signed_int_method!(i16, u16);
    signed_int_method!(i32, u32);
    signed_int_method!(i64, u64);
    signed_int_method!(isize, usize);

    pub fn usize(self, value: usize) -> F::Result {
        self.lit().usize(value)
    }

    pub fn u8(self, value: u8) -> F::Result {
        self.lit().u8(value)
    }

    pub fn u16(self, value: u16) -> F::Result {
        self.lit().u16(value)
    }

    pub fn u32(self, value: u32) -> F::Result {
        self.lit().u32(value)
    }

    pub fn u64(self, value: u64) -> F::Result {
        self.lit().u64(value)
    }

    pub fn f32<S>(self, value: S) -> F::Result
        where S: ToSymbol,
    {
        self.lit().f32(value)
    }

    pub fn f64<S>(self, value: S) -> F::Result
        where S: ToSymbol,
    {
        self.lit().f64(value)
    }

    pub fn str<S>(self, value: S) -> F::Result
        where S: ToSymbol,
    {
        self.lit().str(value)
    }

    pub fn build_unary(self, unop: ast::UnOp, expr: P<ast::Expr>) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Unary(unop, expr))
    }

    pub fn build_deref(self, expr: P<ast::Expr>) -> F::Result {
        self.build_unary(ast::UnOp::Deref, expr)
    }

    pub fn build_not(self, expr: P<ast::Expr>) -> F::Result {
        self.build_unary(ast::UnOp::Not, expr)
    }

    pub fn build_neg(self, expr: P<ast::Expr>) -> F::Result {
        self.build_unary(ast::UnOp::Neg, expr)
    }

    pub fn unary(self, unop: ast::UnOp) -> ExprBuilder<ExprUnaryBuilder<F>> {
        let span = self.span;
        ExprBuilder::with_callback(ExprUnaryBuilder {
            builder: self,
            unop: unop,
        }).span(span)
    }

    pub fn deref(self) -> ExprBuilder<ExprUnaryBuilder<F>> {
        self.unary(ast::UnOp::Deref)
    }

    pub fn not(self) -> ExprBuilder<ExprUnaryBuilder<F>> {
        self.unary(ast::UnOp::Not)
    }

    pub fn neg(self) -> ExprBuilder<ExprUnaryBuilder<F>> {
        self.unary(ast::UnOp::Neg)
    }

    pub fn build_binary(self,
                        binop: ast::BinOpKind,
                        lhs: P<ast::Expr>,
                        rhs: P<ast::Expr>) -> F::Result {
        let binop = respan(self.span, binop);
        self.build_expr_kind(ast::ExprKind::Binary(binop, lhs, rhs))
    }

    pub fn build_add(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Add, lhs, rhs)
    }

    pub fn build_sub(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Sub, lhs, rhs)
    }

    pub fn build_mul(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Mul, lhs, rhs)
    }

    pub fn build_div(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Div, lhs, rhs)
    }

    pub fn build_rem(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Rem, lhs, rhs)
    }

    pub fn build_and(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::And, lhs, rhs)
    }

    pub fn build_or(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Or, lhs, rhs)
    }

    pub fn build_bit_xor(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::BitXor, lhs, rhs)
    }

    pub fn build_bit_and(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::BitAnd, lhs, rhs)
    }

    pub fn build_bit_or(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::BitOr, lhs, rhs)
    }

    pub fn build_shl(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Shl, lhs, rhs)
    }

    pub fn build_shr(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Shr, lhs, rhs)
    }

    pub fn build_eq(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Eq, lhs, rhs)
    }

    pub fn build_lt(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Lt, lhs, rhs)
    }

    pub fn build_le(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Le, lhs, rhs)
    }

    pub fn build_ne(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Ne, lhs, rhs)
    }

    pub fn build_ge(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Ge, lhs, rhs)
    }

    pub fn build_gt(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_binary(ast::BinOpKind::Gt, lhs, rhs)
    }

    pub fn binary(self, binop: ast::BinOpKind) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        let span = self.span;
        ExprBuilder::with_callback(ExprBinaryLhsBuilder {
            builder: self,
            binop: binop,
        }).span(span)
    }

    pub fn add(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Add)
    }

    pub fn sub(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Sub)
    }

    pub fn mul(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Mul)
    }

    pub fn div(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Div)
    }

    pub fn rem(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Rem)
    }

    pub fn and(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::And)
    }

    pub fn or(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Or)
    }

    pub fn bit_xor(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::BitXor)
    }

    pub fn bit_and(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::BitAnd)
    }

    pub fn bit_or(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::BitOr)
    }

    pub fn shl(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Shl)
    }

    pub fn shr(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Shr)
    }

    pub fn eq(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Eq)
    }

    pub fn lt(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Lt)
    }

    pub fn le(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Le)
    }

    pub fn ne(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Ne)
    }

    pub fn ge(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Ge)
    }

    pub fn gt(self) -> ExprBuilder<ExprBinaryLhsBuilder<F>> {
        self.binary(ast::BinOpKind::Gt)
    }

    pub fn ref_(self) -> ExprBuilder<ExprRefBuilder<F>> {
        let span = self.span;
        ExprBuilder::with_callback(ExprRefBuilder {
            builder: self,
            mutability: ast::Mutability::Immutable,
        }).span(span)
    }

    pub fn mut_ref(self) -> ExprBuilder<ExprRefBuilder<F>> {
        let span = self.span;
        ExprBuilder::with_callback(ExprRefBuilder {
            builder: self,
            mutability: ast::Mutability::Mutable,
        }).span(span)
    }

    pub fn break_(self) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Break(None, None))
    }

    pub fn break_to<I>(self, label: I) -> F::Result
        where I: ToIdent,
    {
        let label = respan(self.span, label.to_ident());
        self.build_expr_kind(ast::ExprKind::Break(Some(label), None))
    }

    pub fn continue_(self) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Continue(None))
    }

    pub fn continue_to<I>(self, label: I) -> F::Result
        where I: ToIdent,
    {
        let label = respan(self.span, label.to_ident());
        self.build_expr_kind(ast::ExprKind::Continue(Some(label)))
    }

    pub fn return_(self) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Ret(None))
    }

    pub fn return_expr(self) -> ExprBuilder<ExprReturnBuilder<F>> {
        let span = self.span;
        ExprBuilder::with_callback(ExprReturnBuilder {
            builder: self,
        }).span(span)
    }

    pub fn unit(self) -> F::Result {
        self.tuple().build()
    }

    pub fn tuple(self) -> ExprTupleBuilder<F> {
        ExprTupleBuilder {
            builder: self,
            exprs: Vec::new(),
        }
    }

    pub fn struct_path<P>(self, path: P) -> ExprStructPathBuilder<F>
        where P: IntoPath,
    {
        let span = self.span;
        let path = path.into_path();
        ExprStructPathBuilder {
            builder: self,
            span: span,
            path: path,
            fields: vec![],
        }
    }

    pub fn struct_id<T>(self, id: T) -> ExprStructPathBuilder<F>
        where T: ToIdent,
    {
        self.struct_().id(id).build()
    }

    pub fn struct_(self) -> PathBuilder<ExprStructBuilder<F>> {
        PathBuilder::with_callback(ExprStructBuilder {
            builder: self,
        })
    }

    pub fn self_(self) -> F::Result {
        self.id("self")
    }

    pub fn none(self) -> F::Result {
        self.path()
            .global()
            .id("std").id("option").id("Option").id("None")
            .build()
    }

    pub fn some(self) -> ExprBuilder<ExprPathBuilder<F>> {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .id("std").id("option").id("Option").id("Some")
            .build();
        let span = self.span;

        ExprBuilder::with_callback(ExprPathBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    pub fn ok(self) -> ExprBuilder<ExprPathBuilder<F>> {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .id("std").id("result").id("Result").id("Ok")
            .build();
        let span = self.span;

        ExprBuilder::with_callback(ExprPathBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    pub fn err(self) -> ExprBuilder<ExprPathBuilder<F>> {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .id("std").id("result").id("Result").id("Err")
            .build();
        let span = self.span;

        ExprBuilder::with_callback(ExprPathBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    /// Implement a call for `::std::convert::From::from(value)`
    pub fn from(self) -> ExprBuilder<ExprPathBuilder<F>> {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .ids(&["std", "convert", "From", "from"])
            .build();
        let span = self.span;

        ExprBuilder::with_callback(ExprPathBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    pub fn phantom_data(self) -> F::Result {
        self.path()
            .global()
            .ids(&["std", "marker", "PhantomData"])
            .build()
    }

    pub fn call(self) -> ExprBuilder<ExprCallBuilder<F>> {
        let span = self.span;

        ExprBuilder::with_callback(ExprCallBuilder {
            builder: self,
        }).span(span)
    }

    pub fn method_call<I>(self, id: I) -> ExprBuilder<ExprMethodCallBuilder<F>>
        where I: ToIdent,
    {
        let id = respan(self.span, id.to_ident());
        let span = self.span;

        ExprBuilder::with_callback(ExprMethodCallBuilder {
            builder: self,
            id: id,
        }).span(span)
    }

    pub fn build_block(self, block: P<ast::Block>) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Block(block))
    }

    pub fn block(self) -> BlockBuilder<Self> {
        BlockBuilder::with_callback(self)
    }

    pub fn build_assign(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Assign(lhs, rhs))
    }

    pub fn assign(self) -> ExprBuilder<ExprAssignBuilder<F>> {
        let span = self.span;

        ExprBuilder::with_callback(ExprAssignBuilder {
            builder: self,
        }).span(span)
    }

    pub fn build_assign_op(self,
                           binop: ast::BinOpKind,
                           lhs: P<ast::Expr>,
                           rhs: P<ast::Expr>) -> F::Result {
        let binop = respan(self.span, binop);
        self.build_expr_kind(ast::ExprKind::AssignOp(binop, lhs, rhs))
    }

    pub fn assign_op(self, binop: ast::BinOpKind) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        let span = self.span;

        ExprBuilder::with_callback(ExprAssignOpBuilder {
            builder: self,
            binop: binop,
        }).span(span)
    }

    pub fn add_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::Add)
    }

    pub fn sub_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::Sub)
    }

    pub fn mul_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::Mul)
    }

    pub fn rem_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::Rem)
    }

    pub fn and_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::And)
    }

    pub fn or_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::Or)
    }

    pub fn bit_xor_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::BitXor)
    }

    pub fn bit_and_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::BitAnd)
    }

    pub fn bit_or_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::BitOr)
    }

    pub fn bit_shl_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::Shl)
    }

    pub fn bit_shr_assign(self) -> ExprBuilder<ExprAssignOpBuilder<F>> {
        self.assign_op(ast::BinOpKind::Shr)
    }

    pub fn build_index(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Index(lhs, rhs))
    }

    pub fn index(self) -> ExprBuilder<ExprIndexBuilder<F>> {
        let span = self.span;

        ExprBuilder::with_callback(ExprIndexBuilder {
            builder: self,
        }).span(span)
    }

    pub fn range(self) -> ExprRangeBuilder<F> {
        ExprRangeBuilder {
            builder: self,
        }
    }

    pub fn build_repeat(self, lhs: P<ast::Expr>, rhs: P<ast::Expr>) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Repeat(lhs, rhs))
    }

    pub fn repeat(self) -> ExprBuilder<ExprRepeatBuilder<F>> {
        let span = self.span;

        ExprBuilder::with_callback(ExprRepeatBuilder {
            builder: self,
        }).span(span)
    }

    pub fn loop_(self) -> ExprLoopBuilder<F> {
        let span = self.span;

        ExprLoopBuilder {
            builder: self,
            span: span,
            label: None,
        }
    }

    pub fn if_(self) -> ExprBuilder<ExprIfBuilder<F>> {
        let span = self.span;
        ExprBuilder::with_callback(ExprIfBuilder {
            builder: self,
        }).span(span)
    }

    pub fn match_(self) -> ExprBuilder<ExprMatchBuilder<F>> {
        let span = self.span;

        ExprBuilder::with_callback(ExprMatchBuilder {
            builder: self,
        }).span(span)
    }

    pub fn paren(self) -> ExprBuilder<ExprParenBuilder<F>> {
        let span = self.span;

        ExprBuilder::with_callback(ExprParenBuilder {
            builder: self,
        }).span(span)
    }

    pub fn field<I>(self, id: I) -> ExprBuilder<ExprFieldBuilder<F>>
        where I: ToIdent,
    {
        let id = respan(self.span, id.to_ident());
        let span = self.span;

        ExprBuilder::with_callback(ExprFieldBuilder {
            builder: self,
            id: id,
        }).span(span)
    }

    pub fn tup_field(self, index: usize) -> ExprBuilder<ExprTupFieldBuilder<F>> {
        let index = respan(self.span, index);
        let span = self.span;

        ExprBuilder::with_callback(ExprTupFieldBuilder {
            builder: self,
            index: index,
        }).span(span)
    }

    pub fn box_(self) -> ExprBuilder<ExprPathBuilder<F>> {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .id("std").id("boxed").id("Box").id("new")
            .build();
        let span = self.span;

        ExprBuilder::with_callback(ExprPathBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    pub fn rc(self) -> ExprBuilder<ExprPathBuilder<F>> {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .id("std").id("rc").id("Rc").id("new")
            .build();
        let span = self.span;

        ExprBuilder::with_callback(ExprPathBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    pub fn arc(self) -> ExprBuilder<ExprPathBuilder<F>> {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .id("std").id("arc").id("Arc").id("new")
            .build();
        let span = self.span;

        ExprBuilder::with_callback(ExprPathBuilder {
            builder: self,
            path: path,
        }).span(span)
    }

    pub fn default(self) -> F::Result {
        let path = PathBuilder::new()
            .span(self.span)
            .global()
            .ids(&["std", "default", "Default", "default"])
            .build();

        self.call()
            .build_path(path)
            .build()
    }

    pub fn slice(self) -> ExprSliceBuilder<F> {
        ExprSliceBuilder {
            builder: self,
            exprs: Vec::new(),
        }
    }

    pub fn vec(self) -> ExprSliceBuilder<ExprVecBuilder<F>> {
        ExprBuilder::with_callback(ExprVecBuilder {
            builder: self,
        }).slice()
    }

    /// Represents an equivalent to `try!(...)`. Generates:
    ///
    /// match $expr {
    ///     ::std::result::Result::Ok(expr) => expr,
    ///     ::std::result::Result::Err(err) => {
    ///         return ::std::result::Result::Err(::std::convert::From::from(err))
    ///     }
    /// }
    pub fn try(self) -> ExprBuilder<ExprTryBuilder<F>> {
        let span = self.span;

        ExprBuilder::with_callback(ExprTryBuilder {
            builder: self,
        }).span(span)
    }

    pub fn closure(self) -> ExprClosureBuilder<F> {
        ExprClosureBuilder {
            span: self.span,
            builder: self,
            capture_by: ast::CaptureBy::Ref,
        }
    }

    pub fn while_(self) -> ExprBuilder<ExprWhileBuilder<F>> {
        ExprBuilder::with_callback(ExprWhileBuilder {
            builder: self,
        })
    }

    pub fn type_(self) -> ExprBuilder<ExprTypeBuilder<F>> {
        ExprBuilder::with_callback(ExprTypeBuilder {
            builder: self,
        })
    }

    pub fn build_mac(self, mac: ast::Mac) -> F::Result {
        self.build_expr_kind(ast::ExprKind::Mac(mac))
    }

    pub fn mac(self) -> MacBuilder<Self> {
        let span = self.span;
        MacBuilder::with_callback(self).span(span)
    }
}

impl<F> Invoke<ast::Attribute> for ExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = Self;

    fn invoke(self, attr: ast::Attribute) -> Self {
        self.with_attr(attr)
    }
}

impl<F> Invoke<P<ast::Lit>> for ExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, lit: P<ast::Lit>) -> F::Result {
        self.build_lit(lit)
    }
}

impl<F> Invoke<ast::Path> for ExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, path: ast::Path) -> F::Result {
        self.build_path(path)
    }
}

impl<F> Invoke<(ast::QSelf, ast::Path)> for ExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, (qself, path): (ast::QSelf, ast::Path)) -> F::Result {
        self.build_qpath(qself, path)
    }
}

impl<F> Invoke<P<ast::Block>> for ExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, block: P<ast::Block>) -> F::Result {
        self.build_block(block)
    }
}

impl<F> Invoke<ast::Mac> for ExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, mac: ast::Mac) -> F::Result {
        self.build_mac(mac)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprUnaryBuilder<F> {
    builder: ExprBuilder<F>,
    unop: ast::UnOp,
}

impl<F> Invoke<P<ast::Expr>> for ExprUnaryBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.builder.build_unary(self.unop, expr)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprBinaryLhsBuilder<F> {
    builder: ExprBuilder<F>,
    binop: ast::BinOpKind,
}

impl<F> Invoke<P<ast::Expr>> for ExprBinaryLhsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprBuilder<ExprBinaryRhsBuilder<F>>;

    fn invoke(self, lhs: P<ast::Expr>) -> ExprBuilder<ExprBinaryRhsBuilder<F>> {
        ExprBuilder::with_callback(ExprBinaryRhsBuilder {
            builder: self.builder,
            binop: self.binop,
            lhs: lhs,
        })
    }
}

pub struct ExprBinaryRhsBuilder<F> {
    builder: ExprBuilder<F>,
    binop: ast::BinOpKind,
    lhs: P<ast::Expr>,
}

impl<F> Invoke<P<ast::Expr>> for ExprBinaryRhsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, rhs: P<ast::Expr>) -> F::Result {
        self.builder.build_binary(self.binop, self.lhs, rhs)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprReturnBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprReturnBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Ret(Some(expr)))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprTupleBuilder<F> {
    builder: ExprBuilder<F>,
    exprs: Vec<P<ast::Expr>>,
}

impl<F: Invoke<P<ast::Expr>>> ExprTupleBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    pub fn with_exprs<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=P<ast::Expr>>,
    {
        self.exprs.extend(iter);
        self
    }

    pub fn expr(self) -> ExprBuilder<Self> {
        ExprBuilder::with_callback(self)
    }

    pub fn build(self) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Tup(self.exprs))
    }
}

impl<F> Invoke<P<ast::Expr>> for ExprTupleBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    type Result = ExprTupleBuilder<F>;

    fn invoke(mut self, expr: P<ast::Expr>) -> Self {
        self.exprs.push(expr);
        self
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprStructBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<ast::Path> for ExprStructBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    type Result = ExprStructPathBuilder<F>;

    fn invoke(self, path: ast::Path) -> ExprStructPathBuilder<F> {
        self.builder.struct_path(path)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprStructPathBuilder<F> {
    builder: ExprBuilder<F>,
    span: Span,
    path: ast::Path,
    fields: Vec<ast::Field>,
}

impl<F> ExprStructPathBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_fields<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Field>,
    {
        self.fields.extend(iter);
        self
    }

    pub fn with_id_exprs<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=(ast::Ident, P<ast::Expr>)>,
    {
        for (id, expr) in iter {
            self = self.field(id).build(expr);
        }

        self
    }

    pub fn field<I>(self, id: I) -> ExprBuilder<ExprStructFieldBuilder<I, F>>
        where I: ToIdent,
    {
        let span = self.span;

        ExprBuilder::with_callback(ExprStructFieldBuilder {
            builder: self,
            id: id,
        }).span(span)
    }

    pub fn build_with(self) -> ExprBuilder<Self> {
        ExprBuilder::with_callback(self)
    }

    pub fn build(self) -> F::Result {
        let expr_kind = ast::ExprKind::Struct(self.path, self.fields, None);
        self.builder.build_expr_kind(expr_kind)
    }
}

impl<F> Invoke<P<ast::Expr>> for ExprStructPathBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        let expr_kind = ast::ExprKind::Struct(self.path, self.fields, Some(expr));
        self.builder.build_expr_kind(expr_kind)
    }
}

pub struct ExprStructFieldBuilder<I, F> {
    builder: ExprStructPathBuilder<F>,
    id: I,
}

impl<I, F> Invoke<P<ast::Expr>> for ExprStructFieldBuilder<I, F>
    where I: ToIdent,
          F: Invoke<P<ast::Expr>>,
{
    type Result = ExprStructPathBuilder<F>;

    fn invoke(mut self, expr: P<ast::Expr>) -> ExprStructPathBuilder<F> {
        let field = ast::Field {
            ident: respan(self.builder.span, self.id.to_ident()),
            expr: expr,
            span: self.builder.span,
            is_shorthand: false,
            attrs: Vec::new().into(),
        };
        self.builder.fields.push(field);
        self.builder
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprCallBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprCallBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprCallArgsBuilder<F>;

    fn invoke(self, expr: P<ast::Expr>) -> ExprCallArgsBuilder<F> {
        ExprCallArgsBuilder {
            builder: self.builder,
            fn_: expr,
            args: vec![],
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprCallArgsBuilder<F> {
    builder: ExprBuilder<F>,
    fn_: P<ast::Expr>,
    args: Vec<P<ast::Expr>>,
}

impl<F> ExprCallArgsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn with_args<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=P<ast::Expr>>,
    {
        self.args.extend(iter);
        self
    }

    pub fn with_arg(mut self, arg: P<ast::Expr>) -> Self {
        self.args.push(arg);
        self
    }

    pub fn arg(self) -> ExprBuilder<Self> {
        let span = self.builder.span;
        ExprBuilder::with_callback(self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Call(self.fn_, self.args))
    }
}

impl<F> Invoke<P<ast::Expr>> for ExprCallArgsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = Self;

    fn invoke(self, arg: P<ast::Expr>) -> Self {
        self.with_arg(arg)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprMethodCallBuilder<F> {
    builder: ExprBuilder<F>,
    id: ast::SpannedIdent,
}

impl<F> Invoke<P<ast::Expr>> for ExprMethodCallBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprMethodCallArgsBuilder<F>;

    fn invoke(self, expr: P<ast::Expr>) -> ExprMethodCallArgsBuilder<F> {
        ExprMethodCallArgsBuilder {
            builder: self.builder,
            id: self.id,
            tys: vec![],
            args: vec![expr],
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprMethodCallArgsBuilder<F> {
    builder: ExprBuilder<F>,
    id: ast::SpannedIdent,
    tys: Vec<P<ast::Ty>>,
    args: Vec<P<ast::Expr>>,
}

impl<F> ExprMethodCallArgsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
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

    pub fn with_args<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=P<ast::Expr>>,
    {
        self.args.extend(iter);
        self
    }

    pub fn with_arg(mut self, arg: P<ast::Expr>) -> Self {
        self.args.push(arg);
        self
    }

    pub fn arg(self) -> ExprBuilder<Self> {
        let span = self.builder.span;
        ExprBuilder::with_callback(self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::MethodCall(self.id, self.tys, self.args))
    }
}

impl<F> Invoke<P<ast::Ty>> for ExprMethodCallArgsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = Self;

    fn invoke(self, ty: P<ast::Ty>) -> Self {
        self.with_ty(ty)
    }
}

impl<F> Invoke<P<ast::Expr>> for ExprMethodCallArgsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = Self;

    fn invoke(self, arg: P<ast::Expr>) -> Self {
        self.with_arg(arg)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprRefBuilder<F> {
    builder: ExprBuilder<F>,
    mutability: ast::Mutability,
}

impl<F> Invoke<P<ast::Expr>> for ExprRefBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::AddrOf(self.mutability, expr))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprPathBuilder<F> {
    builder: ExprBuilder<F>,
    path: ast::Path,
}

impl<F> Invoke<P<ast::Expr>> for ExprPathBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, arg: P<ast::Expr>) -> F::Result {
        self.builder.call()
            .build_path(self.path)
            .with_arg(arg)
            .build()
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprAssignBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprAssignBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprBuilder<ExprAssignLhsBuilder<F>>;

    fn invoke(self, lhs: P<ast::Expr>) -> ExprBuilder<ExprAssignLhsBuilder<F>> {
        let span = self.builder.span;
        ExprBuilder::with_callback(ExprAssignLhsBuilder {
            builder: self.builder,
            lhs: lhs,
        }).span(span)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprAssignLhsBuilder<F> {
    builder: ExprBuilder<F>,
    lhs: P<ast::Expr>,
}

impl<F> Invoke<P<ast::Expr>> for ExprAssignLhsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, rhs: P<ast::Expr>) -> F::Result {
        self.builder.build_assign(self.lhs, rhs)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprAssignOpBuilder<F> {
    builder: ExprBuilder<F>,
    binop: ast::BinOpKind,
}

impl<F> Invoke<P<ast::Expr>> for ExprAssignOpBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprBuilder<ExprAssignOpLhsBuilder<F>>;

    fn invoke(self, lhs: P<ast::Expr>) -> ExprBuilder<ExprAssignOpLhsBuilder<F>> {
        let span = self.builder.span;
        ExprBuilder::with_callback(ExprAssignOpLhsBuilder {
            builder: self.builder,
            binop: self.binop,
            lhs: lhs,
        }).span(span)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprAssignOpLhsBuilder<F> {
    builder: ExprBuilder<F>,
    binop: ast::BinOpKind,
    lhs: P<ast::Expr>,
}

impl<F> Invoke<P<ast::Expr>> for ExprAssignOpLhsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, rhs: P<ast::Expr>) -> F::Result {
        self.builder.build_assign_op(self.binop, self.lhs, rhs)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprIndexBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprIndexBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprBuilder<ExprIndexLhsBuilder<F>>;

    fn invoke(self, lhs: P<ast::Expr>) -> ExprBuilder<ExprIndexLhsBuilder<F>> {
        let span = self.builder.span;
        ExprBuilder::with_callback(ExprIndexLhsBuilder {
            builder: self.builder,
            lhs: lhs,
        }).span(span)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprIndexLhsBuilder<F> {
    builder: ExprBuilder<F>,
    lhs: P<ast::Expr>,
}

impl<F> Invoke<P<ast::Expr>> for ExprIndexLhsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, rhs: P<ast::Expr>) -> F::Result {
        self.builder.build_index(self.lhs, rhs)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprRangeBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> ExprRangeBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn from(self) -> ExprBuilder<Self> {
        ExprBuilder::with_callback(self)
    }

    pub fn to(self) -> ExprBuilder<ExprRangeToBuilder<F>> {
        self.from_opt(None).to()
    }

    pub fn to_inclusive(self) -> ExprBuilder<ExprRangeToBuilder<F>> {
        self.from_opt(None).to_inclusive()
    }

    pub fn from_opt(self, from: Option<P<ast::Expr>>) -> ExprRangeFromBuilder<F> {
        ExprRangeFromBuilder {
            builder: self.builder,
            from: from,
        }
    }

    pub fn build(self) -> F::Result {
        self.from_opt(None).build()
    }
}

impl<F> Invoke<P<ast::Expr>> for ExprRangeBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprRangeFromBuilder<F>;

    fn invoke(self, from: P<ast::Expr>) -> ExprRangeFromBuilder<F> {
        self.from_opt(Some(from))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprRangeFromBuilder<F> {
    builder: ExprBuilder<F>,
    from: Option<P<ast::Expr>>,
}

impl<F> ExprRangeFromBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn to(self) -> ExprBuilder<ExprRangeToBuilder<F>> {
        ExprBuilder::with_callback(ExprRangeToBuilder {
            builder: self,
            limit: ast::RangeLimits::HalfOpen,
        })
    }

    pub fn to_inclusive(self) -> ExprBuilder<ExprRangeToBuilder<F>> {
        ExprBuilder::with_callback(ExprRangeToBuilder {
            builder: self,
            limit: ast::RangeLimits::Closed,
        })
    }

    pub fn build(self) -> F::Result {
        self.to_opt(None, ast::RangeLimits::HalfOpen)
    }

    pub fn to_opt(self, to: Option<P<ast::Expr>>, limit: ast::RangeLimits) -> F::Result {
        let kind = ast::ExprKind::Range(self.from, to, limit);
        self.builder.build_expr_kind(kind)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprRangeToBuilder<F> {
    builder: ExprRangeFromBuilder<F>,
    limit: ast::RangeLimits,
}

impl<F> Invoke<P<ast::Expr>> for ExprRangeToBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.builder.to_opt(Some(expr), self.limit)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprRepeatBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprRepeatBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprBuilder<ExprRepeatLhsBuilder<F>>;

    fn invoke(self, lhs: P<ast::Expr>) -> ExprBuilder<ExprRepeatLhsBuilder<F>> {
        let span = self.builder.span;
        ExprBuilder::with_callback(ExprRepeatLhsBuilder {
            builder: self.builder,
            lhs: lhs,
        }).span(span)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprRepeatLhsBuilder<F> {
    builder: ExprBuilder<F>,
    lhs: P<ast::Expr>,
}

impl<F> Invoke<P<ast::Expr>> for ExprRepeatLhsBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, rhs: P<ast::Expr>) -> F::Result {
        self.builder.build_repeat(self.lhs, rhs)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprLoopBuilder<F> {
    builder: ExprBuilder<F>,
    span: Span,
    label: Option<Spanned<ast::Ident>>,
}

impl<F> ExprLoopBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn label<I>(mut self, id: I) -> Self
        where I: ToIdent,
    {
        self.label = Some(respan(self.span, id.to_ident()));
        self
    }

    pub fn block(self) -> BlockBuilder<Self> {
        BlockBuilder::with_callback(self)
    }
}

impl<F> Invoke<P<ast::Block>> for ExprLoopBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, block: P<ast::Block>) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Loop(block, self.label))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprIfBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprIfBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprIfThenBuilder<F>;

    fn invoke(self, condition: P<ast::Expr>) -> ExprIfThenBuilder<F> {
        ExprIfThenBuilder {
            builder: self.builder,
            condition: condition,
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprIfThenBuilder<F> {
    builder: ExprBuilder<F>,
    condition: P<ast::Expr>,
}

impl<F> ExprIfThenBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn build_then(self, block: P<ast::Block>) -> ExprIfThenElseBuilder<F> {
        ExprIfThenElseBuilder {
            builder: self.builder,
            condition: self.condition,
            then: block,
            else_ifs: Vec::new(),
        }
    }

    pub fn then(self) -> BlockBuilder<Self> {
        BlockBuilder::with_callback(self)
    }
}

impl<F> Invoke<P<ast::Block>> for ExprIfThenBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprIfThenElseBuilder<F>;

    fn invoke(self, block: P<ast::Block>) -> ExprIfThenElseBuilder<F> {
        self.build_then(block)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprIfThenElseBuilder<F> {
    builder: ExprBuilder<F>,
    condition: P<ast::Expr>,
    then: P<ast::Block>,
    else_ifs: Vec<(P<ast::Expr>, P<ast::Block>)>,
}

impl<F> ExprIfThenElseBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn else_if(self) -> ExprBuilder<ExprElseIfBuilder<F>> {
        let span = self.builder.span;
        ExprBuilder::with_callback(ExprElseIfBuilder {
            builder: self,
        }).span(span)
    }

    fn build_else_expr(self, mut else_: P<ast::Expr>) -> F::Result {
        for (cond, block) in self.else_ifs.into_iter().rev() {
            else_ = ExprBuilder::new().if_()
                .build(cond)
                .build_then(block)
                .build_else_expr(else_);
        }

        self.builder.build_expr_kind(ast::ExprKind::If(self.condition, self.then, Some(else_)))
    }

    pub fn build_else(self, block: P<ast::Block>) -> F::Result {
        let else_ = ExprBuilder::new().build_block(block);
        self.build_else_expr(else_)
    }

    pub fn else_(self) -> BlockBuilder<Self> {
        BlockBuilder::with_callback(self)
    }

    pub fn build(self) -> F::Result {
        let mut else_ifs = self.else_ifs.into_iter().rev();

        let else_ = match else_ifs.next() {
            Some((cond, block)) => {
                let mut else_ = ExprBuilder::new().if_()
                    .build(cond)
                    .build_then(block)
                    .build();

                for (cond, block) in else_ifs.into_iter().rev() {
                    else_ = ExprBuilder::new().if_()
                        .build(cond)
                        .build_then(block)
                        .build_else_expr(else_);
                }

                Some(else_)
            }
            None => None
        };

        self.builder.build_expr_kind(ast::ExprKind::If(self.condition, self.then, else_))
    }
}

impl<F> Invoke<P<ast::Block>> for ExprIfThenElseBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, block: P<ast::Block>) -> F::Result {
        self.build_else(block)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprElseIfBuilder<F> {
    builder: ExprIfThenElseBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprElseIfBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprElseIfThenBuilder<F>;

    fn invoke(self, expr: P<ast::Expr>) -> ExprElseIfThenBuilder<F> {
        ExprElseIfThenBuilder {
            builder: self.builder,
            condition: expr,
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprElseIfThenBuilder<F> {
    builder: ExprIfThenElseBuilder<F>,
    condition: P<ast::Expr>,
}

impl<F> ExprElseIfThenBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn build_then(mut self, block: P<ast::Block>) -> ExprIfThenElseBuilder<F> {
        self.builder.else_ifs.push((self.condition, block));
        self.builder
    }

    pub fn then(self) -> BlockBuilder<Self> {
        BlockBuilder::with_callback(self)
    }
}

impl<F> Invoke<P<ast::Block>> for ExprElseIfThenBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprIfThenElseBuilder<F>;

    fn invoke(self, block: P<ast::Block>) -> ExprIfThenElseBuilder<F> {
        self.build_then(block)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprMatchBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprMatchBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprMatchArmBuilder<F>;

    fn invoke(self, expr: P<ast::Expr>) -> ExprMatchArmBuilder<F> {
        ExprMatchArmBuilder {
            builder: self.builder,
            expr: expr,
            arms: Vec::new(),
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

pub struct ExprMatchArmBuilder<F> {
    builder: ExprBuilder<F>,
    expr: P<ast::Expr>,
    arms: Vec<ast::Arm>,
}

impl<F> ExprMatchArmBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn with_arms<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Arm>,
    {
        self.arms.extend(iter);
        self
    }

    pub fn with_arm(mut self, arm: ast::Arm) -> Self {
        self.arms.push(arm);
        self
    }

    pub fn arm(self) -> ArmBuilder<Self> {
        ArmBuilder::with_callback(self)
    }

    pub fn build(self) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Match(self.expr, self.arms))
    }
}

impl<F> Invoke<ast::Arm> for ExprMatchArmBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = Self;

    fn invoke(self, arm: ast::Arm) -> Self {
        self.with_arm(arm)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprParenBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprParenBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Paren(expr))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprFieldBuilder<F> {
    builder: ExprBuilder<F>,
    id: ast::SpannedIdent,
}

impl<F> Invoke<P<ast::Expr>> for ExprFieldBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Field(expr, self.id))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprTupFieldBuilder<F> {
    builder: ExprBuilder<F>,
    index: Spanned<usize>,
}

impl<F> Invoke<P<ast::Expr>> for ExprTupFieldBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::TupField(expr, self.index))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprSliceBuilder<F> {
    builder: ExprBuilder<F>,
    exprs: Vec<P<ast::Expr>>,
}

impl<F: Invoke<P<ast::Expr>>> ExprSliceBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    pub fn with_exprs<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=P<ast::Expr>>,
    {
        self.exprs.extend(iter);
        self
    }

    pub fn expr(self) -> ExprBuilder<Self> {
        let span = self.builder.span;
        ExprBuilder::with_callback(self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Array(self.exprs))
    }
}

impl<F> Invoke<P<ast::Expr>> for ExprSliceBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    type Result = ExprSliceBuilder<F>;

    fn invoke(mut self, expr: P<ast::Expr>) -> Self {
        self.exprs.push(expr);
        self
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprVecBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprVecBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        let qpath = ExprBuilder::new().qpath()
            .ty().slice().infer()
            .id("into_vec");

        self.builder.call()
            .build(qpath)
            .arg().box_().build(expr)
            .build()
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprTryBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprTryBuilder<F>
    where F: Invoke<P<ast::Expr>>
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        // ::std::result::Result::Ok(value) => value,
        let ok_arm = ArmBuilder::new().span(self.builder.span)
            .pat().ok().id("value")
            .body().id("value");

        // ::std::result::Result::Err(err) =>
        //     return ::std::convert::From::from(err),
        let err_arm = ArmBuilder::new().span(self.builder.span)
            .pat().err().id("err")
            .body().return_expr().err().from().id("err");

        // match $expr {
        //     $ok_arm,
        //     $err_arm,
        // }
        self.builder.match_().build(expr.clone())
            .with_arm(ok_arm)
            .with_arm(err_arm)
            .build()
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprClosureBuilder<F> {
    builder: ExprBuilder<F>,
    capture_by: ast::CaptureBy,
    span: Span,
}

impl<F> ExprClosureBuilder<F> {
    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn by_value(mut self) -> Self {
        self.capture_by = ast::CaptureBy::Value;
        self
    }

    pub fn by_ref(mut self) -> Self {
        self.capture_by = ast::CaptureBy::Ref;
        self
    }

    pub fn fn_decl(self) -> FnDeclBuilder<Self> {
        FnDeclBuilder::with_callback(self)
    }

    pub fn build_fn_decl(self, fn_decl: P<ast::FnDecl>) -> ExprClosureExprBuilder<F> {
        ExprClosureExprBuilder {
            builder: self.builder,
            capture_by: self.capture_by,
            fn_decl: fn_decl,
            span: self.span,
        }
    }
}

impl<F> Invoke<P<ast::FnDecl>> for ExprClosureBuilder<F> {
    type Result = ExprClosureExprBuilder<F>;

    fn invoke(self, fn_decl: P<ast::FnDecl>) -> ExprClosureExprBuilder<F> {
        self.build_fn_decl(fn_decl)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprClosureExprBuilder<F> {
    builder: ExprBuilder<F>,
    capture_by: ast::CaptureBy,
    fn_decl: P<ast::FnDecl>,
    span: Span,
}

impl<F> ExprClosureExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn expr(self) -> ExprBuilder<Self> {
        let span = self.span;
        ExprBuilder::with_callback(self).span(span)
    }

    pub fn build_expr(self, expr: P<ast::Expr>) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Closure(self.capture_by, self.fn_decl, expr, self.span))
    }
}

impl<F> Invoke<P<ast::Expr>> for ExprClosureExprBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, expr: P<ast::Expr>) -> F::Result {
        self.build_expr(expr)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprWhileBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprWhileBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = ExprWhileBlockBuilder<F>;

    fn invoke(self, condition: P<ast::Expr>) -> ExprWhileBlockBuilder<F> {
        ExprWhileBlockBuilder {
            span: self.builder.span,
            builder: self.builder,
            condition: condition,
            pat: None,
            label: None,
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprWhileBlockBuilder<F> {
    builder: ExprBuilder<F>,
    condition: P<ast::Expr>,
    pat: Option<P<ast::Pat>>,
    span: Span,
    label: Option<ast::SpannedIdent>,
}

impl<F> ExprWhileBlockBuilder<F> {
    pub fn pat(self) -> PatBuilder<Self> {
        PatBuilder::with_callback(self)
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn label<I>(mut self, id: I) -> Self
        where I: ToIdent,
    {
        self.label = Some(respan(self.span, id.to_ident()));
        self
    }

    pub fn build_pat(mut self, pat: P<ast::Pat>) -> Self {
        self.pat = Some(pat);
        self
    }
}

impl<F> ExprWhileBlockBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    pub fn block(self) -> BlockBuilder<Self> {
        BlockBuilder::with_callback(self)
    }

    pub fn build_block(self, block: P<ast::Block>) -> F::Result {
        match self.pat {
            Some(p) => self.builder.build_expr_kind(ast::ExprKind::WhileLet(
                p, self.condition, block, self.label)),
            None => self.builder.build_expr_kind(ast::ExprKind::While(
                self.condition, block, self.label)),
        }
    }
}

impl<F> Invoke<P<ast::Pat>> for ExprWhileBlockBuilder<F> {
    type Result = Self;

    fn invoke(self, pat: P<ast::Pat>) -> Self {
        self.build_pat(pat)
    }
}

impl<F> Invoke<P<ast::Block>> for ExprWhileBlockBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, block: P<ast::Block>) -> F::Result {
        self.build_block(block)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprTypeBuilder<F> {
    builder: ExprBuilder<F>,
}

impl<F> Invoke<P<ast::Expr>> for ExprTypeBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = TyBuilder<ExprTypeTyBuilder<F>>;

    fn invoke(self, expr: P<ast::Expr>) -> TyBuilder<ExprTypeTyBuilder<F>> {
        TyBuilder::with_callback(ExprTypeTyBuilder {
            builder: self.builder,
            expr: expr,
        })
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ExprTypeTyBuilder<F> {
    builder: ExprBuilder<F>,
    expr: P<ast::Expr>,
}

impl<F> Invoke<P<ast::Ty>> for ExprTypeTyBuilder<F>
    where F: Invoke<P<ast::Expr>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.builder.build_expr_kind(ast::ExprKind::Type(self.expr, ty))
    }
}
