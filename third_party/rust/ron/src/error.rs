use std::{error::Error as StdError, fmt, io, str::Utf8Error, string::FromUtf8Error};

use serde::{de, ser};

use crate::parse::{is_ident_first_char, is_ident_other_char, is_ident_raw_char, BASE64_ENGINE};

/// This type represents all possible errors that can occur when
/// serializing or deserializing RON data.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SpannedError {
    pub code: Error,
    pub position: Position,
}

pub type Result<T, E = Error> = std::result::Result<T, E>;
pub type SpannedResult<T> = std::result::Result<T, SpannedError>;

#[derive(Clone, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum Error {
    Io(String),
    Message(String),
    Base64Error(base64::DecodeError),
    Eof,
    ExpectedArray,
    ExpectedArrayEnd,
    ExpectedAttribute,
    ExpectedAttributeEnd,
    ExpectedBoolean,
    ExpectedComma,
    ExpectedChar,
    ExpectedFloat,
    FloatUnderscore,
    ExpectedInteger,
    ExpectedOption,
    ExpectedOptionEnd,
    ExpectedMap,
    ExpectedMapColon,
    ExpectedMapEnd,
    ExpectedDifferentStructName {
        expected: &'static str,
        found: String,
    },
    ExpectedStructLike,
    ExpectedNamedStructLike(&'static str),
    ExpectedStructLikeEnd,
    ExpectedUnit,
    ExpectedString,
    ExpectedStringEnd,
    ExpectedIdentifier,

    InvalidEscape(&'static str),

    IntegerOutOfBounds,

    NoSuchExtension(String),

    UnclosedBlockComment,
    UnderscoreAtBeginning,
    UnexpectedByte(char),

    Utf8Error(Utf8Error),
    TrailingCharacters,

    InvalidValueForType {
        expected: String,
        found: String,
    },
    ExpectedDifferentLength {
        expected: String,
        found: usize,
    },
    NoSuchEnumVariant {
        expected: &'static [&'static str],
        found: String,
        outer: Option<String>,
    },
    NoSuchStructField {
        expected: &'static [&'static str],
        found: String,
        outer: Option<String>,
    },
    MissingStructField {
        field: &'static str,
        outer: Option<String>,
    },
    DuplicateStructField {
        field: &'static str,
        outer: Option<String>,
    },
    InvalidIdentifier(String),
    SuggestRawIdentifier(String),
    ExceededRecursionLimit,
}

