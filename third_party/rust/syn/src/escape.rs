use std::{char, str};
use std::num::ParseIntError;
use synom::IResult;

pub fn cooked_string(input: &str) -> IResult<&str, String> {
    let mut s = String::new();
    let mut chars = input.char_indices().peekable();
    while let Some((byte_offset, ch)) = chars.next() {
        match ch {
            '"' => {
                return IResult::Done(&input[byte_offset..], s);
            }
            '\r' => {
                if let Some((_, '\n')) = chars.next() {
                    s.push('\n');
                } else {
                    break;
                }
            }
            '\\' => {
                match chars.next() {
                    Some((_, 'x')) => {
                        match backslash_x_char(&mut chars) {
                            Some(ch) => s.push(ch),
                            None => break,
                        }
                    }
                    Some((_, 'n')) => s.push('\n'),
                    Some((_, 'r')) => s.push('\r'),
                    Some((_, 't')) => s.push('\t'),
                    Some((_, '\\')) => s.push('\\'),
                    Some((_, '0')) => s.push('\0'),
                    Some((_, 'u')) => {
                        match backslash_u(&mut chars) {
                            Some(ch) => s.push(ch),
                            None => break,
                        }
                    }
                    Some((_, '\'')) => s.push('\''),
                    Some((_, '"')) => s.push('"'),
                    Some((_, '\n')) | Some((_, '\r')) => {
                        while let Some(&(_, ch)) = chars.peek() {
                            if ch.is_whitespace() {
                                chars.next();
                            } else {
                                break;
                            }
                        }
                    }
                    _ => break,
                }
            }
            ch => {
                s.push(ch);
            }
        }
    }
    IResult::Error
}

pub fn cooked_byte_string(mut input: &str) -> IResult<&str, Vec<u8>> {
    let mut vec = Vec::new();
    let mut bytes = input.bytes().enumerate();
    'outer: while let Some((offset, b)) = bytes.next() {
        match b {
            b'"' => {
                return IResult::Done(&input[offset..], vec);
            }
            b'\r' => {
                if let Some((_, b'\n')) = bytes.next() {
                    vec.push(b'\n');
                } else {
                    break;
                }
            }
            b'\\' => {
                match bytes.next() {
                    Some((_, b'x')) => {
                        match backslash_x_byte(&mut bytes) {
                            Some(b) => vec.push(b),
                            None => break,
                        }
                    }
                    Some((_, b'n')) => vec.push(b'\n'),
                    Some((_, b'r')) => vec.push(b'\r'),
                    Some((_, b't')) => vec.push(b'\t'),
                    Some((_, b'\\')) => vec.push(b'\\'),
                    Some((_, b'0')) => vec.push(b'\0'),
                    Some((_, b'\'')) => vec.push(b'\''),
                    Some((_, b'"')) => vec.push(b'"'),
                    Some((newline, b'\n')) |
                    Some((newline, b'\r')) => {
                        let rest = &input[newline + 1..];
                        for (offset, ch) in rest.char_indices() {
                            if !ch.is_whitespace() {
                                input = &rest[offset..];
                                bytes = input.bytes().enumerate();
                                continue 'outer;
                            }
                        }
                        break;
                    }
                    _ => break,
                }
            }
            b if b < 0x80 => {
                vec.push(b);
            }
            _ => break,
        }
    }
    IResult::Error
}

pub fn cooked_char(input: &str) -> IResult<&str, char> {
    let mut chars = input.char_indices();
    let ch = match chars.next().map(|(_, ch)| ch) {
        Some('\\') => {
            match chars.next().map(|(_, ch)| ch) {
                Some('x') => backslash_x_char(&mut chars),
                Some('n') => Some('\n'),
                Some('r') => Some('\r'),
                Some('t') => Some('\t'),
                Some('\\') => Some('\\'),
                Some('0') => Some('\0'),
                Some('u') => backslash_u(&mut chars),
                Some('\'') => Some('\''),
                Some('"') => Some('"'),
                _ => None,
            }
        }
        ch => ch,
    };
    match ch {
        Some(ch) => IResult::Done(chars.as_str(), ch),
        None => IResult::Error,
    }
}

pub fn cooked_byte(input: &str) -> IResult<&str, u8> {
    let mut bytes = input.bytes().enumerate();
    let b = match bytes.next().map(|(_, b)| b) {
        Some(b'\\') => {
            match bytes.next().map(|(_, b)| b) {
                Some(b'x') => backslash_x_byte(&mut bytes),
                Some(b'n') => Some(b'\n'),
                Some(b'r') => Some(b'\r'),
                Some(b't') => Some(b'\t'),
                Some(b'\\') => Some(b'\\'),
                Some(b'0') => Some(b'\0'),
                Some(b'\'') => Some(b'\''),
                Some(b'"') => Some(b'"'),
                _ => None,
            }
        }
        b => b,
    };
    match b {
        Some(b) => {
            match bytes.next() {
                Some((offset, _)) => IResult::Done(&input[offset..], b),
                None => IResult::Done("", b),
            }
        }
        None => IResult::Error,
    }
}

