use std::borrow::Cow;
use std::char;
use std::str;
use std::string;

use self::Token::*;

#[derive(Eq, PartialEq, Debug)]
pub enum Token<'a> {
    Whitespace(&'a str),
    Newline,
    Comment(&'a str),

    Equals,
    Period,
    Comma,
    Colon,
    Plus,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,

    Keylike(&'a str),
    String { src: &'a str, val: Cow<'a, str> },
}

#[derive(Eq, PartialEq, Debug)]
pub enum Error {
    InvalidCharInString(usize, char),
    InvalidEscape(usize, char),
    InvalidHexEscape(usize, char),
    InvalidEscapeValue(usize, u32),
    NewlineInString(usize),
    Unexpected(usize, char),
    UnterminatedString(usize),
    NewlineInTableKey(usize),
    EmptyTableKey(usize),
    Wanted { at: usize, expected: &'static str, found: &'static str },
}

#[derive(Clone)]
pub struct Tokenizer<'a> {
    input: &'a str,
    chars: CrlfFold<'a>,
}

#[derive(Clone)]
struct CrlfFold<'a> {
    chars: str::CharIndices<'a>,
}

#[derive(Debug)]
enum MaybeString {
    NotEscaped(usize),
    Owned(string::String),
}

impl<'a> Tokenizer<'a> {
    pub fn new(input: &'a str) -> Tokenizer<'a> {
        let mut t = Tokenizer {
            input: input,
            chars: CrlfFold {
                chars: input.char_indices(),
            },
        };
        // Eat utf-8 BOM
        t.eatc('\u{feff}');
        t
    }

    pub fn next(&mut self) -> Result<Option<Token<'a>>, Error> {
        let token = match self.chars.next() {
            Some((_, '\n')) => Newline,
            Some((start, ' ')) => self.whitespace_token(start),
            Some((start, '\t')) => self.whitespace_token(start),
            Some((start, '#')) => self.comment_token(start),
            Some((_, '=')) => Equals,
            Some((_, '.')) => Period,
            Some((_, ',')) => Comma,
            Some((_, ':')) => Colon,
            Some((_, '+')) => Plus,
            Some((_, '{')) => LeftBrace,
            Some((_, '}')) => RightBrace,
            Some((_, '[')) => LeftBracket,
            Some((_, ']')) => RightBracket,
            Some((start, '\'')) => return self.literal_string(start).map(Some),
            Some((start, '"')) => return self.basic_string(start).map(Some),
            Some((start, ch)) if is_keylike(ch) => self.keylike(start),

            Some((start, ch)) => return Err(Error::Unexpected(start, ch)),
            None => return Ok(None),
        };
        Ok(Some(token))
    }

