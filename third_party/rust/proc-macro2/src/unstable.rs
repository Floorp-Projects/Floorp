#![cfg_attr(not(procmacro2_semver_exempt), allow(dead_code))]

use std::fmt;
use std::iter;
use std::panic;
use std::str::FromStr;

use proc_macro;
use stable;

use {Delimiter, Group, Punct, Spacing, TokenTree};

#[derive(Clone)]
pub enum TokenStream {
    Nightly(proc_macro::TokenStream),
    Stable(stable::TokenStream),
}

pub enum LexError {
    Nightly(proc_macro::LexError),
    Stable(stable::LexError),
}

fn nightly_works() -> bool {
    use std::sync::atomic::*;
    static WORKS: AtomicUsize = ATOMIC_USIZE_INIT;

    match WORKS.load(Ordering::SeqCst) {
        1 => return false,
        2 => return true,
        _ => {}
    }
    let works = panic::catch_unwind(|| proc_macro::Span::call_site()).is_ok();
    WORKS.store(works as usize + 1, Ordering::SeqCst);
    works
}

fn mismatch() -> ! {
    panic!("stable/nightly mismatch")
}

impl TokenStream {
    pub fn new() -> TokenStream {
        if nightly_works() {
            TokenStream::Nightly(proc_macro::TokenStream::new())
        } else {
            TokenStream::Stable(stable::TokenStream::new())
        }
    }

    pub fn is_empty(&self) -> bool {
        match self {
            TokenStream::Nightly(tts) => tts.is_empty(),
            TokenStream::Stable(tts) => tts.is_empty(),
        }
    }

    fn unwrap_nightly(self) -> proc_macro::TokenStream {
        match self {
            TokenStream::Nightly(s) => s,
            TokenStream::Stable(_) => mismatch(),
        }
    }
}

impl FromStr for TokenStream {
    type Err = LexError;

    fn from_str(src: &str) -> Result<TokenStream, LexError> {
        if nightly_works() {
            Ok(TokenStream::Nightly(src.parse()?))
        } else {
            Ok(TokenStream::Stable(src.parse()?))
        }
    }
}

impl fmt::Display for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            TokenStream::Nightly(tts) => tts.fmt(f),
            TokenStream::Stable(tts) => tts.fmt(f),
        }
    }
}

impl From<proc_macro::TokenStream> for TokenStream {
    fn from(inner: proc_macro::TokenStream) -> TokenStream {
        TokenStream::Nightly(inner)
    }
}

impl From<TokenStream> for proc_macro::TokenStream {
    fn from(inner: TokenStream) -> proc_macro::TokenStream {
        match inner {
            TokenStream::Nightly(inner) => inner,
            TokenStream::Stable(inner) => inner.to_string().parse().unwrap(),
        }
    }
}

impl From<stable::TokenStream> for TokenStream {
    fn from(inner: stable::TokenStream) -> TokenStream {
        TokenStream::Stable(inner)
    }
}

impl From<TokenTree> for TokenStream {
    fn from(token: TokenTree) -> TokenStream {
        if !nightly_works() {
            return TokenStream::Stable(token.into());
        }
        let tt: proc_macro::TokenTree = match token {
            TokenTree::Group(tt) => {
                let delim = match tt.delimiter() {
                    Delimiter::Parenthesis => proc_macro::Delimiter::Parenthesis,
                    Delimiter::Bracket => proc_macro::Delimiter::Bracket,
                    Delimiter::Brace => proc_macro::Delimiter::Brace,
                    Delimiter::None => proc_macro::Delimiter::None,
                };
                let span = tt.span();
                let mut group = proc_macro::Group::new(delim, tt.stream.inner.unwrap_nightly());
                group.set_span(span.inner.unwrap_nightly());
                group.into()
            }
            TokenTree::Punct(tt) => {
                let spacing = match tt.spacing() {
                    Spacing::Joint => proc_macro::Spacing::Joint,
                    Spacing::Alone => proc_macro::Spacing::Alone,
                };
                let mut op = proc_macro::Punct::new(tt.as_char(), spacing);
                op.set_span(tt.span().inner.unwrap_nightly());
                op.into()
            }
            TokenTree::Ident(tt) => tt.inner.unwrap_nightly().into(),
            TokenTree::Literal(tt) => tt.inner.unwrap_nightly().into(),
        };
        TokenStream::Nightly(tt.into())
    }
}

