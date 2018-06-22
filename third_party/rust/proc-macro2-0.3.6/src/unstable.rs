#![cfg_attr(not(procmacro2_semver_exempt), allow(dead_code))]

use std::fmt;
use std::iter;
use std::str::FromStr;

use proc_macro;

use {Delimiter, Group, Op, Spacing, TokenTree};

#[derive(Clone)]
pub struct TokenStream(proc_macro::TokenStream);

pub struct LexError(proc_macro::LexError);

impl TokenStream {
    pub fn empty() -> TokenStream {
        TokenStream(proc_macro::TokenStream::empty())
    }

    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}

impl FromStr for TokenStream {
    type Err = LexError;

    fn from_str(src: &str) -> Result<TokenStream, LexError> {
        Ok(TokenStream(src.parse().map_err(LexError)?))
    }
}

impl fmt::Display for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl From<proc_macro::TokenStream> for TokenStream {
    fn from(inner: proc_macro::TokenStream) -> TokenStream {
        TokenStream(inner)
    }
}

impl From<TokenStream> for proc_macro::TokenStream {
    fn from(inner: TokenStream) -> proc_macro::TokenStream {
        inner.0
    }
}

impl From<TokenTree> for TokenStream {
    fn from(token: TokenTree) -> TokenStream {
        let tt: proc_macro::TokenTree = match token {
            TokenTree::Group(tt) => {
                let delim = match tt.delimiter() {
                    Delimiter::Parenthesis => proc_macro::Delimiter::Parenthesis,
                    Delimiter::Bracket => proc_macro::Delimiter::Bracket,
                    Delimiter::Brace => proc_macro::Delimiter::Brace,
                    Delimiter::None => proc_macro::Delimiter::None,
                };
                let span = tt.span();
                let mut group = proc_macro::Group::new(delim, tt.stream.inner.0);
                group.set_span(span.inner.0);
                group.into()
            }
            TokenTree::Op(tt) => {
                let spacing = match tt.spacing() {
                    Spacing::Joint => proc_macro::Spacing::Joint,
                    Spacing::Alone => proc_macro::Spacing::Alone,
                };
                let mut op = proc_macro::Op::new(tt.op(), spacing);
                op.set_span(tt.span().inner.0);
                op.into()
            }
            TokenTree::Term(tt) => tt.inner.term.into(),
            TokenTree::Literal(tt) => tt.inner.lit.into(),
        };
        TokenStream(tt.into())
    }
}

impl iter::FromIterator<TokenTree> for TokenStream {
    fn from_iter<I: IntoIterator<Item = TokenTree>>(streams: I) -> Self {
        let streams = streams.into_iter().map(TokenStream::from)
            .flat_map(|t| t.0);
        TokenStream(streams.collect::<proc_macro::TokenStream>())
    }
}

impl fmt::Debug for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl fmt::Debug for LexError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

pub struct TokenTreeIter(proc_macro::token_stream::IntoIter);

impl IntoIterator for TokenStream {
    type Item = TokenTree;
    type IntoIter = TokenTreeIter;

    fn into_iter(self) -> TokenTreeIter {
        TokenTreeIter(self.0.into_iter())
    }
}

impl Iterator for TokenTreeIter {
    type Item = TokenTree;

    fn next(&mut self) -> Option<TokenTree> {
        let token = self.0.next()?;
        Some(match token {
            proc_macro::TokenTree::Group(tt) => {
                let delim = match tt.delimiter() {
                    proc_macro::Delimiter::Parenthesis => Delimiter::Parenthesis,
                    proc_macro::Delimiter::Bracket => Delimiter::Bracket,
                    proc_macro::Delimiter::Brace => Delimiter::Brace,
                    proc_macro::Delimiter::None => Delimiter::None,
                };
                let stream = ::TokenStream::_new(TokenStream(tt.stream()));
                let mut g = Group::new(delim, stream);
                g.set_span(::Span::_new(Span(tt.span())));
                g.into()
            }
            proc_macro::TokenTree::Op(tt) => {
                let spacing = match tt.spacing() {
                    proc_macro::Spacing::Joint => Spacing::Joint,
                    proc_macro::Spacing::Alone => Spacing::Alone,
                };
                let mut o = Op::new(tt.op(), spacing);
                o.set_span(::Span::_new(Span(tt.span())));
                o.into()
            }
            proc_macro::TokenTree::Term(s) => {
                ::Term::_new(Term {
                    term: s,
                }).into()
            }
            proc_macro::TokenTree::Literal(l) => {
                ::Literal::_new(Literal {
                    lit: l,
                }).into()
            }
        })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.0.size_hint()
    }
}

impl fmt::Debug for TokenTreeIter {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("TokenTreeIter").finish()
    }
}