pub fn raw_string(input: &str) -> IResult<&str, (String, usize)> {
    let mut chars = input.char_indices();
    let mut n = 0;
    while let Some((byte_offset, ch)) = chars.next() {
        match ch {
            '"' => {
                n = byte_offset;
                break;
            }
            '#' => {}
            _ => return IResult::Error,
        }
    }
    let mut s = String::new();
    for (byte_offset, ch) in chars {
        match ch {
            '"' if input[byte_offset + 1..].starts_with(&input[..n]) => {
                let rest = &input[byte_offset + 1 + n..];
                return IResult::Done(rest, (s, n));
            }
            '\r' => {}
            _ => s.push(ch),
        }
    }
    IResult::Error
}

macro_rules! next_ch {
    ($chars:ident @ $pat:pat $(| $rest:pat)*) => {
        match $chars.next() {
            Some((_, ch)) => match ch {
                $pat $(| $rest)*  => ch,
                _ => return None,
            },
            None => return None,
        }
    };
}

trait FromStrRadix: Sized {
    fn from_str_radix(src: &str, radix: u32) -> Result<Self, ParseIntError>;
}

impl FromStrRadix for u8 {
    fn from_str_radix(src: &str, radix: u32) -> Result<Self, ParseIntError> {
        u8::from_str_radix(src, radix)
    }
}

impl FromStrRadix for u32 {
    fn from_str_radix(src: &str, radix: u32) -> Result<Self, ParseIntError> {
        u32::from_str_radix(src, radix)
    }
}

macro_rules! from_hex {
    ($($ch:ident)+) => {{
        let hex_bytes = &[$($ch as u8),*];
        let hex_str = str::from_utf8(hex_bytes).unwrap();
        FromStrRadix::from_str_radix(hex_str, 16).unwrap()
    }};
}

#[cfg_attr(feature = "cargo-clippy", allow(diverging_sub_expression))]
fn backslash_x_char<I>(chars: &mut I) -> Option<char>
    where I: Iterator<Item = (usize, char)>
{
    let a = next_ch!(chars @ '0'...'7');
    let b = next_ch!(chars @ '0'...'9' | 'a'...'f' | 'A'...'F');
    char::from_u32(from_hex!(a b))
}

#[cfg_attr(feature = "cargo-clippy", allow(diverging_sub_expression))]
fn backslash_x_byte<I>(chars: &mut I) -> Option<u8>
    where I: Iterator<Item = (usize, u8)>
{
    let a = next_ch!(chars @ b'0'...b'9' | b'a'...b'f' | b'A'...b'F');
    let b = next_ch!(chars @ b'0'...b'9' | b'a'...b'f' | b'A'...b'F');
    Some(from_hex!(a b))
}

#[cfg_attr(feature = "cargo-clippy", allow(diverging_sub_expression, many_single_char_names))]
fn backslash_u<I>(chars: &mut I) -> Option<char>
    where I: Iterator<Item = (usize, char)>
{
    next_ch!(chars @ '{');
    let a = next_ch!(chars @ '0'...'9' | 'a'...'f' | 'A'...'F');
    let b = next_ch!(chars @ '0'...'9' | 'a'...'f' | 'A'...'F' | '}');
    if b == '}' {
        return char::from_u32(from_hex!(a));
    }
    let c = next_ch!(chars @ '0'...'9' | 'a'...'f' | 'A'...'F' | '}');
    if c == '}' {
        return char::from_u32(from_hex!(a b));
    }
    let d = next_ch!(chars @ '0'...'9' | 'a'...'f' | 'A'...'F' | '}');
    if d == '}' {
        return char::from_u32(from_hex!(a b c));
    }
    let e = next_ch!(chars @ '0'...'9' | 'a'...'f' | 'A'...'F' | '}');
    if e == '}' {
        return char::from_u32(from_hex!(a b c d));
    }
    let f = next_ch!(chars @ '0'...'9' | 'a'...'f' | 'A'...'F' | '}');
    if f == '}' {
        return char::from_u32(from_hex!(a b c d e));
    }
    next_ch!(chars @ '}');
    char::from_u32(from_hex!(a b c d e f))
}

#[test]
fn test_cooked_string() {
    let input = "\\x62 \\\n \\u{7} \\u{64} \\u{bf5} \\u{12ba} \\u{1F395} \\u{102345}\"";
    let expected = "\x62 \u{7} \u{64} \u{bf5} \u{12ba} \u{1F395} \u{102345}";
    assert_eq!(cooked_string(input),
               IResult::Done("\"", expected.to_string()));
}

#[test]
fn test_cooked_byte_string() {
    let input = "\\x62 \\\n \\xEF\"";
    let expected = b"\x62 \xEF";
    assert_eq!(cooked_byte_string(input),
               IResult::Done("\"", expected.to_vec()));
}
