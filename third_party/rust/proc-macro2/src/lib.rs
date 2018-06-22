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
//! # Unstable Features
//!
//! `proc-macro2` supports exporting some methods from `proc_macro` which are
//! currently highly unstable, and may not be stabilized in the first pass of
//! `proc_macro` stabilizations. These features are not exported by default.
//! Minor versions of `proc-macro2` may make breaking changes to them at any
//! time.
//!
//! To enable these features, the `procmacro2_semver_exempt` config flag must be
//! passed to rustc.
//!
//! ```sh
//! RUSTFLAGS='--cfg procmacro2_semver_exempt' cargo build
//! ```
//!
//! Note that this must not only be done for your crate, but for any crate that
//! depends on your crate. This infectious nature is intentional, as it serves
//! as a reminder that you are outside of the normal semver guarantees.
//!
//! [`proc_macro`]: https://doc.rust-lang.org/proc_macro/
//! [ts]: https://doc.rust-lang.org/proc_macro/struct.TokenStream.html

// Proc-macro2 types in rustdoc of other crates get linked to here.
#![doc(html_root_url = "https://docs.rs/proc-macro2/0.4.6")]
#![cfg_attr(feature = "nightly", feature(proc_macro))]

#[cfg(feature = "proc-macro")]
extern crate proc_macro;
extern crate unicode_xid;

use std::cmp::Ordering;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::iter::FromIterator;
use std::marker;
use std::rc::Rc;
use std::str::FromStr;

#[macro_use]
mod strnom;
mod stable;

#[cfg(not(feature = "nightly"))]
use stable as imp;
#[path = "unstable.rs"]
#[cfg(feature = "nightly")]
mod imp;

/// An abstract stream of tokens, or more concretely a sequence of token trees.
///
/// This type provides interfaces for iterating over token trees and for
/// collecting token trees into one stream.
///
/// Token stream is both the input and output of `#[proc_macro]`,
/// `#[proc_macro_attribute]` and `#[proc_macro_derive]` definitions.
#[derive(Clone)]
pub struct TokenStream {
    inner: imp::TokenStream,
    _marker: marker::PhantomData<Rc<()>>,
}

/// Error returned from `TokenStream::from_str`.
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

    fn _new_stable(inner: stable::TokenStream) -> TokenStream {
        TokenStream {
            inner: inner.into(),
            _marker: marker::PhantomData,
        }
    }

    /// Returns an empty `TokenStream` containing no token trees.
    pub fn new() -> TokenStream {
        TokenStream::_new(imp::TokenStream::new())
    }

    #[deprecated(since = "0.4.4", note = "please use TokenStream::new")]
    pub fn empty() -> TokenStream {
        TokenStream::new()
    }

    /// Checks if this `TokenStream` is empty.
    pub fn is_empty(&self) -> bool {
        self.inner.is_empty()
    }
}

/// Attempts to break the string into tokens and parse those tokens into a token
/// stream.
///
/// May fail for a number of reasons, for example, if the string contains
/// unbalanced delimiters or characters not existing in the language.
///
/// NOTE: Some errors may cause panics instead of returning `LexError`. We
/// reserve the right to change these errors into `LexError`s later.
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

impl Extend<TokenTree> for TokenStream {
    fn extend<I: IntoIterator<Item = TokenTree>>(&mut self, streams: I) {
        self.inner.extend(streams)
    }
}

/// Collects a number of token trees into a single stream.
impl FromIterator<TokenTree> for TokenStream {
    fn from_iter<I: IntoIterator<Item = TokenTree>>(streams: I) -> Self {
        TokenStream::_new(streams.into_iter().collect())
    }
}

