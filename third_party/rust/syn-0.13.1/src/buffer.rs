// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A stably addressed token buffer supporting efficient traversal based on a
//! cheaply copyable cursor.
//!
//! The [`Synom`] trait is implemented for syntax tree types that can be parsed
//! from one of these token cursors.
//!
//! [`Synom`]: ../synom/trait.Synom.html
//!
//! *This module is available if Syn is built with the `"parsing"` feature.*
//!
//! # Example
//!
//! This example shows a basic token parser for parsing a token stream without
//! using Syn's parser combinator macros.
//!
//! ```
//! #![feature(proc_macro)]
//!
//! extern crate syn;
//! extern crate proc_macro;
//!
//! #[macro_use]
//! extern crate quote;
//!
//! use syn::{token, ExprTuple};
//! use syn::buffer::{Cursor, TokenBuffer};
//! use syn::spanned::Spanned;
//! use syn::synom::Synom;
//! use proc_macro::{Diagnostic, Span, TokenStream};
//!
//! /// A basic token parser for parsing a token stream without using Syn's
//! /// parser combinator macros.
//! pub struct Parser<'a> {
//!     cursor: Cursor<'a>,
//! }
//!
//! impl<'a> Parser<'a> {
//!     pub fn new(cursor: Cursor<'a>) -> Self {
//!         Parser { cursor }
//!     }
//!
//!     pub fn current_span(&self) -> Span {
//!         self.cursor.span().unstable()
//!     }
//!
//!     pub fn parse<T: Synom>(&mut self) -> Result<T, Diagnostic> {
//!         let (val, rest) = T::parse(self.cursor)
//!             .map_err(|e| match T::description() {
//!                 Some(desc) => {
//!                     self.current_span().error(format!("{}: expected {}", e, desc))
//!                 }
//!                 None => {
//!                     self.current_span().error(e.to_string())
//!                 }
//!             })?;
//!
//!         self.cursor = rest;
//!         Ok(val)
//!     }
//!
//!     pub fn expect_eof(&mut self) -> Result<(), Diagnostic> {
//!         if !self.cursor.eof() {
//!             return Err(self.current_span().error("trailing characters; expected eof"));
//!         }
//!
//!         Ok(())
//!     }
//! }
//!
//! fn eval(input: TokenStream) -> Result<TokenStream, Diagnostic> {
//!     let buffer = TokenBuffer::new(input);
//!     let mut parser = Parser::new(buffer.begin());
//!
//!     // Parse some syntax tree types out of the input tokens. In this case we
//!     // expect something like:
//!     //
//!     //     (a, b, c) = (1, 2, 3)
//!     let a = parser.parse::<ExprTuple>()?;
//!     parser.parse::<token::Eq>()?;
//!     let b = parser.parse::<ExprTuple>()?;
//!     parser.expect_eof()?;
//!
//!     // Perform some validation and report errors.
//!     let (a_len, b_len) = (a.elems.len(), b.elems.len());
//!     if a_len != b_len {
//!         let diag = b.span().unstable()
//!             .error(format!("expected {} element(s), got {}", a_len, b_len))
//!             .span_note(a.span().unstable(), "because of this");
//!
//!         return Err(diag);
//!     }
//!
//!     // Build the output tokens.
//!     let out = quote! {
//!         println!("All good! Received two tuples of size {}", #a_len);
//!     };
//!
//!     Ok(out.into())
//! }
//! #
//! # extern crate proc_macro2;
//! #
//! # // This method exists on proc_macro2::Span but is behind the "nightly"
//! # // feature.
//! # trait ToUnstableSpan {
//! #     fn unstable(&self) -> Span;
//! # }
//! #
//! # impl ToUnstableSpan for proc_macro2::Span {
//! #     fn unstable(&self) -> Span {
//! #         unimplemented!()
//! #     }
//! # }
//! #
//! # fn main() {}
//! ```

// This module is heavily commented as it contains the only unsafe code in Syn,
// and caution should be used when editing it. The public-facing interface is
// 100% safe but the implementation is fragile internally.

