//! Deserializing TOML into Rust structures.
//!
//! This module contains all the Serde support for deserializing TOML documents
//! into Rust structures. Note that some top-level functions here are also
//! provided at the top of the crate.

use std::borrow::Cow;
use std::error;
use std::fmt;
use std::str;
use std::vec;

use serde::de;
use serde::de::IntoDeserializer;

use tokens::{Tokenizer, Token, Error as TokenError};
use datetime::{SERDE_STRUCT_FIELD_NAME, SERDE_STRUCT_NAME};

/// Deserializes a byte slice into a type.
///
/// This function will attempt to interpret `bytes` as UTF-8 data and then
/// deserialize `T` from the TOML document provided.
pub fn from_slice<'de, T>(bytes: &'de [u8]) -> Result<T, Error>
    where T: de::Deserialize<'de>,
{
    match str::from_utf8(bytes) {
        Ok(s) => from_str(s),
        Err(e) => Err(Error::custom(e.to_string())),
    }
}

/// Deserializes a string into a type.
///
/// This function will attempt to interpret `s` as a TOML document and
/// deserialize `T` from the document.
///
/// # Examples
///
/// ```
/// #[macro_use]
/// extern crate serde_derive;
/// extern crate toml;
///
/// #[derive(Deserialize)]
/// struct Config {
///     title: String,
///     owner: Owner,
/// }
///
/// #[derive(Deserialize)]
/// struct Owner {
///     name: String,
/// }
///
/// fn main() {
///     let config: Config = toml::from_str(r#"
///         title = 'TOML Example'
///
///         [owner]
///         name = 'Lisa'
///     "#).unwrap();
///
///     assert_eq!(config.title, "TOML Example");
///     assert_eq!(config.owner.name, "Lisa");
/// }
/// ```
pub fn from_str<'de, T>(s: &'de str) -> Result<T, Error>
    where T: de::Deserialize<'de>,
{
    let mut d = Deserializer::new(s);
    let ret = T::deserialize(&mut d)?;
    d.end()?;
    Ok(ret)
}

/// Errors that can occur when deserializing a type.
#[derive(Debug, Clone)]
pub struct Error {
    inner: Box<ErrorInner>,
}

#[derive(Debug, Clone)]
struct ErrorInner {
    kind: ErrorKind,
    line: Option<usize>,
    col: usize,
    message: String,
    key: Vec<String>,
}

/// Errors that can occur when deserializing a type.
#[derive(Debug, Clone)]
enum ErrorKind {
    /// EOF was reached when looking for a value
    UnexpectedEof,

    /// An invalid character not allowed in a string was found
    InvalidCharInString(char),

    /// An invalid character was found as an escape
    InvalidEscape(char),

    /// An invalid character was found in a hex escape
    InvalidHexEscape(char),

    /// An invalid escape value was specified in a hex escape in a string.
    ///
    /// Valid values are in the plane of unicode codepoints.
    InvalidEscapeValue(u32),

    /// A newline in a string was encountered when one was not allowed.
    NewlineInString,

    /// An unexpected character was encountered, typically when looking for a
    /// value.
    Unexpected(char),

    /// An unterminated string was found where EOF was found before the ending
    /// EOF mark.
    UnterminatedString,

    /// A newline was found in a table key.
    NewlineInTableKey,

    /// A number failed to parse
    NumberInvalid,

    /// A date or datetime was invalid
    DateInvalid,

    /// Wanted one sort of token, but found another.
    Wanted {
        /// Expected token type
        expected: &'static str,
        /// Actually found token type
        found: &'static str,
    },

    /// An array was decoded but the types inside of it were mixed, which is
    /// disallowed by TOML.
    MixedArrayType,

    /// A duplicate table definition was found.
    DuplicateTable(String),

    /// A previously defined table was redefined as an array.
    RedefineAsArray,

    /// An empty table key was found.
    EmptyTableKey,

    /// A custom error which could be generated when deserializing a particular
    /// type.
    Custom,

    /// A struct was expected but something else was found
    ExpectedString,

    #[doc(hidden)]
    __Nonexhaustive,
}

/// Deserialization implementation for TOML.
pub struct Deserializer<'a> {
    require_newline_after_table: bool,
    input: &'a str,
    tokens: Tokenizer<'a>,
}

