//! Types for working with darling errors and results.

use std::error::Error as StdError;
use std::fmt;
use std::iter::{self, Iterator};
use std::string::ToString;
use std::vec;

/// An alias of `Result` specific to attribute parsing.
pub type Result<T> = ::std::result::Result<T, Error>;

/// An error encountered during attribute parsing.
///
/// Given that most errors darling encounters represent code bugs in dependent crates,
/// the internal structure of the error is deliberately opaque.
#[derive(Debug)]
// Don't want to publicly commit to Error supporting equality yet, but
// not having it makes testing very difficult.
#[cfg_attr(test, derive(Clone, PartialEq, Eq))]
pub struct Error {
    kind: ErrorKind,
    locations: Vec<String>,
}

impl Error {
    fn new(kind: ErrorKind) -> Self {
        Error {
            kind: kind,
            locations: Vec::new(),
        }
    }

    /// Creates a new error with a custom message.
    pub fn custom<T: fmt::Display>(msg: T) -> Self {
        Error::new(ErrorKind::Custom(msg.to_string()))
    }

    /// Creates a new error for a field that appears twice in the input.
    pub fn duplicate_field(name: &str) -> Self {
        Error::new(ErrorKind::DuplicateField(name.into()))
    }

    /// Creates a new error for a non-optional field that does not appear in the input.
    pub fn missing_field(name: &str) -> Self {
        Error::new(ErrorKind::MissingField(name.into()))
    }

    /// Creates a new error for a field name that appears in the input but does not correspond
    /// to a known field.
    pub fn unknown_field(name: &str) -> Self {
        Error::new(ErrorKind::UnknownField(name.into()))
    }

    /// Creates a new error for a struct or variant that does not adhere to the supported shape.
    pub fn unsupported_shape(shape: &str) -> Self {
        Error::new(ErrorKind::UnsupportedShape(shape.into()))
    }

    pub fn unsupported_format(format: &str) -> Self {
        Error::new(ErrorKind::UnexpectedFormat(format.into()))
    }

    /// Creates a new error for a field which has an unexpected literal type.
    pub fn unexpected_type(ty: &str) -> Self {
        Error::new(ErrorKind::UnexpectedType(ty.into()))
    }

    /// Creates a new error for a value which doesn't match a set of expected literals.
    pub fn unknown_value(value: &str) -> Self {
        Error::new(ErrorKind::UnknownValue(value.into()))
    }

    /// Creates a new error for a list which did not get enough items to proceed.
    pub fn too_few_items(min: usize) -> Self {
        Error::new(ErrorKind::TooFewItems(min))
    }

    /// Creates a new error when a list got more items than it supports. The `max` argument
    /// is the largest number of items the receiver could accept.
    pub fn too_many_items(max: usize) -> Self {
        Error::new(ErrorKind::TooManyItems(max))
    }

    /// Bundle a set of multiple errors into a single `Error` instance.
    ///
    /// # Panics
    /// This function will panic if `errors.is_empty() == true`.
    pub fn multiple(mut errors: Vec<Error>) -> Self {
        if errors.len() > 1 {
            Error::new(ErrorKind::Multiple(errors))
        } else if errors.len() == 1 {
            errors
                .pop()
                .expect("Error array of length 1 has a first item")
        } else {
            panic!("Can't deal with 0 errors")
        }
    }

    /// Recursively converts a tree of errors to a flattened list.
    pub fn flatten(self) -> Self {
        Error::multiple(self.to_vec())
    }

    fn to_vec(self) -> Vec<Self> {
        if let ErrorKind::Multiple(errors) = self.kind {
            let mut flat = Vec::new();
            for error in errors {
                flat.extend(error.prepend_at(self.locations.clone()).to_vec());
            }

            flat
        } else {
            vec![self]
        }
    }

    /// Adds a location to the error, such as a field or variant.
    /// Locations must be added in reverse order of specificity.
    pub fn at<T: fmt::Display>(mut self, location: T) -> Self {
        self.locations.insert(0, location.to_string());
        self
    }

    /// Gets the number of individual errors in this error.
    ///
    /// This function should never return `0`, as it shouldn't be possible to construct
    /// a multi-error from an empty `Vec`.
    pub fn len(&self) -> usize {
        self.kind.len()
    }

    /// Adds a location chain to the head of the error's existing locations.
    fn prepend_at(mut self, mut locations: Vec<String>) -> Self {
        if !locations.is_empty() {
            locations.extend(self.locations);
            self.locations = locations;
        }

        self
    }

    /// Gets the location slice.
    #[cfg(test)]
    pub(crate) fn location(&self) -> Vec<&str> {
        self.locations.iter().map(|i| i.as_str()).collect()
    }
}

impl StdError for Error {
    fn description(&self) -> &str {
        &self.kind.description()
    }

    fn cause(&self) -> Option<&StdError> {
        None
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.kind)?;
        if !self.locations.is_empty() {
            write!(f, " at {}", self.locations.join("/"))?;
        }

        Ok(())
    }
}

impl IntoIterator for Error {
    type Item = Error;
    type IntoIter = IntoIter;

    fn into_iter(self) -> IntoIter {
        if let ErrorKind::Multiple(errors) = self.kind {
            IntoIter {
                inner: IntoIterEnum::Multiple(errors.into_iter()),
            }
        } else {
            IntoIter {
                inner: IntoIterEnum::Single(iter::once(self)),
            }
        }
    }
}

