// Copyright 2012 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(not(feature = "with-syntex"), feature(rustc_private))]
#![cfg_attr(feature = "unstable-testing", feature(plugin))]
#![cfg_attr(feature = "unstable-testing", plugin(clippy))]

extern crate aster;

#[cfg(feature = "with-syntex")]
extern crate syntex;
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
#[cfg(not(feature = "with-syntex"))]
extern crate rustc_plugin;

#[cfg(feature = "with-syntex")]
use std::path::Path;

use syntax::ast;
use syntax::codemap::{Span, respan};
use syntax::ext::base::ExtCtxt;
use syntax::ext::base;
use syntax::parse::token::*;
use syntax::parse::token;
use syntax::ptr::P;
use syntax::symbol::Symbol;
use syntax::tokenstream::{self, TokenTree};

///  Quasiquoting works via token trees.
///
///  This is registered as a set of expression syntax extension called quote!
///  that lifts its argument token-tree to an AST representing the
///  construction of the same token tree, with `token::SubstNt` interpreted
///  as antiquotes (splices).

fn expand_quote_tokens<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree],
) -> Box<base::MacResult + 'cx> {
    let (cx_expr, expr) = expand_tts(cx, sp, tts);
    let expanded = expand_wrapper(sp, cx_expr, expr);
    base::MacEager::expr(expanded)
}

fn expand_quote_ty<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let expanded = expand_parse_call(
        cx,
        sp,
        &["quasi", "parse_ty_panic"],
        vec!(),
        tts);
    base::MacEager::expr(expanded)
}

fn expand_quote_expr<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let expanded = expand_parse_call(
        cx,
        sp,
        &["quasi", "parse_expr_panic"],
        Vec::new(),
        tts);
    base::MacEager::expr(expanded)
}

fn expand_quote_stmt<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let expanded = expand_parse_call(
        cx,
        sp,
        &["quasi", "parse_stmt_panic"],
        vec!(),
        tts);
    base::MacEager::expr(expanded)
}

fn expand_quote_attr<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let builder = aster::AstBuilder::new().span(sp);

    let expanded = expand_parse_call(
        cx,
        sp,
        &["quasi", "parse_attribute_panic"],
        vec![builder.expr().bool(true)],
        tts);

    base::MacEager::expr(expanded)
}

fn expand_quote_matcher<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let builder = aster::AstBuilder::new().span(sp);

    let (cx_expr, tts) = parse_arguments_to_quote(cx, tts);
    let mut vector = mk_stmts_let(&builder);
    match statements_mk_tts(&tts[..], true) {
        Ok(stmts) => vector.extend(stmts.stmts.into_iter()),
        Err(_) => cx.span_fatal(sp, "attempted to repeat an expression containing \
                                     no syntax variables matched as repeating at this depth"),
    }

    let block = builder.expr().block()
        .with_stmts(vector)
        .expr().id("tt");

    let expanded = expand_wrapper(sp, cx_expr, block);
    base::MacEager::expr(expanded)
}

fn expand_quote_pat<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let expanded = expand_parse_call(
        cx,
        sp,
        &["quasi", "parse_pat_panic"],
        vec!(),
        tts);
    base::MacEager::expr(expanded)
}

fn expand_quote_arm<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let expanded = expand_parse_call(
        cx,
        sp,
        &["quasi", "parse_arm_panic"],
        vec!(),
        tts);
    base::MacEager::expr(expanded)
}

fn expand_quote_block<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let expanded = expand_parse_call(
        cx,
        sp,
        &["syntax", "parse", "parser", "Parser", "parse_block"],
        Vec::new(),
        tts);
    base::MacEager::expr(expanded)
}

fn expand_quote_item<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let expanded = expand_parse_call(
        cx,
        sp,
        &["quasi", "parse_item_panic"],
        vec!(),
        tts);
    base::MacEager::expr(expanded)
}

