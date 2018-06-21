// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Syn is a parsing library for parsing a stream of Rust tokens into a syntax
//! tree of Rust source code.
//!
//! Currently this library is geared toward the [custom derive] use case but
//! contains some APIs that may be useful for Rust procedural macros more
//! generally.
//!
//! [custom derive]: https://github.com/rust-lang/rfcs/blob/master/text/1681-macros-1.1.md
//!
//! - **Data structures** — Syn provides a complete syntax tree that can
//!   represent any valid Rust source code. The syntax tree is rooted at
//!   [`syn::File`] which represents a full source file, but there are other
//!   entry points that may be useful to procedural macros including
//!   [`syn::Item`], [`syn::Expr`] and [`syn::Type`].
//!
//! - **Custom derives** — Of particular interest to custom derives is
//!   [`syn::DeriveInput`] which is any of the three legal input items to a
//!   derive macro. An example below shows using this type in a library that can
//!   derive implementations of a trait of your own.
//!
//! - **Parser combinators** — Parsing in Syn is built on a suite of public
//!   parser combinator macros that you can use for parsing any token-based
//!   syntax you dream up within a `functionlike!(...)` procedural macro. Every
//!   syntax tree node defined by Syn is individually parsable and may be used
//!   as a building block for custom syntaxes, or you may do it all yourself
//!   working from the most primitive tokens.
//!
//! - **Location information** — Every token parsed by Syn is associated with a
//!   `Span` that tracks line and column information back to the source of that
//!   token. These spans allow a procedural macro to display detailed error
//!   messages pointing to all the right places in the user's code. There is an
//!   example of this below.
//!
//! - **Feature flags** — Functionality is aggressively feature gated so your
//!   procedural macros enable only what they need, and do not pay in compile
//!   time for all the rest.
//!
//! [`syn::File`]: struct.File.html
//! [`syn::Item`]: enum.Item.html
//! [`syn::Expr`]: enum.Expr.html
//! [`syn::Type`]: enum.Type.html
//! [`syn::DeriveInput`]: struct.DeriveInput.html
//!
//! *Version requirement: Syn supports any compiler version back to Rust's very
//! first support for procedural macros in Rust 1.15.0. Some features especially
//! around error reporting are only available in newer compilers or on the
//! nightly channel.*
//!
//! ## Example of a custom derive
//!
//! The canonical custom derive using Syn looks like this. We write an ordinary
//! Rust function tagged with a `proc_macro_derive` attribute and the name of
//! the trait we are deriving. Any time that derive appears in the user's code,
//! the Rust compiler passes their data structure as tokens into our macro. We
//! get to execute arbitrary Rust code to figure out what to do with those
//! tokens, then hand some tokens back to the compiler to compile into the
//! user's crate.
//!
//! [`TokenStream`]: https://doc.rust-lang.org/proc_macro/struct.TokenStream.html
//!
//! ```toml
//! [dependencies]
//! syn = "0.14"
//! quote = "0.6"
//!
//! [lib]
//! proc-macro = true
//! ```
//!
//! ```rust
//! extern crate proc_macro;
//! extern crate syn;
//!
//! #[macro_use]
//! extern crate quote;
//!
//! use proc_macro::TokenStream;
//! use syn::DeriveInput;
//!
//! # const IGNORE_TOKENS: &str = stringify! {
//! #[proc_macro_derive(MyMacro)]
//! # };
//! pub fn my_macro(input: TokenStream) -> TokenStream {
//!     // Parse the input tokens into a syntax tree
//!     let input: DeriveInput = syn::parse(input).unwrap();
//!
//!     // Build the output, possibly using quasi-quotation
//!     let expanded = quote! {
//!         // ...
//!     };
//!
//!     // Hand the output tokens back to the compiler
//!     expanded.into()
//! }
//! #
//! # fn main() {}
//! ```
//!
//! The [`heapsize`] example directory shows a complete working Macros 1.1
//! implementation of a custom derive. It works on any Rust compiler \>=1.15.0.
//! The example derives a `HeapSize` trait which computes an estimate of the
//! amount of heap memory owned by a value.
//!
//! [`heapsize`]: https://github.com/dtolnay/syn/tree/master/examples/heapsize
//!
//! ```rust
//! pub trait HeapSize {
//!     /// Total number of bytes of heap memory owned by `self`.
//!     fn heap_size_of_children(&self) -> usize;
//! }
//! ```
//!
//! The custom derive allows users to write `#[derive(HeapSize)]` on data
//! structures in their program.
//!
//! ```rust
//! # const IGNORE_TOKENS: &str = stringify! {
//! #[derive(HeapSize)]
//! # };
//! struct Demo<'a, T: ?Sized> {
//!     a: Box<T>,
//!     b: u8,
//!     c: &'a str,
//!     d: String,
//! }
//! ```
//!
//! ## Spans and error reporting
//!
//! The [`heapsize2`] example directory is an extension of the `heapsize`
//! example that demonstrates some of the hygiene and error reporting properties
//! of Macros 2.0. This example currently requires a nightly Rust compiler
//! \>=1.24.0-nightly but we are working to stabilize all of the APIs involved.
//!
//! [`heapsize2`]: https://github.com/dtolnay/syn/tree/master/examples/heapsize2
//!
//! The token-based procedural macro API provides great control over where the
//! compiler's error messages are displayed in user code. Consider the error the
//! user sees if one of their field types does not implement `HeapSize`.
//!
//! ```rust
//! # const IGNORE_TOKENS: &str = stringify! {
//! #[derive(HeapSize)]
//! # };
//! struct Broken {
//!     ok: String,
//!     bad: std::thread::Thread,
//! }
//! ```
//!
//! In the Macros 1.1 string-based procedural macro world, the resulting error
//! would point unhelpfully to the invocation of the derive macro and not to the
//! actual problematic field.
//!
//! ```text
//! error[E0599]: no method named `heap_size_of_children` found for type `std::thread::Thread` in the current scope
//!  --> src/main.rs:4:10
//!   |
//! 4 | #[derive(HeapSize)]
//!   |          ^^^^^^^^
//! ```
//!
//! By tracking span information all the way through the expansion of a
//! procedural macro as shown in the `heapsize2` example, token-based macros in
//! Syn are able to trigger errors that directly pinpoint the source of the
//! problem.
//!
//! ```text
//! error[E0277]: the trait bound `std::thread::Thread: HeapSize` is not satisfied
//!  --> src/main.rs:7:5
//!   |
//! 7 |     bad: std::thread::Thread,
//!   |     ^^^^^^^^^^^^^^^^^^^^^^^^ the trait `HeapSize` is not implemented for `Thread`
//! ```
//!
//! ## Parsing a custom syntax using combinators
//!
//! The [`lazy-static`] example directory shows the implementation of a
//! `functionlike!(...)` procedural macro in which the input tokens are parsed
//! using [`nom`]-style parser combinators.
//!
//! [`lazy-static`]: https://github.com/dtolnay/syn/tree/master/examples/lazy-static
//! [`nom`]: https://github.com/Geal/nom
//!
//! The example reimplements the popular `lazy_static` crate from crates.io as a
//! procedural macro.
//!
//! ```
//! # macro_rules! lazy_static {
//! #     ($($tt:tt)*) => {}
//! # }
//! #
//! lazy_static! {
//!     static ref USERNAME: Regex = Regex::new("^[a-z0-9_-]{3,16}$").unwrap();
//! }
//! ```
//!
//! The implementation shows how to trigger custom warnings and error messages
//! on the macro input.
//!
//! ```text
//! warning: come on, pick a more creative name
//!   --> src/main.rs:10:16
//!    |
//! 10 |     static ref FOO: String = "lazy_static".to_owned();
//!    |                ^^^
//! ```
//!
//! ## Debugging
//!
//! When developing a procedural macro it can be helpful to look at what the
//! generated code looks like. Use `cargo rustc -- -Zunstable-options
//! --pretty=expanded` or the [`cargo expand`] subcommand.
//!
//! [`cargo expand`]: https://github.com/dtolnay/cargo-expand
//!
//! To show the expanded code for some crate that uses your procedural macro,
//! run `cargo expand` from that crate. To show the expanded code for one of
//! your own test cases, run `cargo expand --test the_test_case` where the last
//! argument is the name of the test file without the `.rs` extension.
//!
//! This write-up by Brandon W Maister discusses debugging in more detail:
//! [Debugging Rust's new Custom Derive system][debugging].
//!
//! [debugging]: https://quodlibetor.github.io/posts/debugging-rusts-new-custom-derive-system/
//!
//! ## Optional features
//!
//! Syn puts a lot of functionality behind optional features in order to
//! optimize compile time for the most common use cases. The following features
//! are available.
//!
//! - **`derive`** *(enabled by default)* — Data structures for representing the
//!   possible input to a custom derive, including structs and enums and types.
//! - **`full`** — Data structures for representing the syntax tree of all valid
//!   Rust source code, including items and expressions.
//! - **`parsing`** *(enabled by default)* — Ability to parse input tokens into
//!   a syntax tree node of a chosen type.
//! - **`printing`** *(enabled by default)* — Ability to print a syntax tree
//!   node as tokens of Rust source code.
//! - **`visit`** — Trait for traversing a syntax tree.
//! - **`visit-mut`** — Trait for traversing and mutating in place a syntax
//!   tree.
//! - **`fold`** — Trait for transforming an owned syntax tree.
//! - **`clone-impls`** *(enabled by default)* — Clone impls for all syntax tree
//!   types.
//! - **`extra-traits`** — Debug, Eq, PartialEq, Hash impls for all syntax tree
//!   types.
//! - **`proc-macro`** *(enabled by default)* — Runtime dependency on the
//!   dynamic library libproc_macro from rustc toolchain.