/// Prints the token stream as a string that is supposed to be losslessly
/// convertible back into the same token stream (modulo spans), except for
/// possibly `TokenTree::Group`s with `Delimiter::None` delimiters and negative
/// numeric literals.
impl fmt::Display for TokenStream {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

/// Prints token in a form convenient for debugging.
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

/// The source file of a given `Span`.
///
/// This type is semver exempt and not exposed by default.
#[cfg(procmacro2_semver_exempt)]
#[derive(Clone, PartialEq, Eq)]
pub struct SourceFile(imp::SourceFile);

#[cfg(procmacro2_semver_exempt)]
impl SourceFile {
    /// Get the path to this source file.
    ///
    /// ### Note
    ///
    /// If the code span associated with this `SourceFile` was generated by an
    /// external macro, this may not be an actual path on the filesystem. Use
    /// [`is_real`] to check.
    ///
    /// Also note that even if `is_real` returns `true`, if
    /// `--remap-path-prefix` was passed on the command line, the path as given
    /// may not actually be valid.
    ///
    /// [`is_real`]: #method.is_real
    pub fn path(&self) -> &FileName {
        self.0.path()
    }

    /// Returns `true` if this source file is a real source file, and not
    /// generated by an external macro's expansion.
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

/// A line-column pair representing the start or end of a `Span`.
///
/// This type is semver exempt and not exposed by default.
#[cfg(procmacro2_semver_exempt)]
pub struct LineColumn {
    /// The 1-indexed line in the source file on which the span starts or ends
    /// (inclusive).
    pub line: usize,
    /// The 0-indexed column (in UTF-8 characters) in the source file on which
    /// the span starts or ends (inclusive).
    pub column: usize,
}

/// A region of source code, along with macro expansion information.
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

    fn _new_stable(inner: stable::Span) -> Span {
        Span {
            inner: inner.into(),
            _marker: marker::PhantomData,
        }
    }

    /// The span of the invocation of the current procedural macro.
    ///
    /// Identifiers created with this span will be resolved as if they were
    /// written directly at the macro call location (call-site hygiene) and
    /// other code at the macro call site will be able to refer to them as well.
    pub fn call_site() -> Span {
        Span::_new(imp::Span::call_site())
    }

    /// A span that resolves at the macro definition site.
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn def_site() -> Span {
        Span::_new(imp::Span::def_site())
    }

    /// Creates a new span with the same line/column information as `self` but
    /// that resolves symbols as though it were at `other`.
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn resolved_at(&self, other: Span) -> Span {
        Span::_new(self.inner.resolved_at(other.inner))
    }

    /// Creates a new span with the same name resolution behavior as `self` but
    /// with the line/column information of `other`.
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn located_at(&self, other: Span) -> Span {
        Span::_new(self.inner.located_at(other.inner))
    }

    /// This method is only available when the `"nightly"` feature is enabled.
    #[cfg(all(feature = "nightly", feature = "proc-macro"))]
    pub fn unstable(self) -> proc_macro::Span {
        self.inner.unstable()
    }

    /// The original source file into which this span points.
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn source_file(&self) -> SourceFile {
        SourceFile(self.inner.source_file())
    }

    /// Get the starting line/column in the source file for this span.
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn start(&self) -> LineColumn {
        let imp::LineColumn { line, column } = self.inner.start();
        LineColumn {
            line: line,
            column: column,
        }
    }

    /// Get the ending line/column in the source file for this span.
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn end(&self) -> LineColumn {
        let imp::LineColumn { line, column } = self.inner.end();
        LineColumn {
            line: line,
            column: column,
        }
    }

    /// Create a new span encompassing `self` and `other`.
    ///
    /// Returns `None` if `self` and `other` are from different files.
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn join(&self, other: Span) -> Option<Span> {
        self.inner.join(other.inner).map(Span::_new)
    }

    /// Compares to spans to see if they're equal.
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn eq(&self, other: &Span) -> bool {
        self.inner.eq(&other.inner)
    }
}

/// Prints a span in a form convenient for debugging.
impl fmt::Debug for Span {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

/// A single token or a delimited sequence of token trees (e.g. `[1, (), ..]`).
#[derive(Clone)]
pub enum TokenTree {
    /// A token stream surrounded by bracket delimiters.
    Group(Group),
    /// An identifier.
    Ident(Ident),
    /// A single punctuation character (`+`, `,`, `$`, etc.).
    Punct(Punct),
    /// A literal character (`'a'`), string (`"hello"`), number (`2.3`), etc.
    Literal(Literal),
}

impl TokenTree {
    /// Returns the span of this tree, delegating to the `span` method of
    /// the contained token or a delimited stream.
    pub fn span(&self) -> Span {
        match *self {
            TokenTree::Group(ref t) => t.span(),
            TokenTree::Ident(ref t) => t.span(),
            TokenTree::Punct(ref t) => t.span(),
            TokenTree::Literal(ref t) => t.span(),
        }
    }