#[derive(Clone, PartialEq, Eq)]
pub struct FileName(String);

impl fmt::Display for FileName {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

// NOTE: We have to generate our own filename object here because we can't wrap
// the one provided by proc_macro.
#[derive(Clone, PartialEq, Eq)]
pub struct SourceFile(proc_macro::SourceFile, FileName);

impl SourceFile {
    fn new(sf: proc_macro::SourceFile) -> Self {
        let filename = FileName(sf.path().to_string());
        SourceFile(sf, filename)
    }

    /// Get the path to this source file as a string.
    pub fn path(&self) -> &FileName {
        &self.1
    }

    pub fn is_real(&self) -> bool {
        self.0.is_real()
    }
}

impl AsRef<FileName> for SourceFile {
    fn as_ref(&self) -> &FileName {
        self.path()
    }
}

impl fmt::Debug for SourceFile {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

pub struct LineColumn {
    pub line: usize,
    pub column: usize,
}

#[derive(Copy, Clone)]
pub struct Span(proc_macro::Span);

impl From<proc_macro::Span> for ::Span {
    fn from(proc_span: proc_macro::Span) -> ::Span {
        ::Span::_new(Span(proc_span))
    }
}

impl Span {
    pub fn call_site() -> Span {
        Span(proc_macro::Span::call_site())
    }

    pub fn def_site() -> Span {
        Span(proc_macro::Span::def_site())
    }

    pub fn resolved_at(&self, other: Span) -> Span {
        Span(self.0.resolved_at(other.0))
    }

    pub fn located_at(&self, other: Span) -> Span {
        Span(self.0.located_at(other.0))
    }

    pub fn unstable(self) -> proc_macro::Span {
        self.0
    }

    pub fn source_file(&self) -> SourceFile {
        SourceFile::new(self.0.source_file())
    }

    pub fn start(&self) -> LineColumn {
        let proc_macro::LineColumn { line, column } = self.0.start();
        LineColumn { line, column }
    }

    pub fn end(&self) -> LineColumn {
        let proc_macro::LineColumn { line, column } = self.0.end();
        LineColumn { line, column }
    }

    pub fn join(&self, other: Span) -> Option<Span> {
        self.0.join(other.0).map(Span)
    }

    pub fn eq(&self, other: &Span) -> bool {
        self.0.eq(&other.0)
    }
}

impl fmt::Debug for Span {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[derive(Copy, Clone)]
pub struct Term {
    term: proc_macro::Term,
}

impl Term {
    pub fn new(string: &str, span: Span) -> Term {
        Term {
            term: proc_macro::Term::new(string, span.0),
        }
    }

    pub fn as_str(&self) -> &str {
        self.term.as_str()
    }

    pub fn span(&self) -> Span {
        Span(self.term.span())
    }

    pub fn set_span(&mut self, span: Span) {
        self.term.set_span(span.0);
    }
}

impl fmt::Debug for Term {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.term.fmt(f)
    }
}

#[derive(Clone)]
pub struct Literal {
    lit: proc_macro::Literal,
}

macro_rules! suffixed_numbers {
    ($($name:ident => $kind:ident,)*) => ($(
        pub fn $name(n: $kind) -> Literal {
            Literal::_new(proc_macro::Literal::$name(n))
        }
    )*)
}

macro_rules! unsuffixed_integers {
    ($($name:ident => $kind:ident,)*) => ($(
        pub fn $name(n: $kind) -> Literal {
            Literal::_new(proc_macro::Literal::$name(n))
        }
    )*)
}

impl Literal {
    fn _new(lit: proc_macro::Literal) -> Literal {
        Literal {
            lit,
        }
    }

    suffixed_numbers! {
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

        f32_suffixed => f32,
        f64_suffixed => f64,
    }

    unsuffixed_integers! {
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

    pub fn f32_unsuffixed(f: f32) -> Literal {
        Literal::_new(proc_macro::Literal::f32_unsuffixed(f))
    }

    pub fn f64_unsuffixed(f: f64) -> Literal {
        Literal::_new(proc_macro::Literal::f64_unsuffixed(f))
    }


    pub fn string(t: &str) -> Literal {
        Literal::_new(proc_macro::Literal::string(t))
    }

    pub fn character(t: char) -> Literal {
        Literal::_new(proc_macro::Literal::character(t))
    }

    pub fn byte_string(bytes: &[u8]) -> Literal {
        Literal::_new(proc_macro::Literal::byte_string(bytes))
    }

    pub fn span(&self) -> Span {
        Span(self.lit.span())
    }

    pub fn set_span(&mut self, span: Span) {
        self.lit.set_span(span.0);
    }
}

impl fmt::Display for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.lit.fmt(f)
    }
}

impl fmt::Debug for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.lit.fmt(f)
    }
}