fn expand_quote_impl_item<'cx>(
    cx: &'cx mut ExtCtxt,
    sp: Span,
    tts: &[TokenTree]
) -> Box<base::MacResult + 'cx> {
    let expanded = expand_parse_call(
        cx,
        sp,
        &["syntax", "parse", "parser", "Parser", "parse_impl_item"],
        vec!(),
        tts);
    base::MacEager::expr(expanded)
}

/*
fn expand_quote_where_clause<'cx>(cx: &mut ExtCtxt,
                                      sp: Span,
                                      tts: &[TokenTree])
                                      -> Box<base::MacResult+'cx> {
    let expanded = expand_parse_call(cx, sp, "parse_where_clause",
                                    vec!(), tts);
    base::MacEager::expr(expanded)
}
*/

// Lift an ident to the expr that evaluates to that ident.
fn mk_ident(builder: &aster::AstBuilder, ident: ast::Ident) -> P<ast::Expr> {
    builder.expr().method_call("ident_of").id("ext_cx")
        .arg().str(ident)
        .build()
}

// Lift a name to the expr that evaluates to that name
fn mk_name<T>(builder: &aster::AstBuilder, name: T) -> P<ast::Expr>
    where T: aster::symbol::ToSymbol
{
    builder.expr().method_call("name_of").id("ext_cx")
        .arg().str(name)
        .build()
}

fn mk_tt_path(builder: &aster::AstBuilder, name: &str) -> P<ast::Expr> {
    builder.expr().path()
        .global()
        .ids(&["syntax", "tokenstream", "TokenTree", name])
        .build()
}

fn mk_token_path(builder: &aster::AstBuilder, name: &str) -> P<ast::Expr> {
    builder.expr().path()
        .global()
        .ids(&["syntax", "parse", "token", name])
        .build()
}

fn mk_binop(builder: &aster::AstBuilder, bop: token::BinOpToken) -> P<ast::Expr> {
    let name = match bop {
        token::Plus     => "Plus",
        token::Minus    => "Minus",
        token::Star     => "Star",
        token::Slash    => "Slash",
        token::Percent  => "Percent",
        token::Caret    => "Caret",
        token::And      => "And",
        token::Or       => "Or",
        token::Shl      => "Shl",
        token::Shr      => "Shr"
    };
    mk_token_path(builder, name)
}

fn mk_delim(builder: &aster::AstBuilder, delim: token::DelimToken) -> P<ast::Expr> {
    let name = match delim {
        token::Paren     => "Paren",
        token::Bracket   => "Bracket",
        token::Brace     => "Brace",
        token::NoDelim   => "NoDelim",
    };
    mk_token_path(builder, name)
}

