/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use cow_rc_str::CowRcStr;
use std::borrow::Cow;
use tokenizer::Token;

macro_rules! size_of_test {
    ($testname: ident, $t: ty, $expected_size: expr) => {
        #[test]
        fn $testname() {
            let new = ::std::mem::size_of::<$t>();
            let old = $expected_size;
            if new < old {
                panic!(
                    "Your changes have decreased the stack size of {} from {} to {}. \
                     Good work! Please update the expected size in {}.",
                    stringify!($t), old, new, file!()
                )
            } else if new > old {
                panic!(
                    "Your changes have increased the stack size of {} from {} to {}. \
                     Please consider choosing a design which avoids this increase. \
                     If you feel that the increase is necessary, update the size in {}.",
                    stringify!($t), old, new, file!()
                )
            }
        }
    }
}

// Some of these assume 64-bit
size_of_test!(token, Token, 32);
size_of_test!(std_cow_str, Cow<'static, str>, 32);
size_of_test!(cow_rc_str, CowRcStr, 16);

size_of_test!(tokenizer, ::tokenizer::Tokenizer, 72);
size_of_test!(parser_input, ::parser::ParserInput, if cfg!(rustc_has_pr45225) { 136 } else { 144 });
size_of_test!(parser, ::parser::Parser, 16);
size_of_test!(source_position, ::SourcePosition, 8);
size_of_test!(parser_state, ::ParserState, 24);

size_of_test!(basic_parse_error, ::BasicParseError, 48);
size_of_test!(parse_error_lower_bound, ::ParseError<()>, if cfg!(rustc_has_pr45225) { 48 } else { 56 });