    /// Configures the span for *only this token*.
    ///
    /// Note that if this token is a `Group` then this method will not configure
    /// the span of each of the internal tokens, this will simply delegate to
    /// the `set_span` method of each variant.
    pub fn set_span(&mut self, span: Span) {
        match *self {
            TokenTree::Group(ref mut t) => t.set_span(span),
            TokenTree::Ident(ref mut t) => t.set_span(span),
            TokenTree::Punct(ref mut t) => t.set_span(span),
            TokenTree::Literal(ref mut t) => t.set_span(span),
        }
    }
}

impl From<Group> for TokenTree {
    fn from(g: Group) -> TokenTree {
        TokenTree::Group(g)
    }
}

impl From<Ident> for TokenTree {
    fn from(g: Ident) -> TokenTree {
        TokenTree::Ident(g)
    }
}

impl From<Punct> for TokenTree {
    fn from(g: Punct) -> TokenTree {
        TokenTree::Punct(g)
    }
}

impl From<Literal> for TokenTree {
    fn from(g: Literal) -> TokenTree {
        TokenTree::Literal(g)
    }
}

/// Prints the token tree as a string that is supposed to be losslessly
/// convertible back into the same token tree (modulo spans), except for
/// possibly `TokenTree::Group`s with `Delimiter::None` delimiters and negative
/// numeric literals.
impl fmt::Display for TokenTree {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            TokenTree::Group(ref t) => t.fmt(f),
            TokenTree::Ident(ref t) => t.fmt(f),
            TokenTree::Punct(ref t) => t.fmt(f),
            TokenTree::Literal(ref t) => t.fmt(f),
        }
    }
}

/// Prints token tree in a form convenient for debugging.
impl fmt::Debug for TokenTree {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // Each of these has the name in the struct type in the derived debug,
        // so don't bother with an extra layer of indirection
        match *self {
            TokenTree::Group(ref t) => t.fmt(f),
            TokenTree::Ident(ref t) => {
                let mut debug = f.debug_struct("Ident");
                debug.field("sym", &format_args!("{}", t));
                #[cfg(any(feature = "nightly", procmacro2_semver_exempt))]
                debug.field("span", &t.span());
                debug.finish()
            }
            TokenTree::Punct(ref t) => t.fmt(f),
            TokenTree::Literal(ref t) => t.fmt(f),
        }
    }
}

/// A delimited token stream.
///
/// A `Group` internally contains a `TokenStream` which is surrounded by
/// `Delimiter`s.
#[derive(Clone)]
pub struct Group {
    delimiter: Delimiter,
    stream: TokenStream,
    span: Span,
}

/// Describes how a sequence of token trees is delimited.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Delimiter {
    /// `( ... )`
    Parenthesis,
    /// `{ ... }`
    Brace,
    /// `[ ... ]`
    Bracket,
    /// `Ø ... Ø`
    ///
    /// An implicit delimiter, that may, for example, appear around tokens
    /// coming from a "macro variable" `$var`. It is important to preserve
    /// operator priorities in cases like `$var * 3` where `$var` is `1 + 2`.
    /// Implicit delimiters may not survive roundtrip of a token stream through
    /// a string.
    None,
}

impl Group {
    /// Creates a new `Group` with the given delimiter and token stream.
    ///
    /// This constructor will set the span for this group to
    /// `Span::call_site()`. To change the span you can use the `set_span`
    /// method below.
    pub fn new(delimiter: Delimiter, stream: TokenStream) -> Group {
        Group {
            delimiter: delimiter,
            stream: stream,
            span: Span::call_site(),
        }
    }

    /// Returns the delimiter of this `Group`
    pub fn delimiter(&self) -> Delimiter {
        self.delimiter
    }

    /// Returns the `TokenStream` of tokens that are delimited in this `Group`.
    ///
    /// Note that the returned token stream does not include the delimiter
    /// returned above.
    pub fn stream(&self) -> TokenStream {
        self.stream.clone()
    }

