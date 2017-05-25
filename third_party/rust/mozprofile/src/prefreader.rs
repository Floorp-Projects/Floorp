use preferences::{Pref, PrefValue, Preferences};
use std::borrow::Borrow;
use std::borrow::Cow;
use std::char;
use std::error::Error;
use std::fmt;
use std::io::{self, Write};
use std::iter::Iterator;
use std::str;
use std::mem;
use std::ops::Deref;

impl PrefReaderError {
    fn new(message: &'static str, position: Position, parent: Option<Box<Error>>) -> PrefReaderError {
        PrefReaderError {
            message: message,
            position: position,
            parent: parent
        }
    }
}


impl fmt::Display for PrefReaderError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{} at line {}, column {}",
               self.message, self.position.line, self.position.column)
    }
}


impl Error for PrefReaderError {
    fn description(&self) -> &str {
        self.message
    }

    fn cause(&self) -> Option<&Error> {
        match self.parent {
            None => None,
            Some(ref cause) => Some(cause.deref())
        }
    }
}

impl From<io::Error> for PrefReaderError {
    fn from(err: io::Error) -> PrefReaderError {
        PrefReaderError::new("IOError",
                             Position::new(),
                             Some(err.into()))
    }
}

#[derive(Copy, Clone, Debug, PartialEq)]
enum TokenizerState {
    Junk,
    CommentStart,
    CommentLine,
    CommentBlock,
    FunctionName,
    AfterFunctionName,
    FunctionArgs,
    FunctionArg,
    DoubleQuotedString,
    SingleQuotedString,
    Number,
    Bool,
    AfterFunctionArg,
    AfterFunction,
    Error,
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub struct Position {
    line: u32,
    column: u32
}

impl Position {
    pub fn new() -> Position {
        Position {
            line: 1,
            column: 0
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum TokenType {
    None,
    PrefFunction,
    UserPrefFunction,
    StickyPrefFunction,
    CommentBlock,
    CommentLine,
    CommentBashLine,
    Paren,
    Semicolon,
    Comma,
    String,
    Int,
    Bool,
    Error
}

#[derive(Debug, PartialEq)]
pub enum PrefToken<'a> {
    PrefFunction(Position),
    UserPrefFunction(Position),
    StickyPrefFunction(Position),
    CommentBlock(Cow<'a, str>, Position),
    CommentLine(Cow<'a, str>, Position),
    CommentBashLine(Cow<'a, str>, Position),
    Paren(char, Position),
    Semicolon(Position),
    Comma(Position),
    String(Cow<'a, str>, Position),
    Int(i64, Position),
    Bool(bool, Position),
    Error(&'static str, Position)
}

impl<'a> PrefToken<'a> {
    fn position(&self) -> Position {
        match *self {
            PrefToken::PrefFunction(position) => position,
            PrefToken::UserPrefFunction(position) => position,
            PrefToken::StickyPrefFunction(position) => position,
            PrefToken::CommentBlock(_, position) => position,
            PrefToken::CommentLine(_, position) => position,
            PrefToken::CommentBashLine(_, position) => position,
            PrefToken::Paren(_, position) => position,
            PrefToken::Semicolon(position) => position,
            PrefToken::Comma(position) => position,
            PrefToken::String(_, position) => position,
            PrefToken::Int(_, position) => position,
            PrefToken::Bool(_, position) => position,
            PrefToken::Error(_, position) => position
        }
    }
}

#[derive(Debug)]
pub struct PrefReaderError {
    message: &'static str,
    position: Position,
    parent: Option<Box<Error>>
}

struct TokenData<'a> {
    token_type: TokenType,
    complete: bool,
    position: Position,
    data: Cow<'a, str>,
    start_pos: usize,
}

impl<'a> TokenData<'a> {
    fn new(token_type: TokenType, position: Position, start_pos: usize) -> TokenData<'a> {
        TokenData {
            token_type: token_type,
            complete: false,
            position: position,
            data: Cow::Borrowed(""),
            start_pos: start_pos,
        }
    }

    fn start(&mut self, tokenizer: &PrefTokenizer, token_type: TokenType) {
        self.token_type = token_type;
        self.position = tokenizer.position;
        self.start_pos = tokenizer.pos;
    }

    fn end(&mut self, buf: &'a [u8], end_pos: usize) -> Result<(), PrefReaderError> {
        self.complete = true;
        self.add_slice_to_token(buf, end_pos)
    }

    fn add_slice_to_token(&mut self, buf: &'a [u8], end_pos: usize) -> Result<(), PrefReaderError> {
        let data = match str::from_utf8(&buf[self.start_pos..end_pos]) {
            Ok(x) => x,
            Err(_) => {
                return Err(PrefReaderError::new("Could not convert string to utf8",
                                               self.position, None));
            }
        };
        if self.data != "" {
            self.data.to_mut().push_str(&data)
        } else {
            self.data = Cow::Borrowed(&data)
        };
        Ok(())
    }

    fn push_char(&mut self, tokenizer: &PrefTokenizer, data: char) {
        self.data.to_mut().push(data);
        self.start_pos = tokenizer.pos + 1;
    }
}

pub struct PrefTokenizer<'a> {
    data: &'a [u8],
    pos: usize,
    cur: Option<char>,
    position: Position,
    state: TokenizerState,
    next_state: Option<TokenizerState>,
}

impl<'a> PrefTokenizer<'a> {
    pub fn new(data: &'a [u8]) -> PrefTokenizer<'a> {
        PrefTokenizer {
            data: data,
            pos: 0,
            cur: None,
            position: Position::new(),
            state: TokenizerState::Junk,
            next_state: Some(TokenizerState::FunctionName),
        }
    }

    fn make_token(&mut self, token_data: TokenData<'a>) -> PrefToken<'a> {
        let buf = token_data.data;
        let position = token_data.position;
        match token_data.token_type {
            TokenType::None => panic!("Got a token without a type"),
            TokenType::PrefFunction => PrefToken::PrefFunction(position),
            TokenType::UserPrefFunction => PrefToken::UserPrefFunction(position),
            TokenType::StickyPrefFunction => PrefToken::StickyPrefFunction(position),
            TokenType::CommentBlock => PrefToken::CommentBlock(buf, position),
            TokenType::CommentLine => PrefToken::CommentLine(buf, position),
            TokenType::CommentBashLine => PrefToken::CommentBashLine(buf, position),
            TokenType::Paren => {
                if buf.len() != 1 {
                    panic!("Expected a buffer of length one");
                }
                PrefToken::Paren(buf.chars().next().unwrap(), position)
            },
            TokenType::Semicolon => PrefToken::Semicolon(position),
            TokenType::Comma => PrefToken::Comma(position),
            TokenType::String => PrefToken::String(buf, position),
            TokenType::Int => {
                let value = i64::from_str_radix(buf.borrow(), 10)
                    .expect("Integer wasn't parsed as an i64");
                PrefToken::Int(value, position)
            },
            TokenType::Bool => {
                let value = match buf.borrow() {
                    "true" => true,
                    "false" => false,
                    x => panic!(format!("Boolean wasn't 'true' or 'false' (was {})", x))
                };
                PrefToken::Bool(value, position)
            },
            TokenType::Error => panic!("make_token can't construct errors")
        }
    }

    fn get_char(&mut self) -> Option<char> {
        if self.pos >= self.data.len() - 1 {
            self.cur = None;
            return None
        };
        if self.cur.is_some() {
            self.pos += 1;
        }
        let c = self.data[self.pos] as char;
        if self.cur == Some('\n') {
            self.position.line += 1;
            self.position.column = 0;
        } else if self.cur.is_some() {
            self.position.column += 1;
        };
        self.cur = Some(c);
        self.cur
    }

    fn unget_char(&mut self) -> Option<char> {
        if self.pos == 0 {
            self.position.column = 0;
            self.cur = None
        } else {
            self.pos -= 1;
            let c = self.data[self.pos] as char;
            if c == '\n' {
                self.position.line -= 1;
                let mut col_pos = self.pos - 1;
                while col_pos > 0 && self.data[col_pos] as char != '\n' {
                    col_pos -= 1;
                }
                self.position.column = (self.pos - col_pos as usize - 1) as u32;
            } else {
                self.position.column -= 1;
            }
            self.cur = Some(c);
        }
        self.cur
    }

    fn is_space(c: char) -> bool {
        match c {
            ' ' | '\t' | '\r' | '\n' => true,
            _ => false
        }
    }

    fn skip_whitespace(&mut self) -> Option<char> {
        while let Some(c) = self.cur {
            if PrefTokenizer::is_space(c) {
                self.get_char();
            } else {
                break
            };
        }
        self.cur
    }

    fn consume_escape(&mut self, token_data: &mut TokenData<'a>) -> Result<(), PrefReaderError> {
        let pos = self.pos;
        let escaped = try!(self.read_escape());
        if let Some(escape_char) = escaped {
            try!(token_data.add_slice_to_token(&self.data, pos));
            token_data.push_char(&self, escape_char);
        };
        Ok(())
    }

    fn read_escape(&mut self) -> Result<Option<char>, PrefReaderError> {
        let escape_char = match self.get_char() {
            Some('u') => try!(self.read_hex_escape(4, true)),
            Some('x') => try!(self.read_hex_escape(2, true)),
            Some('\\') => '\\' as u32,
            Some('"') => '"' as u32,
            Some('\'') => '\'' as u32,
            Some('r') => '\r' as u32,
            Some('n') => '\n' as u32,
            Some(_) => return Ok(None),
            None => return Err(PrefReaderError::new("EOF in character escape",
                                                   self.position, None))
        };
        Ok(Some(try!(char::from_u32(escape_char)
                     .ok_or(PrefReaderError::new("Invalid codepoint decoded from escape",
                                                 self.position, None)))))
    }

    fn read_hex_escape(&mut self, hex_chars: isize, first: bool) -> Result<u32, PrefReaderError> {
        let mut value = 0;
        for _ in 0..hex_chars {
            match self.get_char() {
                Some(x) => {
                    value = value << 4;
                    match x {
                        '0'...'9' => value += x as u32 - '0' as u32,
                        'a'...'f' => value += x as u32 - 'a' as u32,
                        'A'...'F' => value += x as u32 - 'A' as u32,
                        _ => return Err(PrefReaderError::new(
                            "Unexpected character in escape", self.position, None))
                    }
                },
                None => return Err(PrefReaderError::new(
                    "Unexpected EOF in escape", self.position, None))
            }
        }
        if first && value >= 0xD800 && value <= 0xDBFF {
            // First part of a surrogate pair
            if self.get_char() != Some('\\') || self.get_char() != Some('u') {
                return Err(PrefReaderError::new("Lone high surrogate in surrogate pair",
                                               self.position, None))
            }
            self.unget_char();
            let high_surrogate = value;
            let low_surrogate = try!(self.read_hex_escape(4, false));
            let high_value = (high_surrogate - 0xD800) << 10;
            let low_value = low_surrogate - 0xDC00;
            value = high_value + low_value + 0x10000;
        } else if first && value >= 0xDC00 && value <= 0xDFFF {
            return Err(PrefReaderError::new("Lone low surrogate",
                                            self.position,
                                            None))
        } else if !first && (value < 0xDC00 || value > 0xDFFF) {
            return Err(PrefReaderError::new("Invalid low surrogate in surrogate pair",
                                            self.position,
                                            None));
        }
        Ok(value)
    }

    fn get_match(&mut self, target: &str, separators: &str) -> bool {
        let initial_pos = self.pos;
        let mut matched = true;
        for c in target.chars() {
            if self.cur == Some(c) {
                self.get_char();
            } else {
                matched = false;
                break;
            }
        }

        if !matched {
            for _ in 0..(self.pos - initial_pos) {
                self.unget_char();
            }
        } else {
            // Check that the next character is whitespace or a separator
            if let Some(c) = self.cur {
                if !(PrefTokenizer::is_space(c) || separators.contains(c) || c == '/') {
                    matched = false;
                }
            }
            self.unget_char();
        }

        matched
    }

    fn next_token(&mut self) -> Result<Option<TokenData<'a>>, PrefReaderError> {
        let mut token_data = TokenData::new(TokenType::None, Position::new(), 0);

        loop {
            let mut c = match self.get_char() {
                Some(x) => x,
                None => return Ok(None)
            };

            self.state = match self.state {
                TokenizerState::Junk => {
                    c = match self.skip_whitespace() {
                        Some(x) => x,
                        None => return Ok(None)
                    };
                    match c {
                        '/' => TokenizerState::CommentStart,
                        '#' => {
                            token_data.start(&self, TokenType::CommentBashLine);
                            token_data.start_pos = self.pos + 1;
                            TokenizerState::CommentLine
                        },
                        _ => {
                            self.unget_char();
                            let next = match self.next_state {
                                Some(x) => x,
                                None => {
                                    return Err(PrefReaderError::new(
                                        "In Junk state without a next state defined",
                                        self.position,
                                        None))
                                }
                            };
                            self.next_state = None;
                            next
                        }
                    }
                },
                TokenizerState::CommentStart => {
                    match c {
                        '*' => {
                            token_data.start(&self, TokenType::CommentBlock);
                            token_data.start_pos = self.pos + 1;
                            TokenizerState::CommentBlock
                        },
                        '/' => {
                            token_data.start(&self, TokenType::CommentLine);
                            token_data.start_pos = self.pos + 1;
                            TokenizerState::CommentLine
                        },
                        _ => {
                            return Err(PrefReaderError::new(
                                "Invalid character after /", self.position, None))
                        }
                    }

                },
                TokenizerState::CommentLine => {
                    match c {
                        '\n' => {
                            try!(token_data.end(&self.data, self.pos));
                            TokenizerState::Junk
                        },
                        _ => {
                            TokenizerState::CommentLine
                        }
                    }
                },
                TokenizerState::CommentBlock => {
                    match c {
                        '*' => {
                            if self.get_char() == Some('/') {
                                try!(token_data.end(&self.data, self.pos - 1));
                                TokenizerState::Junk
                            } else {
                                TokenizerState::CommentBlock
                            }
                        },
                        _ => TokenizerState::CommentBlock
                    }
                },
                TokenizerState::FunctionName => {
                    let position = self.position;
                    let start_pos = self.pos;
                    match c {
                        'u' => {
                            if self.get_match("user_pref", "(") {
                                token_data.start(&self, TokenType::UserPrefFunction);
                            }
                        },
                        's' => {
                            if self.get_match("sticky_pref", "(") {
                                token_data.start(&self, TokenType::StickyPrefFunction);
                            }
                        }
                        'p' => {
                            if self.get_match("pref", "(") {
                                token_data.start(&self, TokenType::PrefFunction);
                            }
                        },
                        _ => {}
                    };
                    if token_data.token_type == TokenType::None {
                        // We didn't match anything
                        return Err(PrefReaderError::new(
                            "Expected a pref function name", position, None))
                    } else {
                        token_data.start_pos = start_pos;
                        token_data.position = position;
                        try!(token_data.end(&self.data, self.pos + 1));
                        self.next_state = Some(TokenizerState::AfterFunctionName);
                        TokenizerState::Junk
                    }
                },
                TokenizerState::AfterFunctionName => {
                    match c {
                        '(' => {
                            self.next_state = Some(TokenizerState::FunctionArgs);
                            token_data.start(&self, TokenType::Paren);
                            try!(token_data.end(&self.data, self.pos + 1));
                            self.next_state = Some(TokenizerState::FunctionArgs);
                            TokenizerState::Junk
                        },
                        _ => {
                            return Err(PrefReaderError::new(
                                "Expected an opening paren", self.position, None))
                        }
                    }
                },
                TokenizerState::FunctionArgs => {
                    match c {
                        ')' => {
                            token_data.start(&self, TokenType::Paren);
                            try!(token_data.end(&self.data, self.pos + 1));
                            self.next_state = Some(TokenizerState::AfterFunction);
                            TokenizerState::Junk
                        },
                        _ => {
                            self.unget_char();
                            TokenizerState::FunctionArg
                        }
                    }
                },
                TokenizerState::FunctionArg => {
                    match c {
                        '"' => {
                            token_data.start(&self, TokenType::String);
                            token_data.start_pos = self.pos + 1;
                            TokenizerState::DoubleQuotedString
                        },
                        '\'' => {
                            token_data.start(&self, TokenType::String);
                            token_data.start_pos = self.pos + 1;
                            TokenizerState::SingleQuotedString
                        },
                        't' | 'f' => {
                            self.unget_char();
                            TokenizerState::Bool
                        },
                        '0'...'9' | '-' |'+' => {
                            token_data.start(&self, TokenType::Int);
                            TokenizerState::Number
                        },
                        _ => {
                            return Err(PrefReaderError::new(
                                "Invalid character at start of function argument",
                                self.position, None))
                        }
                    }
                },
                TokenizerState::DoubleQuotedString => {
                    match c {
                        '"' => {
                            try!(token_data.end(&self.data, self.pos));
                            self.next_state = Some(TokenizerState::AfterFunctionArg);
                            TokenizerState::Junk

                        },
                        '\n' => {
                            return Err(PrefReaderError::new(
                                "EOL in double quoted string", self.position, None))
                        },
                        '\\' => {
                            try!(self.consume_escape(&mut token_data));
                            TokenizerState::DoubleQuotedString
                        },
                        _ => TokenizerState::DoubleQuotedString
                    }
                },
                TokenizerState::SingleQuotedString => {
                    match c {
                        '\'' => {
                            try!(token_data.end(&self.data, self.pos));
                            self.next_state = Some(TokenizerState::AfterFunctionArg);
                            TokenizerState::Junk

                        },
                        '\n' => {
                            return Err(PrefReaderError::new(
                                "EOL in single quoted string", self.position, None))
                        },
                        '\\' => {
                            try!(self.consume_escape(&mut token_data));
                            TokenizerState::SingleQuotedString
                        }
                        _ => TokenizerState::SingleQuotedString
                    }
                },
                TokenizerState::Number => {
                    match c {
                        '0'...'9' => TokenizerState::Number,
                        ')' | ',' => {
                            try!(token_data.end(&self.data, self.pos));
                            self.unget_char();
                            self.next_state = Some(TokenizerState::AfterFunctionArg);
                            TokenizerState::Junk
                        },
                        x if PrefTokenizer::is_space(x) => {
                            try!(token_data.end(&self.data, self.pos));
                            self.next_state = Some(TokenizerState::AfterFunctionArg);
                            TokenizerState::Junk
                        },
                        _ => {
                            return Err(PrefReaderError::new(
                                "Invalid character in number literal", self.position, None))
                        }
                    }
                },
                TokenizerState::Bool => {
                    let start_pos = self.pos;
                    let position = self.position;
                    match c {
                        't' => {
                            if self.get_match("true", ",)") {
                                token_data.start(&self, TokenType::Bool)
                            }
                        },
                        'f' => {
                            if self.get_match("false", ",)") {
                                token_data.start(&self, TokenType::Bool)
                            }
                        }
                        _ => {}
                    };
                    if token_data.token_type == TokenType::None {
                        return Err(PrefReaderError::new(
                            "Unexpected characters in function argument",
                            position, None));
                    } else {
                        token_data.start_pos = start_pos;
                        token_data.position = position;
                        try!(token_data.end(&self.data, self.pos + 1));
                        self.next_state = Some(TokenizerState::AfterFunctionArg);
                        TokenizerState::Junk
                    }
                },
                TokenizerState::AfterFunctionArg => {
                    match c {
                        ',' => {
                            token_data.start(&self, TokenType::Comma);
                            try!(token_data.end(&self.data, self.pos + 1));
                            self.next_state = Some(TokenizerState::FunctionArg);
                            TokenizerState::Junk
                        }
                        ')' => {
                            token_data.start(&self, TokenType::Paren);
                            try!(token_data.end(&self.data, self.pos + 1));
                            self.next_state = Some(TokenizerState::AfterFunction);
                            TokenizerState::Junk
                        }
                        _ => return Err(PrefReaderError::new
                                        ("Unexpected character after function argument",
                                         self.position,
                                         None))
                    }
                },
                TokenizerState::AfterFunction => {
                    match c {
                        ';' => {
                            token_data.start(&self, TokenType::Semicolon);
                            try!(token_data.end(&self.data, self.pos));
                            self.next_state = Some(TokenizerState::FunctionName);
                            TokenizerState::Junk
                        }
                        _ => return Err(PrefReaderError::new(
                            "Unexpected character after function",
                            self.position, None))
                    }
                },
                TokenizerState::Error => TokenizerState::Error
            };
            if token_data.complete {
                return Ok(Some(token_data));
            }
        }
    }
}

impl<'a> Iterator for PrefTokenizer<'a> {
    type Item = PrefToken<'a>;

    fn next(&mut self) -> Option<PrefToken<'a>> {
        if let TokenizerState::Error = self.state {
            return None;
        }
        let token_data = match self.next_token() {
            Err(e) => {
                self.state = TokenizerState::Error;
                return Some(PrefToken::Error(e.message,
                                             e.position))

            },
            Ok(Some(token_data)) => token_data,
            Ok(None) => return None,
        };
        let token = self.make_token(token_data);
        Some(token)
    }
}

pub fn tokenize<'a>(data: &'a [u8]) -> PrefTokenizer<'a> {
    PrefTokenizer::new(data)
}

pub fn serialize_token<T: Write>(token: &PrefToken, output: &mut T) -> Result<(), PrefReaderError> {
    let mut data_buf = String::new();

    let data = match token {
        &PrefToken::PrefFunction(_) => "pref",
        &PrefToken::UserPrefFunction(_) => "user_pref",
        &PrefToken::StickyPrefFunction(_) => "sticky_pref",
        &PrefToken::CommentBlock(ref data, _) => {
            data_buf.reserve(data.len() + 4);
            data_buf.push_str("/*");
            data_buf.push_str(data.borrow());
            data_buf.push_str("*");
            &*data_buf
        },
        &PrefToken::CommentLine(ref data, _) => {
            data_buf.reserve(data.len() + 2);
            data_buf.push_str("//");
            data_buf.push_str(data.borrow());
            &*data_buf
        },
        &PrefToken::CommentBashLine(ref data, _) => {
            data_buf.reserve(data.len() + 1);
            data_buf.push_str("#");
            data_buf.push_str(data.borrow());
            &*data_buf
        },
        &PrefToken::Paren(data, _) => {
            data_buf.push(data);
            &*data_buf
        },
        &PrefToken::Comma(_) => ",",
        &PrefToken::Semicolon(_) => ";\n",
        &PrefToken::String(ref data, _) => {
            data_buf.reserve(data.len() + 2);
            data_buf.push('"');
            data_buf.push_str(escape_quote(data.borrow()).borrow());
            data_buf.push('"');
            &*data_buf
        },
        &PrefToken::Int(data, _) => {
            data_buf.push_str(&*data.to_string());
            &*data_buf
        },
        &PrefToken::Bool(data, _) => {
            if data {"true"} else {"false"}
        },
        &PrefToken::Error(data, pos) => return Err(PrefReaderError::new(data, pos, None))
    };
    try!(output.write(data.as_bytes()));
    Ok(())
}

pub fn serialize_tokens<'a, I, W>(tokens: I, output: &mut W) -> Result<(), PrefReaderError>
    where I: Iterator<Item=&'a PrefToken<'a>>, W: Write {
    for token in tokens {
        try!(serialize_token(token, output));
    }
    Ok(())
}

fn escape_quote<'a>(data: &'a str) -> Cow<'a, str> {
    // Not very efficient…
    if data.contains("\"") || data.contains("\\") {
        let new_data = Cow::Owned(data
            .replace(r#"\"#, r#"\\"#)
            .replace(r#"""#, r#"\""#));
        new_data
    } else {
        Cow::Borrowed(data)
    }
}

#[derive(Debug, PartialEq)]
enum ParserState {
    Function,
    Key,
    Value,
}

struct PrefBuilder {
    key: Option<String>,
    value: Option<PrefValue>,
    sticky: bool
}

impl PrefBuilder {
    fn new() -> PrefBuilder {
        PrefBuilder {
            key: None,
            value: None,
            sticky: false
        }
    }
}


fn skip_comments<'a>(tokenizer: &mut PrefTokenizer<'a>) -> Option<PrefToken<'a>> {
    loop {
        match tokenizer.next() {
            Some(PrefToken::CommentBashLine(_, _)) |
            Some(PrefToken::CommentBlock(_, _)) |
            Some(PrefToken::CommentLine(_, _)) => {},
            Some(x) => return Some(x),
            None => return None,
        }
    }
}

pub fn parse_tokens<'a>(tokenizer: &mut PrefTokenizer<'a>) -> Result<Preferences,
                                                                     PrefReaderError> {

    let mut state = ParserState::Function;
    let mut current_pref = PrefBuilder::new();
    let mut rv = Preferences::new();

    loop {
        // Not just using a for loop here seems strange, but this restricts the
        // scope of the borrow
        let token = {
            match tokenizer.next() {
                Some(x) => x,
                None => break
            }
        };
        // First deal with comments and errors
        match token {
            PrefToken::Error(msg, position) => {
                return Err(PrefReaderError::new(msg.into(), position, None));
            },
            PrefToken::CommentBashLine(_, _) |
            PrefToken::CommentLine(_, _) |
            PrefToken::CommentBlock(_, _) => {
                continue
            },
            _ => {}
        }
        state = match state {
            ParserState::Function => {
                match token {
                    PrefToken::PrefFunction(_) => {
                        current_pref.sticky = false;
                    }
                    PrefToken::UserPrefFunction(_) => {
                        current_pref.sticky = false;
                    },
                    PrefToken::StickyPrefFunction(_) => {
                        current_pref.sticky = true;
                    },
                    _ => {
                        return Err(PrefReaderError::new("Expected pref function".into(),
                                                        token.position(), None));
                    }
                }
                let next = skip_comments(tokenizer);
                match next {
                    Some(PrefToken::Paren('(', _)) => ParserState::Key,
                    _ => return Err(PrefReaderError::new("Expected open paren".into(),
                                                         next.map(|x| x.position())
                                                         .unwrap_or(tokenizer.position),
                                                         None))
                }
            },
            ParserState::Key => {
                match token {
                    PrefToken::String(data, _) => current_pref.key = Some(data.into_owned()),
                    _ => {
                        return Err(PrefReaderError::new("Expected string", token.position(), None));
                    }
                }
                let next = skip_comments(tokenizer);
                match next {
                    Some(PrefToken::Comma(_)) => ParserState::Value,
                    _ => return Err(PrefReaderError::new("Expected comma",
                                                         next.map(|x| x.position())
                                                         .unwrap_or(tokenizer.position),
                                                         None))
                }
            },
            ParserState::Value => {
                match token {
                    PrefToken::String(data, _) => {
                        current_pref.value = Some(PrefValue::String(data.into_owned()))
                    },
                    PrefToken::Int(data, _) => {
                        current_pref.value = Some(PrefValue::Int(data))
                    }
                    PrefToken::Bool(data, _) => {
                        current_pref.value = Some(PrefValue::Bool(data))
                    },
                    _ => {
                        return Err(PrefReaderError::new("Expected value", token.position(),
                                                        None))
                    }
                }
                let next = skip_comments(tokenizer);
                match next {
                    Some(PrefToken::Paren(')', _)) => {}
                    _ => return Err(PrefReaderError::new("Expected close paren",
                                                         next.map(|x| x.position())
                                                         .unwrap_or(tokenizer.position),
                                                         None))
                }
                let next = skip_comments(tokenizer);
                match next {
                    Some(PrefToken::Semicolon(_)) |
                    None => {},
                    _ => return Err(PrefReaderError::new("Expected semicolon",
                                                         next.map(|x| x.position())
                                                         .unwrap_or(tokenizer.position),
                                                         None))
                }
                let key = mem::replace(&mut current_pref.key, None);
                let value = mem::replace(&mut current_pref.value, None);
                let pref = if current_pref.sticky {
                    Pref::new_sticky(value.unwrap())
                } else {
                    Pref::new(value.unwrap())
                };
                rv.insert(key.unwrap(), pref);
                current_pref.sticky = false;
                ParserState::Function
            },
        }
    }
    match state {
        ParserState::Key | ParserState::Value => {
            return Err(PrefReaderError::new("EOF in middle of function",
                                            tokenizer.position, None));
        },
        _ => {}
    }
    Ok(rv)
}

pub fn serialize<W: Write>(prefs: &Preferences, output: &mut W) -> io::Result<()> {
    let mut p: Vec<_> = prefs.iter().collect();
    p.sort_by(|a, b| a.0.cmp(&b.0));
    for &(key, pref) in p.iter() {
        let func = if pref.sticky {
            "sticky_pref("
        } else {
            "user_pref("
        }.as_bytes();
        try!(output.write(func));
        try!(output.write("\"".as_bytes()));
        try!(output.write(escape_quote(key).as_bytes()));
        try!(output.write("\"".as_bytes()));
        try!(output.write(", ".as_bytes()));
        match pref.value {
            PrefValue::Bool(x) => {
                try!(output.write((if x {"true"} else {"false"}).as_bytes()));
            },
            PrefValue::Int(x) => {
                try!(output.write(x.to_string().as_bytes()));
            },
            PrefValue::String(ref x) => {
                try!(output.write("\"".as_bytes()));
                try!(output.write(escape_quote(x).as_bytes()));
                try!(output.write("\"".as_bytes()));
            }
        };
        try!(output.write(");\n".as_bytes()));
    };
    Ok(())
}

pub fn parse<'a>(data: &'a [u8]) -> Result<Preferences, PrefReaderError> {
    let mut tokenizer = tokenize(data);
    parse_tokens(&mut tokenizer)
}
