use super::ToTokens;
use std::fmt::{self, Display};
use std::str::FromStr;

/// Tokens produced by a `quote!(...)` invocation.
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Tokens(String);

impl Tokens {
    /// Empty tokens.
    pub fn new() -> Self {
        Tokens(String::new())
    }

    /// For use by `ToTokens` implementations.
    ///
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # use quote::{Tokens, ToTokens};
    /// # fn main() {
    /// struct X;
    ///
    /// impl ToTokens for X {
    ///     fn to_tokens(&self, tokens: &mut Tokens) {
    ///         tokens.append("a");
    ///         tokens.append("b");
    ///         tokens.append("c");
    ///     }
    /// }
    ///
    /// let tokens = quote!(#X);
    /// assert_eq!(tokens.as_str(), "a b c");
    /// # }
    /// ```
    pub fn append<T: AsRef<str>>(&mut self, token: T) {
        if !self.0.is_empty() && !token.as_ref().is_empty() {
            self.0.push(' ');
        }
        self.0.push_str(token.as_ref());
    }

    /// For use by `ToTokens` implementations.
    ///
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # use quote::{Tokens, ToTokens};
    /// # fn main() {
    /// struct X;
    ///
    /// impl ToTokens for X {
    ///     fn to_tokens(&self, tokens: &mut Tokens) {
    ///         tokens.append_all(&[true, false]);
    ///     }
    /// }
    ///
    /// let tokens = quote!(#X);
    /// assert_eq!(tokens.as_str(), "true false");
    /// # }
    /// ```
    pub fn append_all<T, I>(&mut self, iter: I)
        where T: ToTokens,
              I: IntoIterator<Item = T>
    {
        for token in iter {
            token.to_tokens(self);
        }
    }

    /// For use by `ToTokens` implementations.
    ///
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # use quote::{Tokens, ToTokens};
    /// # fn main() {
    /// struct X;
    ///
    /// impl ToTokens for X {
    ///     fn to_tokens(&self, tokens: &mut Tokens) {
    ///         tokens.append_separated(&[true, false], ",");
    ///     }
    /// }
    ///
    /// let tokens = quote!(#X);
    /// assert_eq!(tokens.as_str(), "true , false");
    /// # }
    /// ```
    pub fn append_separated<T, I, S: AsRef<str>>(&mut self, iter: I, sep: S)
        where T: ToTokens,
              I: IntoIterator<Item = T>
    {
        for (i, token) in iter.into_iter().enumerate() {
            if i > 0 {
                self.append(sep.as_ref());
            }
            token.to_tokens(self);
        }
    }

    /// For use by `ToTokens` implementations.
    ///
    /// ```
    /// # #[macro_use] extern crate quote;
    /// # use quote::{Tokens, ToTokens};
    /// # fn main() {
    /// struct X;
    ///
    /// impl ToTokens for X {
    ///     fn to_tokens(&self, tokens: &mut Tokens) {
    ///         tokens.append_terminated(&[true, false], ",");
    ///     }
    /// }
    ///
    /// let tokens = quote!(#X);
    /// assert_eq!(tokens.as_str(), "true , false ,");
    /// # }
    /// ```
    pub fn append_terminated<T, I, S: AsRef<str>>(&mut self, iter: I, term: S)
        where T: ToTokens,
              I: IntoIterator<Item = T>
    {
        for token in iter {
            token.to_tokens(self);
            self.append(term.as_ref());
        }
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }

    pub fn into_string(self) -> String {
        self.0
    }

    pub fn parse<T: FromStr>(&self) -> Result<T, T::Err> {
        FromStr::from_str(&self.0)
    }
}

impl Default for Tokens {
    fn default() -> Self {
        Tokens::new()
    }
}

impl Display for Tokens {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        self.0.fmt(formatter)
    }
}

impl AsRef<str> for Tokens {
    fn as_ref(&self) -> &str {
        &self.0
    }
}