// Syn types in rustdoc of other crates get linked to here.
#![doc(html_root_url = "https://docs.rs/syn/0.14.2")]
#![cfg_attr(feature = "cargo-clippy", deny(clippy, clippy_pedantic))]
// Ignored clippy lints.
#![cfg_attr(
    feature = "cargo-clippy",
    allow(
        const_static_lifetime, doc_markdown, large_enum_variant, match_bool, redundant_closure,
        needless_pass_by_value, redundant_field_names
    )
)]
// Ignored clippy_pedantic lints.
#![cfg_attr(
    feature = "cargo-clippy",
    allow(
        cast_possible_truncation, cast_possible_wrap, if_not_else, items_after_statements,
        similar_names, single_match_else, stutter, unseparated_literal_suffix, use_self,
        used_underscore_binding
    )
)]

#[cfg(feature = "proc-macro")]
extern crate proc_macro;
extern crate proc_macro2;
extern crate unicode_xid;

#[cfg(feature = "printing")]
extern crate quote;

#[cfg(feature = "parsing")]
#[macro_use]
#[doc(hidden)]
pub mod parsers;

#[macro_use]
mod macros;

#[macro_use]
pub mod token;

pub use proc_macro2::Ident;

#[cfg(any(feature = "full", feature = "derive"))]
mod attr;
#[cfg(any(feature = "full", feature = "derive"))]
pub use attr::{AttrStyle, Attribute, Meta, MetaList, MetaNameValue, NestedMeta};

