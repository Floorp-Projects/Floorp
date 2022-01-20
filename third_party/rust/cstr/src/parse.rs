use crate::Error;
use proc_macro2::{Delimiter, Ident, Literal, Span, TokenStream, TokenTree};
use std::char;

macro_rules! unexpected_content {
    () => {
        "expected one of: byte string literal, string literal, identifier"
    };
}

pub(crate) fn parse_input(mut input: TokenStream) -> Result<(Vec<u8>, Span), Error> {
    loop {
        let mut tokens = input.into_iter();
        let token = match tokens.next() {
            Some(token) => token,
            None => {
                return Err(Error(
                    Span::call_site(),
                    concat!("unexpected end of input, ", unexpected_content!()),
                ))
            }
        };
        let span = token.span();
        let result = match token {
            // Unwrap any empty group which may be created from macro expansion.
            TokenTree::Group(group) if group.delimiter() == Delimiter::None => Err(group),
            TokenTree::Literal(literal) => match parse_literal(literal) {
                Ok(result) => Ok(result),
                Err(msg) => return Err(Error(span, msg)),
            },
            TokenTree::Ident(ident) => Ok(parse_ident(ident)),
            _ => return Err(Error(span, unexpected_content!())),
        };
        if let Some(token) = tokens.next() {
            return Err(Error(token.span(), "unexpected token"));
        }
        match result {
            Ok(result) => return Ok((result, span)),
            Err(group) => input = group.stream(),
        }
    }
}

fn parse_literal(literal: Literal) -> Result<Vec<u8>, &'static str> {
    let s = literal.to_string();
    let s = s.as_bytes();
    match s[0] {
        b'"' => Ok(parse_cooked_content(&s)),
        b'r' => Ok(parse_raw_content(&s[1..])),
        b'b' => match s[1] {
            b'"' => Ok(parse_cooked_content(&s[1..])),
            b'r' => Ok(parse_raw_content(&s[2..])),
            _ => Err(unexpected_content!()),
        },
        _ => Err(unexpected_content!()),
    }
}

fn all_pounds(bytes: &[u8]) -> bool {
    bytes.iter().all(|b| *b == b'#')
}

/// Parses raw string / bytes content after `r` prefix.
fn parse_raw_content(s: &[u8]) -> Vec<u8> {
    let q_start = s.iter().position(|b| *b == b'"').unwrap();
    let q_end = s.iter().rposition(|b| *b == b'"').unwrap();
    assert!(all_pounds(&s[0..q_start]));
    assert!(all_pounds(&s[q_end + 1..q_end + q_start + 1]));
    Vec::from(&s[q_start + 1..q_end])
}

/// Parses the cooked string / bytes content within quotes.
fn parse_cooked_content(mut s: &[u8]) -> Vec<u8> {
    s = &s[1..s.iter().rposition(|b| *b == b'"').unwrap()];
    let mut result = Vec::new();
    while !s.is_empty() {
        match s[0] {
            b'\\' => {}
            b'\r' => {
                assert_eq!(s[1], b'\n');
                result.push(b'\n');
                s = &s[2..];
                continue;
            }
            b => {
                result.push(b);
                s = &s[1..];
                continue;
            }
        }
        let b = s[1];
        s = &s[2..];
        match b {
            b'x' => {
                let (b, rest) = backslash_x(&s);
                result.push(b);
                s = rest;
            }
            b'u' => {
                let (c, rest) = backslash_u(&s);
                result.extend_from_slice(c.encode_utf8(&mut [0; 4]).as_bytes());
                s = rest;
            }
            b'n' => result.push(b'\n'),
            b'r' => result.push(b'\r'),
            b't' => result.push(b'\t'),
            b'\\' => result.push(b'\\'),
            b'0' => result.push(b'\0'),
            b'\'' => result.push(b'\''),
            b'"' => result.push(b'"'),
            b'\r' | b'\n' => {
                let next = s.iter().position(|b| {
                    let ch = char::from_u32(u32::from(*b)).unwrap();
                    !ch.is_whitespace()
                });
                match next {
                    Some(pos) => s = &s[pos..],
                    None => s = b"",
                }
            }
            b => panic!("unexpected byte {:?} after \\", b),
        }
    }
    result
}

