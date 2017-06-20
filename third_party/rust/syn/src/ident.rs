use std::borrow::Cow;
use std::fmt::{self, Display};

#[derive(Debug, Clone, Eq, Hash, Ord, PartialOrd)]
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

impl<'a> From<Cow<'a, str>> for Ident {
    fn from(s: Cow<'a, str>) -> Self {
        Ident(s.into_owned())
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
    use synom::IResult;
    use synom::space::skip_whitespace;
    use unicode_xid::UnicodeXID;

    pub fn ident(input: &str) -> IResult<&str, Ident> {
        let (rest, id) = match word(input) {
            IResult::Done(rest, id) => (rest, id),
            IResult::Error => return IResult::Error,
        };

        match id.as_ref() {
            // From https://doc.rust-lang.org/grammar.html#keywords
            "abstract" | "alignof" | "as" | "become" | "box" | "break" | "const" | "continue" |
            "crate" | "do" | "else" | "enum" | "extern" | "false" | "final" | "fn" | "for" |
            "if" | "impl" | "in" | "let" | "loop" | "macro" | "match" | "mod" | "move" |
            "mut" | "offsetof" | "override" | "priv" | "proc" | "pub" | "pure" | "ref" |
            "return" | "Self" | "self" | "sizeof" | "static" | "struct" | "super" | "trait" |
            "true" | "type" | "typeof" | "unsafe" | "unsized" | "use" | "virtual" | "where" |
            "while" | "yield" => IResult::Error,
            _ => IResult::Done(rest, id),
        }
    }

    pub fn word(mut input: &str) -> IResult<&str, Ident> {
        input = skip_whitespace(input);

        let mut chars = input.char_indices();
        match chars.next() {
            Some((_, ch)) if UnicodeXID::is_xid_start(ch) || ch == '_' => {}
            _ => return IResult::Error,
        }

        for (i, ch) in chars {
            if !UnicodeXID::is_xid_continue(ch) {
                return IResult::Done(&input[i..], input[..i].into());
            }
        }

        IResult::Done("", input.into())
    }

    #[cfg(feature = "full")]
    pub fn wordlike(mut input: &str) -> IResult<&str, Ident> {
        input = skip_whitespace(input);

        for (i, ch) in input.char_indices() {
            if !UnicodeXID::is_xid_start(ch) && !UnicodeXID::is_xid_continue(ch) {
                return if i == 0 {
                           IResult::Error
                       } else {
                           IResult::Done(&input[i..], input[..i].into())
                       };
            }
        }

        IResult::Done("", input.into())
    }
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
