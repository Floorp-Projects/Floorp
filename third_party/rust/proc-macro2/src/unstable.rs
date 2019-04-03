#![cfg_attr(not(super_unstable), allow(dead_code))]

use std::fmt;
use std::iter;
use std::panic::{self, PanicInfo};
#[cfg(super_unstable)]
use std::path::PathBuf;
use std::str::FromStr;

use proc_macro;
use stable;

use {Delimiter, Punct, Spacing, TokenTree};

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
    use std::sync::Once;

    static WORKS: AtomicUsize = ATOMIC_USIZE_INIT;
    static INIT: Once = Once::new();

    match WORKS.load(Ordering::SeqCst) {
        1 => return false,
        2 => return true,
        _ => {}
    }

    // Swap in a null panic hook to avoid printing "thread panicked" to stderr,
    // then use catch_unwind to determine whether the compiler's proc_macro is
    // working. When proc-macro2 is used from outside of a procedural macro all
    // of the proc_macro crate's APIs currently panic.
    //
    // The Once is to prevent the possibility of this ordering:
    //
    //     thread 1 calls take_hook, gets the user's original hook
    //     thread 1 calls set_hook with the null hook
    //     thread 2 calls take_hook, thinks null hook is the original hook
    //     thread 2 calls set_hook with the null hook
    //     thread 1 calls set_hook with the actual original hook
    //     thread 2 calls set_hook with what it thinks is the original hook
    //
    // in which the user's hook has been lost.
    //
    // There is still a race condition where a panic in a different thread can
    // happen during the interval that the user's original panic hook is
    // unregistered such that their hook is incorrectly not called. This is
    // sufficiently unlikely and less bad than printing panic messages to stderr
    // on correct use of this crate. Maybe there is a libstd feature request
    // here. For now, if a user needs to guarantee that this failure mode does
    // not occur, they need to call e.g. `proc_macro2::Span::call_site()` from
    // the main thread before launching any other threads.
    INIT.call_once(|| {
        type PanicHook = Fn(&PanicInfo) + Sync + Send + 'static;

        let null_hook: Box<PanicHook> = Box::new(|_panic_info| { /* ignore */ });
        let sanity_check = &*null_hook as *const PanicHook;
        let original_hook = panic::take_hook();
        panic::set_hook(null_hook);

        let works = panic::catch_unwind(|| proc_macro::Span::call_site()).is_ok();
        WORKS.store(works as usize + 1, Ordering::SeqCst);

        let hopefully_null_hook = panic::take_hook();
        panic::set_hook(original_hook);
        if sanity_check != &*hopefully_null_hook {
            panic!("observed race condition in proc_macro2::nightly_works");
        }
    });
    nightly_works()
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

    fn unwrap_stable(self) -> stable::TokenStream {
        match self {
            TokenStream::Nightly(_) => mismatch(),
            TokenStream::Stable(s) => s,
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
            TokenTree::Group(tt) => tt.inner.unwrap_nightly().into(),
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

impl iter::FromIterator<TokenStream> for TokenStream {
    fn from_iter<I: IntoIterator<Item = TokenStream>>(streams: I) -> Self {
        let mut streams = streams.into_iter();
        match streams.next() {
            #[cfg(slow_extend)]
            Some(TokenStream::Nightly(first)) => {
                let stream = iter::once(first).chain(streams.map(|s| {
                    match s {
                        TokenStream::Nightly(s) => s,
                        TokenStream::Stable(_) => mismatch(),
                    }
                })).collect();
                TokenStream::Nightly(stream)
            }
            #[cfg(not(slow_extend))]
            Some(TokenStream::Nightly(mut first)) => {
                first.extend(streams.map(|s| {
                    match s {
                        TokenStream::Nightly(s) => s,
                        TokenStream::Stable(_) => mismatch(),
                    }
                }));
                TokenStream::Nightly(first)
            }
            Some(TokenStream::Stable(mut first)) => {
                first.extend(streams.map(|s| {
                    match s {
                        TokenStream::Stable(s) => s,
                        TokenStream::Nightly(_) => mismatch(),
                    }
                }));
                TokenStream::Stable(first)
            }
            None => TokenStream::new(),

        }
    }
}

impl Extend<TokenTree> for TokenStream {
    fn extend<I: IntoIterator<Item = TokenTree>>(&mut self, streams: I) {
        match self {
            TokenStream::Nightly(tts) => {
                #[cfg(not(slow_extend))]
                {
                    tts.extend(
                        streams
                            .into_iter()
                            .map(|t| TokenStream::from(t).unwrap_nightly()),
                    );
                }
                #[cfg(slow_extend)]
                {
                    *tts = tts
                        .clone()
                        .into_iter()
                        .chain(
                            streams
                                .into_iter()
                                .map(TokenStream::from)
                                .flat_map(|t| match t {
                                    TokenStream::Nightly(tts) => tts.into_iter(),
                                    _ => mismatch(),
                                }),
                        ).collect();
                }
            }
            TokenStream::Stable(tts) => tts.extend(streams),
        }
    }
}

impl Extend<TokenStream> for TokenStream {
    fn extend<I: IntoIterator<Item = TokenStream>>(&mut self, streams: I) {
        match self {
            TokenStream::Nightly(tts) => {
                #[cfg(not(slow_extend))]
                {
                    tts.extend(streams.into_iter().map(|stream| stream.unwrap_nightly()));
                }
                #[cfg(slow_extend)]
                {
                    *tts = tts
                        .clone()
                        .into_iter()
                        .chain(
                            streams
                                .into_iter()
                                .flat_map(|t| match t {
                                    TokenStream::Nightly(tts) => tts.into_iter(),
                                    _ => mismatch(),
                                }),
                        ).collect();
                }
            }
            TokenStream::Stable(tts) => {
                tts.extend(streams.into_iter().map(|stream| stream.unwrap_stable()))
            }
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
            proc_macro::TokenTree::Group(tt) => ::Group::_new(Group::Nightly(tt)).into(),
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

#[derive(Clone, PartialEq, Eq)]
#[cfg(super_unstable)]
pub enum SourceFile {
    Nightly(proc_macro::SourceFile),
    Stable(stable::SourceFile),
}

#[cfg(super_unstable)]
impl SourceFile {
    fn nightly(sf: proc_macro::SourceFile) -> Self {
        SourceFile::Nightly(sf)
    }

    /// Get the path to this source file as a string.
    pub fn path(&self) -> PathBuf {
        match self {
            SourceFile::Nightly(a) => a.path(),
            SourceFile::Stable(a) => a.path(),
        }
    }

    pub fn is_real(&self) -> bool {
        match self {
            SourceFile::Nightly(a) => a.is_real(),
            SourceFile::Stable(a) => a.is_real(),
        }
    }
}

#[cfg(super_unstable)]
impl fmt::Debug for SourceFile {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            SourceFile::Nightly(a) => a.fmt(f),
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

    #[cfg(super_unstable)]
    pub fn def_site() -> Span {
        if nightly_works() {
            Span::Nightly(proc_macro::Span::def_site())
        } else {
            Span::Stable(stable::Span::def_site())
        }
    }

    #[cfg(super_unstable)]
    pub fn resolved_at(&self, other: Span) -> Span {
        match (self, other) {
            (Span::Nightly(a), Span::Nightly(b)) => Span::Nightly(a.resolved_at(b)),
            (Span::Stable(a), Span::Stable(b)) => Span::Stable(a.resolved_at(b)),
            _ => mismatch(),
        }
    }

    #[cfg(super_unstable)]
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

    #[cfg(super_unstable)]
    pub fn source_file(&self) -> SourceFile {
        match self {
            Span::Nightly(s) => SourceFile::nightly(s.source_file()),
            Span::Stable(s) => SourceFile::Stable(s.source_file()),
        }
    }

    #[cfg(super_unstable)]
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

    #[cfg(super_unstable)]
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

    #[cfg(super_unstable)]
    pub fn join(&self, other: Span) -> Option<Span> {
        let ret = match (self, other) {
            (Span::Nightly(a), Span::Nightly(b)) => Span::Nightly(a.join(b)?),
            (Span::Stable(a), Span::Stable(b)) => Span::Stable(a.join(b)?),
            _ => return None,
        };
        Some(ret)
    }

    #[cfg(super_unstable)]
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
pub enum Group {
    Nightly(proc_macro::Group),
    Stable(stable::Group),
}

impl Group {
    pub fn new(delimiter: Delimiter, stream: TokenStream) -> Group {
        match stream {
            TokenStream::Nightly(stream) => {
                let delimiter = match delimiter {
                    Delimiter::Parenthesis => proc_macro::Delimiter::Parenthesis,
                    Delimiter::Bracket => proc_macro::Delimiter::Bracket,
                    Delimiter::Brace => proc_macro::Delimiter::Brace,
                    Delimiter::None => proc_macro::Delimiter::None,
                };
                Group::Nightly(proc_macro::Group::new(delimiter, stream))
            }
            TokenStream::Stable(stream) => {
                Group::Stable(stable::Group::new(delimiter, stream))
            }
        }
    }

    pub fn delimiter(&self) -> Delimiter {
        match self {
            Group::Nightly(g) => match g.delimiter() {
                proc_macro::Delimiter::Parenthesis => Delimiter::Parenthesis,
                proc_macro::Delimiter::Bracket => Delimiter::Bracket,
                proc_macro::Delimiter::Brace => Delimiter::Brace,
                proc_macro::Delimiter::None => Delimiter::None,
            }
            Group::Stable(g) => g.delimiter(),
        }
    }

    pub fn stream(&self) -> TokenStream {
        match self {
            Group::Nightly(g) => TokenStream::Nightly(g.stream()),
            Group::Stable(g) => TokenStream::Stable(g.stream()),
        }
    }

    pub fn span(&self) -> Span {
        match self {
            Group::Nightly(g) => Span::Nightly(g.span()),
            Group::Stable(g) => Span::Stable(g.span()),
        }
    }

    #[cfg(super_unstable)]
    pub fn span_open(&self) -> Span {
        match self {
            Group::Nightly(g) => Span::Nightly(g.span_open()),
            Group::Stable(g) => Span::Stable(g.span_open()),
        }
    }

    #[cfg(super_unstable)]
    pub fn span_close(&self) -> Span {
        match self {
            Group::Nightly(g) => Span::Nightly(g.span_close()),
            Group::Stable(g) => Span::Stable(g.span_close()),
        }
    }

    pub fn set_span(&mut self, span: Span) {
        match (self, span) {
            (Group::Nightly(g), Span::Nightly(s)) => g.set_span(s),
            (Group::Stable(g), Span::Stable(s)) => g.set_span(s),
            _ => mismatch(),
        }
    }

    fn unwrap_nightly(self) -> proc_macro::Group {
        match self {
            Group::Nightly(g) => g,
            Group::Stable(_) => mismatch(),
        }
    }
}

impl From<stable::Group> for Group {
    fn from(g: stable::Group) -> Self {
        Group::Stable(g)
    }
}

impl fmt::Display for Group {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Group::Nightly(group) => group.fmt(formatter),
            Group::Stable(group) => group.fmt(formatter),
        }
    }
}

impl fmt::Debug for Group {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Group::Nightly(group) => group.fmt(formatter),
            Group::Stable(group) => group.fmt(formatter),
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
            Span::Nightly(s) => {
                let p: proc_macro::TokenStream = string.parse().unwrap();
                let ident = match p.into_iter().next() {
                    Some(proc_macro::TokenTree::Ident(mut i)) => {
                        i.set_span(s);
                        i
                    }
                    _ => panic!(),
                };
                Ident::Nightly(ident)
            }
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

impl PartialEq for Ident {
    fn eq(&self, other: &Ident) -> bool {
        match (self, other) {
            (Ident::Nightly(t), Ident::Nightly(o)) => t.to_string() == o.to_string(),
            (Ident::Stable(t), Ident::Stable(o)) => t == o,
            _ => mismatch(),
        }
    }
}

impl<T> PartialEq<T> for Ident
where
    T: ?Sized + AsRef<str>,
{
    fn eq(&self, other: &T) -> bool {
        let other = other.as_ref();
        match self {
            Ident::Nightly(t) => t.to_string() == other,
            Ident::Stable(t) => t == other,
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

    #[cfg(u128)]
    suffixed_numbers! {
        i128_suffixed => i128,
        u128_suffixed => u128,
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

    #[cfg(u128)]
    unsuffixed_integers! {
        i128_unsuffixed => i128,
        u128_unsuffixed => u128,
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