    /// Returns the span for the delimiters of this token stream, spanning the
    /// entire `Group`.
    pub fn span(&self) -> Span {
        self.span
    }

    /// Configures the span for this `Group`'s delimiters, but not its internal
    /// tokens.
    ///
    /// This method will **not** set the span of all the internal tokens spanned
    /// by this group, but rather it will only set the span of the delimiter
    /// tokens at the level of the `Group`.
    pub fn set_span(&mut self, span: Span) {
        self.span = span;
    }
}

/// Prints the group as a string that should be losslessly convertible back
/// into the same group (modulo spans), except for possibly `TokenTree::Group`s
/// with `Delimiter::None` delimiters.
impl fmt::Display for Group {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let (left, right) = match self.delimiter {
            Delimiter::Parenthesis => ("(", ")"),
            Delimiter::Brace => ("{", "}"),
            Delimiter::Bracket => ("[", "]"),
            Delimiter::None => ("", ""),
        };

        f.write_str(left)?;
        self.stream.fmt(f)?;
        f.write_str(right)?;

        Ok(())
    }
}

impl fmt::Debug for Group {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let mut debug = fmt.debug_struct("Group");
        debug.field("delimiter", &self.delimiter);
        debug.field("stream", &self.stream);
        #[cfg(procmacro2_semver_exempt)]
        debug.field("span", &self.span);
        debug.finish()
    }
}

/// An `Punct` is an single punctuation character like `+`, `-` or `#`.
///
/// Multicharacter operators like `+=` are represented as two instances of
/// `Punct` with different forms of `Spacing` returned.
#[derive(Clone)]
pub struct Punct {
    op: char,
    spacing: Spacing,
    span: Span,
}

/// Whether an `Punct` is followed immediately by another `Punct` or followed by
/// another token or whitespace.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Spacing {
    /// E.g. `+` is `Alone` in `+ =`, `+ident` or `+()`.
    Alone,
    /// E.g. `+` is `Joint` in `+=` or `'#`.
    ///
    /// Additionally, single quote `'` can join with identifiers to form
    /// lifetimes `'ident`.
    Joint,
}

impl Punct {
    /// Creates a new `Punct` from the given character and spacing.
    ///
    /// The `ch` argument must be a valid punctuation character permitted by the
    /// language, otherwise the function will panic.
    ///
    /// The returned `Punct` will have the default span of `Span::call_site()`
    /// which can be further configured with the `set_span` method below.
    pub fn new(op: char, spacing: Spacing) -> Punct {
        Punct {
            op: op,
            spacing: spacing,
            span: Span::call_site(),
        }
    }

    /// Returns the value of this punctuation character as `char`.
    pub fn as_char(&self) -> char {
        self.op
    }

    /// Returns the spacing of this punctuation character, indicating whether
    /// it's immediately followed by another `Punct` in the token stream, so
    /// they can potentially be combined into a multicharacter operator
    /// (`Joint`), or it's followed by some other token or whitespace (`Alone`)
    /// so the operator has certainly ended.
    pub fn spacing(&self) -> Spacing {
        self.spacing
    }

    /// Returns the span for this punctuation character.
    pub fn span(&self) -> Span {
        self.span
    }

    /// Configure the span for this punctuation character.
    pub fn set_span(&mut self, span: Span) {
        self.span = span;
    }
}

/// Prints the punctuation character as a string that should be losslessly
/// convertible back into the same character.
impl fmt::Display for Punct {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.op.fmt(f)
    }
}

impl fmt::Debug for Punct {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let mut debug = fmt.debug_struct("Punct");
        debug.field("op", &self.op);
        debug.field("spacing", &self.spacing);
        #[cfg(procmacro2_semver_exempt)]
        debug.field("span", &self.span);
        debug.finish()
    }
}

