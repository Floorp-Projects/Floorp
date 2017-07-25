/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ascii::AsciiExt;

use super::{Token, Parser, BasicParseError};


/// Parse the *An+B* notation, as found in the `:nth-child()` selector.
/// The input is typically the arguments of a function,
/// in which case the caller needs to check if the arguments’ parser is exhausted.
/// Return `Ok((A, B))`, or `Err(())` for a syntax error.
pub fn parse_nth<'i, 't>(input: &mut Parser<'i, 't>) -> Result<(i32, i32), BasicParseError<'i>> {
    // FIXME: remove .clone() when lifetimes are non-lexical.
    match input.next()?.clone() {
        Token::Number { int_value: Some(b), .. } => {
            Ok((0, b))
        }
        Token::Dimension { int_value: Some(a), unit, .. } => {
            match_ignore_ascii_case! {
                &unit,
                "n" => Ok(try!(parse_b(input, a))),
                "n-" => Ok(try!(parse_signless_b(input, a, -1))),
                _ => match parse_n_dash_digits(&*unit) {
                    Ok(b) => Ok((a, b)),
                    Err(()) => Err(BasicParseError::UnexpectedToken(Token::Ident(unit.clone())))
                }
            }
        }
        Token::Ident(value) => {
            match_ignore_ascii_case! { &value,
                "even" => Ok((2, 0)),
                "odd" => Ok((2, 1)),
                "n" => Ok(try!(parse_b(input, 1))),
                "-n" => Ok(try!(parse_b(input, -1))),
                "n-" => Ok(try!(parse_signless_b(input, 1, -1))),
                "-n-" => Ok(try!(parse_signless_b(input, -1, -1))),
                _ => {
                    let (slice, a) = if value.starts_with("-") {
                        (&value[1..], -1)
                    } else {
                        (&*value, 1)
                    };
                    match parse_n_dash_digits(slice) {
                        Ok(b) => Ok((a, b)),
                        Err(()) => Err(BasicParseError::UnexpectedToken(Token::Ident(value.clone())))
                    }
                }
            }
        }
        // FIXME: remove .clone() when lifetimes are non-lexical.
        Token::Delim('+') => match input.next_including_whitespace()?.clone() {
            Token::Ident(value) => {
                match_ignore_ascii_case! { &value,
                    "n" => parse_b(input, 1),
                    "n-" => parse_signless_b(input, 1, -1),
                    _ => match parse_n_dash_digits(&*value) {
                        Ok(b) => Ok((1, b)),
                        Err(()) => Err(BasicParseError::UnexpectedToken(Token::Ident(value.clone())))
                    }
                }
            }
            token => Err(BasicParseError::UnexpectedToken(token)),
        },
        token => Err(BasicParseError::UnexpectedToken(token)),
    }
}


fn parse_b<'i, 't>(input: &mut Parser<'i, 't>, a: i32) -> Result<(i32, i32), BasicParseError<'i>> {
    let start_position = input.position();
    match input.next() {
        Ok(&Token::Delim('+')) => parse_signless_b(input, a, 1),
        Ok(&Token::Delim('-')) => parse_signless_b(input, a, -1),
        Ok(&Token::Number { has_sign: true, int_value: Some(b), .. }) => Ok((a, b)),
        _ => {
            input.reset(start_position);
            Ok((a, 0))
        }
    }
}

fn parse_signless_b<'i, 't>(input: &mut Parser<'i, 't>, a: i32, b_sign: i32) -> Result<(i32, i32), BasicParseError<'i>> {
    match *input.next()? {
        Token::Number { has_sign: false, int_value: Some(b), .. } => Ok((a, b_sign * b)),
        ref token => Err(BasicParseError::UnexpectedToken(token.clone()))
    }
}

fn parse_n_dash_digits(string: &str) -> Result<i32, ()> {
    if string.len() >= 3
    && string[..2].eq_ignore_ascii_case("n-")
    && string[2..].chars().all(|c| matches!(c, '0'...'9'))
    {
        Ok(string[1..].parse().unwrap())  // Include the minus sign
    } else {
        Err(())
    }
}