#[cfg(feature = "proc-macro")]
use proc_macro as pm;
use proc_macro2::{Delimiter, Literal, Span, Term, TokenStream};
use proc_macro2::{Group, TokenTree, Op};

use std::ptr;
use std::marker::PhantomData;

#[cfg(synom_verbose_trace)]
use std::fmt::{self, Debug};

/// Internal type which is used instead of `TokenTree` to represent a token tree
/// within a `TokenBuffer`.
enum Entry {
    // Mimicking types from proc-macro.
    Group(Span, Delimiter, TokenBuffer),
    Term(Term),
    Op(Op),
    Literal(Literal),
    // End entries contain a raw pointer to the entry from the containing
    // token tree, or null if this is the outermost level.
    End(*const Entry),
}

/// A buffer that can be efficiently traversed multiple times, unlike
/// `TokenStream` which requires a deep copy in order to traverse more than
/// once.
///
/// See the [module documentation] for an example of `TokenBuffer` in action.
///
/// [module documentation]: index.html
///
/// *This type is available if Syn is built with the `"parsing"` feature.*
pub struct TokenBuffer {
    // NOTE: Do not derive clone on this - there are raw pointers inside which
    // will be messed up. Moving the `TokenBuffer` itself is safe as the actual
    // backing slices won't be moved.
    data: Box<[Entry]>,
}

impl TokenBuffer {
    // NOTE: DO NOT MUTATE THE `Vec` RETURNED FROM THIS FUNCTION ONCE IT
    // RETURNS, THE ADDRESS OF ITS BACKING MEMORY MUST REMAIN STABLE.
    fn inner_new(stream: TokenStream, up: *const Entry) -> TokenBuffer {
        // Build up the entries list, recording the locations of any Groups
        // in the list to be processed later.
        let mut entries = Vec::new();
        let mut seqs = Vec::new();
        for tt in stream {
            match tt {
                TokenTree::Term(sym) => {
                    entries.push(Entry::Term(sym));
                }
                TokenTree::Op(op) => {
                    entries.push(Entry::Op(op));
                }
                TokenTree::Literal(l) => {
                    entries.push(Entry::Literal(l));
                }
                TokenTree::Group(g) => {
                    // Record the index of the interesting entry, and store an
                    // `End(null)` there temporarially.
                    seqs.push((entries.len(), g.span(), g.delimiter(), g.stream().clone()));
                    entries.push(Entry::End(ptr::null()));
                }
            }
        }
        // Add an `End` entry to the end with a reference to the enclosing token
        // stream which was passed in.
        entries.push(Entry::End(up));

        // NOTE: This is done to ensure that we don't accidentally modify the
        // length of the backing buffer. The backing buffer must remain at a
        // constant address after this point, as we are going to store a raw
        // pointer into it.
        let mut entries = entries.into_boxed_slice();
        for (idx, span, delim, seq_stream) in seqs {
            // We know that this index refers to one of the temporary
            // `End(null)` entries, and we know that the last entry is
            // `End(up)`, so the next index is also valid.
            let seq_up = &entries[idx + 1] as *const Entry;

            // The end entry stored at the end of this Entry::Group should
            // point to the Entry which follows the Group in the list.
            let inner = Self::inner_new(seq_stream, seq_up);
            entries[idx] = Entry::Group(span, delim, inner);
        }

        TokenBuffer { data: entries }
    }

    /// Creates a `TokenBuffer` containing all the tokens from the input
    /// `TokenStream`.
    #[cfg(feature = "proc-macro")]
    pub fn new(stream: pm::TokenStream) -> TokenBuffer {
        Self::new2(stream.into())
    }

    /// Creates a `TokenBuffer` containing all the tokens from the input
    /// `TokenStream`.
    pub fn new2(stream: TokenStream) -> TokenBuffer {
        Self::inner_new(stream, ptr::null())
    }

