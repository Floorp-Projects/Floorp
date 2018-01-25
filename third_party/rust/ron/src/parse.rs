use std::fmt::{Display, Formatter, Result as FmtResult};
use std::ops::Neg;
use std::result::Result as StdResult;
use std::str::{FromStr, from_utf8, from_utf8_unchecked};

use de::{Error, ParseError, Result};

const DIGITS: &[u8] = b"0123456789ABCDEFabcdef";
const FLOAT_CHARS: &[u8] = b"0123456789.+-eE";
const IDENT_FIRST: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
const IDENT_CHAR: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789";
const WHITE_SPACE: &[u8] = b"\n\t\r ";

#[derive(Clone, Copy, Debug)]
pub struct Bytes<'a> {
    bytes: &'a [u8],
    column: usize,
    line: usize,
}

impl<'a> Bytes<'a> {
    pub fn new(bytes: &'a [u8]) -> Self {
        let mut b = Bytes {
            bytes,
            column: 1,
            line: 1,
        };

        b.skip_ws();

        b
    }

    pub fn advance(&mut self, bytes: usize) -> Result<()> {
        for _ in 0..bytes {
            self.advance_single()?;
        }

        Ok(())
    }

    pub fn advance_single(&mut self) -> Result<()> {
        if self.peek_or_eof()? == b'\n' {
            self.line += 1;
            self.column = 1;
        } else {
            self.column += 1;
        }

        self.bytes = &self.bytes[1..];

        Ok(())
    }

    pub fn bool(&mut self) -> Result<bool> {
        if self.consume("true") {
            Ok(true)
        } else if self.consume("false") {
            Ok(false)
        } else {
            self.err(ParseError::ExpectedBoolean)
        }
    }

    pub fn bytes(&self) -> &[u8] {
        &self.bytes
    }

    pub fn char(&mut self) -> Result<char> {
        if !self.consume("'") {
            return self.err(ParseError::ExpectedChar);
        }

        let c = self.eat_byte()?;

        let c = if c == b'\\' {
            let c = self.eat_byte()?;

            if c != b'\\' && c != b'\'' {
                return self.err(ParseError::InvalidEscape);
            }

            c
        } else {
            c
        };

        if !self.consume("'") {
            return self.err(ParseError::ExpectedChar);
        }

        Ok(c as char)
    }

    pub fn comma(&mut self) -> bool {
        self.skip_ws();

        if self.consume(",") {
            self.skip_ws();

            true
        } else {
            false
        }
    }

    /// Only returns true if the char after `ident` cannot belong
    /// to an identifier.
    pub fn check_ident(&mut self, ident: &str) -> bool {
        self.test_for(ident) && !self.check_ident_char(ident.len())
    }

    fn check_ident_char(&self, index: usize) -> bool {
        self.bytes.get(index).map(|b| IDENT_CHAR.contains(b)).unwrap_or(false)
    }

    /// Only returns true if the char after `ident` cannot belong
    /// to an identifier.
    pub fn consume_ident(&mut self, ident: &str) -> bool {
        if self.check_ident(ident) {
            let _ = self.advance(ident.len());

            true
        } else {
            false
        }
    }

    pub fn consume(&mut self, s: &str) -> bool {
        if self.test_for(s) {
            let _ = self.advance(s.len());

            true
        } else {
            false
        }
    }

    pub fn eat_byte(&mut self) -> Result<u8> {
        let peek = self.peek_or_eof()?;
        let _ = self.advance_single();

        Ok(peek)
    }

    pub fn err<T>(&self, kind: ParseError) -> Result<T> {
        Err(self.error(kind))
    }

    pub fn error(&self, kind: ParseError) -> Error {
        Error::Parser(kind, Position { line: self.line, col: self.column })
    }

    pub fn float<T>(&mut self) -> Result<T>
        where T: FromStr
    {
        let num_bytes = self.next_bytes_contained_in(FLOAT_CHARS);

        let s = unsafe { from_utf8_unchecked(&self.bytes[0..num_bytes]) };
        let res = FromStr::from_str(s).map_err(|_| self.error(ParseError::ExpectedFloat));

        let _ = self.advance(num_bytes);

        res
    }