impl fmt::Display for SpannedError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if (self.position == Position { line: 0, col: 0 }) {
            write!(f, "{}", self.code)
        } else {
            write!(f, "{}: {}", self.position, self.code)
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Error::Io(ref s) => f.write_str(s),
            Error::Message(ref s) => f.write_str(s),
            Error::Base64Error(ref e) => fmt::Display::fmt(e, f),
            Error::Eof => f.write_str("Unexpected end of RON"),
            Error::ExpectedArray => f.write_str("Expected opening `[`"),
            Error::ExpectedArrayEnd => f.write_str("Expected closing `]`"),
            Error::ExpectedAttribute => f.write_str("Expected an `#![enable(...)]` attribute"),
            Error::ExpectedAttributeEnd => {
                f.write_str("Expected closing `)]` after the enable attribute")
            }
            Error::ExpectedBoolean => f.write_str("Expected boolean"),
            Error::ExpectedComma => f.write_str("Expected comma"),
            Error::ExpectedChar => f.write_str("Expected char"),
            Error::ExpectedFloat => f.write_str("Expected float"),
            Error::FloatUnderscore => f.write_str("Unexpected underscore in float"),
            Error::ExpectedInteger => f.write_str("Expected integer"),
            Error::ExpectedOption => f.write_str("Expected option"),
            Error::ExpectedOptionEnd => f.write_str("Expected closing `)`"),
            Error::ExpectedMap => f.write_str("Expected opening `{`"),
            Error::ExpectedMapColon => f.write_str("Expected colon"),
            Error::ExpectedMapEnd => f.write_str("Expected closing `}`"),
            Error::ExpectedDifferentStructName {
                expected,
                ref found,
            } => write!(
                f,
                "Expected struct {} but found {}",
                Identifier(expected),
                Identifier(found)
            ),
            Error::ExpectedStructLike => f.write_str("Expected opening `(`"),
            Error::ExpectedNamedStructLike(name) => {
                if name.is_empty() {
                    f.write_str("Expected only opening `(`, no name, for un-nameable struct")
                } else {
                    write!(f, "Expected opening `(` for struct {}", Identifier(name))
                }
            }
            Error::ExpectedStructLikeEnd => f.write_str("Expected closing `)`"),
            Error::ExpectedUnit => f.write_str("Expected unit"),
            Error::ExpectedString => f.write_str("Expected string"),
            Error::ExpectedStringEnd => f.write_str("Expected end of string"),
            Error::ExpectedIdentifier => f.write_str("Expected identifier"),
            Error::InvalidEscape(s) => f.write_str(s),
            Error::IntegerOutOfBounds => f.write_str("Integer is out of bounds"),
            Error::NoSuchExtension(ref name) => {
                write!(f, "No RON extension named {}", Identifier(name))
            }
            Error::Utf8Error(ref e) => fmt::Display::fmt(e, f),
            Error::UnclosedBlockComment => f.write_str("Unclosed block comment"),
            Error::UnderscoreAtBeginning => {
                f.write_str("Unexpected leading underscore in an integer")
            }
            Error::UnexpectedByte(ref byte) => write!(f, "Unexpected byte {:?}", byte),
            Error::TrailingCharacters => f.write_str("Non-whitespace trailing characters"),
            Error::InvalidValueForType {
                ref expected,
                ref found,
            } => {
                write!(f, "Expected {} but found {} instead", expected, found)
            }
            Error::ExpectedDifferentLength {
                ref expected,
                found,
            } => {
                write!(f, "Expected {} but found ", expected)?;

                match found {
                    0 => f.write_str("zero elements")?,
                    1 => f.write_str("one element")?,
                    n => write!(f, "{} elements", n)?,
                }

                f.write_str(" instead")
            }
            Error::NoSuchEnumVariant {
                expected,
                ref found,
                ref outer,
            } => {
                f.write_str("Unexpected ")?;

                if outer.is_none() {
                    f.write_str("enum ")?;
                }

                write!(f, "variant named {}", Identifier(found))?;

                if let Some(outer) = outer {
                    write!(f, "in enum {}", Identifier(outer))?;
                }

                write!(
                    f,
                    ", {}",
                    OneOf {
                        alts: expected,
                        none: "variants"
                    }
                )
            }
            Error::NoSuchStructField {
                expected,
                ref found,
                ref outer,
            } => {
                write!(f, "Unexpected field named {}", Identifier(found))?;

                if let Some(outer) = outer {
                    write!(f, "in {}", Identifier(outer))?;
                }

                write!(
                    f,
                    ", {}",
                    OneOf {
                        alts: expected,
                        none: "fields"
                    }
                )
            }
            Error::MissingStructField { field, ref outer } => {
                write!(f, "Unexpected missing field {}", Identifier(field))?;

                match outer {
                    Some(outer) => write!(f, " in {}", Identifier(outer)),
                    None => Ok(()),
                }
            }
            Error::DuplicateStructField { field, ref outer } => {
                write!(f, "Unexpected duplicate field {}", Identifier(field))?;

                match outer {
                    Some(outer) => write!(f, " in {}", Identifier(outer)),
                    None => Ok(()),
                }
            }
            Error::InvalidIdentifier(ref invalid) => write!(f, "Invalid identifier {:?}", invalid),
            Error::SuggestRawIdentifier(ref identifier) => write!(
                f,
                "Found invalid std identifier `{}`, try the raw identifier `r#{}` instead",
                identifier, identifier
            ),
            Error::ExceededRecursionLimit => f.write_str("Exceeded recursion limit, try increasing the limit and using `serde_stacker` to protect against a stack overflow"),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Position {
    pub line: usize,
    pub col: usize,
}

impl fmt::Display for Position {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}", self.line, self.col)
    }
}

impl ser::Error for Error {
    #[cold]
    fn custom<T: fmt::Display>(msg: T) -> Self {
        Error::Message(msg.to_string())
    }
}

impl de::Error for Error {
    #[cold]
    fn custom<T: fmt::Display>(msg: T) -> Self {
        Error::Message(msg.to_string())
    }

    #[cold]
    fn invalid_type(unexp: de::Unexpected, exp: &dyn de::Expected) -> Self {
        // Invalid type and invalid value are merged given their similarity in ron
        Self::invalid_value(unexp, exp)
    }

    #[cold]
    fn invalid_value(unexp: de::Unexpected, exp: &dyn de::Expected) -> Self {
        struct UnexpectedSerdeTypeValue<'a>(de::Unexpected<'a>);