impl<'de, 'b> de::Deserializer<'de> for &'b mut Deserializer<'de> {
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: de::Visitor<'de>,
    {
        let mut tables = Vec::new();
        let mut cur_table = Table {
            at: 0,
            header: Vec::new(),
            values: None,
            array: false,
        };

        while let Some(line) = self.line()? {
            match line {
                Line::Table { at, mut header, array } => {
                    if !cur_table.header.is_empty() || cur_table.values.is_some() {
                        tables.push(cur_table);
                    }
                    cur_table = Table {
                        at: at,
                        header: Vec::new(),
                        values: Some(Vec::new()),
                        array: array,
                    };
                    loop {
                        let part = header.next().map_err(|e| {
                            self.token_error(e)
                        });
                        match part? {
                            Some(part) => cur_table.header.push(part),
                            None => break,
                        }
                    }
                }
                Line::KeyValue(key, value) => {
                    if cur_table.values.is_none() {
                        cur_table.values = Some(Vec::new());
                    }
                    cur_table.values.as_mut().unwrap().push((key, value));
                }
            }
        }
        if !cur_table.header.is_empty() || cur_table.values.is_some() {
            tables.push(cur_table);
        }

        visitor.visit_map(MapVisitor {
            values: Vec::new().into_iter(),
            next_value: None,
            depth: 0,
            cur: 0,
            cur_parent: 0,
            max: tables.len(),
            tables: &mut tables,
            array: false,
            de: self,
        })
    }

    fn deserialize_enum<V>(
        self,
        _name: &'static str,
        _variants: &'static [&'static str],
        visitor: V
    ) -> Result<V::Value, Error>
        where V: de::Visitor<'de>
    {
        if let Some(next) = self.next()? {
            match next {
                Token::String { val, .. } => {
                    visitor.visit_enum(val.into_deserializer())
                },
                _ => Err(Error::from_kind(ErrorKind::ExpectedString))
            }
        } else {
            Err(Error::from_kind(ErrorKind::UnexpectedEof))
        }
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string seq
        bytes byte_buf map struct unit newtype_struct
        ignored_any unit_struct tuple_struct tuple option identifier
    }
}

struct Table<'a> {
    at: usize,
    header: Vec<Cow<'a, str>>,
    values: Option<Vec<(Cow<'a, str>, Value<'a>)>>,
    array: bool,
}

#[doc(hidden)]
pub struct MapVisitor<'de: 'b, 'b> {
    values: vec::IntoIter<(Cow<'de, str>, Value<'de>)>,
    next_value: Option<(Cow<'de, str>, Value<'de>)>,
    depth: usize,
    cur: usize,
    cur_parent: usize,
    max: usize,
    tables: &'b mut [Table<'de>],
    array: bool,
    de: &'b mut Deserializer<'de>,
}

impl<'de, 'b> de::MapAccess<'de> for MapVisitor<'de, 'b> {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Error>
        where K: de::DeserializeSeed<'de>,
    {
        if self.cur_parent == self.max || self.cur == self.max {
            return Ok(None)
        }

        loop {
            assert!(self.next_value.is_none());
            if let Some((key, value)) = self.values.next() {
                let ret = seed.deserialize(StrDeserializer::new(key.clone()))?;
                self.next_value = Some((key, value));
                return Ok(Some(ret))
            }

            let next_table = {
                let prefix = &self.tables[self.cur_parent].header[..self.depth];
                self.tables[self.cur..self.max].iter().enumerate().find(|&(_, t)| {
                    if t.values.is_none() {
                        return false
                    }
                    match t.header.get(..self.depth) {
                        Some(header) => header == prefix,
                        None => false,
                    }
                }).map(|(i, _)| i + self.cur)
            };

            let pos = match next_table {
                Some(pos) => pos,
                None => return Ok(None),
            };
            self.cur = pos;

            // Test to see if we're duplicating our parent's table, and if so
            // then this is an error in the toml format
            if self.cur_parent != pos &&
               self.tables[self.cur_parent].header == self.tables[pos].header {
                let at = self.tables[pos].at;
                let name = self.tables[pos].header.join(".");
                return Err(self.de.error(at, ErrorKind::DuplicateTable(name)))
            }

            let table = &mut self.tables[pos];

            // If we're not yet at the appropriate depth for this table then we
            // just next the next portion of its header and then continue
            // decoding.
            if self.depth != table.header.len() {
                let key = &table.header[self.depth];
                let key = seed.deserialize(StrDeserializer::new(key.clone()))?;
                return Ok(Some(key))
            }

            // Rule out cases like:
            //
            //      [[foo.bar]]
            //      [[foo]]
            if table.array  {
                let kind = ErrorKind::RedefineAsArray;
                return Err(self.de.error(table.at, kind))
            }

            self.values = table.values.take().expect("Unable to read table values").into_iter();
        }
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Error>
        where V: de::DeserializeSeed<'de>,
    {
        if let Some((k, v)) = self.next_value.take() {
            match seed.deserialize(ValueDeserializer::new(v)) {
                Ok(v) => return Ok(v),
                Err(mut e) => {
                    e.add_key_context(&k);
                    return Err(e)
                }
            }
        }

        let array = self.tables[self.cur].array &&
                    self.depth == self.tables[self.cur].header.len() - 1;
        self.cur += 1;
        let res = seed.deserialize(MapVisitor {
            values: Vec::new().into_iter(),
            next_value: None,
            depth: self.depth + if array {0} else {1},
            cur_parent: self.cur - 1,
            cur: 0,
            max: self.max,
            array: array,
            tables: &mut *self.tables,
            de: &mut *self.de,
        });
        res.map_err(|mut e| {
            e.add_key_context(&self.tables[self.cur - 1].header[self.depth]);
            e
        })
    }
}

