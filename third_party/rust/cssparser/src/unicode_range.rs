/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! https://drafts.csswg.org/css-syntax/#urange

use {Parser, ToCss};
use std::char;
use std::cmp;
use std::fmt;
use std::io::{self, Write};
use tokenizer::{Token, NumericValue};

/// One contiguous range of code points.
///
/// Can not be empty. Can represent a single code point when start == end.
#[derive(PartialEq, Eq, Clone, Hash)]
pub struct UnicodeRange {
    /// Inclusive start of the range. In [0, end].
    pub start: u32,

    /// Inclusive end of the range. In [0, 0x10FFFF].
    pub end: u32,
}

impl UnicodeRange {
    /// https://drafts.csswg.org/css-syntax/#urange-syntax
    pub fn parse(input: &mut Parser) -> Result<Self, ()> {
        // <urange> =
        //   u '+' <ident-token> '?'* |
        //   u <dimension-token> '?'* |
        //   u <number-token> '?'* |
        //   u <number-token> <dimension-token> |
        //   u <number-token> <number-token> |
        //   u '+' '?'+

        input.expect_ident_matching("u")?;

        // Since start or end can’t be above 0x10FFFF, they can’t have more than 6 hex digits
        // Conversely, input with more digits would end up returning Err anyway.
        const MAX_LENGTH_AFTER_U_PLUS: usize = 6 + 1 + 6; // 6 digits, '-', 6 digits
        let mut buffer = [0; MAX_LENGTH_AFTER_U_PLUS];

        let remaining_len;
        {
            let mut remaining = &mut buffer[..];
            concatenate_tokens(input, &mut remaining)?;
            remaining_len = remaining.len();
        }

        let text_len = buffer.len() - remaining_len;
        let text = &buffer[..text_len];
        let range = parse_concatenated(text)?;
        if range.end > char::MAX as u32 || range.start > range.end {
            Err(())
        } else {
            Ok(range)
        }
    }
}

fn concatenate_tokens(input: &mut Parser, remaining: &mut &mut [u8]) -> Result<(), Error> {
    match input.next_including_whitespace()? {
        Token::Delim('+') => {
            match input.next_including_whitespace()? {
                Token::Ident(ident) => remaining.write_all(ident.as_bytes())?,
                Token::Delim('?') => remaining.write_all(b"?")?,
                _ => return Err(Error)
            }
            parse_question_marks(input, remaining)
        }

        Token::Dimension(ref value, ref unit) => {
            // Require a '+' sign as part of the number
            let int_value = positive_integer_with_plus_sign(value)?;
            write!(remaining, "{}{}", int_value, unit)?;
            parse_question_marks(input, remaining)
        }

        Token::Number(ref value) => {
            // Require a '+' sign as part of the number
            let int_value = positive_integer_with_plus_sign(value)?;
            write!(remaining, "{}", int_value)?;

            match input.next_including_whitespace() {
                // EOF here is fine
                Err(()) => {},

                Ok(Token::Delim('?')) => {
                    // If `remaining` is already full, `int_value` has too many digits
                    // so we can use `result?` Rust syntax.
                    remaining.write_all(b"?")?;
                    parse_question_marks(input, remaining)
                }

                Ok(Token::Dimension(ref value, ref unit)) => {
                    // Require a '-' sign as part of the number
                    let int_value = negative_integer(value)?;
                    write!(remaining, "{}{}", int_value, unit)?
                }

                Ok(Token::Number(ref value)) => {
                    // Require a '-' sign as part of the number
                    let int_value = negative_integer(value)?;
                    write!(remaining, "{}", int_value)?
                }

                _ => return Err(Error)
            }
        }

        _ => return Err(Error)
    }
    Ok(())
}