    pub fn identifier(&mut self) -> Result<&[u8]> {
        if IDENT_FIRST.contains(&self.peek_or_eof()?) {
            let bytes = self.next_bytes_contained_in(IDENT_CHAR);

            let ident = &self.bytes[..bytes];
            let _ = self.advance(bytes);

            Ok(ident)
        } else {
            self.err(ParseError::ExpectedIdentifier)
        }
    }

    pub fn next_bytes_contained_in(&self, allowed: &[u8]) -> usize {
        self.bytes
            .iter()
            .take_while(|b| allowed.contains(b))
            .fold(0, |acc, _| acc + 1)
    }

    pub fn skip_ws(&mut self) {
        while self.peek().map(|c| WHITE_SPACE.contains(&c)).unwrap_or(false) {
            let _ = self.advance_single();
        }

        if self.skip_comment() {
            self.skip_ws();
        }
    }

    pub fn peek(&self) -> Option<u8> {
        self.bytes.get(0).map(|b| *b)
    }

    pub fn peek_or_eof(&self) -> Result<u8> {
        self.bytes.get(0).map(|b| *b).ok_or(self.error(ParseError::Eof))
    }

    pub fn signed_integer<T>(&mut self) -> Result<T>
        where T: Neg<Output=T> + Num,
    {
        match self.peek_or_eof()? {
            b'+' => {
                let _ = self.advance_single();

                self.unsigned_integer()
            }
            b'-' => {
                let _ = self.advance_single();

                self.unsigned_integer::<T>().map(Neg::neg)
            }
            _ => self.unsigned_integer(),
        }
    }

    pub fn string(&mut self) -> Result<ParsedStr> {
        if !self.consume("\"") {
            return self.err(ParseError::ExpectedString);
        }

        let (i, end_or_escape) = self.bytes
            .iter()
            .enumerate()
            .find(|&(_, &b)| b == b'\\' || b == b'"')
            .ok_or(self.error(ParseError::ExpectedStringEnd))?;

        if *end_or_escape == b'"' {
            let s = from_utf8(&self.bytes[..i]).map_err(|e| self.error(e.into()))?;

            // Advance by the number of bytes of the string
            // + 1 for the `"`.
            let _ = self.advance(i + 1);

            Ok(ParsedStr::Slice(s))
        } else {
            let mut i = i;
            let mut s: Vec<_> = self.bytes[..i].to_vec();

            loop {
                let _ = self.advance(i + 1);
                self.parse_str_escape(&mut s)?;

                let (new_i, end_or_escape) = self.bytes
                    .iter()
                    .enumerate()
                    .find(|&(_, &b)| b == b'\\' || b == b'"')
                    .ok_or(ParseError::Eof)
                    .map_err(|e| self.error(e))?;

                i = new_i;
                s.extend_from_slice(&self.bytes[..i]);

                if *end_or_escape == b'"' {
                    let _ = self.advance(i + 1);

                    break Ok(ParsedStr::Allocated(String::from_utf8(s)
                        .map_err(|e| self.error(e.into()))?));
                }
            }
        }
    }

    fn test_for(&self, s: &str) -> bool {
        s.bytes().enumerate().all(|(i, b)| self.bytes.get(i).map(|t| *t == b).unwrap_or(false))
    }

    pub fn unsigned_integer<T: Num>(&mut self) -> Result<T> {
        let base = if self.peek() == Some(b'0') {
            match self.bytes.get(1).cloned() {
                Some(b'x') => 16,
                Some(b'b') => 2,
                Some(b'o') => 8,
                _ => 10,
            }
        } else {
            10
        };

        if base != 10 {
            // If we have `0x45A` for example,
            // cut it to `45A`.
            let _ = self.advance(2);
        }

        let num_bytes = self.next_bytes_contained_in(DIGITS);

        if num_bytes == 0 {
            return self.err(ParseError::Eof);
        }

        let res = Num::from_str(unsafe { from_utf8_unchecked(&self.bytes[0..num_bytes]) }, base)
            .map_err(|_| self.error(ParseError::ExpectedInteger));

        let _ = self.advance(num_bytes);

        res
    }