impl<'de, 'b> de::SeqAccess<'de> for MapVisitor<'de, 'b> {
    type Error = Error;

    fn next_element_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Error>
        where K: de::DeserializeSeed<'de>,
    {
        assert!(self.next_value.is_none());
        assert!(self.values.next().is_none());

        if self.cur_parent == self.max {
            return Ok(None)
        }

        let next = self.tables[..self.max]
            .iter()
            .enumerate()
            .skip(self.cur_parent + 1)
            .find(|&(_, table)| {
                table.array && table.header == self.tables[self.cur_parent].header
            }).map(|p| p.0)
            .unwrap_or(self.max);

        let ret = seed.deserialize(MapVisitor {
            values: self.tables[self.cur_parent].values.take().expect("Unable to read table values").into_iter(),
            next_value: None,
            depth: self.depth + 1,
            cur_parent: self.cur_parent,
            max: next,
            cur: 0,
            array: false,
            tables: &mut self.tables,
            de: &mut self.de,
        })?;
        self.cur_parent = next;
        Ok(Some(ret))
    }
}

impl<'de, 'b> de::Deserializer<'de> for MapVisitor<'de, 'b> {
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: de::Visitor<'de>,
    {
        if self.array  {
            visitor.visit_seq(self)
        } else {
            visitor.visit_map(self)
        }
    }

    // `None` is interpreted as a missing field so be sure to implement `Some`
    // as a present field.
    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: de::Visitor<'de>,
    {
        visitor.visit_some(self)
    }

    fn deserialize_newtype_struct<V>(
        self,
        _name: &'static str,
        visitor: V
    ) -> Result<V::Value, Error>
        where V: de::Visitor<'de>
    {
        visitor.visit_newtype_struct(self)
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string seq
        bytes byte_buf map struct unit identifier
        ignored_any unit_struct tuple_struct tuple enum
    }
}

struct StrDeserializer<'a> {
    key: Cow<'a, str>,
}

impl<'a> StrDeserializer<'a> {
    fn new(key: Cow<'a, str>) -> StrDeserializer<'a> {
        StrDeserializer {
            key: key,
        }
    }
}

impl<'de> de::Deserializer<'de> for StrDeserializer<'de> {
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: de::Visitor<'de>,
    {
        match self.key {
            Cow::Borrowed(s) => visitor.visit_borrowed_str(s),
            Cow::Owned(s) => visitor.visit_string(s),
        }
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string seq
        bytes byte_buf map struct option unit newtype_struct
        ignored_any unit_struct tuple_struct tuple enum identifier
    }
}

struct ValueDeserializer<'a> {
    value: Value<'a>,
}

impl<'a> ValueDeserializer<'a> {
    fn new(value: Value<'a>) -> ValueDeserializer<'a> {
        ValueDeserializer {
            value: value,
        }
    }
}

impl<'de> de::Deserializer<'de> for ValueDeserializer<'de> {
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: de::Visitor<'de>,
    {
        match self.value {
            Value::Integer(i) => visitor.visit_i64(i),
            Value::Boolean(b) => visitor.visit_bool(b),
            Value::Float(f) => visitor.visit_f64(f),
            Value::String(Cow::Borrowed(s)) => visitor.visit_borrowed_str(s),
            Value::String(Cow::Owned(s)) => visitor.visit_string(s),
            Value::Datetime(s) => visitor.visit_map(DatetimeDeserializer {
                date: s,
                visited: false,
            }),
            Value::Array(values) => {
                let mut s = de::value::SeqDeserializer::new(values.into_iter());
                let ret = visitor.visit_seq(&mut s)?;
                s.end()?;
                Ok(ret)
            }
            Value::InlineTable(values) => {
                visitor.visit_map(InlineTableDeserializer {
                    values: values.into_iter(),
                    next_value: None,
                })
            }
        }
    }

