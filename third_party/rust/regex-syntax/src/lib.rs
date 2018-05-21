// Copyright 2018 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/*!
This crate provides a robust regular expression parser.

This crate defines two primary types:

* [`Ast`](ast/enum.Ast.html) is the abstract syntax of a regular expression.
  An abstract syntax corresponds to a *structured representation* of the
  concrete syntax of a regular expression, where the concrete syntax is the
  pattern string itself (e.g., `foo(bar)+`). Given some abstract syntax, it
  can be converted back to the original concrete syntax (modulo some details,
  like whitespace). To a first approximation, the abstract syntax is complex
  and difficult to analyze.
* [`Hir`](hir/struct.Hir.html) is the high-level intermediate representation
  ("HIR" or "high-level IR" for short) of regular expression. It corresponds to
  an intermediate state of a regular expression that sits between the abstract
  syntax and the low level compiled opcodes that are eventually responsible for
  executing a regular expression search. Given some high-level IR, it is not
  possible to produce the original concrete syntax (although it is possible to
  produce an equivalent conrete syntax, but it will likely scarcely resemble
  the original pattern). To a first approximation, the high-level IR is simple
  and easy to analyze.

These two types come with conversion routines:

* An [`ast::parse::Parser`](ast/parse/struct.Parser.html) converts concrete
  syntax (a `&str`) to an [`Ast`](ast/enum.Ast.html).
* A [`hir::translate::Translator`](hir/translate/struct.Translator.html)
  converts an [`Ast`](ast/enum.Ast.html) to a [`Hir`](hir/struct.Hir.html).

As a convenience, the above two conversion routines are combined into one via
the top-level [`Parser`](struct.Parser.html) type. This `Parser` will first
convert your pattern to an `Ast` and then convert the `Ast` to an `Hir`.


# Example

This example shows how to parse a pattern string into its HIR:

```
use regex_syntax::Parser;
use regex_syntax::hir::{self, Hir};

let hir = Parser::new().parse("a|b").unwrap();
assert_eq!(hir, Hir::alternation(vec![
    Hir::literal(hir::Literal::Unicode('a')),
    Hir::literal(hir::Literal::Unicode('b')),
]));
```


# Concrete syntax supported

The concrete syntax is documented as part of the public API of the
[`regex` crate](https://docs.rs/regex/%2A/regex/#syntax).


# Input safety

A key feature of this library is that it is safe to use with end user facing
input. This plays a significant role in the internal implementation. In
particular:

1. Parsers provide a `nest_limit` option that permits callers to control how
   deeply nested a regular expression is allowed to be. This makes it possible
   to do case analysis over an `Ast` or an `Hir` using recursion without
   worrying about stack overflow.
2. Since relying on a particular stack size is brittle, this crate goes to
   great lengths to ensure that all interactions with both the `Ast` and the
   `Hir` do not use recursion. Namely, they use constant stack space and heap
   space proportional to the size of the original pattern string (in bytes).
   This includes the type's corresponding destructors. (One exception to this
   is literal extraction, but this will eventually get fixed.)


# Error reporting

The `Display` implementations on all `Error` types exposed in this library
provide nice human readable errors that are suitable for showing to end users
in a monospace font.


# Literal extraction

This crate provides limited support for
[literal extraction from `Hir` values](hir/literal/struct.Literals.html).
Be warned that literal extraction currently uses recursion, and therefore,
stack size proportional to the size of the `Hir`.

The purpose of literal extraction is to speed up searches. That is, if you
know a regular expression must match a prefix or suffix literal, then it is
often quicker to search for instances of that literal, and then confirm or deny
the match using the full regular expression engine. These optimizations are
done automatically in the `regex` crate.
*/

#![deny(missing_docs)]

extern crate ucd_util;

pub use error::{Error, Result};
pub use parser::{Parser, ParserBuilder};

pub mod ast;
mod either;
mod error;
pub mod hir;
mod parser;
mod unicode;
mod unicode_tables;

/// Escapes all regular expression meta characters in `text`.
///
/// The string returned may be safely used as a literal in a regular
/// expression.
pub fn escape(text: &str) -> String {
    let mut quoted = String::with_capacity(text.len());
    escape_into(text, &mut quoted);
    quoted
}

/// Escapes all meta characters in `text` and writes the result into `buf`.
///
/// This will append escape characters into the given buffer. The characters
/// that are appended are safe to use as a literal in a regular expression.
pub fn escape_into(text: &str, buf: &mut String) {
    for c in text.chars() {
        if is_meta_character(c) {
            buf.push('\\');
        }
        buf.push(c);
    }
}

/// Returns true if the give character has significance in a regex.
///
/// These are the only characters that are allowed to be escaped, with one
/// exception: an ASCII space character may be escaped when extended mode (with
/// the `x` flag) is enabld. In particular, `is_meta_character(' ')` returns
/// `false`.
///
/// Note that the set of characters for which this function returns `true` or
/// `false` is fixed and won't change in a semver compatible release.
pub fn is_meta_character(c: char) -> bool {
    match c {
        '\\' | '.' | '+' | '*' | '?' | '(' | ')' | '|' |
        '[' | ']' | '{' | '}' | '^' | '$' | '#' | '&' | '-' | '~' => true,
        _ => false,
    }
}

/// Returns true if and only if the given character is a Unicode word
/// character.
///
/// A Unicode word character is defined by
/// [UTS#18 Annex C](http://unicode.org/reports/tr18/#Compatibility_Properties).
/// In particular, a character
/// is considered a word character if it is in either of the `Alphabetic` or
/// `Join_Control` properties, or is in one of the `Decimal_Number`, `Mark`
/// or `Connector_Punctuation` general categories.
pub fn is_word_character(c: char) -> bool {
    use std::cmp::Ordering;
    use unicode_tables::perl_word::PERL_WORD;

    if c <= 0x7F as char && is_word_byte(c as u8) {
        return true;
    }
    PERL_WORD
        .binary_search_by(|&(start, end)| {
            if start <= c && c <= end {
                Ordering::Equal
            } else if start > c {
                Ordering::Greater
            } else {
                Ordering::Less
            }
        }).is_ok()
}

/// Returns true if and only if the given character is an ASCII word character.
///
/// An ASCII word character is defined by the following character class:
/// `[_0-9a-zA-Z]'.
pub fn is_word_byte(c: u8) -> bool {
    match c {
        b'_' | b'0' ... b'9' | b'a' ... b'z' | b'A' ... b'Z'  => true,
        _ => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn escape_meta() {
        assert_eq!(
            escape(r"\.+*?()|[]{}^$#&-~"),
            r"\\\.\+\*\?\(\)\|\[\]\{\}\^\$\#\&\-\~".to_string());
    }

    #[test]
    fn word() {
        assert!(is_word_byte(b'a'));
        assert!(!is_word_byte(b'-'));

        assert!(is_word_character('a'));
        assert!(is_word_character('β'));
        assert!(!is_word_character('-'));
        assert!(!is_word_character('☃'));
    }
}