enum IntoIterEnum {
    Single(iter::Once<Error>),
    Multiple(vec::IntoIter<Error>),
}

impl Iterator for IntoIterEnum {
    type Item = Error;

    fn next(&mut self) -> Option<Self::Item> {
        match *self {
            IntoIterEnum::Single(ref mut content) => content.next(),
            IntoIterEnum::Multiple(ref mut content) => content.next(),
        }
    }
}

/// An iterator that moves out of an `Error`.
pub struct IntoIter {
    inner: IntoIterEnum,
}

impl Iterator for IntoIter {
    type Item = Error;

    fn next(&mut self) -> Option<Error> {
        self.inner.next()
    }
}

type DeriveInputShape = String;
type FieldName = String;
type MetaFormat = String;

#[derive(Debug)]
// Don't want to publicly commit to ErrorKind supporting equality yet, but
// not having it makes testing very difficult.
#[cfg_attr(test, derive(Clone, PartialEq, Eq))]
enum ErrorKind {
    /// An arbitrary error message.
    Custom(String),
    DuplicateField(FieldName),
    MissingField(FieldName),
    UnsupportedShape(DeriveInputShape),
    UnknownField(FieldName),
    UnexpectedFormat(MetaFormat),
    UnexpectedType(String),
    UnknownValue(String),
    TooFewItems(usize),
    TooManyItems(usize),
    /// A set of errors.
    Multiple(Vec<Error>),

    // TODO make this variant take `!` so it can't exist
    #[doc(hidden)]
    __NonExhaustive,
}

impl ErrorKind {
    pub fn description(&self) -> &str {
        use self::ErrorKind::*;

        match *self {
            Custom(ref s) => s,
            DuplicateField(_) => "Duplicate field",
            MissingField(_) => "Missing field",
            UnknownField(_) => "Unexpected field",
            UnsupportedShape(_) => "Unsupported shape",
            UnexpectedFormat(_) => "Unexpected meta-item format",
            UnexpectedType(_) => "Unexpected literal type",
            UnknownValue(_) => "Unknown literal value",
            TooFewItems(_) => "Too few items",
            TooManyItems(_) => "Too many items",
            Multiple(_) => "Multiple errors",
            __NonExhaustive => unreachable!(),
        }
    }

    /// Deeply counts the number of errors this item represents.
    pub fn len(&self) -> usize {
        if let ErrorKind::Multiple(ref items) = *self {
            items.iter().map(Error::len).sum()
        } else {
            1
        }
    }
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::ErrorKind::*;

        match *self {
            Custom(ref s) => s.fmt(f),
            DuplicateField(ref field) => write!(f, "Duplicate field `{}`", field),
            MissingField(ref field) => write!(f, "Missing field `{}`", field),
            UnknownField(ref field) => write!(f, "Unexpected field `{}`", field),
            UnsupportedShape(ref shape) => write!(f, "Unsupported shape `{}`", shape),
            UnexpectedFormat(ref format) => write!(f, "Unexpected meta-item format `{}`", format),
            UnexpectedType(ref ty) => write!(f, "Unexpected literal type `{}`", ty),
            UnknownValue(ref val) => write!(f, "Unknown literal value `{}`", val),
            TooFewItems(ref min) => write!(f, "Too few items: Expected at least {}", min),
            TooManyItems(ref max) => write!(f, "Too many items: Expected no more than {}", max),
            Multiple(ref items) if items.len() == 1 => items[0].fmt(f),
            Multiple(ref items) => {
                write!(f, "Multiple errors: (")?;
                let mut first = true;
                for item in items {
                    if !first {
                        write!(f, ", ")?;
                    } else {
                        first = false;
                    }

                    item.fmt(f)?;
                }

                write!(f, ")")
            }
            __NonExhaustive => unreachable!(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::Error;

    #[test]
    fn flatten_noop() {
        let err = Error::duplicate_field("hello").at("world");
        assert_eq!(err.clone().flatten(), err);
    }

    #[test]
    fn flatten_simple() {
        let err = Error::multiple(vec![
            Error::unknown_field("hello").at("world"),
            Error::missing_field("hell_no").at("world"),
        ]).at("foo")
            .flatten();

        assert!(err.location().is_empty());

        let mut err_iter = err.into_iter();

        let first = err_iter.next();
        assert!(first.is_some());
        assert_eq!(first.unwrap().location(), vec!["foo", "world"]);

        let second = err_iter.next();
        assert!(second.is_some());

        assert_eq!(second.unwrap().location(), vec!["foo", "world"]);

        assert!(err_iter.next().is_none());
    }

    #[test]
    fn len_single() {
        let err = Error::duplicate_field("hello");
        assert_eq!(1, err.len());
    }

    #[test]
    fn len_multiple() {
        let err = Error::multiple(vec![
            Error::duplicate_field("hello"),
            Error::missing_field("hell_no"),
        ]);
        assert_eq!(2, err.len());
    }

    #[test]
    fn len_nested() {
        let err = Error::multiple(vec![
            Error::duplicate_field("hello"),
            Error::multiple(vec![
                Error::duplicate_field("hi"),
                Error::missing_field("bye"),
                Error::multiple(vec![Error::duplicate_field("whatsup")]),
            ]),
        ]);

        assert_eq!(4, err.len());
    }
}