/// Consume as many '?' as possible and write them to `remaining` until it’s full
fn parse_question_marks(input: &mut Parser, remaining: &mut &mut [u8]) {
    loop {
        let result = input.try(|input| {
            match input.next_including_whitespace() {
                Ok(Token::Delim('?')) => remaining.write_all(b"?").map_err(|_| ()),
                _ => Err(())
            }
        });
        if result.is_err() {
            return
        }
    }
}

fn positive_integer_with_plus_sign(value: &NumericValue) -> Result<i32, ()> {
    let int_value = value.int_value.ok_or(())?;
    if value.has_sign && int_value >= 0 {
        Ok(int_value)
    } else {
        Err(())
    }
}

fn negative_integer(value: &NumericValue) -> Result<i32, ()> {  // Necessarily had a negative sign.
    let int_value = value.int_value.ok_or(())?;
    if int_value <= 0 {
        Ok(int_value)
    } else {
        Err(())
    }
}

fn parse_concatenated(mut text: &[u8]) -> Result<UnicodeRange, ()> {
    let (first_hex_value, hex_digit_count) = consume_hex(&mut text);
    let question_marks = consume_question_marks(&mut text);
    let consumed = hex_digit_count + question_marks;
    if consumed == 0 || consumed > 6 {
        return Err(())
    }

    if question_marks > 0 {
        if text.is_empty() {
            return Ok(UnicodeRange {
                start: first_hex_value << (question_marks * 4),
                end: ((first_hex_value + 1) << (question_marks * 4)) - 1,
            })
        }
    } else if text.is_empty() {
        return Ok(UnicodeRange {
            start: first_hex_value,
            end: first_hex_value,
        })
    } else {
        if let Some((&b'-', mut text)) = text.split_first() {
            let (second_hex_value, hex_digit_count) = consume_hex(&mut text);
            if hex_digit_count > 0 && hex_digit_count <= 6 && text.is_empty() {
                return Ok(UnicodeRange {
                    start: first_hex_value,
                    end: second_hex_value,
                })
            }
        }
    }
    Err(())
}

fn consume_hex(text: &mut &[u8]) -> (u32, usize) {
    let mut value = 0;
    let mut digits = 0;
    while let Some((&byte, rest)) = text.split_first() {
        if let Some(digit_value) = (byte as char).to_digit(16) {
            value = value * 0x10 + digit_value;
            digits += 1;
            *text = rest
        } else {
            break
        }
    }
    (value, digits)
}

fn consume_question_marks(text: &mut &[u8]) -> usize {
    let mut question_marks = 0;
    while let Some((&b'?', rest)) = text.split_first() {
        question_marks += 1;
        *text = rest
    }
    question_marks
}

impl fmt::Debug for UnicodeRange {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        self.to_css(formatter)
    }
}

impl ToCss for UnicodeRange {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result where W: fmt::Write {
        dest.write_str("U+")?;

        // How many bits are 0 at the end of start and also 1 at the end of end.
        let bits = cmp::min(self.start.trailing_zeros(), (!self.end).trailing_zeros());

        let question_marks = bits / 4;

        // How many lower bits can be represented as question marks
        let bits = question_marks * 4;

        let truncated_start = self.start >> bits;
        let truncated_end = self.end >> bits;
        if truncated_start == truncated_end {
            // Bits not covered by question marks are the same in start and end,
            // we can use the question mark syntax.
            if truncated_start != 0 {
                write!(dest, "{:X}", truncated_start)?;
            }
            for _ in 0..question_marks {
                dest.write_str("?")?;
            }
        } else {
            write!(dest, "{:X}", self.start)?;
            if self.end != self.start {
                write!(dest, "-{:X}", self.end)?;
            }
        }
        Ok(())
    }
}

/// Make conversions from io::Error implicit in `?` syntax.
struct Error;

impl From<Error> for () {
    fn from(_: Error) -> Self { () }
}

impl From<()> for Error {
    fn from(_: ()) -> Self { Error }
}

impl From<io::Error> for Error {
    fn from(_: io::Error) -> Self { Error }
}