#[cfg(any(feature = "full", feature = "derive"))]
mod data;
#[cfg(any(feature = "full", feature = "derive"))]
pub use data::{
    Field, Fields, FieldsNamed, FieldsUnnamed, Variant, VisCrate, VisPublic, VisRestricted,
    Visibility,
};

#[cfg(any(feature = "full", feature = "derive"))]
mod expr;
#[cfg(any(feature = "full", feature = "derive"))]
pub use expr::{
    Expr, ExprArray, ExprAssign, ExprAssignOp, ExprBinary, ExprBlock, ExprBox, ExprBreak, ExprCall,
    ExprCast, ExprCatch, ExprClosure, ExprContinue, ExprField, ExprForLoop, ExprGroup, ExprIf,
    ExprIfLet, ExprInPlace, ExprIndex, ExprLit, ExprLoop, ExprMacro, ExprMatch, ExprMethodCall,
    ExprParen, ExprPath, ExprRange, ExprReference, ExprRepeat, ExprReturn, ExprStruct, ExprTry,
    ExprTuple, ExprType, ExprUnary, ExprUnsafe, ExprVerbatim, ExprWhile, ExprWhileLet, ExprYield,
    Index, Member,
};

#[cfg(feature = "full")]
pub use expr::{
    Arm, Block, FieldPat, FieldValue, GenericMethodArgument, Label, Local, MethodTurbofish, Pat,
    PatBox, PatIdent, PatLit, PatMacro, PatPath, PatRange, PatRef, PatSlice, PatStruct, PatTuple,
    PatTupleStruct, PatVerbatim, PatWild, RangeLimits, Stmt,
};