impl iter::FromIterator<TokenTree> for TokenStream {
    fn from_iter<I: IntoIterator<Item = TokenTree>>(trees: I) -> Self {
        if nightly_works() {
            let trees = trees
                .into_iter()
                .map(TokenStream::from)
                .flat_map(|t| match t {
                    TokenStream::Nightly(s) => s,
                    TokenStream::Stable(_) => mismatch(),
                });
            TokenStream::Nightly(trees.collect())
        } else {
            TokenStream::Stable(trees.into_iter().collect())
        }
    }
}

impl Extend<TokenTree> for TokenStream {
    fn extend<I: IntoIterator<Item = TokenTree>>(&mut self, streams: I) {
        match self {
            TokenStream::Nightly(tts) => {
                *tts = tts
                    .clone()
                    .into_iter()
                    .chain(
                        streams
                            .into_iter()
                            .map(TokenStream::from)
                            .flat_map(|t| match t {
                                TokenStream::Nightly(tts) => tts.into_iter(),
                                _ => panic!(),
                            }),
                    )
                    .collect();
            }
            TokenStream::Stable(tts) => tts.extend(streams),
        }
    }
}

impl fmt::Debug for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            TokenStream::Nightly(tts) => tts.fmt(f),
            TokenStream::Stable(tts) => tts.fmt(f),
        }
    }
}

impl From<proc_macro::LexError> for LexError {
    fn from(e: proc_macro::LexError) -> LexError {
        LexError::Nightly(e)
    }
}

impl From<stable::LexError> for LexError {
    fn from(e: stable::LexError) -> LexError {
        LexError::Stable(e)
    }
}

impl fmt::Debug for LexError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            LexError::Nightly(e) => e.fmt(f),
            LexError::Stable(e) => e.fmt(f),
        }
    }
}

pub enum TokenTreeIter {
    Nightly(proc_macro::token_stream::IntoIter),
    Stable(stable::TokenTreeIter),
}

impl IntoIterator for TokenStream {
    type Item = TokenTree;
    type IntoIter = TokenTreeIter;

    fn into_iter(self) -> TokenTreeIter {
        match self {
            TokenStream::Nightly(tts) => TokenTreeIter::Nightly(tts.into_iter()),
            TokenStream::Stable(tts) => TokenTreeIter::Stable(tts.into_iter()),
        }
    }
}

impl Iterator for TokenTreeIter {
    type Item = TokenTree;

    fn next(&mut self) -> Option<TokenTree> {
        let token = match self {
            TokenTreeIter::Nightly(iter) => iter.next()?,
            TokenTreeIter::Stable(iter) => return iter.next(),
        };
        Some(match token {
            proc_macro::TokenTree::Group(tt) => {
                let delim = match tt.delimiter() {
                    proc_macro::Delimiter::Parenthesis => Delimiter::Parenthesis,
                    proc_macro::Delimiter::Bracket => Delimiter::Bracket,
                    proc_macro::Delimiter::Brace => Delimiter::Brace,
                    proc_macro::Delimiter::None => Delimiter::None,
                };
                let stream = ::TokenStream::_new(TokenStream::Nightly(tt.stream()));
                let mut g = Group::new(delim, stream);
                g.set_span(::Span::_new(Span::Nightly(tt.span())));
                g.into()
            }
            proc_macro::TokenTree::Punct(tt) => {
                let spacing = match tt.spacing() {
                    proc_macro::Spacing::Joint => Spacing::Joint,
                    proc_macro::Spacing::Alone => Spacing::Alone,
                };
                let mut o = Punct::new(tt.as_char(), spacing);
                o.set_span(::Span::_new(Span::Nightly(tt.span())));
                o.into()
            }
            proc_macro::TokenTree::Ident(s) => ::Ident::_new(Ident::Nightly(s)).into(),
            proc_macro::TokenTree::Literal(l) => ::Literal::_new(Literal::Nightly(l)).into(),
        })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        match self {
            TokenTreeIter::Nightly(tts) => tts.size_hint(),
            TokenTreeIter::Stable(tts) => tts.size_hint(),
        }
    }
}

impl fmt::Debug for TokenTreeIter {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("TokenTreeIter").finish()
    }
}

pub use stable::FileName;

// NOTE: We have to generate our own filename object here because we can't wrap
// the one provided by proc_macro.
#[derive(Clone, PartialEq, Eq)]
pub enum SourceFile {
    Nightly(proc_macro::SourceFile, FileName),
    Stable(stable::SourceFile),
}

impl SourceFile {
    fn nightly(sf: proc_macro::SourceFile) -> Self {
        let filename = stable::file_name(sf.path().to_string());
        SourceFile::Nightly(sf, filename)
    }