/// A word of Rust code, which may be a keyword or legal variable name.
///
/// An identifier consists of at least one Unicode code point, the first of
/// which has the XID_Start property and the rest of which have the XID_Continue
/// property.
///
/// - The empty string is not an identifier. Use `Option<Ident>`.
/// - A lifetime is not an identifier. Use `syn::Lifetime` instead.
///
/// An identifier constructed with `Ident::new` is permitted to be a Rust
/// keyword, though parsing one through its [`Synom`] implementation rejects
/// Rust keywords. Use `call!(Ident::parse_any)` when parsing to match the
/// behaviour of `Ident::new`.
///
/// [`Synom`]: https://docs.rs/syn/0.14/syn/synom/trait.Synom.html
///
/// # Examples
///
/// A new ident can be created from a string using the `Ident::new` function.
/// A span must be provided explicitly which governs the name resolution
/// behavior of the resulting identifier.
///
/// ```rust
/// extern crate proc_macro2;
///
/// use proc_macro2::{Ident, Span};
///
/// fn main() {
///     let call_ident = Ident::new("calligraphy", Span::call_site());
///
///     println!("{}", call_ident);
/// }
/// ```
///
/// An ident can be interpolated into a token stream using the `quote!` macro.
///
/// ```rust
/// #[macro_use]
/// extern crate quote;
///
/// extern crate proc_macro2;
///
/// use proc_macro2::{Ident, Span};
///
/// fn main() {
///     let ident = Ident::new("demo", Span::call_site());
///
///     // Create a variable binding whose name is this ident.
///     let expanded = quote! { let #ident = 10; };
///
///     // Create a variable binding with a slightly different name.
///     let temp_ident = Ident::new(&format!("new_{}", ident), Span::call_site());
///     let expanded = quote! { let #temp_ident = 10; };
/// }
/// ```
///
/// A string representation of the ident is available through the `to_string()`
/// method.
///
/// ```rust
/// # extern crate proc_macro2;
/// #
/// # use proc_macro2::{Ident, Span};
/// #
/// # let ident = Ident::new("another_identifier", Span::call_site());
/// #
/// // Examine the ident as a string.
/// let ident_string = ident.to_string();
/// if ident_string.len() > 60 {
///     println!("Very long identifier: {}", ident_string)
/// }
/// ```
#[derive(Clone)]
pub struct Ident {
    inner: imp::Ident,
    _marker: marker::PhantomData<Rc<()>>,
}

impl Ident {
    fn _new(inner: imp::Ident) -> Ident {
        Ident {
            inner: inner,
            _marker: marker::PhantomData,
        }
    }

    /// Creates a new `Ident` with the given `string` as well as the specified
    /// `span`.
    ///
    /// The `string` argument must be a valid identifier permitted by the
    /// language, otherwise the function will panic.
    ///
    /// Note that `span`, currently in rustc, configures the hygiene information
    /// for this identifier.
    ///
    /// As of this time `Span::call_site()` explicitly opts-in to "call-site"
    /// hygiene meaning that identifiers created with this span will be resolved
    /// as if they were written directly at the location of the macro call, and
    /// other code at the macro call site will be able to refer to them as well.
    ///
    /// Later spans like `Span::def_site()` will allow to opt-in to
    /// "definition-site" hygiene meaning that identifiers created with this
    /// span will be resolved at the location of the macro definition and other
    /// code at the macro call site will not be able to refer to them.
    ///
    /// Due to the current importance of hygiene this constructor, unlike other
    /// tokens, requires a `Span` to be specified at construction.
    ///
    /// # Panics
    ///
    /// Panics if the input string is neither a keyword nor a legal variable
    /// name.
    pub fn new(string: &str, span: Span) -> Ident {
        Ident::_new(imp::Ident::new(string, span.inner))
    }

    /// Same as `Ident::new`, but creates a raw identifier (`r#ident`).
    ///
    /// This method is semver exempt and not exposed by default.
    #[cfg(procmacro2_semver_exempt)]
    pub fn new_raw(string: &str, span: Span) -> Ident {
        Ident::_new_raw(string, span)
    }

    fn _new_raw(string: &str, span: Span) -> Ident {
        Ident::_new(imp::Ident::new_raw(string, span.inner))
    }

    /// Returns the span of this `Ident`.
    pub fn span(&self) -> Span {
        Span::_new(self.inner.span())
    }