#[allow(non_upper_case_globals)]
fn expr_mk_token(builder: &aster::AstBuilder, tok: &token::Token) -> P<ast::Expr> {
    macro_rules! mk_lit {
        ($name: expr, $suffix: expr, $($args: expr),+) => {{
            let inner = builder.expr().call()
                .build(mk_token_path(builder, $name))
                $(.with_arg($args))+
                .build();

            let suffix = match $suffix {
                Some(name) => builder.expr().some().build(mk_name(builder, name)),
                None => builder.expr().none(),
            };

            builder.expr().call()
                .build(mk_token_path(builder, "Literal"))
                .with_arg(inner)
                .with_arg(suffix)
                .build()
        }}
    }

    match *tok {
        token::BinOp(binop) => {
            builder.expr().call()
                .build(mk_token_path(builder, "BinOp"))
                .with_arg(mk_binop(builder, binop))
                .build()
        }
        token::BinOpEq(binop) => {
            builder.expr().call()
                .build(mk_token_path(builder, "BinOpEq"))
                .with_arg(mk_binop(builder, binop))
                .build()
        }

        token::OpenDelim(delim) => {
            builder.expr().call()
                .build(mk_token_path(builder, "OpenDelim"))
                .with_arg(mk_delim(builder, delim))
                .build()
        }
        token::CloseDelim(delim) => {
            builder.expr().call()
                .build(mk_token_path(builder, "CloseDelim"))
                .with_arg(mk_delim(builder, delim))
                .build()
        }

        token::Literal(token::Byte(i), suf) => {
            let e_byte = mk_name(builder, i.as_str());
            mk_lit!("Byte", suf, e_byte)
        }

        token::Literal(token::Char(i), suf) => {
            let e_char = mk_name(builder, i.as_str());
            mk_lit!("Char", suf, e_char)
        }

        token::Literal(token::Integer(i), suf) => {
            let e_int = mk_name(builder, i.as_str());
            mk_lit!("Integer", suf, e_int)
        }

        token::Literal(token::Float(fident), suf) => {
            let e_fident = mk_name(builder, fident.as_str());
            mk_lit!("Float", suf, e_fident)
        }

        token::Literal(token::ByteStr(ident), suf) => {
            mk_lit!("ByteStr", suf, mk_name(builder, ident.as_str()))
        }

        token::Literal(token::ByteStrRaw(ident, n), suf) => {
            mk_lit!(
                "ByteStrRaw",
                suf,
                mk_name(builder, ident.as_str()),
                builder.expr().usize(n))
        }

        token::Literal(token::Str_(ident), suf) => {
            mk_lit!("Str_", suf, mk_name(builder, ident.as_str()))
        }

        token::Literal(token::StrRaw(ident, n), suf) => {
            mk_lit!(
                "StrRaw",
                suf,
                mk_name(builder, ident.as_str()),
                builder.expr().usize(n))
        }

        token::Ident(ident) => {
            builder.expr().call()
                .build(mk_token_path(builder, "Ident"))
                .with_arg(mk_ident(builder, ident))
                .build()
        }

        token::Lifetime(ident) => {
            builder.expr().call()
                .build(mk_token_path(builder, "Lifetime"))
                .with_arg(mk_ident(builder, ident))
                .build()
        }

        token::DocComment(ident) => {
            builder.expr().call()
                .build(mk_token_path(builder, "DocComment"))
                .with_arg(mk_name(builder, ident))
                .build()
        }

        token::Eq           => mk_token_path(builder, "Eq"),
        token::Lt           => mk_token_path(builder, "Lt"),
        token::Le           => mk_token_path(builder, "Le"),
        token::EqEq         => mk_token_path(builder, "EqEq"),
        token::Ne           => mk_token_path(builder, "Ne"),
        token::Ge           => mk_token_path(builder, "Ge"),
        token::Gt           => mk_token_path(builder, "Gt"),
        token::AndAnd       => mk_token_path(builder, "AndAnd"),
        token::OrOr         => mk_token_path(builder, "OrOr"),
        token::Not          => mk_token_path(builder, "Not"),
        token::Tilde        => mk_token_path(builder, "Tilde"),
        token::At           => mk_token_path(builder, "At"),
        token::Dot          => mk_token_path(builder, "Dot"),
        token::DotDot       => mk_token_path(builder, "DotDot"),
        token::Comma        => mk_token_path(builder, "Comma"),
        token::Semi         => mk_token_path(builder, "Semi"),
        token::Colon        => mk_token_path(builder, "Colon"),
        token::ModSep       => mk_token_path(builder, "ModSep"),
        token::RArrow       => mk_token_path(builder, "RArrow"),
        token::LArrow       => mk_token_path(builder, "LArrow"),
        token::FatArrow     => mk_token_path(builder, "FatArrow"),
        token::Pound        => mk_token_path(builder, "Pound"),
        token::Dollar       => mk_token_path(builder, "Dollar"),
        token::Underscore   => mk_token_path(builder, "Underscore"),
        token::Eof          => mk_token_path(builder, "Eof"),
        token::DotDotDot    => mk_token_path(builder, "DotDotDot"),
        token::Question     => mk_token_path(builder, "Question"),
        token::Whitespace   => mk_token_path(builder, "Whitespace"),
        token::Comment      => mk_token_path(builder, "Comment"),

        token::Shebang(s) => {
            builder.expr().call()
                .build(mk_token_path(builder, "Shebang"))
                .arg().str(s)
                .build()
        }

        token::MatchNt(name, kind) => {
            builder.expr().call()
                .build(mk_token_path(builder, "MatchNt"))
                .arg().build(mk_ident(builder, name))
                .arg().build(mk_ident(builder, kind))
                .build()
        }

        token::Interpolated(..)
        | token::SubstNt(..) => {
            panic!("quote! with {:?} token", tok)
        }
    }
}

