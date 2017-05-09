// Copyright 2012-2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(not(feature = "with-syntex"), feature(rustc_private, i128_type))]
#![cfg_attr(feature = "unstable-testing", feature(plugin))]
#![cfg_attr(feature = "unstable-testing", plugin(clippy))]

#[macro_use]
#[cfg(feature = "with-syntex")]
extern crate syntex_syntax as syntax;
#[cfg(feature = "with-syntex")]
extern crate syntex_errors as errors;

#[macro_use]
#[cfg(not(feature = "with-syntex"))]
extern crate syntax;
#[cfg(not(feature = "with-syntex"))]
extern crate rustc_errors as errors;

use std::iter;
use std::marker;
use std::rc::Rc;
use std::usize;

use syntax::ast;
use syntax::codemap::{DUMMY_SP, Spanned, dummy_spanned};
use syntax::ext::base::ExtCtxt;
use syntax::parse::parser::Parser;
use syntax::parse::{self, classify, parse_tts_from_source_str, token};
use syntax::print::pprust;
use syntax::ptr::P;
use syntax::symbol::Symbol;
use syntax::tokenstream::{self, TokenTree};
use syntax::util::ThinVec;

pub trait ToTokens {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree>;
}

impl ToTokens for TokenTree {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec!(self.clone())
    }
}

impl<'a, T: ToTokens> ToTokens for &'a T {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        (**self).to_tokens(cx)
    }
}

impl<'a, T: ToTokens> ToTokens for &'a [T] {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        self.iter()
            .flat_map(|t| t.to_tokens(cx).into_iter())
            .collect()
    }
}

impl<T: ToTokens> ToTokens for Vec<T> {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        self.iter().flat_map(|t| t.to_tokens(cx).into_iter()).collect()
    }
}

impl<T: ToTokens> ToTokens for Spanned<T> {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        // FIXME: use the span?
        self.node.to_tokens(cx)
    }
}

impl<T: ToTokens> ToTokens for Option<T> {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        match *self {
            Some(ref t) => t.to_tokens(cx),
            None => Vec::new(),
        }
    }
}

impl ToTokens for ast::Ident {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(DUMMY_SP, token::Ident(*self))]
    }
}

impl ToTokens for ast::Path {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(DUMMY_SP, token::Interpolated(Rc::new(token::NtPath(self.clone()))))]
    }
}

impl ToTokens for ast::Ty {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtTy(P(self.clone())))))]
    }
}

impl ToTokens for P<ast::Ty> {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtTy(self.clone()))))]
    }
}

impl ToTokens for P<ast::Block> {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtBlock(self.clone()))))]
    }
}

impl ToTokens for P<ast::Item> {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtItem(self.clone()))))]
    }
}

impl ToTokens for P<ast::ImplItem> {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtImplItem((**self).clone()))))]
    }
}

impl ToTokens for P<ast::TraitItem> {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtTraitItem((**self).clone()))))]
    }
}

impl ToTokens for ast::Generics {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        /*
        vec![TokenTree::Token(DUMMY_SP, token::Interpolated(token::NtGenerics(self.clone())))]
        */

        let s = pprust::generics_to_string(self);

        panictry!(parse_tts_from_source_str(
            "<quote expansion>".to_string(),
            s,
            cx.parse_sess()))
    }
}

impl ToTokens for ast::WhereClause {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        /*
        vec![TokenTree::Token(DUMMY_SP,
                              token::Interpolated(token::NtWhereClause(self.clone())))]
        */

        let s = pprust::to_string(|s| {
            s.print_where_clause(self)
        });

        panictry!(parse_tts_from_source_str(
            "<quote expansion>".to_string(),
            s,
            cx.parse_sess()))
    }
}

impl ToTokens for ast::Stmt {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        let mut tts = vec![
            TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtStmt(self.clone()))))
        ];

        // Some statements require a trailing semicolon.
        if classify::stmt_ends_with_semi(&self.node) {
            tts.push(TokenTree::Token(self.span, token::Semi));
        }

        tts
    }
}

impl ToTokens for P<ast::Expr> {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtExpr(self.clone()))))]
    }
}

impl ToTokens for P<ast::Pat> {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(self.span, token::Interpolated(Rc::new(token::NtPat(self.clone()))))]
    }
}

