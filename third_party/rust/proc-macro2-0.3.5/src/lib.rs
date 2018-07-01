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
#![doc(html_root_url = "https://docs.rs/proc-macro2/0.3.5")]
#![cfg_attr(feature = "nightly", feature(proc_macro))]

#[cfg(feature = "proc-macro")]
extern crate proc_macro;

#[cfg(not(feature = "nightly"))]
extern crate unicode_xid;

use std::fmt;
use std::iter::FromIterator;
use std::marker;
use std::rc::Rc;
use std::str::FromStr;

#[macro_use]
#[cfg(not(feature = "nightly"))]
mod strnom;

#[path = "stable.rs"]
#[cfg(not(feature = "nightly"))]
mod imp;
#[path = "unstable.rs"]
#[cfg(feature = "nightly")]
mod imp;

#[derive(Clone)]
pub struct TokenStream {
    inner: imp::TokenStream,
    _marker: marker::PhantomData<Rc<()>>,
}

pub struct LexError {
    inner: imp::LexError,
    _marker: marker::PhantomData<Rc<()>>,
}

impl TokenStream {
    fn _new(inner: imp::TokenStream) -> TokenStream {
        TokenStream {
            inner: inner,
            _marker: marker::PhantomData,
        }
    }

    pub fn empty() -> TokenStream {
        TokenStream::_new(imp::TokenStream::empty())
    }

    pub fn is_empty(&self) -> bool {
        self.inner.is_empty()
    }
}

impl FromStr for TokenStream {
    type Err = LexError;

    fn from_str(src: &str) -> Result<TokenStream, LexError> {
        let e = src.parse().map_err(|e| LexError {
            inner: e,
            _marker: marker::PhantomData,
        })?;
        Ok(TokenStream::_new(e))
    }
}

#[cfg(feature = "proc-macro")]
impl From<proc_macro::TokenStream> for TokenStream {
    fn from(inner: proc_macro::TokenStream) -> TokenStream {
        TokenStream::_new(inner.into())
    }
}

#[cfg(feature = "proc-macro")]
impl From<TokenStream> for proc_macro::TokenStream {
    fn from(inner: TokenStream) -> proc_macro::TokenStream {
        inner.inner.into()
    }
}

impl FromIterator<TokenTree> for TokenStream {
    fn from_iter<I: IntoIterator<Item = TokenTree>>(streams: I) -> Self {
        TokenStream::_new(streams.into_iter().collect())
    }
}

impl fmt::Display for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

impl fmt::Debug for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

impl fmt::Debug for LexError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
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
pub struct Span {
    inner: imp::Span,
    _marker: marker::PhantomData<Rc<()>>,
}

impl Span {
    fn _new(inner: imp::Span) -> Span {
        Span {
            inner: inner,
            _marker: marker::PhantomData,
        }
    }