struct QuoteStmts {
    stmts: Vec<ast::Stmt>,
    idents: Vec<ast::SpannedIdent>,
}

fn statements_mk_tt(tt: &TokenTree, matcher: bool) -> Result<QuoteStmts, ()> {
    let builder = aster::AstBuilder::new();

    match *tt {
        TokenTree::Token(sp, SubstNt(ident)) => {
            // tt.extend($ident.to_tokens(ext_cx).into_iter())

            let builder = builder.span(sp);

            let to_tokens = builder.path()
                .global()
                .ids(&["quasi", "ToTokens", "to_tokens"])
                .build();

            let e_to_toks = builder.expr().call()
                .build_path(to_tokens)
                .arg().ref_().id(ident)
                .arg().id("ext_cx")
                .build();

            let e_to_toks = builder.expr().method_call("into_iter")
                .build(e_to_toks)
                .build();

            let e_push = builder.expr().method_call("extend")
                .id("tt")
                .with_arg(e_to_toks)
                .build();

            Ok(QuoteStmts {
                stmts: vec![builder.stmt().build_expr(e_push)],
                idents: vec![respan(sp, ident)],
            })
        }
        ref tt @ TokenTree::Token(_, MatchNt(..)) if !matcher => {
            let mut seq = vec![];
            for i in 0..tt.len() {
                seq.push(tt.get_tt(i));
            }
            statements_mk_tts(&seq[..], matcher)
        }
        TokenTree::Token(sp, ref tok) => {
            let builder = builder.span(sp);

            let e_tok = builder.expr().call()
                .build(mk_tt_path(&builder, "Token"))
                .arg().id("_sp")
                .with_arg(expr_mk_token(&builder, tok))
                .build();

            let e_push = builder.expr().method_call("push")
                .id("tt")
                .with_arg(e_tok)
                .build();

            Ok(QuoteStmts {
                stmts: vec![builder.stmt().build_expr(e_push)],
                idents: vec![],
            })
        },
        TokenTree::Delimited(sp, ref delimed) => {
            let delimited = try!(statements_mk_tts(&delimed.tts[..], matcher));
            let open = try!(statements_mk_tt(&delimed.open_tt(sp), matcher)).stmts.into_iter();
            let close = try!(statements_mk_tt(&delimed.close_tt(sp), matcher)).stmts.into_iter();
            Ok(QuoteStmts {
                stmts: open.chain(delimited.stmts.into_iter()).chain(close).collect(),
                idents: delimited.idents,
            })
        },
        TokenTree::Sequence(sp, ref seq) if matcher => {
            let builder = builder.span(sp);

            let e_sp = builder.expr().id("_sp");

            let stmt_let_tt = builder.stmt().let_()
                .mut_id("tt")
                .expr().vec().build();

            let mut tts_stmts = vec![stmt_let_tt];
            tts_stmts.extend(try!(statements_mk_tts(&seq.tts[..], matcher)).stmts.into_iter());

            let e_tts = builder.expr().block()
                .with_stmts(tts_stmts)
                .expr().id("tt");

            let e_separator = match seq.separator {
                Some(ref sep) => builder.expr().some().build(expr_mk_token(&builder, sep)),
                None => builder.expr().none(),
            };

            let e_op = match seq.op {
                tokenstream::KleeneOp::ZeroOrMore => "ZeroOrMore",
                tokenstream::KleeneOp::OneOrMore => "OneOrMore",
            };
            let e_op = builder.expr().path()
                .global()
                .ids(&["syntax", "tokenstream", "KleeneOp", e_op])
                .build();

            let e_seq_struct = builder.expr().struct_()
                .global().ids(&["syntax", "tokenstream", "SequenceRepetition"]).build()
                .field("tts").build(e_tts)
                .field("separator").build(e_separator)
                .field("op").build(e_op)
                .field("num_captures").usize(seq.num_captures)
                .build();

            let e_rc_new = builder.expr().rc()
                .build(e_seq_struct);

            let e_tok = builder.expr().call()
                .build(mk_tt_path(&builder, "Sequence"))
                .arg().build(e_sp)
                .arg().build(e_rc_new)
                .build();

            let e_push = builder.expr().method_call("push").id("tt")
                .arg().build(e_tok)
                .build();

            Ok(QuoteStmts {
                stmts: vec![builder.stmt().expr().build(e_push)],
                idents: vec![],
            })
        }
        TokenTree::Sequence(sp, ref seq) => {
            // Repeating fragments in a loop:
            // for (...(a, b), ...) in a.into_wrapped_iter()
            //                          .zip_wrap(b.into_wrapped_iter())...
            //                          .check(true/false) {
            //     // (quasiquotation with $a, $b, ...)
            //}
            let QuoteStmts { mut stmts, idents } = try!(statements_mk_tts(&seq.tts[..], matcher));
            if idents.is_empty() {
                return Err(());
            }
            let builder = builder.span(sp);
            let one_or_more = builder.expr().bool(seq.op == tokenstream::KleeneOp::OneOrMore);
            let mut iter = idents.iter().cloned();
            let first = iter.next().unwrap();
            let mut zipped = builder.span(first.span).expr()
                .method_call("into_wrapped_iter")
                    .id(first.node)
                .build();
            let mut pat = builder.span(first.span).pat().id(first.node);
            for ident in iter {
                // Repeating calls to zip_wrap:
                // $zipped.zip_wrap($ident.into_wrapped_iter())
                zipped = builder.expr()
                    .method_call("zip_wrap")
                        .build(zipped)
                        .arg().span(ident.span).method_call("into_wrapped_iter")
                            .id(ident.node)
                        .build()
                    .build();
                let span = ident.span;
                pat = builder.pat().tuple().with_pat(pat).pat().span(span).id(ident.node).build();
            }
            // Assertion: zipped iterators must have at least one element
            // if one_or_more == `true`.
            zipped = builder.expr()
                .method_call("check")
                    .build(zipped)
                    .with_arg(one_or_more)
                .build();
            // Repetition can have a separator.
            if let Some(ref tok) = seq.separator {
                // Add the separator after each iteration.
                let sep_token = TokenTree::Token(sp, tok.clone());
                let mk_sep = try!(statements_mk_tt(&sep_token, false));
                stmts.extend(mk_sep.stmts.into_iter());
            }

            let stmt_for = builder.stmt().expr().build_expr_kind(
                ast::ExprKind::ForLoop(pat, zipped, builder.block().with_stmts(stmts).build(), None)
            );

            let stmts_for = if seq.separator.is_some() {
                // Pop the last occurence of the separator after the last iteration
                // only if there was at least one iteration (which changes the number of tokens).
                let tt_len = builder.expr().method_call("len").id("tt").build();
                let tt_len_id = Symbol::intern("tt_len");
                let cond = builder.expr().ne().build(tt_len.clone()).id(tt_len_id);
                let then = builder.block()
                        .stmt().semi().method_call("pop").id("tt").build()
                    .build();
                let if_len_eq = builder.expr().build_expr_kind(
                    ast::ExprKind::If(cond, then, None)
                );
                vec![
                    builder.stmt().let_id(tt_len_id).build_expr(tt_len),
                    stmt_for,
                    builder.stmt().build_expr(if_len_eq)]
            } else {
                vec![stmt_for]
            };

            Ok(QuoteStmts {
                stmts: stmts_for,
                idents: idents.clone(),
            })
        }
    }
}