impl ToTokens for ast::Arm {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(DUMMY_SP, token::Interpolated(Rc::new(token::NtArm(self.clone()))))]
    }
}

macro_rules! impl_to_tokens_slice {
    ($t: ty, $sep: expr) => {
        impl ToTokens for [$t] {
            fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
                let mut v = vec![];
                for (i, x) in self.iter().enumerate() {
                    if i > 0 {
                        v.extend($sep.iter().cloned());
                    }
                    v.extend(x.to_tokens(cx));
                }
                v
            }
        }
    };
}

impl_to_tokens_slice! { ast::Ty, [TokenTree::Token(DUMMY_SP, token::Comma)] }
impl_to_tokens_slice! { P<ast::Item>, [] }

impl ToTokens for ast::MetaItem {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Token(DUMMY_SP, token::Interpolated(Rc::new(token::NtMeta(self.clone()))))]
    }
}

impl ToTokens for ast::Arg {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        let mut v = self.pat.to_tokens(cx);
        v.push(TokenTree::Token(DUMMY_SP, token::Colon));
        v.extend(self.ty.to_tokens(cx));
        v.push(TokenTree::Token(DUMMY_SP, token::Comma));
        v
    }
}

impl ToTokens for ast::Attribute {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        let mut r = vec![];
        // FIXME: The spans could be better
        r.push(TokenTree::Token(self.span, token::Pound));
        if self.style == ast::AttrStyle::Inner {
            r.push(TokenTree::Token(self.span, token::Not));
        }
        r.push(TokenTree::Delimited(self.span, Rc::new(tokenstream::Delimited {
            delim: token::Bracket,
            tts: self.value.to_tokens(cx),
        })));
        r
    }
}

impl ToTokens for str {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        let lit
         = ast::LitKind::Str(
            Symbol::intern(self), ast::StrStyle::Cooked);
        dummy_spanned(lit).to_tokens(cx)
    }
}

impl ToTokens for String {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        let lit
         = ast::LitKind::Str(
            Symbol::intern(self), ast::StrStyle::Cooked);
        dummy_spanned(lit).to_tokens(cx)
    }
}

impl ToTokens for () {
    fn to_tokens(&self, _cx: &ExtCtxt) -> Vec<TokenTree> {
        vec![TokenTree::Delimited(DUMMY_SP, Rc::new(tokenstream::Delimited {
            delim: token::Paren,
            tts: vec![],
        }))]
    }
}

impl ToTokens for ast::Lit {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        // FIXME: This is wrong
        P(ast::Expr {
            id: ast::DUMMY_NODE_ID,
            node: ast::ExprKind::Lit(P(self.clone())),
            span: DUMMY_SP,
            attrs: ThinVec::new(),
        }).to_tokens(cx)
    }
}

impl ToTokens for bool {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        dummy_spanned(ast::LitKind::Bool(*self)).to_tokens(cx)
    }
}

impl ToTokens for char {
    fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
        dummy_spanned(ast::LitKind::Char(*self)).to_tokens(cx)
    }
}

macro_rules! impl_wrap_repeat {
    ($t:ty) => (
        impl IntoWrappedRepeat for $t {
            type Item = $t;
            type IntoIter = iter::Repeat<$t>;
            fn into_wrappable_iter(self) -> iter::Repeat<$t> {
                iter::repeat(self)
            }
        }
    )
}

#[cfg(feature = "with-syntex")]
#[allow(non_camel_case_types)]
type umax = u64;
#[cfg(not(feature = "with-syntex"))]
#[allow(non_camel_case_types)]
type umax = u128;

macro_rules! impl_to_tokens_int {
    (signed, $t:ty, $tag:expr) => (
        impl_wrap_repeat! { $t }
        impl ToTokens for $t {
            fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
                let val = if *self < 0 {
                    -self
                } else {
                    *self
                };
                let lit = ast::LitKind::Int(val as umax, ast::LitIntType::Signed($tag));
                dummy_spanned(lit).to_tokens(cx)
            }
        }
    );
    (unsigned, $t:ty, $tag:expr) => (
        impl_wrap_repeat! { $t }
        impl ToTokens for $t {
            fn to_tokens(&self, cx: &ExtCtxt) -> Vec<TokenTree> {
                let lit = ast::LitKind::Int(*self as umax, ast::LitIntType::Unsigned($tag));
                dummy_spanned(lit).to_tokens(cx)
            }
        }
    );
}

