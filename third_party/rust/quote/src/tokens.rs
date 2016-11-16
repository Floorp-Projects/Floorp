use super::ToTokens;
use std::fmt::{self, Display};

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Tokens(String);

impl Tokens {
    pub fn new() -> Self {
        Tokens(String::new())
    }

    pub fn append(&mut self, token: &str) {
        if !self.0.is_empty() && !token.is_empty() {
            self.0.push(' ');
        }
        self.0.push_str(token);
    }

    pub fn append_all<T, I>(&mut self, iter: I)
        where T: ToTokens,
              I: IntoIterator<Item = T>
    {
        for token in iter {
            token.to_tokens(self);
        }
    }

    pub fn append_separated<T, I>(&mut self, iter: I, sep: &str)
        where T: ToTokens,
              I: IntoIterator<Item = T>
    {
        for (i, token) in iter.into_iter().enumerate() {
            if i > 0 {
                self.append(sep);
            }
            token.to_tokens(self);
        }
    }

    pub fn append_terminated<T, I>(&mut self, iter: I, term: &str)
        where T: ToTokens,
              I: IntoIterator<Item = T>
    {
        for token in iter {
            token.to_tokens(self);
            self.append(term);
        }
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
