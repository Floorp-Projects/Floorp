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
  and can otherwise be done with the `try!` macro.
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
    if input.try(|input| input.expect_ident_matching("none")).is_ok() {
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
    let first = try!(LengthOrPercentage::parse);
    let second = input.try(LengthOrPercentage::parse).unwrap_or(first);
    (first, second)
}
```

*/

#![recursion_limit="200"]  // For color::parse_color_keyword

extern crate dtoa_short;
extern crate itoa;
#[macro_use] extern crate cssparser_macros;
#[macro_use] extern crate matches;
#[macro_use] extern crate procedural_masquerade;
#[doc(hidden)] pub extern crate phf as _internal__phf;
#[cfg(test)] extern crate encoding_rs;
#[cfg(test)] extern crate difference;
#[cfg(test)] extern crate rustc_serialize;
#[cfg(feature = "serde")] extern crate serde;
#[cfg(feature = "heapsize")] #[macro_use] extern crate heapsize;
extern crate smallvec;

pub use cssparser_macros::*;

pub use tokenizer::{Token, SourcePosition, SourceLocation};
pub use rules_and_declarations::{parse_important};
pub use rules_and_declarations::{DeclarationParser, DeclarationListParser, parse_one_declaration};
pub use rules_and_declarations::{RuleListParser, parse_one_rule};
pub use rules_and_declarations::{AtRuleType, QualifiedRuleParser, AtRuleParser};
pub use from_bytes::{stylesheet_encoding, EncodingSupport};
pub use color::{RGBA, Color, parse_color_keyword, AngleOrNumber, NumberOrPercentage, ColorComponentParser};
pub use nth::parse_nth;
pub use serializer::{ToCss, CssStringWriter, serialize_identifier, serialize_name, serialize_string, TokenSerializationType};
pub use parser::{Parser, Delimiter, Delimiters, ParserState, ParserInput};
pub use parser::{ParseError, ParseErrorKind, BasicParseError, BasicParseErrorKind};
pub use unicode_range::UnicodeRange;
pub use cow_rc_str::CowRcStr;

// For macros
#[doc(hidden)] pub use macros::_internal__to_lowercase;

// For macros when used in this crate. Unsure how $crate works with procedural-masquerade.
mod cssparser { pub use _internal__phf; }

#[macro_use]
mod macros;

mod rules_and_declarations;

#[cfg(feature = "dummy_match_byte")]
mod tokenizer;

#[cfg(not(feature = "dummy_match_byte"))]
mod tokenizer {
    include!(concat!(env!("OUT_DIR"), "/tokenizer.rs"));
}
mod parser;
mod from_bytes;
mod color;
mod nth;
mod serializer;
mod unicode_range;
mod cow_rc_str;

#[cfg(test)] mod tests;
#[cfg(test)] mod size_of_tests;