    /// Get the path to this source file as a string.
    pub fn path(&self) -> &FileName {
        match self {
            SourceFile::Nightly(_, f) => f,
            SourceFile::Stable(a) => a.path(),
        }
    }

    pub fn is_real(&self) -> bool {
        match self {
            SourceFile::Nightly(a, _) => a.is_real(),
            SourceFile::Stable(a) => a.is_real(),
        }
    }
}

impl AsRef<FileName> for SourceFile {
    fn as_ref(&self) -> &FileName {
        self.path()
    }
}

impl fmt::Debug for SourceFile {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            SourceFile::Nightly(a, _) => a.fmt(f),
            SourceFile::Stable(a) => a.fmt(f),
        }
    }
}

pub struct LineColumn {
    pub line: usize,
    pub column: usize,
}

#[derive(Copy, Clone)]
pub enum Span {
    Nightly(proc_macro::Span),
    Stable(stable::Span),
}

impl Span {
    pub fn call_site() -> Span {
        if nightly_works() {
            Span::Nightly(proc_macro::Span::call_site())
        } else {
            Span::Stable(stable::Span::call_site())
        }
    }

    pub fn def_site() -> Span {
        if nightly_works() {
            Span::Nightly(proc_macro::Span::def_site())
        } else {
            Span::Stable(stable::Span::def_site())
        }
    }

    pub fn resolved_at(&self, other: Span) -> Span {
        match (self, other) {
            (Span::Nightly(a), Span::Nightly(b)) => Span::Nightly(a.resolved_at(b)),
            (Span::Stable(a), Span::Stable(b)) => Span::Stable(a.resolved_at(b)),
            _ => mismatch(),
        }
    }

    pub fn located_at(&self, other: Span) -> Span {
        match (self, other) {
            (Span::Nightly(a), Span::Nightly(b)) => Span::Nightly(a.located_at(b)),
            (Span::Stable(a), Span::Stable(b)) => Span::Stable(a.located_at(b)),
            _ => mismatch(),
        }
    }

    pub fn unstable(self) -> proc_macro::Span {
        match self {
            Span::Nightly(s) => s,
            Span::Stable(_) => mismatch(),
        }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn source_file(&self) -> SourceFile {
        match self {
            Span::Nightly(s) => SourceFile::nightly(s.source_file()),
            Span::Stable(s) => SourceFile::Stable(s.source_file()),
        }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn start(&self) -> LineColumn {
        match self {
            Span::Nightly(s) => {
                let proc_macro::LineColumn { line, column } = s.start();
                LineColumn { line, column }
            }
            Span::Stable(s) => {
                let stable::LineColumn { line, column } = s.start();
                LineColumn { line, column }
            }
        }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn end(&self) -> LineColumn {
        match self {
            Span::Nightly(s) => {
                let proc_macro::LineColumn { line, column } = s.end();
                LineColumn { line, column }
            }
            Span::Stable(s) => {
                let stable::LineColumn { line, column } = s.end();
                LineColumn { line, column }
            }
        }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn join(&self, other: Span) -> Option<Span> {
        let ret = match (self, other) {
            (Span::Nightly(a), Span::Nightly(b)) => Span::Nightly(a.join(b)?),
            (Span::Stable(a), Span::Stable(b)) => Span::Stable(a.join(b)?),
            _ => return None,
        };
        Some(ret)
    }

    pub fn eq(&self, other: &Span) -> bool {
        match (self, other) {
            (Span::Nightly(a), Span::Nightly(b)) => a.eq(b),
            (Span::Stable(a), Span::Stable(b)) => a.eq(b),
            _ => false,
        }
    }

    fn unwrap_nightly(self) -> proc_macro::Span {
        match self {
            Span::Nightly(s) => s,
            Span::Stable(_) => mismatch(),
        }
    }
}

impl From<proc_macro::Span> for ::Span {
    fn from(proc_span: proc_macro::Span) -> ::Span {
        ::Span::_new(Span::Nightly(proc_span))
    }
}

impl From<stable::Span> for Span {
    fn from(inner: stable::Span) -> Span {
        Span::Stable(inner)
    }
}

impl fmt::Debug for Span {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Span::Nightly(s) => s.fmt(f),
            Span::Stable(s) => s.fmt(f),
        }
    }
}

#[derive(Clone)]
pub enum Ident {
    Nightly(proc_macro::Ident),
    Stable(stable::Ident),
}

impl Ident {
    pub fn new(string: &str, span: Span) -> Ident {
        match span {
            Span::Nightly(s) => Ident::Nightly(proc_macro::Ident::new(string, s)),
            Span::Stable(s) => Ident::Stable(stable::Ident::new(string, s)),
        }
    }

    pub fn new_raw(string: &str, span: Span) -> Ident {
        match span {
            Span::Nightly(s) => Ident::Nightly(proc_macro::Ident::new_raw(string, s)),
            Span::Stable(s) => Ident::Stable(stable::Ident::new_raw(string, s)),
        }
    }

    pub fn span(&self) -> Span {
        match self {
            Ident::Nightly(t) => Span::Nightly(t.span()),
            Ident::Stable(t) => Span::Stable(t.span()),
        }
    }

    pub fn set_span(&mut self, span: Span) {
        match (self, span) {
            (Ident::Nightly(t), Span::Nightly(s)) => t.set_span(s),
            (Ident::Stable(t), Span::Stable(s)) => t.set_span(s),
            _ => mismatch(),
        }
    }

    fn unwrap_nightly(self) -> proc_macro::Ident {
        match self {
            Ident::Nightly(s) => s,
            Ident::Stable(_) => mismatch(),
        }
    }
}

impl fmt::Display for Ident {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Ident::Nightly(t) => t.fmt(f),
            Ident::Stable(t) => t.fmt(f),
        }
    }
}