impl_to_tokens_int! { signed, isize, ast::IntTy::Is }
impl_to_tokens_int! { signed, i8,  ast::IntTy::I8 }
impl_to_tokens_int! { signed, i16, ast::IntTy::I16 }
impl_to_tokens_int! { signed, i32, ast::IntTy::I32 }
impl_to_tokens_int! { signed, i64, ast::IntTy::I64 }

impl_to_tokens_int! { unsigned, usize, ast::UintTy::Us }
impl_to_tokens_int! { unsigned, u8,   ast::UintTy::U8 }
impl_to_tokens_int! { unsigned, u16,  ast::UintTy::U16 }
impl_to_tokens_int! { unsigned, u32,  ast::UintTy::U32 }
impl_to_tokens_int! { unsigned, u64,  ast::UintTy::U64 }

pub trait ExtParseUtils {
    fn parse_item(&self, s: String) -> P<ast::Item>;
    fn parse_expr(&self, s: String) -> P<ast::Expr>;
    fn parse_stmt(&self, s: String) -> ast::Stmt;
    fn parse_tts(&self, s: String) -> Vec<TokenTree>;
}

impl<'a> ExtParseUtils for ExtCtxt<'a> {

    fn parse_item(&self, s: String) -> P<ast::Item> {
        panictry!(parse::parse_item_from_source_str(
            "<quote expansion>".to_string(),
            s,
            self.parse_sess())).expect("parse error")
    }

    fn parse_stmt(&self, s: String) -> ast::Stmt {
        panictry!(parse::parse_stmt_from_source_str(
            "<quote expansion>".to_string(),
            s,
            self.parse_sess())).expect("parse error")
    }

    fn parse_expr(&self, s: String) -> P<ast::Expr> {
        panictry!(parse::parse_expr_from_source_str(
            "<quote expansion>".to_string(),
            s,
            self.parse_sess()))
    }

    fn parse_tts(&self, s: String) -> Vec<TokenTree> {
        panictry!(parse::parse_tts_from_source_str(
            "<quote expansion>".to_string(),
            s,
            self.parse_sess()))
    }
}

pub fn parse_expr_panic(parser: &mut Parser) -> P<ast::Expr> {
        panictry!(parser.parse_expr())
}

pub fn parse_item_panic(parser: &mut Parser) -> Option<P<ast::Item>> {
        panictry!(parser.parse_item())
}

pub fn parse_pat_panic(parser: &mut Parser) -> P<ast::Pat> {
        panictry!(parser.parse_pat())
}

pub fn parse_arm_panic(parser: &mut Parser) -> ast::Arm {
        panictry!(parser.parse_arm())
}

pub fn parse_ty_panic(parser: &mut Parser) -> P<ast::Ty> {
        panictry!(parser.parse_ty())
}

pub fn parse_stmt_panic(parser: &mut Parser) -> Option<ast::Stmt> {
    panictry!(parser.parse_stmt())
}

pub fn parse_attribute_panic(parser: &mut Parser, permit_inner: bool) -> ast::Attribute {
        panictry!(parser.parse_attribute(permit_inner))
}

pub struct IterWrapper<I> {
    inner: I,
    require_nonempty: bool,
    is_repeat: bool,
}

impl<I> IterWrapper<I> where I: Iterator {
    pub fn zip_wrap<J>(self, other: IterWrapper<J>) -> IterWrapper<ZipLockstep<I, J>> {
        IterWrapper {
            inner: ZipLockstep {
                a: self.inner,
                b: other.inner,
                contains_repeat: self.is_repeat || other.is_repeat,
            },
            require_nonempty: false,
            is_repeat: self.is_repeat && other.is_repeat,
        }
    }

    pub fn check(mut self, one_or_more: bool) -> Self {
        // This check prevents running out of memory.
        assert!(self.size_hint() != (usize::MAX, None),
                "unbounded repetition in quasiquotation");
        self.require_nonempty = one_or_more;
        self
    }
}

