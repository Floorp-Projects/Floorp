use std::fmt::{self, Display};

#[derive(Debug, Clone, Eq, Hash)]
pub struct Ident(String);

impl Ident {
    pub fn new<T: Into<Ident>>(t: T) -> Self {
        t.into()
    }
}

impl<'a> From<&'a str> for Ident {
    fn from(s: &str) -> Self {
        Ident(s.to_owned())
    }
}

impl From<String> for Ident {
    fn from(s: String) -> Self {
        Ident(s)
    }
}

impl From<usize> for Ident {
    fn from(u: usize) -> Self {
        Ident(u.to_string())
    }
}

impl AsRef<str> for Ident {
    fn as_ref(&self) -> &str {
        &self.0
    }
}

impl Display for Ident {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        self.0.fmt(formatter)
    }
}

impl<T: ?Sized> PartialEq<T> for Ident
    where T: AsRef<str>
{
    fn eq(&self, other: &T) -> bool {
        self.0 == other.as_ref()
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use space::whitespace;

    fn ident_ch(ch: char) -> bool {
        ch.is_alphanumeric() || ch == '_'
    }

    named!(pub ident -> Ident, preceded!(
        option!(whitespace),
        map!(take_while1!(ident_ch), Into::into)
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Ident {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append(self.as_ref())
        }
    }
}