    /// Creates a cursor referencing the first token in the buffer and able to
    /// traverse until the end of the buffer.
    pub fn begin(&self) -> Cursor {
        unsafe { Cursor::create(&self.data[0], &self.data[self.data.len() - 1]) }
    }
}

/// A cheaply copyable cursor into a `TokenBuffer`.
///
/// This cursor holds a shared reference into the immutable data which is used
/// internally to represent a `TokenStream`, and can be efficiently manipulated
/// and copied around.
///
/// An empty `Cursor` can be created directly, or one may create a `TokenBuffer`
/// object and get a cursor to its first token with `begin()`.
///
/// Two cursors are equal if they have the same location in the same input
/// stream, and have the same scope.
///
/// See the [module documentation] for an example of a `Cursor` in action.
///
/// [module documentation]: index.html
///
/// *This type is available if Syn is built with the `"parsing"` feature.*
#[derive(Copy, Clone, Eq, PartialEq)]
pub struct Cursor<'a> {
    /// The current entry which the `Cursor` is pointing at.
    ptr: *const Entry,
    /// This is the only `Entry::End(..)` object which this cursor is allowed to
    /// point at. All other `End` objects are skipped over in `Cursor::create`.
    scope: *const Entry,
    /// This uses the &'a reference which guarantees that these pointers are
    /// still valid.
    marker: PhantomData<&'a Entry>,
}

impl<'a> Cursor<'a> {
    /// Creates a cursor referencing a static empty TokenStream.
    pub fn empty() -> Self {
        // It's safe in this situation for us to put an `Entry` object in global
        // storage, despite it not actually being safe to send across threads
        // (`Term` is a reference into a thread-local table). This is because
        // this entry never includes a `Term` object.
        //
        // This wrapper struct allows us to break the rules and put a `Sync`
        // object in global storage.
        struct UnsafeSyncEntry(Entry);
        unsafe impl Sync for UnsafeSyncEntry {}
        static EMPTY_ENTRY: UnsafeSyncEntry = UnsafeSyncEntry(Entry::End(0 as *const Entry));

        Cursor {
            ptr: &EMPTY_ENTRY.0,
            scope: &EMPTY_ENTRY.0,
            marker: PhantomData,
        }
    }

    /// This create method intelligently exits non-explicitly-entered
    /// `None`-delimited scopes when the cursor reaches the end of them,
    /// allowing for them to be treated transparently.
    unsafe fn create(mut ptr: *const Entry, scope: *const Entry) -> Self {
        // NOTE: If we're looking at a `End(..)`, we want to advance the cursor
        // past it, unless `ptr == scope`, which means that we're at the edge of
        // our cursor's scope. We should only have `ptr != scope` at the exit
        // from None-delimited groups entered with `ignore_none`.
        while let Entry::End(exit) = *ptr {
            if ptr == scope {
                break;
            }
            ptr = exit;
        }

        Cursor {
            ptr: ptr,
            scope: scope,
            marker: PhantomData,
        }
    }

