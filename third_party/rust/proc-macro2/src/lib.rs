//! A "shim crate" intended to multiplex the [`proc_macro`] API on to stable
//! Rust.
//!
//! Procedural macros in Rust operate over the upstream
//! [`proc_macro::TokenStream`][ts] type. This type currently is quite
//! conservative and exposed no internal implementation details. Nightly
//! compilers, however, contain a much richer interface. This richer interface
//! allows fine-grained inspection of the token stream which avoids
//! stringification/re-lexing and also preserves span information.
//!
//! The upcoming APIs added to [`proc_macro`] upstream are the foundation for
//! productive procedural macros in the ecosystem. To help prepare the ecosystem
//! for using them this crate serves to both compile on stable and nightly and
//! mirrors the API-to-be. The intention is that procedural macros which switch
//! to use this crate will be trivially able to switch to the upstream
//! `proc_macro` crate once its API stabilizes.
//!
//! In the meantime this crate also has a `nightly` Cargo feature which
//! enables it to reimplement itself with the unstable API of [`proc_macro`].
//! This'll allow immediate usage of the beneficial upstream API, particularly
//! around preserving span information.
//!
//! [`proc_macro`]: https://doc.rust-lang.org/proc_macro/
//! [ts]: https://doc.rust-lang.org/proc_macro/struct.TokenStream.html

// Proc-macro2 types in rustdoc of other crates get linked to here.
#![doc(html_root_url = "https://docs.rs/proc-macro2/0.2.3")]

#![cfg_attr(feature = "nightly", feature(proc_macro))]

#[cfg(feature = "proc-macro")]
extern crate proc_macro;

#[cfg(not(feature = "nightly"))]
extern crate unicode_xid;

use std::fmt;
use std::str::FromStr;
use std::iter::FromIterator;

#[macro_use]
#[cfg(not(feature = "nightly"))]
mod strnom;

#[path = "stable.rs"]
#[cfg(not(feature = "nightly"))]
mod imp;
#[path = "unstable.rs"]
#[cfg(feature = "nightly")]
mod imp;

#[macro_use]
mod macros;

#[derive(Clone)]
pub struct TokenStream(imp::TokenStream);

pub struct LexError(imp::LexError);

impl FromStr for TokenStream {
    type Err = LexError;

    fn from_str(src: &str) -> Result<TokenStream, LexError> {
        match src.parse() {
            Ok(e) => Ok(TokenStream(e)),
            Err(e) => Err(LexError(e)),
        }
    }
}

#[cfg(feature = "proc-macro")]
impl From<proc_macro::TokenStream> for TokenStream {
    fn from(inner: proc_macro::TokenStream) -> TokenStream {
        TokenStream(inner.into())
    }
}

#[cfg(feature = "proc-macro")]
impl From<TokenStream> for proc_macro::TokenStream {
    fn from(inner: TokenStream) -> proc_macro::TokenStream {
        inner.0.into()
    }
}

impl From<TokenTree> for TokenStream {
    fn from(tree: TokenTree) -> TokenStream {
        TokenStream(tree.into())
    }
}

impl<T: Into<TokenStream>> FromIterator<T> for TokenStream {
    fn from_iter<I: IntoIterator<Item = T>>(streams: I) -> Self {
        TokenStream(streams.into_iter().map(|t| t.into().0).collect())
    }
}

impl IntoIterator for TokenStream {
    type Item = TokenTree;
    type IntoIter = TokenTreeIter;

    fn into_iter(self) -> TokenTreeIter {
        TokenTreeIter(self.0.into_iter())
    }
}

impl TokenStream {
    pub fn empty() -> TokenStream {
        TokenStream(imp::TokenStream::empty())
    }

    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}

// Returned by reference, so we can't easily wrap it.
#[cfg(procmacro2_semver_exempt)]
pub use imp::FileName;

#[cfg(procmacro2_semver_exempt)]
#[derive(Clone, PartialEq, Eq)]
pub struct SourceFile(imp::SourceFile);

#[cfg(procmacro2_semver_exempt)]
impl SourceFile {
    /// Get the path to this source file as a string.
    pub fn path(&self) -> &FileName {
        self.0.path()
    }

    pub fn is_real(&self) -> bool {
        self.0.is_real()
    }
}

#[cfg(procmacro2_semver_exempt)]
impl AsRef<FileName> for SourceFile {
    fn as_ref(&self) -> &FileName {
        self.0.path()
    }
}

