// Based on https://github.com/dtolnay/proc-macro-hack/blob/0.5.18/src/iter.rs

use proc_macro::{token_stream, Delimiter, TokenStream, TokenTree};

pub(crate) struct TokenIter {
    stack: Vec<token_stream::IntoIter>,
    peeked: Option<TokenTree>,
}

impl TokenIter {
    pub(crate) fn new(tokens: TokenStream) -> Self {
        Self { stack: vec![tokens.into_iter()], peeked: None }
    }

    pub(crate) fn peek(&mut self) -> Option<&TokenTree> {
        self.peeked = self.next();
        self.peeked.as_ref()
    }
}

impl Iterator for TokenIter {
    type Item = TokenTree;

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(tt) = self.peeked.take() {
            return Some(tt);
        }
        loop {
            let top = self.stack.last_mut()?;
            match top.next() {
                None => drop(self.stack.pop()),
                Some(TokenTree::Group(ref group)) if group.delimiter() == Delimiter::None => {
                    self.stack.push(group.stream().into_iter());
                }
                Some(tt) => return Some(tt),
            }
        }
    }
}