    fn deserialize_struct<V>(self,
                             name: &'static str,
                             fields: &'static [&'static str],
                             visitor: V) -> Result<V::Value, Error>
        where V: de::Visitor<'de>,
    {
        if name == SERDE_STRUCT_NAME && fields == &[SERDE_STRUCT_FIELD_NAME] {
            if let Value::Datetime(s) = self.value {
                return visitor.visit_map(DatetimeDeserializer {
                    date: s,
                    visited: false,
                })
            }
        }

        self.deserialize_any(visitor)
    }

    // `None` is interpreted as a missing field so be sure to implement `Some`
    // as a present field.
    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: de::Visitor<'de>,
    {
        visitor.visit_some(self)
    }

    fn deserialize_enum<V>(
        self,
        _name: &'static str,
        _variants: &'static [&'static str],
        visitor: V
    ) -> Result<V::Value, Error>
        where V: de::Visitor<'de>
    {
        match self.value {
            Value::String(val) => visitor.visit_enum(val.into_deserializer()),
            _ => Err(Error::from_kind(ErrorKind::ExpectedString))
        }
    }

    fn deserialize_newtype_struct<V>(
        self,
        _name: &'static str,
        visitor: V
    ) -> Result<V::Value, Error>
        where V: de::Visitor<'de>
    {
        visitor.visit_newtype_struct(self)
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string seq
        bytes byte_buf map unit identifier
        ignored_any unit_struct tuple_struct tuple
    }
}

impl<'de> de::IntoDeserializer<'de, Error> for Value<'de> {
    type Deserializer = ValueDeserializer<'de>;

    fn into_deserializer(self) -> Self::Deserializer {
        ValueDeserializer::new(self)
    }
}

struct DatetimeDeserializer<'a> {
    visited: bool,
    date: &'a str,
}

impl<'de> de::MapAccess<'de> for DatetimeDeserializer<'de> {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Error>
        where K: de::DeserializeSeed<'de>,
    {
        if self.visited {
            return Ok(None)
        }
        self.visited = true;
        seed.deserialize(DatetimeFieldDeserializer).map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Error>
        where V: de::DeserializeSeed<'de>,
    {
        seed.deserialize(StrDeserializer::new(self.date.into()))
    }
}

struct DatetimeFieldDeserializer;

impl<'de> de::Deserializer<'de> for DatetimeFieldDeserializer {
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Error>
        where V: de::Visitor<'de>,
    {
        visitor.visit_borrowed_str(SERDE_STRUCT_FIELD_NAME)
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string seq
        bytes byte_buf map struct option unit newtype_struct
        ignored_any unit_struct tuple_struct tuple enum identifier
    }
}

struct InlineTableDeserializer<'a> {
    values: vec::IntoIter<(Cow<'a, str>, Value<'a>)>,
    next_value: Option<Value<'a>>,
}

impl<'de> de::MapAccess<'de> for InlineTableDeserializer<'de> {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Error>
        where K: de::DeserializeSeed<'de>,
    {
        let (key, value) = match self.values.next() {
            Some(pair) => pair,
            None => return Ok(None),
        };
        self.next_value = Some(value);
        seed.deserialize(StrDeserializer::new(key)).map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Error>
        where V: de::DeserializeSeed<'de>,
    {
        let value = self.next_value.take().expect("Unable to read table values");
        seed.deserialize(ValueDeserializer::new(value))
    }
}


impl<'a> Deserializer<'a> {
    /// Creates a new deserializer which will be deserializing the string
    /// provided.
    pub fn new(input: &'a str) -> Deserializer<'a> {
        Deserializer {
            tokens: Tokenizer::new(input),
            input: input,
            require_newline_after_table: true,
        }
    }

    /// The `Deserializer::end` method should be called after a value has been
    /// fully deserialized.  This allows the `Deserializer` to validate that the
    /// input stream is at the end or that it only has trailing
    /// whitespace/comments.
    pub fn end(&mut self) -> Result<(), Error> {
        Ok(())
    }

    /// Historical versions of toml-rs accidentally allowed a newline after a
    /// table definition, but the TOML spec requires a newline after a table
    /// definition header.
    ///
    /// This option can be set to `false` (the default is `true`) to emulate
    /// this behavior for backwards compatibility with older toml-rs versions.
    pub fn set_require_newline_after_table(&mut self, require: bool) {
        self.require_newline_after_table = require;
    }

    fn line(&mut self) -> Result<Option<Line<'a>>, Error> {
        loop {
            self.eat_whitespace()?;
            if self.eat_comment()? {
                continue
            }
            if self.eat(Token::Newline)? {
                continue
            }
            break
        }

        match self.peek()? {
            Some(Token::LeftBracket) => self.table_header().map(Some),
            Some(_) => self.key_value().map(Some),
            None => Ok(None),
        }
    }

