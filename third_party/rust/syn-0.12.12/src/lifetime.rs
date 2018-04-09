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

use proc_macro2::{Span, Term};
use unicode_xid::UnicodeXID;

/// A Rust lifetime: `'a`.
///
/// Lifetime names must conform to the following rules:
///
/// - Must start with an apostrophe.
/// - Must not consist of just an apostrophe: `'`.
/// - Must not consist of apostrophe + underscore: `'_`.
/// - Character after the apostrophe must be `_` or a Unicode code point with
///   the XID_Start property.
/// - All following characters must be Unicode code points with the XID_Continue
///   property.
///
/// *This type is available if Syn is built with the `"derive"` or `"full"`
/// feature.*
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Copy, Clone)]
pub struct Lifetime {
    term: Term,
    pub span: Span,
}

impl Lifetime {
    pub fn new(term: Term, span: Span) -> Self {
        let s = term.as_str();

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

        if s == "'_" {
            panic!("\"'_\" is not a valid lifetime name");
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
            term: term,
            span: span,
        }
    }
}

impl Display for Lifetime {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        self.term.as_str().fmt(formatter)
    }
}

impl PartialEq for Lifetime {
    fn eq(&self, other: &Lifetime) -> bool {
        self.term.as_str() == other.term.as_str()
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
        self.term.as_str().cmp(other.term.as_str())
    }
}

impl Hash for Lifetime {
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.term.as_str().hash(h)
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use synom::Synom;
    use buffer::Cursor;
    use parse_error;
    use synom::PResult;

    impl Synom for Lifetime {
        fn parse(input: Cursor) -> PResult<Self> {
            let (span, term, rest) = match input.term() {
                Some(term) => term,
                _ => return parse_error(),
            };
            if !term.as_str().starts_with('\'') {
                return parse_error();
            }

            Ok((
                Lifetime {
                    term: term,
                    span: span,
                },
                rest,
            ))
        }

        fn description() -> Option<&'static str> {
            Some("lifetime")
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use quote::{ToTokens, Tokens};
    use proc_macro2::{TokenNode, TokenTree};

    impl ToTokens for Lifetime {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append(TokenTree {
                span: self.span,
                kind: TokenNode::Term(self.term),
            })
        }
    }
}