fn parse_arguments_to_quote(cx: &ExtCtxt, tts: &[TokenTree])
                            -> (P<ast::Expr>, Vec<TokenTree>) {
    // NB: It appears that the main parser loses its mind if we consider
    // $foo as a SubstNt during the main parse, so we have to re-parse
    // under quote_depth > 0. This is silly and should go away; the _guess_ is
    // it has to do with transition away from supporting old-style macros, so
    // try removing it when enough of them are gone.

    let mut p = cx.new_parser_from_tts(tts);
    p.quote_depth += 1;

    let cx_expr = panictry!(p.parse_expr());

    if !p.eat(&token::Comma) {
        let _ = p.diagnostic().fatal("expected token `,`");
    }

    let tts = panictry!(p.parse_all_token_trees());
    p.abort_if_errors();

    (cx_expr, tts)
}

fn mk_stmts_let(builder: &aster::AstBuilder) -> Vec<ast::Stmt> {
    // We also bind a single value, sp, to ext_cx.call_site()
    //
    // This causes every span in a token-tree quote to be attributed to the
    // call site of the extension using the quote. We can't really do much
    // better since the source of the quote may well be in a library that
    // was not even parsed by this compilation run, that the user has no
    // source code for (eg. in libsyntax, which they're just _using_).
    //
    // The old quasiquoter had an elaborate mechanism for denoting input
    // file locations from which quotes originated; unfortunately this
    // relied on feeding the source string of the quote back into the
    // compiler (which we don't really want to do) and, in any case, only
    // pushed the problem a very small step further back: an error
    // resulting from a parse of the resulting quote is still attributed to
    // the site the string literal occurred, which was in a source file
    // _other_ than the one the user has control over. For example, an
    // error in a quote from the protocol compiler, invoked in user code
    // using macro_rules! for example, will be attributed to the macro_rules.rs
    // file in libsyntax, which the user might not even have source to (unless
    // they happen to have a compiler on hand). Over all, the phase distinction
    // just makes quotes "hard to attribute". Possibly this could be fixed
    // by recreating some of the original qq machinery in the tt regime
    // (pushing fake FileMaps onto the parser to account for original sites
    // of quotes, for example) but at this point it seems not likely to be
    // worth the hassle.

    let e_sp = builder.expr().method_call("call_site")
        .id("ext_cx")
        .build();

    let stmt_let_sp = builder.stmt()
        .let_id("_sp").build_expr(e_sp);

    let stmt_let_tt = builder.stmt().let_().mut_id("tt")
        .expr().call()
            .path().global().ids(&["std", "vec", "Vec", "new"]).build()
            .build();

    vec!(stmt_let_sp, stmt_let_tt)
}

