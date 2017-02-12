use std::convert::Into;
use std::rc::Rc;

use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span};
use syntax::ptr::P;

use invoke::{Invoke, Identity};

use symbol::ToSymbol;

#[cfg(feature = "with-syntex")]
#[allow(non_camel_case_types)]
type umax = u64;
#[cfg(not(feature = "with-syntex"))]
#[allow(non_camel_case_types)]
type umax = u128;

//////////////////////////////////////////////////////////////////////////////

pub struct LitBuilder<F=Identity> {
    callback: F,
    span: Span,
}

impl LitBuilder {
    pub fn new() -> LitBuilder {
        LitBuilder::with_callback(Identity)
    }
}

impl<F> LitBuilder<F>
    where F: Invoke<P<ast::Lit>>,
{
    pub fn with_callback(callback: F) -> Self {
        LitBuilder {
            callback: callback,
            span: DUMMY_SP,
        }
    }

    pub fn span(mut self, span: Span) -> LitBuilder<F> {
        self.span = span;
        self
    }

    pub fn build_lit(self, lit: ast::LitKind) -> F::Result {
        self.callback.invoke(P(ast::Lit {
            span: self.span,
            node: lit,
        }))
    }

    pub fn bool(self, value: bool) -> F::Result {
        self.build_lit(ast::LitKind::Bool(value))
    }

    pub fn true_(self) -> F::Result {
        self.bool(true)
    }

    pub fn false_(self) -> F::Result {
        self.bool(false)
    }

    pub fn int(self, value: u64) -> F::Result {
        self.build_lit(ast::LitKind::Int(value as umax, ast::LitIntType::Unsuffixed))
    }

    fn build_int(self, value: u64, ty: ast::IntTy) -> F::Result {
        self.build_lit(ast::LitKind::Int(value as umax, ast::LitIntType::Signed(ty)))
    }

    pub fn isize(self, value: usize) -> F::Result {
        self.build_int(value as u64, ast::IntTy::Is)
    }

    pub fn i8(self, value: u8) -> F::Result {
        self.build_int(value as u64, ast::IntTy::I8)
    }

    pub fn i16(self, value: u16) -> F::Result {
        self.build_int(value as u64, ast::IntTy::I16)
    }

    pub fn i32(self, value: u32) -> F::Result {
        self.build_int(value as u64, ast::IntTy::I32)
    }

    pub fn i64(self, value: u64) -> F::Result {
        self.build_int(value as u64, ast::IntTy::I64)
    }

    pub fn uint(self, value: u64) -> F::Result {
        self.build_lit(ast::LitKind::Int(value as umax, ast::LitIntType::Unsuffixed))
    }

    fn build_uint(self, value: u64, ty: ast::UintTy) -> F::Result {
        self.build_lit(ast::LitKind::Int(value as umax, ast::LitIntType::Unsigned(ty)))
    }

    pub fn usize(self, value: usize) -> F::Result {
        self.build_uint(value as u64, ast::UintTy::Us)
    }

    pub fn u8(self, value: u8) -> F::Result {
        self.build_uint(value as u64, ast::UintTy::U8)
    }

    pub fn u16(self, value: u16) -> F::Result {
        self.build_uint(value as u64, ast::UintTy::U16)
    }

    pub fn u32(self, value: u32) -> F::Result {
        self.build_uint(value as u64, ast::UintTy::U32)
    }

    pub fn u64(self, value: u64) -> F::Result {
        self.build_uint(value, ast::UintTy::U64)
    }

    fn build_float<S>(self, value: S, ty: ast::FloatTy) -> F::Result
        where S: ToSymbol,
    {
        self.build_lit(ast::LitKind::Float(value.to_symbol(), ty))
    }

    pub fn f32<S>(self, value: S) -> F::Result
        where S: ToSymbol,
    {
        self.build_float(value, ast::FloatTy::F32)
    }

    pub fn f64<S>(self, value: S) -> F::Result
        where S: ToSymbol,
    {
        self.build_float(value, ast::FloatTy::F64)
    }

    pub fn char(self, value: char) -> F::Result {
        self.build_lit(ast::LitKind::Char(value))
    }

    pub fn byte(self, value: u8) -> F::Result {
        self.build_lit(ast::LitKind::Byte(value))
    }

    pub fn str<S>(self, value: S) -> F::Result
        where S: ToSymbol,
    {
        let value = value.to_symbol();
        self.build_lit(ast::LitKind::Str(value, ast::StrStyle::Cooked))
    }

    pub fn byte_str<T>(self, value: T) -> F::Result
        where T: Into<Vec<u8>>,
    {
        self.build_lit(ast::LitKind::ByteStr(Rc::new(value.into())))
    }
}