#[cfg(procmacro2_semver_exempt)]
impl fmt::Debug for SourceFile {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[cfg(procmacro2_semver_exempt)]
pub struct LineColumn {
    pub line: usize,
    pub column: usize,
}

#[derive(Copy, Clone)]
pub struct Span(imp::Span);

impl Span {
    pub fn call_site() -> Span {
        Span(imp::Span::call_site())
    }

    pub fn def_site() -> Span {
        Span(imp::Span::def_site())
    }

    /// Creates a new span with the same line/column information as `self` but
    /// that resolves symbols as though it were at `other`.
    pub fn resolved_at(&self, other: Span) -> Span {
        Span(self.0.resolved_at(other.0))
    }

    /// Creates a new span with the same name resolution behavior as `self` but
    /// with the line/column information of `other`.
    pub fn located_at(&self, other: Span) -> Span {
        Span(self.0.located_at(other.0))
    }

    /// This method is only available when the `"nightly"` feature is enabled.
    #[cfg(all(feature = "nightly", feature = "proc-macro"))]
    pub fn unstable(self) -> proc_macro::Span {
        self.0.unstable()
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn source_file(&self) -> SourceFile {
        SourceFile(self.0.source_file())
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn start(&self) -> LineColumn {
        let imp::LineColumn{ line, column } = self.0.start();
        LineColumn { line: line, column: column }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn end(&self) -> LineColumn {
        let imp::LineColumn{ line, column } = self.0.end();
        LineColumn { line: line, column: column }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn join(&self, other: Span) -> Option<Span> {
        self.0.join(other.0).map(Span)
    }
}

#[derive(Clone, Debug)]
pub struct TokenTree {
    pub span: Span,
    pub kind: TokenNode,
}

impl From<TokenNode> for TokenTree {
    fn from(kind: TokenNode) -> TokenTree {
        TokenTree { span: Span::def_site(), kind: kind }
    }
}

impl fmt::Display for TokenTree {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        TokenStream::from(self.clone()).fmt(f)
    }
}

#[derive(Clone, Debug)]
pub enum TokenNode {
    Group(Delimiter, TokenStream),
    Term(Term),
    Op(char, Spacing),
    Literal(Literal),
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Delimiter {
    Parenthesis,
    Brace,
    Bracket,
    None,
}

#[derive(Copy, Clone)]
pub struct Term(imp::Term);

impl Term {
    pub fn intern(string: &str) -> Term {
        Term(imp::Term::intern(string))
    }

    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Spacing {
    Alone,
    Joint,
}

#[derive(Clone)]
pub struct Literal(imp::Literal);

macro_rules! int_literals {
    ($($kind:ident,)*) => ($(
        pub fn $kind(n: $kind) -> Literal {
            Literal(n.into())
        }
    )*)
}

impl Literal {
    pub fn integer(s: i64) -> Literal {
        Literal(imp::Literal::integer(s))
    }

    int_literals! {
        u8, u16, u32, u64, usize,
        i8, i16, i32, i64, isize,
    }

    pub fn float(f: f64) -> Literal {
        Literal(imp::Literal::float(f))
    }

    pub fn f64(f: f64) -> Literal {
        Literal(f.into())
    }

    pub fn f32(f: f32) -> Literal {
        Literal(f.into())
    }

    pub fn string(string: &str) -> Literal {
        Literal(string.into())
    }

    pub fn character(ch: char) -> Literal {
        Literal(ch.into())
    }

    pub fn byte_string(s: &[u8]) -> Literal {
        Literal(imp::Literal::byte_string(s))
    }

    // =======================================================================
    // Not present upstream in proc_macro yet

    pub fn byte_char(b: u8) -> Literal {
        Literal(imp::Literal::byte_char(b))
    }

    pub fn doccomment(s: &str) -> Literal {
        Literal(imp::Literal::doccomment(s))
    }

    pub fn raw_string(s: &str, pounds: usize) -> Literal {
        Literal(imp::Literal::raw_string(s, pounds))
    }

    pub fn raw_byte_string(s: &str, pounds: usize) -> Literal {
        Literal(imp::Literal::raw_byte_string(s, pounds))
    }
}

pub struct TokenTreeIter(imp::TokenTreeIter);

impl Iterator for TokenTreeIter {
    type Item = TokenTree;

    fn next(&mut self) -> Option<TokenTree> {
        self.0.next()
    }
}

forward_fmt!(Debug for LexError);
forward_fmt!(Debug for Literal);
forward_fmt!(Debug for Span);
forward_fmt!(Debug for Term);
forward_fmt!(Debug for TokenTreeIter);
forward_fmt!(Debug for TokenStream);
forward_fmt!(Display for Literal);
forward_fmt!(Display for TokenStream);