    /// Get the current entry.
    fn entry(self) -> &'a Entry {
        unsafe { &*self.ptr }
    }

    /// Bump the cursor to point at the next token after the current one. This
    /// is undefined behavior if the cursor is currently looking at an
    /// `Entry::End`.
    unsafe fn bump(self) -> Cursor<'a> {
        Cursor::create(self.ptr.offset(1), self.scope)
    }

    /// If the cursor is looking at a `None`-delimited group, move it to look at
    /// the first token inside instead. If the group is empty, this will move
    /// the cursor past the `None`-delimited group.
    ///
    /// WARNING: This mutates its argument.
    fn ignore_none(&mut self) {
        if let Entry::Group(_, Delimiter::None, ref buf) = *self.entry() {
            // NOTE: We call `Cursor::create` here to make sure that situations
            // where we should immediately exit the span after entering it are
            // handled correctly.
            unsafe {
                *self = Cursor::create(&buf.data[0], self.scope);
            }
        }
    }

    /// Checks whether the cursor is currently pointing at the end of its valid
    /// scope.
    #[inline]
    pub fn eof(self) -> bool {
        // We're at eof if we're at the end of our scope.
        self.ptr == self.scope
    }

    /// If the cursor is pointing at a `Group` with the given delimiter, returns
    /// a cursor into that group and one pointing to the next `TokenTree`.
    pub fn group(mut self, delim: Delimiter) -> Option<(Cursor<'a>, Span, Cursor<'a>)> {
        // If we're not trying to enter a none-delimited group, we want to
        // ignore them. We have to make sure to _not_ ignore them when we want
        // to enter them, of course. For obvious reasons.
        if delim != Delimiter::None {
            self.ignore_none();
        }

        if let Entry::Group(span, group_delim, ref buf) = *self.entry() {
            if group_delim == delim {
                return Some((buf.begin(), span, unsafe { self.bump() }));
            }
        }

        None
    }

    /// If the cursor is pointing at a `Term`, returns it along with a cursor
    /// pointing at the next `TokenTree`.
    pub fn term(mut self) -> Option<(Term, Cursor<'a>)> {
        self.ignore_none();
        match *self.entry() {
            Entry::Term(term) => Some((term, unsafe { self.bump() })),
            _ => None,
        }
    }

    /// If the cursor is pointing at an `Op`, returns it along with a cursor
    /// pointing at the next `TokenTree`.
    pub fn op(mut self) -> Option<(Op, Cursor<'a>)> {
        self.ignore_none();
        match *self.entry() {
            Entry::Op(op) => Some((op, unsafe { self.bump() })),
            _ => None,
        }
    }

    /// If the cursor is pointing at a `Literal`, return it along with a cursor
    /// pointing at the next `TokenTree`.
    pub fn literal(mut self) -> Option<(Literal, Cursor<'a>)> {
        self.ignore_none();
        match *self.entry() {
            Entry::Literal(ref lit) => Some((lit.clone(), unsafe { self.bump() })),
            _ => None,
        }
    }

    /// Copies all remaining tokens visible from this cursor into a
    /// `TokenStream`.
    pub fn token_stream(self) -> TokenStream {
        let mut tts = Vec::new();
        let mut cursor = self;
        while let Some((tt, rest)) = cursor.token_tree() {
            tts.push(tt);
            cursor = rest;
        }
        tts.into_iter().collect()
    }

    /// If the cursor is pointing at a `TokenTree`, returns it along with a
    /// cursor pointing at the next `TokenTree`.
    ///
    /// Returns `None` if the cursor has reached the end of its stream.
    ///
    /// This method does not treat `None`-delimited groups as transparent, and
    /// will return a `Group(None, ..)` if the cursor is looking at one.
    pub fn token_tree(self) -> Option<(TokenTree, Cursor<'a>)> {
        let tree = match *self.entry() {
            Entry::Group(span, delim, ref buf) => {
                let stream = buf.begin().token_stream();
                let mut g = Group::new(delim, stream);
                g.set_span(span);
                TokenTree::from(g)
            }
            Entry::Literal(ref lit) => lit.clone().into(),
            Entry::Term(term) => term.into(),
            Entry::Op(op) => op.into(),
            Entry::End(..) => {
                return None;
            }
        };

        Some((tree, unsafe { self.bump() }))
    }

    /// Returns the `Span` of the current token, or `Span::call_site()` if this
    /// cursor points to eof.
    pub fn span(self) -> Span {
        match *self.entry() {
            Entry::Group(span, ..) => span,
            Entry::Literal(ref l) => l.span(),
            Entry::Term(t) => t.span(),
            Entry::Op(o) => o.span(),
            Entry::End(..) => Span::call_site(),
        }
    }
}

// We do a custom implementation for `Debug` as the default implementation is
// pretty useless.
#[cfg(synom_verbose_trace)]
impl<'a> Debug for Cursor<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // Print what the cursor is currently looking at.
        // This will look like Cursor("some remaining tokens here")
        f.debug_tuple("Cursor")
            .field(&self.token_stream().to_string())
            .finish()
    }
}