impl fmt::Debug for Ident {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Ident::Nightly(t) => t.fmt(f),
            Ident::Stable(t) => t.fmt(f),
        }
    }
}

#[derive(Clone)]
pub enum Literal {
    Nightly(proc_macro::Literal),
    Stable(stable::Literal),
}

macro_rules! suffixed_numbers {
    ($($name:ident => $kind:ident,)*) => ($(
        pub fn $name(n: $kind) -> Literal {
            if nightly_works() {
                Literal::Nightly(proc_macro::Literal::$name(n))
            } else {
                Literal::Stable(stable::Literal::$name(n))
            }
        }
    )*)
}

macro_rules! unsuffixed_integers {
    ($($name:ident => $kind:ident,)*) => ($(
        pub fn $name(n: $kind) -> Literal {
            if nightly_works() {
                Literal::Nightly(proc_macro::Literal::$name(n))
            } else {
                Literal::Stable(stable::Literal::$name(n))
            }
        }
    )*)
}

impl Literal {
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
        if nightly_works() {
            Literal::Nightly(proc_macro::Literal::f32_unsuffixed(f))
        } else {
            Literal::Stable(stable::Literal::f32_unsuffixed(f))
        }
    }

    pub fn f64_unsuffixed(f: f64) -> Literal {
        if nightly_works() {
            Literal::Nightly(proc_macro::Literal::f64_unsuffixed(f))
        } else {
            Literal::Stable(stable::Literal::f64_unsuffixed(f))
        }
    }

    pub fn string(t: &str) -> Literal {
        if nightly_works() {
            Literal::Nightly(proc_macro::Literal::string(t))
        } else {
            Literal::Stable(stable::Literal::string(t))
        }
    }

    pub fn character(t: char) -> Literal {
        if nightly_works() {
            Literal::Nightly(proc_macro::Literal::character(t))
        } else {
            Literal::Stable(stable::Literal::character(t))
        }
    }

    pub fn byte_string(bytes: &[u8]) -> Literal {
        if nightly_works() {
            Literal::Nightly(proc_macro::Literal::byte_string(bytes))
        } else {
            Literal::Stable(stable::Literal::byte_string(bytes))
        }
    }

    pub fn span(&self) -> Span {
        match self {
            Literal::Nightly(lit) => Span::Nightly(lit.span()),
            Literal::Stable(lit) => Span::Stable(lit.span()),
        }
    }

    pub fn set_span(&mut self, span: Span) {
        match (self, span) {
            (Literal::Nightly(lit), Span::Nightly(s)) => lit.set_span(s),
            (Literal::Stable(lit), Span::Stable(s)) => lit.set_span(s),
            _ => mismatch(),
        }
    }

    fn unwrap_nightly(self) -> proc_macro::Literal {
        match self {
            Literal::Nightly(s) => s,
            Literal::Stable(_) => mismatch(),
        }
    }
}

impl From<stable::Literal> for Literal {
    fn from(s: stable::Literal) -> Literal {
        Literal::Stable(s)
    }
}

impl fmt::Display for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Literal::Nightly(t) => t.fmt(f),
            Literal::Stable(t) => t.fmt(f),
        }
    }
}

impl fmt::Debug for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Literal::Nightly(t) => t.fmt(f),
            Literal::Stable(t) => t.fmt(f),
        }
    }
}