        impl<'a> fmt::Display for UnexpectedSerdeTypeValue<'a> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                use de::Unexpected::*;

                match self.0 {
                    Bool(b) => write!(f, "the boolean `{}`", b),
                    Unsigned(i) => write!(f, "the unsigned integer `{}`", i),
                    Signed(i) => write!(f, "the signed integer `{}`", i),
                    Float(n) => write!(f, "the floating point number `{}`", n),
                    Char(c) => write!(f, "the UTF-8 character `{}`", c),
                    Str(s) => write!(f, "the string {:?}", s),
                    Bytes(b) => write!(f, "the bytes \"{}\"", {
                        base64::display::Base64Display::new(b, &BASE64_ENGINE)
                    }),
                    Unit => write!(f, "a unit value"),
                    Option => write!(f, "an optional value"),
                    NewtypeStruct => write!(f, "a newtype struct"),
                    Seq => write!(f, "a sequence"),
                    Map => write!(f, "a map"),
                    Enum => write!(f, "an enum"),
                    UnitVariant => write!(f, "a unit variant"),
                    NewtypeVariant => write!(f, "a newtype variant"),
                    TupleVariant => write!(f, "a tuple variant"),
                    StructVariant => write!(f, "a struct variant"),
                    Other(other) => f.write_str(other),
                }
            }
        }

        Error::InvalidValueForType {
            expected: exp.to_string(),
            found: UnexpectedSerdeTypeValue(unexp).to_string(),
        }
    }

    #[cold]
    fn invalid_length(len: usize, exp: &dyn de::Expected) -> Self {
        Error::ExpectedDifferentLength {
            expected: exp.to_string(),
            found: len,
        }
    }

    #[cold]
    fn unknown_variant(variant: &str, expected: &'static [&'static str]) -> Self {
        Error::NoSuchEnumVariant {
            expected,
            found: variant.to_string(),
            outer: None,
        }
    }

    #[cold]
    fn unknown_field(field: &str, expected: &'static [&'static str]) -> Self {
        Error::NoSuchStructField {
            expected,
            found: field.to_string(),
            outer: None,
        }
    }

    #[cold]
    fn missing_field(field: &'static str) -> Self {
        Error::MissingStructField { field, outer: None }
    }

    #[cold]
    fn duplicate_field(field: &'static str) -> Self {
        Error::DuplicateStructField { field, outer: None }
    }
}

impl StdError for SpannedError {}
impl StdError for Error {}

impl From<Utf8Error> for Error {
    fn from(e: Utf8Error) -> Self {
        Error::Utf8Error(e)
    }
}

impl From<FromUtf8Error> for Error {
    fn from(e: FromUtf8Error) -> Self {
        Error::Utf8Error(e.utf8_error())
    }
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Self {
        Error::Io(e.to_string())
    }
}

impl From<io::Error> for SpannedError {
    fn from(e: io::Error) -> Self {
        SpannedError {
            code: e.into(),
            position: Position { line: 0, col: 0 },
        }
    }
}

impl From<SpannedError> for Error {
    fn from(e: SpannedError) -> Self {
        e.code
    }
}

struct OneOf {
    alts: &'static [&'static str],
    none: &'static str,
}

impl fmt::Display for OneOf {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.alts {
            [] => write!(f, "there are no {}", self.none),
            [a1] => write!(f, "expected {} instead", Identifier(a1)),
            [a1, a2] => write!(
                f,
                "expected either {} or {} instead",
                Identifier(a1),
                Identifier(a2)
            ),
            [a1, ref alts @ ..] => {
                write!(f, "expected one of {}", Identifier(a1))?;

                for alt in alts {
                    write!(f, ", {}", Identifier(alt))?;
                }

                f.write_str(" instead")
            }
        }
    }
}

struct Identifier<'a>(&'a str);

impl<'a> fmt::Display for Identifier<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.0.is_empty() || !self.0.as_bytes().iter().copied().all(is_ident_raw_char) {
            return write!(f, "{:?}_[invalid identifier]", self.0);
        }

        let mut bytes = self.0.as_bytes().iter().copied();

        if !bytes.next().map_or(false, is_ident_first_char) || !bytes.all(is_ident_other_char) {
            write!(f, "`r#{}`", self.0)
        } else {
            write!(f, "`{}`", self.0)
        }
    }
}