    fn table_header(&mut self) -> Result<Line<'a>, Error> {
        let start = self.tokens.current();
        self.expect(Token::LeftBracket)?;
        let array = self.eat(Token::LeftBracket)?;
        let ret = Header::new(self.tokens.clone(),
                              array,
                              self.require_newline_after_table);
        if self.require_newline_after_table {
            self.tokens.skip_to_newline();
        } else {
            loop {
                match self.next()? {
                    Some(Token::RightBracket) => {
                        if array {
                            self.eat(Token::RightBracket)?;
                        }
                        break
                    }
                    Some(Token::Newline) |
                    None => break,
                    _ => {}
                }
            }
            self.eat_whitespace()?;
        }
        Ok(Line::Table { at: start, header: ret, array: array })
    }

    fn key_value(&mut self) -> Result<Line<'a>, Error> {
        let key = self.table_key()?;
        self.eat_whitespace()?;
        self.expect(Token::Equals)?;
        self.eat_whitespace()?;

        let value = self.value()?;
        self.eat_whitespace()?;
        if !self.eat_comment()? {
            self.eat_newline_or_eof()?;
        }

        Ok(Line::KeyValue(key, value))
    }

    fn value(&mut self) -> Result<Value<'a>, Error> {
        let at = self.tokens.current();
        let value = match self.next()? {
            Some(Token::String { val, .. }) => Value::String(val),
            Some(Token::Keylike("true")) => Value::Boolean(true),
            Some(Token::Keylike("false")) => Value::Boolean(false),
            Some(Token::Keylike(key)) => self.number_or_date(key)?,
            Some(Token::Plus) => self.number_leading_plus()?,
            Some(Token::LeftBrace) => self.inline_table().map(Value::InlineTable)?,
            Some(Token::LeftBracket) => self.array().map(Value::Array)?,
            Some(token) => {
                return Err(self.error(at, ErrorKind::Wanted {
                    expected: "a value",
                    found: token.describe(),
                }))
            }
            None => return Err(self.eof()),
        };
        Ok(value)
    }

    fn number_or_date(&mut self, s: &'a str) -> Result<Value<'a>, Error> {
        if s.contains('T') || (s.len() > 1 && s[1..].contains('-')) &&
           !s.contains("e-") {
            self.datetime(s, false).map(Value::Datetime)
        } else if self.eat(Token::Colon)? {
            self.datetime(s, true).map(Value::Datetime)
        } else {
            self.number(s)
        }
    }

    fn number(&mut self, s: &'a str) -> Result<Value<'a>, Error> {
        if s.contains('e') || s.contains('E') {
            self.float(s, None).map(Value::Float)
        } else if self.eat(Token::Period)? {
            let at = self.tokens.current();
            match self.next()? {
                Some(Token::Keylike(after)) => {
                    self.float(s, Some(after)).map(Value::Float)
                }
                _ => Err(self.error(at, ErrorKind::NumberInvalid)),
            }
        } else {
            self.integer(s).map(Value::Integer)
        }
    }

    fn number_leading_plus(&mut self) -> Result<Value<'a>, Error> {
        let start = self.tokens.current();
        match self.next()? {
            Some(Token::Keylike(s)) => self.number(s),
            _ => Err(self.error(start, ErrorKind::NumberInvalid)),
        }
    }

    fn integer(&self, s: &'a str) -> Result<i64, Error> {
        let (prefix, suffix) = self.parse_integer(s, true, false)?;
        let start = self.tokens.substr_offset(s);
        if suffix != "" {
            return Err(self.error(start, ErrorKind::NumberInvalid))
        }
        prefix.replace("_", "").trim_left_matches('+').parse().map_err(|_e| {
            self.error(start, ErrorKind::NumberInvalid)
        })
    }

    fn parse_integer(&self,
                     s: &'a str,
                     allow_sign: bool,
                     allow_leading_zeros: bool)
                     -> Result<(&'a str, &'a str), Error> {
        let start = self.tokens.substr_offset(s);

        let mut first = true;
        let mut first_zero = false;
        let mut underscore = false;
        let mut end = s.len();
        for (i, c) in s.char_indices() {
            let at = i + start;
            if i == 0 && (c == '+' || c == '-') && allow_sign {
                continue
            }

            match c {
                '0' if first => first_zero = true,
                '0' ... '9' if !first && first_zero && !allow_leading_zeros => {
                    return Err(self.error(at, ErrorKind::NumberInvalid))
                }
                '0' ... '9' => underscore = false,
                '_' if first => {
                    return Err(self.error(at, ErrorKind::NumberInvalid))
                }
                '_' if !underscore => underscore = true,
                _ => {
                    end = i;
                    break
                }

            }
            first = false;
        }
        if first || underscore {
            return Err(self.error(start, ErrorKind::NumberInvalid))
        }
        Ok((&s[..end], &s[end..]))
    }

    fn float(&mut self, s: &'a str, after_decimal: Option<&'a str>)
             -> Result<f64, Error> {
        let (integral, mut suffix) = self.parse_integer(s, true, false)?;
        let start = self.tokens.substr_offset(integral);

        let mut fraction = None;
        if let Some(after) = after_decimal {
            if suffix != "" {
                return Err(self.error(start, ErrorKind::NumberInvalid))
            }
            let (a, b) = self.parse_integer(after, false, true)?;
            fraction = Some(a);
            suffix = b;
        }

        let mut exponent = None;
        if suffix.starts_with('e') || suffix.starts_with('E') {
            let (a, b) = if suffix.len() == 1 {
                self.eat(Token::Plus)?;
                match self.next()? {
                    Some(Token::Keylike(s)) => {
                        self.parse_integer(s, false, false)?
                    }
                    _ => return Err(self.error(start, ErrorKind::NumberInvalid)),
                }
            } else {
                self.parse_integer(&suffix[1..], true, false)?
            };
            if b != "" {
                return Err(self.error(start, ErrorKind::NumberInvalid))
            }
            exponent = Some(a);
        }

        let mut number = integral.trim_left_matches('+')
                                 .chars()
                                 .filter(|c| *c != '_')
                                 .collect::<String>();
        if let Some(fraction) = fraction {
            number.push_str(".");
            number.extend(fraction.chars().filter(|c| *c != '_'));
        }
        if let Some(exponent) = exponent {
            number.push_str("E");
            number.extend(exponent.chars().filter(|c| *c != '_'));
        }
        number.parse().map_err(|_e| {
            self.error(start, ErrorKind::NumberInvalid)
        }).and_then(|n: f64| {
            if n.is_finite() {
                Ok(n)
            } else {
                Err(self.error(start, ErrorKind::NumberInvalid))
            }
        })
    }

    fn datetime(&mut self, date: &'a str, colon_eaten: bool)
                -> Result<&'a str, Error> {
        let start = self.tokens.substr_offset(date);
        if colon_eaten || self.eat(Token::Colon)? {
            // minutes
            match self.next()? {
                Some(Token::Keylike(_)) => {}
                _ => return Err(self.error(start, ErrorKind::DateInvalid)),
            }
            // Seconds
            self.expect(Token::Colon)?;
            match self.next()? {
                Some(Token::Keylike(_)) => {}
                _ => return Err(self.error(start, ErrorKind::DateInvalid)),
            }
            // Fractional seconds
            if self.eat(Token::Period)? {
                match self.next()? {
                    Some(Token::Keylike(_)) => {}
                    _ => return Err(self.error(start, ErrorKind::DateInvalid)),
                }
            }

            // offset
            if self.eat(Token::Plus)? {
                match self.next()? {
                    Some(Token::Keylike(_)) => {}
                    _ => return Err(self.error(start, ErrorKind::DateInvalid)),
                }
            }
            if self.eat(Token::Colon)? {
                match self.next()? {
                    Some(Token::Keylike(_)) => {}
                    _ => return Err(self.error(start, ErrorKind::DateInvalid)),
                }
            }
        }
        let end = self.tokens.current();
        Ok(&self.tokens.input()[start..end])
    }

    // TODO(#140): shouldn't buffer up this entire table in memory, it'd be
    // great to defer parsing everything until later.
    fn inline_table(&mut self) -> Result<Vec<(Cow<'a, str>, Value<'a>)>, Error> {
        let mut ret = Vec::new();
        self.eat_whitespace()?;
        if self.eat(Token::RightBrace)? {
            return Ok(ret)
        }
        loop {
            let key = self.table_key()?;
            self.eat_whitespace()?;
            self.expect(Token::Equals)?;
            self.eat_whitespace()?;
            ret.push((key, self.value()?));

            self.eat_whitespace()?;
            if self.eat(Token::RightBrace)? {
                return Ok(ret)
            }
            self.expect(Token::Comma)?;
            self.eat_whitespace()?;
        }
    }

    // TODO(#140): shouldn't buffer up this entire array in memory, it'd be
    // great to defer parsing everything until later.
    fn array(&mut self) -> Result<Vec<Value<'a>>, Error> {
        let mut ret = Vec::new();

        let intermediate = |me: &mut Deserializer| {
            loop {
                me.eat_whitespace()?;
                if !me.eat(Token::Newline)? && !me.eat_comment()? {
                    break
                }
            }
            Ok(())
        };

        loop {
            intermediate(self)?;
            if self.eat(Token::RightBracket)? {
                return Ok(ret)
            }
            let at = self.tokens.current();
            let value = self.value()?;
            if let Some(last) = ret.last() {
                if !value.same_type(last) {
                    return Err(self.error(at, ErrorKind::MixedArrayType))
                }
            }
            ret.push(value);
            intermediate(self)?;
            if !self.eat(Token::Comma)? {
                break
            }
        }
        intermediate(self)?;
        self.expect(Token::RightBracket)?;
        Ok(ret)
    }

    fn table_key(&mut self) -> Result<Cow<'a, str>, Error> {
        self.tokens.table_key().map_err(|e| self.token_error(e))
    }

    fn eat_whitespace(&mut self) -> Result<(), Error> {
        self.tokens.eat_whitespace().map_err(|e| self.token_error(e))
    }

    fn eat_comment(&mut self) -> Result<bool, Error> {
        self.tokens.eat_comment().map_err(|e| self.token_error(e))
    }

    fn eat_newline_or_eof(&mut self) -> Result<(), Error> {
        self.tokens.eat_newline_or_eof().map_err(|e| self.token_error(e))
    }

    fn eat(&mut self, expected: Token<'a>) -> Result<bool, Error> {
        self.tokens.eat(expected).map_err(|e| self.token_error(e))
    }

    fn expect(&mut self, expected: Token<'a>) -> Result<(), Error> {
        self.tokens.expect(expected).map_err(|e| self.token_error(e))
    }

    fn next(&mut self) -> Result<Option<Token<'a>>, Error> {
        self.tokens.next().map_err(|e| self.token_error(e))
    }

    fn peek(&mut self) -> Result<Option<Token<'a>>, Error> {
        self.tokens.peek().map_err(|e| self.token_error(e))
    }

    fn eof(&self) -> Error {
        self.error(self.input.len(), ErrorKind::UnexpectedEof)
    }

    fn token_error(&self, error: TokenError) -> Error {
        match error {
            TokenError::InvalidCharInString(at, ch) => {
                self.error(at, ErrorKind::InvalidCharInString(ch))
            }
            TokenError::InvalidEscape(at, ch) => {
                self.error(at, ErrorKind::InvalidEscape(ch))
            }
            TokenError::InvalidEscapeValue(at, v) => {
                self.error(at, ErrorKind::InvalidEscapeValue(v))
            }
            TokenError::InvalidHexEscape(at, ch) => {
                self.error(at, ErrorKind::InvalidHexEscape(ch))
            }
            TokenError::NewlineInString(at) => {
                self.error(at, ErrorKind::NewlineInString)
            }
            TokenError::Unexpected(at, ch) => {
                self.error(at, ErrorKind::Unexpected(ch))
            }
            TokenError::UnterminatedString(at) => {
                self.error(at, ErrorKind::UnterminatedString)
            }
            TokenError::NewlineInTableKey(at) => {
                self.error(at, ErrorKind::NewlineInTableKey)
            }
            TokenError::Wanted { at, expected, found } => {
                self.error(at, ErrorKind::Wanted { expected: expected, found: found })
            }
            TokenError::EmptyTableKey(at) => {
                self.error(at, ErrorKind::EmptyTableKey)
            }
        }
    }

    fn error(&self, at: usize, kind: ErrorKind) -> Error {
        let mut err = Error::from_kind(kind);
        let (line, col) = self.to_linecol(at);
        err.inner.line = Some(line);
        err.inner.col = col;
        err
    }

    /// Converts a byte offset from an error message to a (line, column) pair
    ///
    /// All indexes are 0-based.
    fn to_linecol(&self, offset: usize) -> (usize, usize) {
        let mut cur = 0;
        for (i, line) in self.input.lines().enumerate() {
            if cur + line.len() + 1 > offset {
                return (i, offset - cur)
            }
            cur += line.len() + 1;
        }
        (self.input.lines().count(), 0)
    }
}