fn backslash_x(s: &[u8]) -> (u8, &[u8]) {
    let ch = hex_to_u8(s[0]) * 0x10 + hex_to_u8(s[1]);
    (ch, &s[2..])
}

fn hex_to_u8(b: u8) -> u8 {
    match b {
        b'0'..=b'9' => b - b'0',
        b'a'..=b'f' => b - b'a' + 10,
        b'A'..=b'F' => b - b'A' + 10,
        _ => unreachable!("unexpected non-hex character {:?} after \\x", b),
    }
}

fn backslash_u(s: &[u8]) -> (char, &[u8]) {
    assert_eq!(s[0], b'{');
    let end = s[1..].iter().position(|b| *b == b'}').unwrap();
    let mut ch = 0;
    for b in &s[1..=end] {
        ch *= 0x10;
        ch += u32::from(hex_to_u8(*b));
    }
    (char::from_u32(ch).unwrap(), &s[end + 2..])
}

fn parse_ident(ident: Ident) -> Vec<u8> {
    ident.to_string().into_bytes()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::str::FromStr;

    // Tests below were modified from
    // https://github.com/dtolnay/syn/blob/cd5fdc0f530f822446fccaf831669cd0cf4a0fc9/tests/test_lit.rs

    fn lit(s: &str) -> Vec<u8> {
        match TokenStream::from_str(s)
            .unwrap()
            .into_iter()
            .next()
            .unwrap()
        {
            TokenTree::Literal(lit) => parse_literal(lit).unwrap(),
            _ => panic!(),
        }
    }

    #[test]
    fn strings() {
        #[track_caller]
        fn test_string(s: &str, value: &[u8]) {
            assert_eq!(lit(s), value);
        }

        test_string("\"a\"", b"a");
        test_string("\"\\n\"", b"\n");
        test_string("\"\\r\"", b"\r");
        test_string("\"\\t\"", b"\t");
        test_string("\"🐕\"", b"\xf0\x9f\x90\x95"); // NOTE: This is an emoji
        test_string("\"\\\"\"", b"\"");
        test_string("\"'\"", b"'");
        test_string("\"\"", b"");
        test_string("\"\\u{1F415}\"", b"\xf0\x9f\x90\x95");
        test_string(
            "\"contains\nnewlines\\\nescaped newlines\"",
            b"contains\nnewlinesescaped newlines",
        );
        test_string("r\"raw\nstring\\\nhere\"", b"raw\nstring\\\nhere");
        test_string("\"...\"q", b"...");
        test_string("r\"...\"q", b"...");
        test_string("r##\"...\"##q", b"...");
    }

    #[test]
    fn byte_strings() {
        #[track_caller]
        fn test_byte_string(s: &str, value: &[u8]) {
            assert_eq!(lit(s), value);
        }

        test_byte_string("b\"a\"", b"a");
        test_byte_string("b\"\\n\"", b"\n");
        test_byte_string("b\"\\r\"", b"\r");
        test_byte_string("b\"\\t\"", b"\t");
        test_byte_string("b\"\\\"\"", b"\"");
        test_byte_string("b\"'\"", b"'");
        test_byte_string("b\"\"", b"");
        test_byte_string(
            "b\"contains\nnewlines\\\nescaped newlines\"",
            b"contains\nnewlinesescaped newlines",
        );
        test_byte_string("br\"raw\nstring\\\nhere\"", b"raw\nstring\\\nhere");
        test_byte_string("b\"...\"q", b"...");
        test_byte_string("br\"...\"q", b"...");
        test_byte_string("br##\"...\"##q", b"...");
    }
}