fn statements_mk_tts(tts: &[TokenTree], matcher: bool) -> Result<QuoteStmts, ()> {
    let mut ret = QuoteStmts { stmts: vec![], idents: vec![] };
    for tt in tts {
        let QuoteStmts { stmts, idents } = try!(statements_mk_tt(tt, matcher));
        ret.stmts.extend(stmts.into_iter());
        ret.idents.extend(idents.into_iter());
    }
    Ok(ret)
}

fn expand_tts(cx: &ExtCtxt, sp: Span, tts: &[TokenTree])
              -> (P<ast::Expr>, P<ast::Expr>) {
    let builder = aster::AstBuilder::new().span(sp);

    let (cx_expr, tts) = parse_arguments_to_quote(cx, tts);

    let mut vector = mk_stmts_let(&builder);

    match statements_mk_tts(&tts[..], false) {
        Ok(stmts) => vector.extend(stmts.stmts.into_iter()),
        Err(_) => cx.span_fatal(sp, "attempted to repeat an expression containing \
                                     no syntax variables matched as repeating at this depth"),
    }

    let block = builder.expr().block()
        .stmt().item().attr().allow(&["unused_imports"])
                      .use_().ids(&["quasi", "IntoWrappedIterator"]).build().build()
        .stmt().item().attr().allow(&["unused_imports"])
                      .use_().ids(&["quasi", "IntoWrappedRepeat"]).build().build()
        .with_stmts(vector)
        .expr().id("tt");

    (cx_expr, block)
}