    pub fn peek(&mut self) -> Result<Option<Token<'a>>, Error> {
        self.clone().next()
    }

    pub fn eat(&mut self, expected: Token<'a>) -> Result<bool, Error> {
        match self.peek()? {
            Some(ref found) if expected == *found => {}
            Some(_) => return Ok(false),
            None => return Ok(false),
        }
        drop(self.next());
        Ok(true)
    }

    pub fn expect(&mut self, expected: Token<'a>) -> Result<(), Error> {
        let current = self.current();
        match self.next()? {
            Some(found) => {
                if expected == found {
                    Ok(())
                } else {
                    Err(Error::Wanted {
                        at: current,
                        expected: expected.describe(),
                        found: found.describe(),
                    })
                }
            }
            None => {
                Err(Error::Wanted {
                    at: self.input.len(),
                    expected: expected.describe(),
                    found: "eof",
                })
            }
        }
    }

    pub fn table_key(&mut self) -> Result<Cow<'a, str>, Error> {
        let current = self.current();
        match self.next()? {
            Some(Token::Keylike(k)) => Ok(k.into()),
            Some(Token::String { src, val }) => {
                let offset = self.substr_offset(src);
                if val == "" {
                    return Err(Error::EmptyTableKey(offset))
                }
                match src.find('\n') {
                    None => Ok(val),
                    Some(i) => Err(Error::NewlineInTableKey(offset + i)),
                }
            }
            Some(other) => {
                Err(Error::Wanted {
                    at: current,
                    expected: "a table key",
                    found: other.describe(),
                })
            }
            None => {
                Err(Error::Wanted {
                    at: self.input.len(),
                    expected: "a table key",
                    found: "eof",
                })
            }
        }
    }

    pub fn eat_whitespace(&mut self) -> Result<(), Error> {
        while self.eatc(' ') || self.eatc('\t') {
            // ...
        }
        Ok(())
    }

    pub fn eat_comment(&mut self) -> Result<bool, Error> {
        if !self.eatc('#') {
            return Ok(false)
        }
        drop(self.comment_token(0));
        self.eat_newline_or_eof().map(|()| true)
    }

    pub fn eat_newline_or_eof(&mut self) -> Result<(), Error> {
        let current = self.current();
        match self.next()? {
            None |
            Some(Token::Newline) => Ok(()),
            Some(other) => {
                Err(Error::Wanted {
                    at: current,
                    expected: "newline",
                    found: other.describe(),
                })
            }
        }
    }

    pub fn skip_to_newline(&mut self) {
        loop {
            match self.chars.next() {
                Some((_, '\n')) |
                None => break,
                _ => {}
            }
        }
    }

    fn eatc(&mut self, ch: char) -> bool {
        match self.chars.clone().next() {
            Some((_, ch2)) if ch == ch2 => {
                self.chars.next();
                true
            }
            _ => false,
        }
    }

    pub fn current(&mut self) -> usize {
        self.chars.clone().next().map(|i| i.0).unwrap_or(self.input.len())
    }

    pub fn input(&self) -> &'a str {
        self.input
    }

    fn whitespace_token(&mut self, start: usize) -> Token<'a> {
        while self.eatc(' ') || self.eatc('\t') {
            // ...
        }
        Whitespace(&self.input[start..self.current()])
    }

    fn comment_token(&mut self, start: usize) -> Token<'a> {
        while let Some((_, ch)) = self.chars.clone().next() {
            if ch != '\t' && (ch < '\u{20}' || ch > '\u{10ffff}') {
                break
            }
            self.chars.next();
        }
        Comment(&self.input[start..self.current()])
    }

    fn read_string(&mut self,
                   delim: char,
                   start: usize,
                   new_ch: &mut FnMut(&mut Tokenizer, &mut MaybeString,
                                      bool, usize, char)
                                     -> Result<(), Error>)
                   -> Result<Token<'a>, Error> {
        let mut multiline = false;
        if self.eatc(delim) {
            if self.eatc(delim) {
                multiline = true;
            } else {
                return Ok(String {
                    src: &self.input[start..start+2],
                    val: Cow::Borrowed(""),
                })
            }
        }
        let mut val = MaybeString::NotEscaped(self.current());
        let mut n = 0;
        'outer: loop {
            n += 1;
            match self.chars.next() {
                Some((i, '\n')) => {
                    if multiline {
                        if self.input.as_bytes()[i] == b'\r' {
                            val.to_owned(&self.input[..i]);
                        }
                        if n == 1 {
                            val = MaybeString::NotEscaped(self.current());
                        } else {
                            val.push('\n');
                        }
                        continue
                    } else {
                        return Err(Error::NewlineInString(i))
                    }
                }
                Some((i, ch)) if ch == delim => {
                    if multiline {
                        for _ in 0..2 {
                            if !self.eatc(delim) {
                                val.push(delim);
                                continue 'outer
                            }
                        }
                    }
                    return Ok(String {
                        src: &self.input[start..self.current()],
                        val: val.into_cow(&self.input[..i]),
                    })
                }
                Some((i, c)) => try!(new_ch(self, &mut val, multiline, i, c)),
                None => return Err(Error::UnterminatedString(start))
            }
        }
    }

    fn literal_string(&mut self, start: usize) -> Result<Token<'a>, Error> {
        self.read_string('\'', start, &mut |_me, val, _multi, i, ch| {
            if ch == '\u{09}' || ('\u{20}' <= ch && ch <= '\u{10ffff}') {
                val.push(ch);
                Ok(())
            } else {
                Err(Error::InvalidCharInString(i, ch))
            }
        })
    }

    fn basic_string(&mut self, start: usize) -> Result<Token<'a>, Error> {
        self.read_string('"', start, &mut |me, val, multi, i, ch| {
            match ch {
                '\\' => {
                    val.to_owned(&me.input[..i]);
                    match me.chars.next() {
                        Some((_, '"')) => val.push('"'),
                        Some((_, '\\')) => val.push('\\'),
                        Some((_, 'b')) => val.push('\u{8}'),
                        Some((_, 'f')) => val.push('\u{c}'),
                        Some((_, 'n')) => val.push('\n'),
                        Some((_, 'r')) => val.push('\r'),
                        Some((_, 't')) => val.push('\t'),
                        Some((i, c @ 'u')) |
                        Some((i, c @ 'U')) => {
                            let len = if c == 'u' {4} else {8};
                            val.push(try!(me.hex(start, i, len)));
                        }
                        Some((_, '\n')) if multi => {
                            while let Some((_, ch)) = me.chars.clone().next() {
                                match ch {
                                    ' ' | '\t' | '\n' => {
                                        me.chars.next();
                                    }
                                    _ => break,
                                }
                            }
                        }
                        Some((i, c)) => return Err(Error::InvalidEscape(i, c)),
                        None => return Err(Error::UnterminatedString(start)),
                    }
                    Ok(())
                }
                ch if '\u{20}' <= ch && ch <= '\u{10ffff}' => {
                    val.push(ch);
                    Ok(())
                }
                _ => Err(Error::InvalidCharInString(i, ch))
            }
        })
    }

    fn hex(&mut self, start: usize, i: usize, len: usize) -> Result<char, Error> {
        let mut val = 0;
        for _ in 0..len {
            match self.chars.next() {
                Some((_, ch)) if '0' <= ch && ch <= '9' => {
                    val = val * 16 + (ch as u32 - '0' as u32);
                }
                Some((_, ch)) if 'A' <= ch && ch <= 'F' => {
                    val = val * 16 + (ch as u32 - 'A' as u32) + 10;
                }
                Some((i, ch)) => return Err(Error::InvalidHexEscape(i, ch)),
                None => return Err(Error::UnterminatedString(start)),
            }
        }
        match char::from_u32(val) {
            Some(ch) => Ok(ch),
            None => Err(Error::InvalidEscapeValue(i, val)),
        }
    }

    fn keylike(&mut self, start: usize) -> Token<'a> {
        while let Some((_, ch)) = self.chars.clone().next() {
            if !is_keylike(ch) {
                break
            }
            self.chars.next();
        }
        Keylike(&self.input[start..self.current()])
    }

    pub fn substr_offset(&self, s: &'a str) -> usize {
        assert!(s.len() <= self.input.len());
        let a = self.input.as_ptr() as usize;
        let b = s.as_ptr() as usize;
        assert!(a <= b);
        b - a
    }
}