#[cfg(any(feature = "full", feature = "derive"))]
mod generics;
#[cfg(any(feature = "full", feature = "derive"))]
pub use generics::{
    BoundLifetimes, ConstParam, GenericParam, Generics, LifetimeDef, PredicateEq,
    PredicateLifetime, PredicateType, TraitBound, TraitBoundModifier, TypeParam, TypeParamBound,
    WhereClause, WherePredicate,
};
#[cfg(all(any(feature = "full", feature = "derive"), feature = "printing"))]
pub use generics::{ImplGenerics, Turbofish, TypeGenerics};

#[cfg(feature = "full")]
mod item;
#[cfg(feature = "full")]
pub use item::{
    ArgCaptured, ArgSelf, ArgSelfRef, FnArg, FnDecl, ForeignItem, ForeignItemFn, ForeignItemStatic,
    ForeignItemType, ForeignItemVerbatim, ImplItem, ImplItemConst, ImplItemMacro, ImplItemMethod,
    ImplItemType, ImplItemVerbatim, Item, ItemConst, ItemEnum, ItemExternCrate, ItemFn,
    ItemForeignMod, ItemImpl, ItemMacro, ItemMacro2, ItemMod, ItemStatic, ItemStruct, ItemTrait,
    ItemType, ItemUnion, ItemUse, ItemVerbatim, MethodSig, TraitItem, TraitItemConst,
    TraitItemMacro, TraitItemMethod, TraitItemType, TraitItemVerbatim, UseGlob, UseGroup, UseName,
    UsePath, UseRename, UseTree,
};

#[cfg(feature = "full")]
mod file;
#[cfg(feature = "full")]
pub use file::File;

#[cfg(any(feature = "full", feature = "derive"))]
mod lifetime;
#[cfg(any(feature = "full", feature = "derive"))]
pub use lifetime::Lifetime;

#[cfg(any(feature = "full", feature = "derive"))]
mod lit;
#[cfg(any(feature = "full", feature = "derive"))]
pub use lit::{
    FloatSuffix, IntSuffix, Lit, LitBool, LitByte, LitByteStr, LitChar, LitFloat, LitInt, LitStr,
    LitVerbatim, StrStyle,
};

#[cfg(any(feature = "full", feature = "derive"))]
mod mac;
#[cfg(any(feature = "full", feature = "derive"))]
pub use mac::{Macro, MacroDelimiter};

#[cfg(any(feature = "full", feature = "derive"))]
mod derive;
#[cfg(feature = "derive")]
pub use derive::{Data, DataEnum, DataStruct, DataUnion, DeriveInput};

#[cfg(any(feature = "full", feature = "derive"))]
mod op;
#[cfg(any(feature = "full", feature = "derive"))]
pub use op::{BinOp, UnOp};

#[cfg(any(feature = "full", feature = "derive"))]
mod ty;
#[cfg(any(feature = "full", feature = "derive"))]
pub use ty::{
    Abi, BareFnArg, BareFnArgName, ReturnType, Type, TypeArray, TypeBareFn, TypeGroup,
    TypeImplTrait, TypeInfer, TypeMacro, TypeNever, TypeParen, TypePath, TypePtr, TypeReference,
    TypeSlice, TypeTraitObject, TypeTuple, TypeVerbatim,
};