    /// Configures the span of this `Ident`, possibly changing its hygiene
    /// context.
    pub fn set_span(&mut self, span: Span) {
        self.inner.set_span(span.inner);
    }
}

impl PartialEq for Ident {
    fn eq(&self, other: &Ident) -> bool {
        self.to_string() == other.to_string()
    }
}

impl<T> PartialEq<T> for Ident
where
    T: ?Sized + AsRef<str>,
{
    fn eq(&self, other: &T) -> bool {
        self.to_string() == other.as_ref()
    }
}

impl Eq for Ident {}

impl PartialOrd for Ident {
    fn partial_cmp(&self, other: &Ident) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Ident {
    fn cmp(&self, other: &Ident) -> Ordering {
        self.to_string().cmp(&other.to_string())
    }
}

impl Hash for Ident {
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.to_string().hash(hasher)
    }
}

/// Prints the identifier as a string that should be losslessly convertible back
/// into the same identifier.
impl fmt::Display for Ident {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

impl fmt::Debug for Ident {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

/// A literal string (`"hello"`), byte string (`b"hello"`), character (`'a'`),
/// byte character (`b'a'`), an integer or floating point number with or without
/// a suffix (`1`, `1u8`, `2.3`, `2.3f32`).
///
/// Boolean literals like `true` and `false` do not belong here, they are
/// `Ident`s.
#[derive(Clone)]
pub struct Literal {
    inner: imp::Literal,
    _marker: marker::PhantomData<Rc<()>>,
}

macro_rules! suffixed_int_literals {
    ($($name:ident => $kind:ident,)*) => ($(
        /// Creates a new suffixed integer literal with the specified value.
        ///
        /// This function will create an integer like `1u32` where the integer
        /// value specified is the first part of the token and the integral is
        /// also suffixed at the end. Literals created from negative numbers may
        /// not survive rountrips through `TokenStream` or strings and may be
        /// broken into two tokens (`-` and positive literal).
        ///
        /// Literals created through this method have the `Span::call_site()`
        /// span by default, which can be configured with the `set_span` method
        /// below.
        pub fn $name(n: $kind) -> Literal {
            Literal::_new(imp::Literal::$name(n))
        }
    )*)
}

macro_rules! unsuffixed_int_literals {
    ($($name:ident => $kind:ident,)*) => ($(
        /// Creates a new unsuffixed integer literal with the specified value.
        ///
        /// This function will create an integer like `1` where the integer
        /// value specified is the first part of the token. No suffix is
        /// specified on this token, meaning that invocations like
        /// `Literal::i8_unsuffixed(1)` are equivalent to
        /// `Literal::u32_unsuffixed(1)`. Literals created from negative numbers
        /// may not survive rountrips through `TokenStream` or strings and may
        /// be broken into two tokens (`-` and positive literal).
        ///
        /// Literals created through this method have the `Span::call_site()`
        /// span by default, which can be configured with the `set_span` method
        /// below.
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

    fn _new_stable(inner: stable::Literal) -> Literal {
        Literal {
            inner: inner.into(),
            _marker: marker::PhantomData,
        }
    }

    suffixed_int_literals! {
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
    }

    unsuffixed_int_literals! {
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

    /// Creates a new unsuffixed floating-point literal.
    ///
    /// This constructor is similar to those like `Literal::i8_unsuffixed` where
    /// the float's value is emitted directly into the token but no suffix is
    /// used, so it may be inferred to be a `f64` later in the compiler.
    /// Literals created from negative numbers may not survive rountrips through
    /// `TokenStream` or strings and may be broken into two tokens (`-` and
    /// positive literal).
    ///
    /// # Panics
    ///
    /// This function requires that the specified float is finite, for example
    /// if it is infinity or NaN this function will panic.
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

/// Public implementation details for the `TokenStream` type, such as iterators.
pub mod token_stream {
    use std::fmt;
    use std::marker;
    use std::rc::Rc;

    use imp;
    pub use TokenStream;
    use TokenTree;

    /// An iterator over `TokenStream`'s `TokenTree`s.
    ///
    /// The iteration is "shallow", e.g. the iterator doesn't recurse into
    /// delimited groups, and returns whole groups as token trees.
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
