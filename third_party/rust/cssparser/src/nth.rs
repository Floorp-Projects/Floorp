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
    let token = input.next()?;
    match token {
        Token::Number { int_value: Some(b), .. } => {
            Ok((0, b))
        }
        Token::Dimension { int_value: Some(a), ref unit, .. } => {
            match_ignore_ascii_case! {
                &unit,
                "n" => Ok(try!(parse_b(input, a))),
                "n-" => Ok(try!(parse_signless_b(input, a, -1))),
                _ => {
                    parse_n_dash_digits(&*unit).map(|val| (a, val))
                }
            }
        }
        Token::Ident(ref value) => {
            match_ignore_ascii_case! { &value,
                "even" => Ok((2, 0)),
                "odd" => Ok((2, 1)),
                "n" => Ok(try!(parse_b(input, 1))),
                "-n" => Ok(try!(parse_b(input, -1))),
                "n-" => Ok(try!(parse_signless_b(input, 1, -1))),
                "-n-" => Ok(try!(parse_signless_b(input, -1, -1))),
                _ => if value.starts_with("-") {
                    parse_n_dash_digits(&value[1..]).map(|v| (-1, v))
                } else {
                    parse_n_dash_digits(&*value).map(|v| (1, v))
                }
            }
        }
        Token::Delim('+') => match input.next_including_whitespace()? {
            Token::Ident(value) => {
                match_ignore_ascii_case! { &value,
                    "n" => Ok(try!(parse_b(input, 1))),
                    "n-" => Ok(try!(parse_signless_b(input, 1, -1))),
                    _ => parse_n_dash_digits(&*value).map(|v| (1, v))
                }
            }
            t => return Err(BasicParseError::UnexpectedToken(t)),
        },
        _ => Err(()),
    }.map_err(|()| BasicParseError::UnexpectedToken(token))
}


fn parse_b<'i, 't>(input: &mut Parser<'i, 't>, a: i32) -> Result<(i32, i32), BasicParseError<'i>> {
    let start_position = input.position();
    let token = input.next();
    match token {
        Ok(Token::Delim('+')) => Ok(parse_signless_b(input, a, 1)?),
        Ok(Token::Delim('-')) => Ok(parse_signless_b(input, a, -1)?),
        Ok(Token::Number { has_sign: true, int_value: Some(b), .. }) => Ok((a, b)),
        _ => {
            input.reset(start_position);
            Ok((a, 0))
        }
    }.map_err(|()| BasicParseError::UnexpectedToken(token.unwrap()))
}

fn parse_signless_b<'i, 't>(input: &mut Parser<'i, 't>, a: i32, b_sign: i32) -> Result<(i32, i32), BasicParseError<'i>> {
    let token = input.next()?;
    match token {
        Token::Number { has_sign: false, int_value: Some(b), .. } => Ok((a, b_sign * b)),
        _ => Err(())
    }.map_err(|()| BasicParseError::UnexpectedToken(token))
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