#[cfg(any(feature = "full", feature = "derive"))]
mod path;
#[cfg(all(any(feature = "full", feature = "derive"), feature = "printing"))]
pub use path::PathTokens;
#[cfg(any(feature = "full", feature = "derive"))]
pub use path::{
    AngleBracketedGenericArguments, Binding, GenericArgument, ParenthesizedGenericArguments, Path,
    PathArguments, PathSegment, QSelf,
};

#[cfg(feature = "parsing")]
pub mod buffer;
pub mod punctuated;
#[cfg(feature = "parsing")]
pub mod synom;
#[cfg(any(feature = "full", feature = "derive"))]
mod tt;

// Not public API except the `parse_quote!` macro.
#[cfg(feature = "parsing")]
#[doc(hidden)]
pub mod parse_quote;

#[cfg(all(feature = "parsing", feature = "printing"))]
pub mod spanned;

mod gen {
    /// Syntax tree traversal to walk a shared borrow of a syntax tree.
    ///
    /// Each method of the [`Visit`] trait is a hook that can be overridden to
    /// customize the behavior when visiting the corresponding type of node. By
    /// default, every method recursively visits the substructure of the input
    /// by invoking the right visitor method of each of its fields.
    ///
    /// [`Visit`]: trait.Visit.html
    ///
    /// ```rust
    /// # use syn::{Attribute, BinOp, Expr, ExprBinary};
    /// #
    /// pub trait Visit<'ast> {
    ///     /* ... */
    ///
    ///     fn visit_expr_binary(&mut self, node: &'ast ExprBinary) {
    ///         for attr in &node.attrs {
    ///             self.visit_attribute(attr);
    ///         }
    ///         self.visit_expr(&*node.left);
    ///         self.visit_bin_op(&node.op);
    ///         self.visit_expr(&*node.right);
    ///     }
    ///
    ///     /* ... */
    ///     # fn visit_attribute(&mut self, node: &'ast Attribute);
    ///     # fn visit_expr(&mut self, node: &'ast Expr);
    ///     # fn visit_bin_op(&mut self, node: &'ast BinOp);
    /// }
    /// ```
    ///
    /// *This module is available if Syn is built with the `"visit"` feature.*
    #[cfg(feature = "visit")]
    pub mod visit;

    /// Syntax tree traversal to mutate an exclusive borrow of a syntax tree in
    /// place.
    ///
    /// Each method of the [`VisitMut`] trait is a hook that can be overridden
    /// to customize the behavior when mutating the corresponding type of node.
    /// By default, every method recursively visits the substructure of the
    /// input by invoking the right visitor method of each of its fields.
    ///
    /// [`VisitMut`]: trait.VisitMut.html
    ///
    /// ```rust
    /// # use syn::{Attribute, BinOp, Expr, ExprBinary};
    /// #
    /// pub trait VisitMut {
    ///     /* ... */
    ///
    ///     fn visit_expr_binary_mut(&mut self, node: &mut ExprBinary) {
    ///         for attr in &mut node.attrs {
    ///             self.visit_attribute_mut(attr);
    ///         }
    ///         self.visit_expr_mut(&mut *node.left);
    ///         self.visit_bin_op_mut(&mut node.op);
    ///         self.visit_expr_mut(&mut *node.right);
    ///     }
    ///
    ///     /* ... */
    ///     # fn visit_attribute_mut(&mut self, node: &mut Attribute);
    ///     # fn visit_expr_mut(&mut self, node: &mut Expr);
    ///     # fn visit_bin_op_mut(&mut self, node: &mut BinOp);
    /// }
    /// ```
    ///
    /// *This module is available if Syn is built with the `"visit-mut"`
    /// feature.*
    #[cfg(feature = "visit-mut")]
    pub mod visit_mut;

