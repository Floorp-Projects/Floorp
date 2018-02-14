// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::*;
use proc_macro2::TokenStream;
use token::{Brace, Bracket, Paren};

#[cfg(feature = "extra-traits")]
use std::hash::{Hash, Hasher};
#[cfg(feature = "extra-traits")]
use tt::TokenStreamHelper;

ast_struct! {
    /// A macro invocation: `println!("{}", mac)`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct Macro #manual_extra_traits {
        pub path: Path,
        pub bang_token: Token![!],
        pub delimiter: MacroDelimiter,
        pub tts: TokenStream,
    }
}

ast_enum! {
    /// A grouping token that surrounds a macro body: `m!(...)` or `m!{...}` or `m![...]`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub enum MacroDelimiter {
        Paren(Paren),
        Brace(Brace),
        Bracket(Bracket),
    }
}

#[cfg(feature = "extra-traits")]
impl Eq for Macro {}

#[cfg(feature = "extra-traits")]
impl PartialEq for Macro {
    fn eq(&self, other: &Self) -> bool {
        self.path == other.path && self.bang_token == other.bang_token
            && self.delimiter == other.delimiter
            && TokenStreamHelper(&self.tts) == TokenStreamHelper(&other.tts)
    }
}

#[cfg(feature = "extra-traits")]
impl Hash for Macro {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        self.path.hash(state);
        self.bang_token.hash(state);
        self.delimiter.hash(state);
        TokenStreamHelper(&self.tts).hash(state);
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;

    use synom::Synom;

    impl Synom for Macro {
        named!(parse -> Self, do_parse!(
            what: call!(Path::parse_mod_style) >>
            bang: punct!(!) >>
            body: call!(tt::delimited) >>
            (Macro {
                path: what,
                bang_token: bang,
                delimiter: body.0,
                tts: body.1,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("macro invocation")
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use quote::{ToTokens, Tokens};

    impl ToTokens for Macro {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.path.to_tokens(tokens);
            self.bang_token.to_tokens(tokens);
            match self.delimiter {
                MacroDelimiter::Paren(ref paren) => {
                    paren.surround(tokens, |tokens| self.tts.to_tokens(tokens));
                }
                MacroDelimiter::Brace(ref brace) => {
                    brace.surround(tokens, |tokens| self.tts.to_tokens(tokens));
                }
                MacroDelimiter::Bracket(ref bracket) => {
                    bracket.surround(tokens, |tokens| self.tts.to_tokens(tokens));
                }
            }
        }
    }
}
