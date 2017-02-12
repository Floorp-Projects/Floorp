/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ascii::AsciiExt;

use super::{Token, Parser};


/// Parse the *An+B* notation, as found in the `:nth-child()` selector.
/// The input is typically the arguments of a function,
/// in which case the caller needs to check if the arguments’ parser is exhausted.
/// Return `Ok((A, B))`, or `Err(())` for a syntax error.
pub fn parse_nth(input: &mut Parser) -> Result<(i32, i32), ()> {
    match try!(input.next()) {
        Token::Number(value) => Ok((0, try!(value.int_value.ok_or(())) as i32)),
        Token::Dimension(value, unit) => {
            let a = try!(value.int_value.ok_or(())) as i32;
            match_ignore_ascii_case! { unit,
                "n" => parse_b(input, a),
                "n-" => parse_signless_b(input, a, -1),
                _ => Ok((a, try!(parse_n_dash_digits(&*unit))))
            }
        }
        Token::Ident(value) => {
            match_ignore_ascii_case! { value,
                "even" => Ok((2, 0)),
                "odd" => Ok((2, 1)),
                "n" => parse_b(input, 1),
                "-n" => parse_b(input, -1),
                "n-" => parse_signless_b(input, 1, -1),
                "-n-" => parse_signless_b(input, -1, -1),
                _ => if value.starts_with("-") {
                    Ok((-1, try!(parse_n_dash_digits(&value[1..]))))
                } else {
                    Ok((1, try!(parse_n_dash_digits(&*value))))
                }
            }
        }
        Token::Delim('+') => match try!(input.next_including_whitespace()) {
            Token::Ident(value) => {
                match_ignore_ascii_case! { value,
                    "n" => parse_b(input, 1),
                    "n-" => parse_signless_b(input, 1, -1),
                    _ => Ok((1, try!(parse_n_dash_digits(&*value))))
                }
            }
            _ => Err(())
        },
        _ => Err(())
    }
}


fn parse_b(input: &mut Parser, a: i32) -> Result<(i32, i32), ()> {
    let start_position = input.position();
    match input.next() {
        Ok(Token::Delim('+')) => parse_signless_b(input, a, 1),
        Ok(Token::Delim('-')) => parse_signless_b(input, a, -1),
        Ok(Token::Number(ref value)) if value.has_sign => {
            Ok((a, try!(value.int_value.ok_or(())) as i32))
        }
        _ => {
            input.reset(start_position);
            Ok((a, 0))
        }
    }
}

fn parse_signless_b(input: &mut Parser, a: i32, b_sign: i32) -> Result<(i32, i32), ()> {
    match try!(input.next()) {
        Token::Number(ref value) if !value.has_sign => {
            Ok((a, b_sign * (try!(value.int_value.ok_or(())) as i32)))
        }
        _ => Err(())
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