    /// Syntax tree traversal to transform the nodes of an owned syntax tree.
    ///
    /// Each method of the [`Fold`] trait is a hook that can be overridden to
    /// customize the behavior when transforming the corresponding type of node.
    /// By default, every method recursively visits the substructure of the
    /// input by invoking the right visitor method of each of its fields.
    ///
    /// [`Fold`]: trait.Fold.html
    ///
    /// ```rust
    /// # use syn::{Attribute, BinOp, Expr, ExprBinary};
    /// #
    /// pub trait Fold {
    ///     /* ... */
    ///
    ///     fn fold_expr_binary(&mut self, node: ExprBinary) -> ExprBinary {
    ///         ExprBinary {
    ///             attrs: node.attrs
    ///                        .into_iter()
    ///                        .map(|attr| self.fold_attribute(attr))
    ///                        .collect(),
    ///             left: Box::new(self.fold_expr(*node.left)),
    ///             op: self.fold_bin_op(node.op),
    ///             right: Box::new(self.fold_expr(*node.right)),
    ///         }
    ///     }
    ///
    ///     /* ... */
    ///     # fn fold_attribute(&mut self, node: Attribute) -> Attribute;
    ///     # fn fold_expr(&mut self, node: Expr) -> Expr;
    ///     # fn fold_bin_op(&mut self, node: BinOp) -> BinOp;
    /// }
    /// ```
    ///
    /// *This module is available if Syn is built with the `"fold"` feature.*
    #[cfg(feature = "fold")]
    pub mod fold;

    #[cfg(any(feature = "full", feature = "derive"))]
    #[path = "../gen_helper.rs"]
    mod helper;
}
pub use gen::*;

////////////////////////////////////////////////////////////////////////////////

#[cfg(feature = "parsing")]
use synom::{Parser, Synom};

#[cfg(feature = "parsing")]
mod error;
#[cfg(feature = "parsing")]
use error::ParseError;

// Not public API.
#[cfg(feature = "parsing")]
#[doc(hidden)]
pub use error::parse_error;

/// Parse tokens of source code into the chosen syntax tree node.
///
/// This is preferred over parsing a string because tokens are able to preserve
/// information about where in the user's code they were originally written (the
/// "span" of the token), possibly allowing the compiler to produce better error
/// messages.
///
/// This function parses a `proc_macro::TokenStream` which is the type used for
/// interop with the compiler in a procedural macro. To parse a
/// `proc_macro2::TokenStream`, use [`syn::parse2`] instead.
///
/// [`syn::parse2`]: fn.parse2.html
///
/// *This function is available if Syn is built with both the `"parsing"` and
/// `"proc-macro"` features.*
///
/// # Examples
///
/// ```rust
/// extern crate proc_macro;
/// use proc_macro::TokenStream;
///
/// extern crate syn;
///
/// #[macro_use]
/// extern crate quote;
///
/// use syn::DeriveInput;
///
/// # const IGNORE_TOKENS: &str = stringify! {
/// #[proc_macro_derive(MyMacro)]
/// # };
/// pub fn my_macro(input: TokenStream) -> TokenStream {
///     // Parse the tokens into a syntax tree
///     let ast: DeriveInput = syn::parse(input).unwrap();
///
///     // Build the output, possibly using quasi-quotation
///     let expanded = quote! {
///         /* ... */
///     };
///
///     // Convert into a token stream and return it
///     expanded.into()
/// }
/// #
/// # fn main() {}
/// ```
#[cfg(all(feature = "parsing", feature = "proc-macro"))]
pub fn parse<T>(tokens: proc_macro::TokenStream) -> Result<T, ParseError>
where
    T: Synom,
{
    parse2(tokens.into())
}