impl Error {
    /// Produces a (line, column) pair of the position of the error if available
    ///
    /// All indexes are 0-based.
    pub fn line_col(&self) -> Option<(usize, usize)> {
        self.inner.line.map(|line| (line, self.inner.col))
    }

    fn from_kind(kind: ErrorKind) -> Error {
        Error {
            inner: Box::new(ErrorInner {
                kind: kind,
                line: None,
                col: 0,
                message: String::new(),
                key: Vec::new(),
            }),
        }
    }

    fn custom(s: String) -> Error {
        Error {
            inner: Box::new(ErrorInner {
                kind: ErrorKind::Custom,
                line: None,
                col: 0,
                message: s,
                key: Vec::new(),
            }),
        }
    }

    /// Do not call this method, it may be removed at any time, it's just an
    /// internal implementation detail.
    #[doc(hidden)]
    pub fn add_key_context(&mut self, key: &str) {
        self.inner.key.insert(0, key.to_string());
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.inner.kind {
            ErrorKind::UnexpectedEof => "unexpected eof encountered".fmt(f)?,
            ErrorKind::InvalidCharInString(c) => {
                write!(f, "invalid character in string: `{}`",
                       c.escape_default().collect::<String>())?
            }
            ErrorKind::InvalidEscape(c) => {
                write!(f, "invalid escape character in string: `{}`",
                       c.escape_default().collect::<String>())?
            }
            ErrorKind::InvalidHexEscape(c) => {
                write!(f, "invalid hex escape character in string: `{}`",
                       c.escape_default().collect::<String>())?
            }
            ErrorKind::InvalidEscapeValue(c) => {
                write!(f, "invalid escape value: `{}`", c)?
            }
            ErrorKind::NewlineInString => "newline in string found".fmt(f)?,
            ErrorKind::Unexpected(ch) => {
                write!(f, "unexpected character found: `{}`",
                       ch.escape_default().collect::<String>())?
            }
            ErrorKind::UnterminatedString => "unterminated string".fmt(f)?,
            ErrorKind::NewlineInTableKey => "found newline in table key".fmt(f)?,
            ErrorKind::Wanted { expected, found } => {
                write!(f, "expected {}, found {}", expected, found)?
            }
            ErrorKind::NumberInvalid => "invalid number".fmt(f)?,
            ErrorKind::DateInvalid => "invalid date".fmt(f)?,
            ErrorKind::MixedArrayType => "mixed types in an array".fmt(f)?,
            ErrorKind::DuplicateTable(ref s) => {
                write!(f, "redefinition of table `{}`", s)?;
            }
            ErrorKind::RedefineAsArray => "table redefined as array".fmt(f)?,
            ErrorKind::EmptyTableKey => "empty table key found".fmt(f)?,
            ErrorKind::Custom => self.inner.message.fmt(f)?,
            ErrorKind::ExpectedString => "expected string".fmt(f)?,
            ErrorKind::__Nonexhaustive => panic!(),
        }

        if !self.inner.key.is_empty() {
            write!(f, " for key `")?;
            for (i, k) in self.inner.key.iter().enumerate() {
                if i > 0 {
                    write!(f, ".")?;
                }
                write!(f, "{}", k)?;
            }
            write!(f, "`")?;
        }

        if let Some(line) = self.inner.line {
            write!(f, " at line {}", line + 1)?;
        }

        Ok(())
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        match self.inner.kind {
            ErrorKind::UnexpectedEof => "unexpected eof encountered",
            ErrorKind::InvalidCharInString(_) => "invalid char in string",
            ErrorKind::InvalidEscape(_) => "invalid escape in string",
            ErrorKind::InvalidHexEscape(_) => "invalid hex escape in string",
            ErrorKind::InvalidEscapeValue(_) => "invalid escape value in string",
            ErrorKind::NewlineInString => "newline in string found",
            ErrorKind::Unexpected(_) => "unexpected or invalid character",
            ErrorKind::UnterminatedString => "unterminated string",
            ErrorKind::NewlineInTableKey => "found newline in table key",
            ErrorKind::Wanted { .. } => "expected a token but found another",
            ErrorKind::NumberInvalid => "invalid number",
            ErrorKind::DateInvalid => "invalid date",
            ErrorKind::MixedArrayType => "mixed types in an array",
            ErrorKind::DuplicateTable(_) => "duplicate table",
            ErrorKind::RedefineAsArray => "table redefined as array",
            ErrorKind::EmptyTableKey => "empty table key found",
            ErrorKind::Custom => "a custom error",
            ErrorKind::ExpectedString => "expected string",
            ErrorKind::__Nonexhaustive => panic!(),
        }
    }
}