    fn decode_hex_escape(&mut self) -> Result<u16> {
        let mut n = 0;
        for _ in 0..4 {
            n = match self.eat_byte()? {
                c @ b'0' ... b'9' => n * 16_u16 + ((c as u16) - (b'0' as u16)),
                b'a' | b'A' => n * 16_u16 + 10_u16,
                b'b' | b'B' => n * 16_u16 + 11_u16,
                b'c' | b'C' => n * 16_u16 + 12_u16,
                b'd' | b'D' => n * 16_u16 + 13_u16,
                b'e' | b'E' => n * 16_u16 + 14_u16,
                b'f' | b'F' => n * 16_u16 + 15_u16,
                _ => {
                    return self.err(ParseError::InvalidEscape);
                }
            };
        }

        Ok(n)
    }

    fn parse_str_escape(&mut self, store: &mut Vec<u8>) -> Result<()> {
        use std::iter::repeat;

        match self.eat_byte()? {
            b'"' => store.push(b'"'),
            b'\\' => store.push(b'\\'),
            b'b' => store.push(b'\x08'),
            b'f' => store.push(b'\x0c'),
            b'n' => store.push(b'\n'),
            b'r' => store.push(b'\r'),
            b't' => store.push(b'\t'),
            b'u' => {
                let c: char = match self.decode_hex_escape()? {
                    0xDC00 ... 0xDFFF => {
                        return self.err(ParseError::InvalidEscape);
                    }

                    n1 @ 0xD800 ... 0xDBFF => {
                        if self.eat_byte()? != b'\\' {
                            return self.err(ParseError::InvalidEscape);
                        }

                        if self.eat_byte()? != b'u' {
                            return self.err(ParseError::InvalidEscape);
                        }

                        let n2 = self.decode_hex_escape()?;

                        if n2 < 0xDC00 || n2 > 0xDFFF {
                            return self.err(ParseError::InvalidEscape);
                        }

                        let n = (((n1 - 0xD800) as u32) << 10 | (n2 - 0xDC00) as u32) + 0x1_0000;

                        match ::std::char::from_u32(n as u32) {
                            Some(c) => c,
                            None => {
                                return self.err(ParseError::InvalidEscape);
                            }
                        }
                    }

                    n => {
                        match ::std::char::from_u32(n as u32) {
                            Some(c) => c,
                            None => {
                                return self.err(ParseError::InvalidEscape);
                            }
                        }
                    }
                };

                let char_start = store.len();
                store.extend(repeat(0).take(c.len_utf8()));
                c.encode_utf8(&mut store[char_start..]);
            }
            _ => {
                return self.err(ParseError::InvalidEscape);
            }
        }

        Ok(())
    }

    fn skip_comment(&mut self) -> bool {
        if self.consume("//") {
            let bytes = self.bytes.iter().take_while(|&&b| b != b'\n').count();

            let _ = self.advance(bytes);

            true
        } else {
            false
        }
    }
}

pub trait Num: Sized {
    fn from_str(src: &str, radix: u32) -> StdResult<Self, ()>;
}

macro_rules! impl_num {
    ($ty:ident) => {
        impl Num for $ty {
            fn from_str(src: &str, radix: u32) -> StdResult<Self, ()> {
                $ty::from_str_radix(src, radix).map_err(|_| ())
            }
        }
    };
    ($($tys:ident)*) => {
        $( impl_num!($tys); )*
    };
}

impl_num!(u8 u16 u32 u64 i8 i16 i32 i64);

#[derive(Clone, Debug)]
pub enum ParsedStr<'a> {
    Allocated(String),
    Slice(&'a str),
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Position {
    pub col: usize,
    pub line: usize,
}

impl Display for Position {
    fn fmt(&self, f: &mut Formatter) -> FmtResult {
        write!(f, "{}:{}", self.line, self.col)
    }
}