/// Parse a proc-macro2 token stream into the chosen syntax tree node.
///
/// This function parses a `proc_macro2::TokenStream` which is commonly useful
/// when the input comes from a node of the Syn syntax tree, for example the tts
/// of a [`Macro`] node. When in a procedural macro parsing the
/// `proc_macro::TokenStream` provided by the compiler, use [`syn::parse`]
/// instead.
///
/// [`Macro`]: struct.Macro.html
/// [`syn::parse`]: fn.parse.html
///
/// *This function is available if Syn is built with the `"parsing"` feature.*
#[cfg(feature = "parsing")]
pub fn parse2<T>(tokens: proc_macro2::TokenStream) -> Result<T, ParseError>
where
    T: Synom,
{
    let parser = T::parse;
    parser.parse2(tokens).map_err(|err| match T::description() {
        Some(s) => ParseError::new(format!("failed to parse {}: {}", s, err)),
        None => err,
    })
}

/// Parse a string of Rust code into the chosen syntax tree node.
///
/// *This function is available if Syn is built with the `"parsing"` feature.*
///
/// # Hygiene
///
/// Every span in the resulting syntax tree will be set to resolve at the macro
/// call site.
///
/// # Examples
///
/// ```rust
/// extern crate syn;
/// #
/// #
/// # type Result<T> = std::result::Result<T, Box<std::error::Error>>;
///
/// use syn::Expr;
///
/// fn run() -> Result<()> {
///     let code = "assert_eq!(u8::max_value(), 255)";
///     let expr = syn::parse_str::<Expr>(code)?;
///     println!("{:#?}", expr);
///     Ok(())
/// }
/// #
/// # fn main() { run().unwrap() }
/// ```
#[cfg(feature = "parsing")]
pub fn parse_str<T: Synom>(s: &str) -> Result<T, ParseError> {
    match s.parse() {
        Ok(tts) => parse2(tts),
        Err(_) => Err(ParseError::new("error while lexing input string")),
    }
}

// FIXME the name parse_file makes it sound like you might pass in a path to a
// file, rather than the content.
/// Parse the content of a file of Rust code.
///
/// This is different from `syn::parse_str::<File>(content)` in two ways:
///
/// - It discards a leading byte order mark `\u{FEFF}` if the file has one.
/// - It preserves the shebang line of the file, such as `#!/usr/bin/env rustx`.
///
/// If present, either of these would be an error using `from_str`.
///
/// *This function is available if Syn is built with the `"parsing"` and `"full"` features.*
///
/// # Examples
///
/// ```rust,no_run
/// extern crate syn;
/// #
/// #
/// # type Result<T> = std::result::Result<T, Box<std::error::Error>>;
///
/// use std::fs::File;
/// use std::io::Read;
///
/// fn run() -> Result<()> {
///     let mut file = File::open("path/to/code.rs")?;
///     let mut content = String::new();
///     file.read_to_string(&mut content)?;
///
///     let ast = syn::parse_file(&content)?;
///     if let Some(shebang) = ast.shebang {
///         println!("{}", shebang);
///     }
///     println!("{} items", ast.items.len());
///
///     Ok(())
/// }
/// #
/// # fn main() { run().unwrap() }
/// ```
#[cfg(all(feature = "parsing", feature = "full"))]
pub fn parse_file(mut content: &str) -> Result<File, ParseError> {
    // Strip the BOM if it is present
    const BOM: &'static str = "\u{feff}";
    if content.starts_with(BOM) {
        content = &content[BOM.len()..];
    }

    let mut shebang = None;
    if content.starts_with("#!") && !content.starts_with("#![") {
        if let Some(idx) = content.find('\n') {
            shebang = Some(content[..idx].to_string());
            content = &content[idx..];
        } else {
            shebang = Some(content.to_string());
            content = "";
        }
    }

    let mut file: File = parse_str(content)?;
    file.shebang = shebang;
    Ok(file)
}

#[cfg(all(any(feature = "full", feature = "derive"), feature = "printing"))]
struct TokensOrDefault<'a, T: 'a>(&'a Option<T>);

#[cfg(all(any(feature = "full", feature = "derive"), feature = "printing"))]
impl<'a, T> quote::ToTokens for TokensOrDefault<'a, T>
where
    T: quote::ToTokens + Default,
{
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        match *self.0 {
            Some(ref t) => t.to_tokens(tokens),
            None => T::default().to_tokens(tokens),
        }
    }
}
