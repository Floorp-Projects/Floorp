use super::Tokens;

use std::borrow::Cow;

use proc_macro2::{Literal, Span, Term, TokenNode, TokenTree, TokenStream};

fn tt(kind: TokenNode) -> TokenTree {
    TokenTree {
        span: Span::def_site(),
        kind: kind,
    }
}

/// Types that can be interpolated inside a [`quote!`] invocation.
///
/// [`quote!`]: macro.quote.html
pub trait ToTokens {
    /// Write `self` to the given `Tokens`.
    ///
    /// Example implementation for a struct representing Rust paths like
    /// `std::cmp::PartialEq`:
    ///
    /// ```
    /// extern crate quote;
    /// use quote::{Tokens, ToTokens};
    ///
    /// extern crate proc_macro2;
    /// use proc_macro2::{TokenTree, TokenNode, Spacing, Span};
    ///
    /// pub struct Path {
    ///     pub global: bool,
    ///     pub segments: Vec<PathSegment>,
    /// }
    ///
    /// impl ToTokens for Path {
    ///     fn to_tokens(&self, tokens: &mut Tokens) {
    ///         for (i, segment) in self.segments.iter().enumerate() {
    ///             if i > 0 || self.global {
    ///                 // Double colon `::`
    ///                 tokens.append(TokenTree {
    ///                     span: Span::def_site(),
    ///                     kind: TokenNode::Op(':', Spacing::Joint),
    ///                 });
    ///                 tokens.append(TokenTree {
    ///                     span: Span::def_site(),
    ///                     kind: TokenNode::Op(':', Spacing::Alone),
    ///                 });
    ///             }
    ///             segment.to_tokens(tokens);
    ///         }
    ///     }
    /// }
    /// #
    /// # pub struct PathSegment;
    /// #
    /// # impl ToTokens for PathSegment {
    /// #     fn to_tokens(&self, tokens: &mut Tokens) {
    /// #         unimplemented!()
    /// #     }
    /// # }
    /// #
    /// # fn main() {}
    /// ```
    fn to_tokens(&self, tokens: &mut Tokens);

    /// Convert `self` directly into a `Tokens` object.
    ///
    /// This method is implicitly implemented using `to_tokens`, and acts as a
    /// convenience method for consumers of the `ToTokens` trait.
    fn into_tokens(self) -> Tokens
    where
        Self: Sized,
    {
        let mut tokens = Tokens::new();
        self.to_tokens(&mut tokens);
        tokens
    }
}

impl<'a, T: ?Sized + ToTokens> ToTokens for &'a T {
    fn to_tokens(&self, tokens: &mut Tokens) {
        (**self).to_tokens(tokens);
    }
}

impl<'a, T: ?Sized + ToOwned + ToTokens> ToTokens for Cow<'a, T> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        (**self).to_tokens(tokens);
    }
}

impl<T: ?Sized + ToTokens> ToTokens for Box<T> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        (**self).to_tokens(tokens);
    }
}

impl<T: ToTokens> ToTokens for Option<T> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        if let Some(ref t) = *self {
            t.to_tokens(tokens);
        }
    }
}

impl ToTokens for str {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(tt(TokenNode::Literal(Literal::string(self))));
    }
}

impl ToTokens for String {
    fn to_tokens(&self, tokens: &mut Tokens) {
        self.as_str().to_tokens(tokens);
    }
}

macro_rules! primitive {
    ($($t:ident)*) => ($(
        impl ToTokens for $t {
            fn to_tokens(&self, tokens: &mut Tokens) {
                tokens.append(tt(TokenNode::Literal(Literal::$t(*self))));
            }
        }
    )*)
}

primitive! {
    i8 i16 i32 i64 isize
    u8 u16 u32 u64 usize
    f32 f64
}

impl ToTokens for char {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(tt(TokenNode::Literal(Literal::character(*self))));
    }
}

impl ToTokens for bool {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let word = if *self { "true" } else { "false" };
        tokens.append(tt(TokenNode::Term(Term::intern(word))));
    }
}

impl ToTokens for Term {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(tt(TokenNode::Term(*self)));
    }
}

impl ToTokens for Literal {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(tt(TokenNode::Literal(self.clone())));
    }
}

impl ToTokens for TokenNode {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(tt(self.clone()));
    }
}

impl ToTokens for TokenTree {
    fn to_tokens(&self, dst: &mut Tokens) {
        dst.append(self.clone());
    }
}

impl ToTokens for TokenStream {
    fn to_tokens(&self, dst: &mut Tokens) {
        dst.append_all(self.clone().into_iter());
    }
}