    pub fn call_site() -> Span {
        Span::_new(imp::Span::call_site())
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn def_site() -> Span {
        Span::_new(imp::Span::def_site())
    }

    /// Creates a new span with the same line/column information as `self` but
    /// that resolves symbols as though it were at `other`.
    #[cfg(procmacro2_semver_exempt)]
    pub fn resolved_at(&self, other: Span) -> Span {
        Span::_new(self.inner.resolved_at(other.inner))
    }

    /// Creates a new span with the same name resolution behavior as `self` but
    /// with the line/column information of `other`.
    #[cfg(procmacro2_semver_exempt)]
    pub fn located_at(&self, other: Span) -> Span {
        Span::_new(self.inner.located_at(other.inner))
    }

    /// This method is only available when the `"nightly"` feature is enabled.
    #[cfg(all(feature = "nightly", feature = "proc-macro"))]
    pub fn unstable(self) -> proc_macro::Span {
        self.inner.unstable()
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn source_file(&self) -> SourceFile {
        SourceFile(self.inner.source_file())
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn start(&self) -> LineColumn {
        let imp::LineColumn { line, column } = self.inner.start();
        LineColumn {
            line: line,
            column: column,
        }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn end(&self) -> LineColumn {
        let imp::LineColumn { line, column } = self.inner.end();
        LineColumn {
            line: line,
            column: column,
        }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn join(&self, other: Span) -> Option<Span> {
        self.inner.join(other.inner).map(Span::_new)
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn eq(&self, other: &Span) -> bool {
        self.inner.eq(&other.inner)
    }
}

impl fmt::Debug for Span {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

#[derive(Clone, Debug)]
pub enum TokenTree {
    Group(Group),
    Term(Term),
    Op(Op),
    Literal(Literal),
}

impl TokenTree {
    pub fn span(&self) -> Span {
        match *self {
            TokenTree::Group(ref t) => t.span(),
            TokenTree::Term(ref t) => t.span(),
            TokenTree::Op(ref t) => t.span(),
            TokenTree::Literal(ref t) => t.span(),
        }
    }

    pub fn set_span(&mut self, span: Span) {
        match *self {
            TokenTree::Group(ref mut t) => t.set_span(span),
            TokenTree::Term(ref mut t) => t.set_span(span),
            TokenTree::Op(ref mut t) => t.set_span(span),
            TokenTree::Literal(ref mut t) => t.set_span(span),
        }
    }
}

impl From<Group> for TokenTree {
    fn from(g: Group) -> TokenTree {
        TokenTree::Group(g)
    }
}

impl From<Term> for TokenTree {
    fn from(g: Term) -> TokenTree {
        TokenTree::Term(g)
    }
}

impl From<Op> for TokenTree {
    fn from(g: Op) -> TokenTree {
        TokenTree::Op(g)
    }
}

impl From<Literal> for TokenTree {
    fn from(g: Literal) -> TokenTree {
        TokenTree::Literal(g)
    }
}

impl fmt::Display for TokenTree {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            TokenTree::Group(ref t) => t.fmt(f),
            TokenTree::Term(ref t) => t.fmt(f),
            TokenTree::Op(ref t) => t.fmt(f),
            TokenTree::Literal(ref t) => t.fmt(f),
        }
    }
}

#[derive(Clone, Debug)]
pub struct Group {
    delimiter: Delimiter,
    stream: TokenStream,
    span: Span,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Delimiter {
    Parenthesis,
    Brace,
    Bracket,
    None,
}

impl Group {
    pub fn new(delimiter: Delimiter, stream: TokenStream) -> Group {
        Group {
            delimiter: delimiter,
            stream: stream,
            span: Span::call_site(),
        }
    }

    pub fn delimiter(&self) -> Delimiter {
        self.delimiter
    }

    pub fn stream(&self) -> TokenStream {
        self.stream.clone()
    }

    pub fn span(&self) -> Span {
        self.span
    }

    pub fn set_span(&mut self, span: Span) {
        self.span = span;
    }
}

impl fmt::Display for Group {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.stream.fmt(f)
    }
}

#[derive(Copy, Clone, Debug)]
pub struct Op {
    op: char,
    spacing: Spacing,
    span: Span,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Spacing {
    Alone,
    Joint,
}

impl Op {
    pub fn new(op: char, spacing: Spacing) -> Op {
        Op {
            op: op,
            spacing: spacing,
            span: Span::call_site(),
        }
    }

    pub fn op(&self) -> char {
        self.op
    }

    pub fn spacing(&self) -> Spacing {
        self.spacing
    }

    pub fn span(&self) -> Span {
        self.span
    }

    pub fn set_span(&mut self, span: Span) {
        self.span = span;
    }
}

impl fmt::Display for Op {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.op.fmt(f)
    }
}

#[derive(Copy, Clone)]
pub struct Term {
    inner: imp::Term,
    _marker: marker::PhantomData<Rc<()>>,
}

impl Term {
    fn _new(inner: imp::Term) -> Term {
        Term {
            inner: inner,
            _marker: marker::PhantomData,
        }
    }

    pub fn new(string: &str, span: Span) -> Term {
        Term::_new(imp::Term::new(string, span.inner))
    }

    pub fn as_str(&self) -> &str {
        self.inner.as_str()
    }

    pub fn span(&self) -> Span {
        Span::_new(self.inner.span())
    }

    pub fn set_span(&mut self, span: Span) {
        self.inner.set_span(span.inner);
    }
}

impl fmt::Display for Term {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.as_str().fmt(f)
    }
}

impl fmt::Debug for Term {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

#[derive(Clone)]
pub struct Literal {
    inner: imp::Literal,
    _marker: marker::PhantomData<Rc<()>>,
}

macro_rules! int_literals {
    ($($name:ident => $kind:ident,)*) => ($(
        pub fn $name(n: $kind) -> Literal {
            Literal::_new(imp::Literal::$name(n))
        }
    )*)
}

impl Literal {
    fn _new(inner: imp::Literal) -> Literal {
        Literal {
            inner: inner,
            _marker: marker::PhantomData,
        }
    }

    int_literals! {
        u8_suffixed => u8,
        u16_suffixed => u16,
        u32_suffixed => u32,
        u64_suffixed => u64,
        usize_suffixed => usize,
        i8_suffixed => i8,
        i16_suffixed => i16,
        i32_suffixed => i32,
        i64_suffixed => i64,
        isize_suffixed => isize,

        u8_unsuffixed => u8,
        u16_unsuffixed => u16,
        u32_unsuffixed => u32,
        u64_unsuffixed => u64,
        usize_unsuffixed => usize,
        i8_unsuffixed => i8,
        i16_unsuffixed => i16,
        i32_unsuffixed => i32,
        i64_unsuffixed => i64,
        isize_unsuffixed => isize,
    }

    pub fn f64_unsuffixed(f: f64) -> Literal {
        assert!(f.is_finite());
        Literal::_new(imp::Literal::f64_unsuffixed(f))
    }

    pub fn f64_suffixed(f: f64) -> Literal {
        assert!(f.is_finite());
        Literal::_new(imp::Literal::f64_suffixed(f))
    }

    pub fn f32_unsuffixed(f: f32) -> Literal {
        assert!(f.is_finite());
        Literal::_new(imp::Literal::f32_unsuffixed(f))
    }

    pub fn f32_suffixed(f: f32) -> Literal {
        assert!(f.is_finite());
        Literal::_new(imp::Literal::f32_suffixed(f))
    }

    pub fn string(string: &str) -> Literal {
        Literal::_new(imp::Literal::string(string))
    }

    pub fn character(ch: char) -> Literal {
        Literal::_new(imp::Literal::character(ch))
    }

    pub fn byte_string(s: &[u8]) -> Literal {
        Literal::_new(imp::Literal::byte_string(s))
    }

    pub fn span(&self) -> Span {
        Span::_new(self.inner.span())
    }

    pub fn set_span(&mut self, span: Span) {
        self.inner.set_span(span.inner);
    }
}

impl fmt::Debug for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

impl fmt::Display for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

pub mod token_stream {
    use std::fmt;
    use std::marker;
    use std::rc::Rc;

    pub use TokenStream;
    use TokenTree;
    use imp;

    pub struct IntoIter {
        inner: imp::TokenTreeIter,
        _marker: marker::PhantomData<Rc<()>>,
    }

    impl Iterator for IntoIter {
        type Item = TokenTree;

        fn next(&mut self) -> Option<TokenTree> {
            self.inner.next()
        }
    }

    impl fmt::Debug for IntoIter {
        fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
            self.inner.fmt(f)
        }
    }

    impl IntoIterator for TokenStream {
        type Item = TokenTree;
        type IntoIter = IntoIter;

        fn into_iter(self) -> IntoIter {
            IntoIter {
                inner: self.inner.into_iter(),
                _marker: marker::PhantomData,
            }
        }
    }
}
