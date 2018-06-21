// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::cmp::Ordering;
use std::fmt::{self, Display};
use std::hash::{Hash, Hasher};

use proc_macro2::{Ident, Span};
use unicode_xid::UnicodeXID;

use token::Apostrophe;

/// A Rust lifetime: `'a`.
///
/// Lifetime names must conform to the following rules:
///
/// - Must start with an apostrophe.
/// - Must not consist of just an apostrophe: `'`.
/// - Character after the apostrophe must be `_` or a Unicode code point with
///   the XID_Start property.
/// - All following characters must be Unicode code points with the XID_Continue
///   property.
///
/// *This type is available if Syn is built with the `"derive"` or `"full"`
/// feature.*
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct Lifetime {
    pub apostrophe: Apostrophe,
    pub ident: Ident,
}

impl Lifetime {
    pub fn new(s: &str, span: Span) -> Self {
        if !s.starts_with('\'') {
            panic!(
                "lifetime name must start with apostrophe as in \"'a\", \
                 got {:?}",
                s
            );
        }

        if s == "'" {
            panic!("lifetime name must not be empty");
        }

        fn xid_ok(s: &str) -> bool {
            let mut chars = s.chars();
            let first = chars.next().unwrap();
            if !(UnicodeXID::is_xid_start(first) || first == '_') {
                return false;
            }
            for ch in chars {
                if !UnicodeXID::is_xid_continue(ch) {
                    return false;
                }
            }
            true
        }

        if !xid_ok(&s[1..]) {
            panic!("{:?} is not a valid lifetime name", s);
        }

        Lifetime {
            apostrophe: Default::default(),
            ident: Ident::new(&s[1..], span),
        }
    }
}

impl Display for Lifetime {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        "'".fmt(formatter)?;
        self.ident.fmt(formatter)
    }
}

impl PartialEq for Lifetime {
    fn eq(&self, other: &Lifetime) -> bool {
        self.ident.eq(&other.ident)
    }
}

impl Eq for Lifetime {}

impl PartialOrd for Lifetime {
    fn partial_cmp(&self, other: &Lifetime) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Lifetime {
    fn cmp(&self, other: &Lifetime) -> Ordering {
        self.ident.cmp(&other.ident)
    }
}

impl Hash for Lifetime {
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.ident.hash(h)
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use buffer::Cursor;
    use parse_error;
    use synom::PResult;
    use synom::Synom;

    impl Synom for Lifetime {
        fn parse(input: Cursor) -> PResult<Self> {
            let (apostrophe, rest) = Apostrophe::parse(input)?;
            let (ident, rest) = match rest.ident() {
                Some(pair) => pair,
                None => return parse_error(),
            };

            let ret = Lifetime {
                ident: ident,
                apostrophe: apostrophe,
            };
            Ok((ret, rest))
        }

        fn description() -> Option<&'static str> {
            Some("lifetime")
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use proc_macro2::TokenStream;
    use quote::ToTokens;

    impl ToTokens for Lifetime {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.apostrophe.to_tokens(tokens);
            self.ident.to_tokens(tokens);
        }
    }
}
