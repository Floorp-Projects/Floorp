/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![crate_name = "cssparser"]
#![crate_type = "rlib"]
#![cfg_attr(feature = "bench", feature(test))]
#![deny(missing_docs)]

/*!

Implementation of [CSS Syntax Module Level 3](https://drafts.csswg.org/css-syntax/) for Rust.

# Input

Everything is based on `Parser` objects, which borrow a `&str` input.
If you have bytes (from a file, the network, or something)
and want to support character encodings other than UTF-8,
see the `stylesheet_encoding` function,
which can be used together with rust-encoding or encoding-rs.

# Conventions for parsing functions

* Take (at least) a `input: &mut cssparser::Parser` parameter
* Return `Result<_, ()>`
* When returning `Ok(_)`,
  the function must have consumed exactly the amount of input that represents the parsed value.
* When returning `Err(())`, any amount of input may have been consumed.

As a consequence, when calling another parsing function, either:

* Any `Err(())` return value must be propagated.
  This happens by definition for tail calls,
  and can otherwise be done with the `?` operator.
* Or the call must be wrapped in a `Parser::try` call.
  `try` takes a closure that takes a `Parser` and returns a `Result`,
  calls it once,
  and returns itself that same result.
  If the result is `Err`,
  it restores the position inside the input to the one saved before calling the closure.

Examples:

```{rust,ignore}
// 'none' | <image>
fn parse_background_image(context: &ParserContext, input: &mut Parser)
                                    -> Result<Option<Image>, ()> {
    if input.try_parse(|input| input.expect_ident_matching("none")).is_ok() {
        Ok(None)
    } else {
        Image::parse(context, input).map(Some)  // tail call
    }
}
```

```{rust,ignore}
// [ <length> | <percentage> ] [ <length> | <percentage> ]?
fn parse_border_spacing(_context: &ParserContext, input: &mut Parser)
                          -> Result<(LengthOrPercentage, LengthOrPercentage), ()> {
    let first = LengthOrPercentage::parse?;
    let second = input.try_parse(LengthOrPercentage::parse).unwrap_or(first);
    (first, second)
}
```

*/

#![recursion_limit = "200"] // For color::parse_color_keyword

extern crate dtoa_short;
extern crate itoa;
#[macro_use]
extern crate cssparser_macros;
#[macro_use]
extern crate matches;
#[macro_use]
extern crate procedural_masquerade;
#[cfg(test)]
extern crate difference;
#[cfg(test)]
extern crate encoding_rs;
#[doc(hidden)]
pub extern crate phf as _internal__phf;
#[cfg(test)]
extern crate rustc_serialize;
#[cfg(feature = "serde")]
extern crate serde;
#[cfg(feature = "heapsize")]
#[macro_use]
extern crate heapsize;
extern crate smallvec;

pub use cssparser_macros::*;

pub use color::{
    parse_color_keyword, AngleOrNumber, Color, ColorComponentParser, NumberOrPercentage, RGBA,
};
pub use cow_rc_str::CowRcStr;
pub use from_bytes::{stylesheet_encoding, EncodingSupport};
pub use nth::parse_nth;
pub use parser::{BasicParseError, BasicParseErrorKind, ParseError, ParseErrorKind};
pub use parser::{Delimiter, Delimiters, Parser, ParserInput, ParserState};
pub use rules_and_declarations::parse_important;
pub use rules_and_declarations::{parse_one_declaration, DeclarationListParser, DeclarationParser};
pub use rules_and_declarations::{parse_one_rule, RuleListParser};
pub use rules_and_declarations::{AtRuleParser, AtRuleType, QualifiedRuleParser};
pub use serializer::{
    serialize_identifier, serialize_name, serialize_string, CssStringWriter, ToCss,
    TokenSerializationType,
};
pub use tokenizer::{SourceLocation, SourcePosition, Token};
pub use unicode_range::UnicodeRange;

// For macros
#[doc(hidden)]
pub use macros::_internal__to_lowercase;

// For macros when used in this crate. Unsure how $crate works with procedural-masquerade.
mod cssparser {
    pub use _internal__phf;
}

#[macro_use]
mod macros;

mod rules_and_declarations;

#[cfg(feature = "dummy_match_byte")]
mod tokenizer;

#[cfg(not(feature = "dummy_match_byte"))]
mod tokenizer {
    include!(concat!(env!("OUT_DIR"), "/tokenizer.rs"));
}
mod color;
mod cow_rc_str;
mod from_bytes;
mod nth;
mod parser;
mod serializer;
mod unicode_range;

#[cfg(test)]
mod size_of_tests;
#[cfg(test)]
mod tests;