impl de::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Error {
        Error::custom(msg.to_string())
    }
}

enum Line<'a> {
    Table { at: usize, header: Header<'a>, array: bool },
    KeyValue(Cow<'a, str>, Value<'a>),
}

struct Header<'a> {
    first: bool,
    array: bool,
    require_newline_after_table: bool,
    tokens: Tokenizer<'a>,
}

impl<'a> Header<'a> {
    fn new(tokens: Tokenizer<'a>,
           array: bool,
           require_newline_after_table: bool) -> Header<'a> {
        Header {
            first: true,
            array: array,
            tokens: tokens,
            require_newline_after_table: require_newline_after_table,
        }
    }

    fn next(&mut self) -> Result<Option<Cow<'a, str>>, TokenError> {
        self.tokens.eat_whitespace()?;

        if self.first || self.tokens.eat(Token::Period)? {
            self.first = false;
            self.tokens.eat_whitespace()?;
            self.tokens.table_key().map(Some)
        } else {
            self.tokens.expect(Token::RightBracket)?;
            if self.array {
                self.tokens.expect(Token::RightBracket)?;
            }

            self.tokens.eat_whitespace()?;
            if self.require_newline_after_table {
                if !self.tokens.eat_comment()? {
                    self.tokens.eat_newline_or_eof()?;
                }
            }
            Ok(None)
        }
    }
}

#[derive(Debug)]
enum Value<'a> {
    Integer(i64),
    Float(f64),
    Boolean(bool),
    String(Cow<'a, str>),
    Datetime(&'a str),
    Array(Vec<Value<'a>>),
    InlineTable(Vec<(Cow<'a, str>, Value<'a>)>),
}

impl<'a> Value<'a> {
    fn same_type(&self, other: &Value<'a>) -> bool {
        match (self, other) {
            (&Value::String(..), &Value::String(..)) |
            (&Value::Integer(..), &Value::Integer(..)) |
            (&Value::Float(..), &Value::Float(..)) |
            (&Value::Boolean(..), &Value::Boolean(..)) |
            (&Value::Datetime(..), &Value::Datetime(..)) |
            (&Value::Array(..), &Value::Array(..)) |
            (&Value::InlineTable(..), &Value::InlineTable(..)) => true,

            _ => false,
        }
    }
}