impl<T, I: Iterator<Item=T>> Iterator for IterWrapper<I> {
    type Item = T;
    fn next(&mut self) -> Option<T> {
        match self.inner.next() {
            Some(elem) => {
                self.require_nonempty = false;
                Some(elem)
            }
            None => {
                assert!(!self.require_nonempty,
                        "a fragment must repeat at least once in quasiquotation");
                None
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}

pub struct ZipLockstep<A, B> {
    a: A,
    b: B,
    contains_repeat: bool,
}

impl<A, B> Iterator for ZipLockstep<A, B>
    where A: Iterator,
          B: Iterator,
{
    type Item = (A::Item, B::Item);
    fn next(&mut self) -> Option<Self::Item> {
        match (self.a.next(), self.b.next()) {
            (Some(left), Some(right)) => Some((left, right)),
            (None, None) => None,
            _ => {
                if self.contains_repeat {
                    None
                } else {
                    panic!("incorrect lockstep iteration in quasiquotation");
                }
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        struct SizeHint<A>((usize, Option<usize>), marker::PhantomData<A>);
        impl<A> Iterator for SizeHint<A> {
            type Item = A;
            fn next(&mut self) -> Option<A> { None }
            fn size_hint(&self) -> (usize, Option<usize>) {
                ((self.0).0, (self.0).1)
            }
        }
        let a_size_hint = SizeHint(self.a.size_hint(), marker::PhantomData::<()>);
        let b_size_hint = SizeHint(self.b.size_hint(), marker::PhantomData::<()>);
        a_size_hint.zip(b_size_hint).size_hint()
    }
}

// This is a workaround for trait coherence.
// Both traits have almost identical methods. We can generate code that calls
// `into_wrapper_iter` on a value that should implement one of these traits,
// without knowing which.

pub trait IntoWrappedIterator: Sized {
    type Item;
    type IntoIter: Iterator<Item=Self::Item>;
    fn into_wrappable_iter(self) -> Self::IntoIter;
    fn into_wrapped_iter(self) -> IterWrapper<Self::IntoIter> {
        IterWrapper {
            inner: self.into_wrappable_iter(),
            require_nonempty: false,
            is_repeat: false,
        }
    }
}

pub trait IntoWrappedRepeat: Sized {
    type Item;
    type IntoIter: Iterator<Item=Self::Item>;
    fn into_wrappable_iter(self) -> Self::IntoIter;
    fn into_wrapped_iter(self) -> IterWrapper<Self::IntoIter> {
        IterWrapper {
            inner: self.into_wrappable_iter(),
            require_nonempty: false,
            // The iterator repeats an element.
            is_repeat: true,
        }
    }
}

// Can't write a blanket impl for IntoIterator, because P<T> impls
// IntoIterator, and P<T> may already impl IntoWrappedRepeat.
impl<I: Iterator> IntoWrappedIterator for I {
    type Item = I::Item;
    type IntoIter = I;
    fn into_wrappable_iter(self) -> I {
        self
    }
}

impl<T: Clone> IntoWrappedRepeat for Spanned<T> {
    type Item = Spanned<T>;
    type IntoIter = iter::Repeat<Spanned<T>>;
    fn into_wrappable_iter(self) -> iter::Repeat<Spanned<T>> {
        iter::repeat(self)
    }
}

impl_wrap_repeat! { TokenTree }
impl_wrap_repeat! { ast::Ident }
impl_wrap_repeat! { ast::Path }
impl_wrap_repeat! { ast::Ty }
impl_wrap_repeat! { P<ast::Ty> }
impl_wrap_repeat! { P<ast::Block> }
impl_wrap_repeat! { P<ast::Item> }
impl_wrap_repeat! { P<ast::ImplItem> }
impl_wrap_repeat! { P<ast::TraitItem> }
impl_wrap_repeat! { ast::Generics }
impl_wrap_repeat! { ast::WhereClause }
impl_wrap_repeat! { ast::StmtKind }
impl_wrap_repeat! { P<ast::Expr> }
impl_wrap_repeat! { P<ast::Pat> }
impl_wrap_repeat! { ast::Arm }
impl_wrap_repeat! { P<ast::MetaItem> }
impl_wrap_repeat! { ast::Attribute }
impl_wrap_repeat! { () }
impl_wrap_repeat! { ast::LitKind }
impl_wrap_repeat! { bool }
impl_wrap_repeat! { char }