impl<'a> Iterator for CrlfFold<'a> {
    type Item = (usize, char);

    fn next(&mut self) -> Option<(usize, char)> {
        self.chars.next().map(|(i, c)| {
            if c == '\r' {
                let mut attempt = self.chars.clone();
                if let Some((_, '\n')) = attempt.next() {
                    self.chars = attempt;
                    return (i, '\n')
                }
            }
            (i, c)
        })
    }
}

impl MaybeString {
    fn push(&mut self, ch: char) {
        match *self {
            MaybeString::NotEscaped(..) => {}
            MaybeString::Owned(ref mut s) => s.push(ch),
        }
    }

    fn to_owned(&mut self, input: &str) {
        match *self {
            MaybeString::NotEscaped(start) => {
                *self = MaybeString::Owned(input[start..].to_owned());
            }
            MaybeString::Owned(..) => {}
        }
    }

    fn into_cow(self, input: &str) -> Cow<str> {
        match self {
            MaybeString::NotEscaped(start) => Cow::Borrowed(&input[start..]),
            MaybeString::Owned(s) => Cow::Owned(s),
        }
    }
}

fn is_keylike(ch: char) -> bool {
    ('A' <= ch && ch <= 'Z') ||
        ('a' <= ch && ch <= 'z') ||
        ('0' <= ch && ch <= '9') ||
        ch == '-' ||
        ch == '_'
}

impl<'a> Token<'a> {
    pub fn describe(&self) -> &'static str {
        match *self {
            Token::Keylike(_) => "an identifier",
            Token::Equals => "an equals",
            Token::Period => "a period",
            Token::Comment(_) => "a comment",
            Token::Newline => "a newline",
            Token::Whitespace(_) => "whitespace",
            Token::Comma => "a comma",
            Token::RightBrace => "a right brace",
            Token::LeftBrace => "a left brace",
            Token::RightBracket => "a right bracket",
            Token::LeftBracket => "a left bracket",
            Token::String { .. } => "a string",
            Token::Colon => "a colon",
            Token::Plus => "a plus",
        }
    }
}

#[cfg(test)]
mod tests {
    use std::borrow::Cow;
    use super::{Tokenizer, Token, Error};

    fn err(input: &str, err: Error) {
        let mut t = Tokenizer::new(input);
        let token = t.next().unwrap_err();
        assert_eq!(token, err);
        assert!(t.next().unwrap().is_none());
    }

    #[test]
    fn literal_strings() {
        fn t(input: &str, val: &str) {
            let mut t = Tokenizer::new(input);
            let token = t.next().unwrap().unwrap();
            assert_eq!(token, Token::String {
                src: input,
                val: Cow::Borrowed(val),
            });
            assert!(t.next().unwrap().is_none());
        }

        t("''", "");
        t("''''''", "");
        t("'''\n'''", "");
        t("'a'", "a");
        t("'\"a'", "\"a");
        t("''''a'''", "'a");
        t("'''\n'a\n'''", "'a\n");
        t("'''a\n'a\r\n'''", "a\n'a\n");
    }

