use std::ascii;
use std::fmt;
use std::iter;
use std::str::FromStr;

use proc_macro;

use {TokenTree, TokenNode, Delimiter, Spacing};

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
    fn from(tree: TokenTree) -> TokenStream {
        TokenStream(proc_macro::TokenTree {
            span: (tree.span.0).0,
            kind: match tree.kind {
                TokenNode::Group(delim, s) => {
                    let delim = match delim {
                        Delimiter::Parenthesis => proc_macro::Delimiter::Parenthesis,
                        Delimiter::Bracket => proc_macro::Delimiter::Bracket,
                        Delimiter::Brace => proc_macro::Delimiter::Brace,
                        Delimiter::None => proc_macro::Delimiter::None,
                    };
                    proc_macro::TokenNode::Group(delim, (s.0).0)
                }
                TokenNode::Op(ch, kind) => {
                    let kind = match kind {
                        Spacing::Joint => proc_macro::Spacing::Joint,
                        Spacing::Alone => proc_macro::Spacing::Alone,
                    };
                    proc_macro::TokenNode::Op(ch, kind)
                }
                TokenNode::Term(s) => {
                    proc_macro::TokenNode::Term((s.0).0)
                }
                TokenNode::Literal(l) => {
                    proc_macro::TokenNode::Literal((l.0).0)
                }
            },
        }.into())
    }
}

impl iter::FromIterator<TokenStream> for TokenStream {
    fn from_iter<I: IntoIterator<Item=TokenStream>>(streams: I) -> Self {
        let streams = streams.into_iter().map(|s| s.0);
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

pub struct TokenTreeIter(proc_macro::TokenTreeIter);

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
        let token = match self.0.next() {
            Some(n) => n,
            None => return None,
        };
        Some(TokenTree {
            span: ::Span(Span(token.span)),
            kind: match token.kind {
                proc_macro::TokenNode::Group(delim, s) => {
                    let delim = match delim {
                        proc_macro::Delimiter::Parenthesis => Delimiter::Parenthesis,
                        proc_macro::Delimiter::Bracket => Delimiter::Bracket,
                        proc_macro::Delimiter::Brace => Delimiter::Brace,
                        proc_macro::Delimiter::None => Delimiter::None,
                    };
                    TokenNode::Group(delim, ::TokenStream(TokenStream(s)))
                }
                proc_macro::TokenNode::Op(ch, kind) => {
                    let kind = match kind {
                        proc_macro::Spacing::Joint => Spacing::Joint,
                        proc_macro::Spacing::Alone => Spacing::Alone,
                    };
                    TokenNode::Op(ch, kind)
                }
                proc_macro::TokenNode::Term(s) => {
                    TokenNode::Term(::Term(Term(s)))
                }
                proc_macro::TokenNode::Literal(l) => {
                    TokenNode::Literal(::Literal(Literal(l)))
                }
            },
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

#[cfg(procmacro2_semver_exempt)]
#[derive(Clone, PartialEq, Eq)]
pub struct FileName(String);

#[cfg(procmacro2_semver_exempt)]
impl fmt::Display for FileName {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

// NOTE: We have to generate our own filename object here because we can't wrap
// the one provided by proc_macro.
#[cfg(procmacro2_semver_exempt)]
#[derive(Clone, PartialEq, Eq)]
pub struct SourceFile(proc_macro::SourceFile, FileName);

#[cfg(procmacro2_semver_exempt)]
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

#[cfg(procmacro2_semver_exempt)]
impl AsRef<FileName> for SourceFile {
    fn as_ref(&self) -> &FileName {
        self.path()
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
pub struct Span(proc_macro::Span);

impl From<proc_macro::Span> for ::Span {
    fn from(proc_span: proc_macro::Span) -> ::Span {
        ::Span(Span(proc_span))
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

    #[cfg(procmacro2_semver_exempt)]
    pub fn source_file(&self) -> SourceFile {
        SourceFile::new(self.0.source_file())
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn start(&self) -> LineColumn {
        let proc_macro::LineColumn{ line, column } = self.0.start();
        LineColumn { line, column }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn end(&self) -> LineColumn {
        let proc_macro::LineColumn{ line, column } = self.0.end();
        LineColumn { line, column }
    }

    #[cfg(procmacro2_semver_exempt)]
    pub fn join(&self, other: Span) -> Option<Span> {
        self.0.join(other.0).map(Span)
    }
}

impl fmt::Debug for Span {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[derive(Copy, Clone)]
pub struct Term(proc_macro::Term);

impl Term {
    pub fn intern(string: &str) -> Term {
        Term(proc_macro::Term::intern(string))
    }

    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }
}

impl fmt::Debug for Term {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[derive(Clone)]
pub struct Literal(proc_macro::Literal);

impl Literal {
    pub fn byte_char(byte: u8) -> Literal {
        match byte {
            0 => Literal(to_literal("b'\\0'")),
            b'\"' => Literal(to_literal("b'\"'")),
            n => {
                let mut escaped = "b'".to_string();
                escaped.extend(ascii::escape_default(n).map(|c| c as char));
                escaped.push('\'');
                Literal(to_literal(&escaped))
            }
        }
    }

    pub fn byte_string(bytes: &[u8]) -> Literal {
        Literal(proc_macro::Literal::byte_string(bytes))
    }

    pub fn doccomment(s: &str) -> Literal {
        Literal(to_literal(s))
    }

    pub fn float(s: f64) -> Literal {
        Literal(proc_macro::Literal::float(s))
    }

    pub fn integer(s: i64) -> Literal {
        Literal(proc_macro::Literal::integer(s.into()))
    }

    pub fn raw_string(s: &str, pounds: usize) -> Literal {
        let mut ret = format!("r");
        ret.extend((0..pounds).map(|_| "#"));
        ret.push('"');
        ret.push_str(s);
        ret.push('"');
        ret.extend((0..pounds).map(|_| "#"));
        Literal(to_literal(&ret))
    }

    pub fn raw_byte_string(s: &str, pounds: usize) -> Literal {
        let mut ret = format!("br");
        ret.extend((0..pounds).map(|_| "#"));
        ret.push('"');
        ret.push_str(s);
        ret.push('"');
        ret.extend((0..pounds).map(|_| "#"));
        Literal(to_literal(&ret))
    }
}

impl fmt::Display for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl fmt::Debug for Literal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

fn to_literal(s: &str) -> proc_macro::Literal {
    let stream = s.parse::<proc_macro::TokenStream>().unwrap();
    match stream.into_iter().next().unwrap().kind {
        proc_macro::TokenNode::Literal(l) => l,
        _ => unreachable!(),
    }
}

macro_rules! ints {
    ($($t:ident,)*) => {$(
        impl From<$t> for Literal {
            fn from(t: $t) -> Literal {
                Literal(proc_macro::Literal::$t(t))
            }
        }
    )*}
}

ints! {
    u8, u16, u32, u64, usize,
    i8, i16, i32, i64, isize,
}

macro_rules! floats {
    ($($t:ident,)*) => {$(
        impl From<$t> for Literal {
            fn from(t: $t) -> Literal {
                Literal(proc_macro::Literal::$t(t))
            }
        }
    )*}
}

floats! {
    f32, f64,
}

impl<'a> From<&'a str> for Literal {
    fn from(t: &'a str) -> Literal {
        Literal(proc_macro::Literal::string(t))
    }
}

impl From<char> for Literal {
    fn from(t: char) -> Literal {
        Literal(proc_macro::Literal::character(t))
    }
}
