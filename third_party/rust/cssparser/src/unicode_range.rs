/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! https://drafts.csswg.org/css-syntax/#urange

use {Parser, ToCss};
use std::char;
use std::cmp;
use std::fmt;
use tokenizer::Token;

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
        let after_u = input.position();
        parse_tokens(input)?;

        // This deviates from the spec in case there are CSS comments
        // between tokens in the middle of one <unicode-range>,
        // but oh well…
        let concatenated_tokens = input.slice_from(after_u);

        let range = parse_concatenated(concatenated_tokens.as_bytes())?;
        if range.end > char::MAX as u32 || range.start > range.end {
            Err(())
        } else {
            Ok(range)
        }
    }
}

fn parse_tokens(input: &mut Parser) -> Result<(), ()> {
    match input.next_including_whitespace()? {
        Token::Delim('+') => {
            match input.next_including_whitespace()? {
                Token::Ident(_) => {}
                Token::Delim('?') => {}
                _ => return Err(())
            }
            parse_question_marks(input)
        }
        Token::Dimension(..) => {
            parse_question_marks(input)
        }
        Token::Number(_) => {
            let after_number = input.position();
            match input.next_including_whitespace() {
                Ok(Token::Delim('?')) => parse_question_marks(input),
                Ok(Token::Dimension(..)) => {}
                Ok(Token::Number(_)) => {}
                _ => input.reset(after_number)
            }
        }
        _ => return Err(())
    }
    Ok(())
}

/// Consume as many '?' as possible
fn parse_question_marks(input: &mut Parser) {
    loop {
        let position = input.position();
        match input.next_including_whitespace() {
            Ok(Token::Delim('?')) => {}
            _ => {
                input.reset(position);
                return
            }
        }
    }
}

fn parse_concatenated(text: &[u8]) -> Result<UnicodeRange, ()> {
    let mut text = match text.split_first() {
        Some((&b'+', text)) => text,
        _ => return Err(())
    };
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