    #[test]
    fn basic_strings() {
        fn t(input: &str, val: &str) {
            let mut t = Tokenizer::new(input);
            let token = t.next().unwrap().unwrap();
            assert_eq!(token, Token::String {
                src: input,
                val: Cow::Borrowed(val),
            });
            assert!(t.next().unwrap().is_none());
        }

        t(r#""""#, "");
        t(r#""""""""#, "");
        t(r#""a""#, "a");
        t(r#""""a""""#, "a");
        t(r#""\t""#, "\t");
        t(r#""\u0000""#, "\0");
        t(r#""\U00000000""#, "\0");
        t(r#""\U000A0000""#, "\u{A0000}");
        t(r#""\\t""#, "\\t");
        t("\"\"\"\\\n\"\"\"", "");
        t("\"\"\"\\\n     \t   \t  \\\r\n  \t \n  \t \r\n\"\"\"", "");
        t(r#""\r""#, "\r");
        t(r#""\n""#, "\n");
        t(r#""\b""#, "\u{8}");
        t(r#""a\fa""#, "a\u{c}a");
        t(r#""\"a""#, "\"a");
        t("\"\"\"\na\"\"\"", "a");
        t("\"\"\"\n\"\"\"", "");
        err(r#""\a"#, Error::InvalidEscape(2, 'a'));
        err("\"\\\n", Error::InvalidEscape(2, '\n'));
        err("\"\\\r\n", Error::InvalidEscape(2, '\n'));
        err("\"\\", Error::UnterminatedString(0));
        err("\"\u{0}", Error::InvalidCharInString(1, '\u{0}'));
        err(r#""\U00""#, Error::InvalidHexEscape(5, '"'));
        err(r#""\U00"#, Error::UnterminatedString(0));
        err(r#""\uD800"#, Error::InvalidEscapeValue(2, 0xd800));
        err(r#""\UFFFFFFFF"#, Error::InvalidEscapeValue(2, 0xffffffff));
    }

    #[test]
    fn keylike() {
        fn t(input: &str) {
            let mut t = Tokenizer::new(input);
            let token = t.next().unwrap().unwrap();
            assert_eq!(token, Token::Keylike(input));
            assert!(t.next().unwrap().is_none());
        }
        t("foo");
        t("0bar");
        t("bar0");
        t("1234");
        t("a-b");
        t("a_B");
        t("-_-");
        t("___");
    }

    #[test]
    fn all() {
        fn t(input: &str, expected: &[Token]) {
            let mut tokens = Tokenizer::new(input);
            let mut actual = Vec::new();
            while let Some(token) = tokens.next().unwrap() {
                actual.push(token);
            }
            for (a, b) in actual.iter().zip(expected) {
                assert_eq!(a, b);
            }
            assert_eq!(actual.len(), expected.len());
        }

        t(" a ", &[
            Token::Whitespace(" "),
            Token::Keylike("a"),
            Token::Whitespace(" "),
        ]);

        t(" a\t [[]] \t [] {} , . =\n# foo \r\n#foo \n ", &[
            Token::Whitespace(" "),
            Token::Keylike("a"),
            Token::Whitespace("\t "),
            Token::LeftBracket,
            Token::LeftBracket,
            Token::RightBracket,
            Token::RightBracket,
            Token::Whitespace(" \t "),
            Token::LeftBracket,
            Token::RightBracket,
            Token::Whitespace(" "),
            Token::LeftBrace,
            Token::RightBrace,
            Token::Whitespace(" "),
            Token::Comma,
            Token::Whitespace(" "),
            Token::Period,
            Token::Whitespace(" "),
            Token::Equals,
            Token::Newline,
            Token::Comment("# foo "),
            Token::Newline,
            Token::Comment("#foo "),
            Token::Newline,
            Token::Whitespace(" "),
        ]);
    }

    #[test]
    fn bare_cr_bad() {
        err("\r", Error::Unexpected(0, '\r'));
        err("'\n", Error::NewlineInString(1));
        err("'\u{0}", Error::InvalidCharInString(1, '\u{0}'));
        err("'", Error::UnterminatedString(0));
        err("\u{0}", Error::Unexpected(0, '\u{0}'));
    }

    #[test]
    fn bad_comment() {
        let mut t = Tokenizer::new("#\u{0}");
        t.next().unwrap().unwrap();
        assert_eq!(t.next(), Err(Error::Unexpected(1, '\u{0}')));
        assert!(t.next().unwrap().is_none());
    }
}
