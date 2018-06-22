use super::Tokens;

use std::borrow::Cow;

use proc_macro2::{Group, Literal, Op, Span, Term, TokenStream, TokenTree};

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
    /// use proc_macro2::{TokenTree, Spacing, Span, Op};
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
    ///                 tokens.append(Op::new(':', Spacing::Joint));
    ///                 tokens.append(Op::new(':', Spacing::Alone));
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
        tokens.append(Literal::string(self));
    }
}

impl ToTokens for String {
    fn to_tokens(&self, tokens: &mut Tokens) {
        self.as_str().to_tokens(tokens);
    }
}

macro_rules! primitive {
    ($($t:ident => $name:ident)*) => ($(
        impl ToTokens for $t {
            fn to_tokens(&self, tokens: &mut Tokens) {
                tokens.append(Literal::$name(*self));
            }
        }
    )*)
}

primitive! {
    i8 => i8_suffixed
    i16 => i16_suffixed
    i32 => i32_suffixed
    i64 => i64_suffixed
    isize => isize_suffixed

    u8 => u8_suffixed
    u16 => u16_suffixed
    u32 => u32_suffixed
    u64 => u64_suffixed
    usize => usize_suffixed

    f32 => f32_suffixed
    f64 => f64_suffixed
}

impl ToTokens for char {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(Literal::character(*self));
    }
}

impl ToTokens for bool {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let word = if *self { "true" } else { "false" };
        tokens.append(Term::new(word, Span::call_site()));
    }
}

impl ToTokens for Group {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(self.clone());
    }
}

impl ToTokens for Term {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(self.clone());
    }
}

impl ToTokens for Op {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(self.clone());
    }
}

impl ToTokens for Literal {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(self.clone());
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