fn expand_wrapper(sp: Span,
                  cx_expr: P<ast::Expr>,
                  expr: P<ast::Expr>) -> P<ast::Expr> {
    let builder = aster::AstBuilder::new().span(sp);

    // Explicitly borrow to avoid moving from the invoker (#16992)
    let cx_expr_borrow = builder.expr()
        .ref_().build_deref(cx_expr);

    let stmt_let_ext_cx = builder.stmt().let_id("ext_cx")
        .build_expr(cx_expr_borrow);

    builder.expr().block()
        .with_stmt(stmt_let_ext_cx)
        .build_expr(expr)
}

fn expand_parse_call(cx: &ExtCtxt,
                     sp: Span,
                     parse_method: &[&str],
                     arg_exprs: Vec<P<ast::Expr>> ,
                     tts: &[TokenTree]) -> P<ast::Expr> {
    let builder = aster::AstBuilder::new().span(sp);

    let (cx_expr, tts_expr) = expand_tts(cx, sp, tts);

    let parse_sess_call = builder.expr().method_call("parse_sess")
        .id("ext_cx")
        .build();

    let new_parser_from_tts_path = builder.path()
        .global()
        .ids(&["syntax", "parse", "new_parser_from_tts"])
        .build();

    let new_parser_call = builder.expr().call()
        .build_path(new_parser_from_tts_path)
        .with_arg(parse_sess_call)
        .with_arg(tts_expr)
        .build();

    let parse_method_path = builder.path()
        .global()
        .ids(parse_method)
        .build();

    let expr = builder.expr().call()
        .build_path(parse_method_path)
        .arg().mut_ref().build(new_parser_call)
        .with_args(arg_exprs)
        .build();

    expand_wrapper(sp, cx_expr, expr)
}

#[cfg(feature = "with-syntex")]
pub fn expand<S, D>(src: S, dst: D) -> Result<(), syntex::Error>
    where S: AsRef<Path>,
          D: AsRef<Path>,
{
    let src = src.as_ref().to_owned();
    let dst = dst.as_ref().to_owned();

    syntex::with_extra_stack(move || {
        let mut registry = syntex::Registry::new();
        register(&mut registry);
        registry.expand("", src, dst)
    })
}

#[cfg(feature = "with-syntex")]
fn register(reg: &mut syntex::Registry) {
    reg.add_macro("quote_tokens", expand_quote_tokens);
    reg.add_macro("quote_ty", expand_quote_ty);
    reg.add_macro("quote_expr", expand_quote_expr);
    reg.add_macro("quote_matcher", expand_quote_matcher);
    reg.add_macro("quote_stmt", expand_quote_stmt);
    reg.add_macro("quote_attr", expand_quote_attr);
    reg.add_macro("quote_pat", expand_quote_pat);
    reg.add_macro("quote_arm", expand_quote_arm);
    reg.add_macro("quote_block", expand_quote_block);
    reg.add_macro("quote_item", expand_quote_item);
    reg.add_macro("quote_impl_item", expand_quote_impl_item);
    //reg.add_macro("quote_where_clause", expand_quote_where_clause);
}

#[cfg(not(feature = "syntex"))]
pub fn register(reg: &mut rustc_plugin::Registry) {
    reg.register_macro("quote_tokens", expand_quote_tokens);
    reg.register_macro("quote_ty", expand_quote_ty);
    reg.register_macro("quote_expr", expand_quote_expr);
    reg.register_macro("quote_matcher", expand_quote_matcher);
    reg.register_macro("quote_stmt", expand_quote_stmt);
    reg.register_macro("quote_attr", expand_quote_attr);
    reg.register_macro("quote_pat", expand_quote_pat);
    reg.register_macro("quote_arm", expand_quote_arm);
    reg.register_macro("quote_block", expand_quote_block);
    reg.register_macro("quote_item", expand_quote_item);
    reg.register_macro("quote_impl_item", expand_quote_impl_item);
    //reg.register_macro("quote_where_clause", expand_quote_where_clause);
}
