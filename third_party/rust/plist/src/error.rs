use std::{error, fmt, io};

use crate::stream::Event;

/// This type represents all possible errors that can occur when working with plist data.
#[derive(Debug)]
pub struct Error {
    inner: Box<ErrorImpl>,
}

#[derive(Debug)]
pub(crate) struct ErrorImpl {
    kind: ErrorKind,
    file_position: Option<FilePosition>,
}

#[derive(Debug)]
pub(crate) enum ErrorKind {
    UnexpectedEof,
    UnexpectedEndOfEventStream,
    UnexpectedEventType {
        expected: EventKind,
        found: EventKind,
    },

    // Xml format-specific errors
    UnclosedXmlElement,
    UnpairedXmlClosingTag,
    UnexpectedXmlCharactersExpectedElement,
    UnexpectedXmlOpeningTag,
    UnknownXmlElement,
    InvalidXmlSyntax,
    InvalidXmlUtf8,
    InvalidDataString,
    InvalidDateString,
    InvalidIntegerString,
    InvalidRealString,
    UidNotSupportedInXmlPlist,

    // Binary format-specific errors
    ObjectTooLarge,
    InvalidMagic,
    InvalidTrailerObjectOffsetSize, // the size of byte offsets to objects in the object table
    InvalidTrailerObjectReferenceSize, // the size of indices into the object table
    InvalidObjectLength,
    ObjectReferenceTooLarge,
    ObjectOffsetTooLarge,
    RecursiveObject,
    NullObjectUnimplemented,
    FillObjectUnimplemented,
    IntegerOutOfRange,
    InfiniteOrNanDate,
    InvalidUtf8String,
    InvalidUtf16String,
    UnknownObjectType(u8),

    Io(io::Error),
    Serde(String),
}

#[derive(Debug)]
pub(crate) enum FilePosition {
    LineColumn(u64, u64),
    Offset(u64),
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub(crate) enum EventKind {
    StartArray,
    StartDictionary,
    EndCollection,
    Boolean,
    Data,
    Date,
    Integer,
    Real,
    String,
    Uid,

    ValueOrStartCollection,
    DictionaryKeyOrEndCollection,
}

impl Error {
    /// Returns true if this error was caused by a failure to read or write bytes on an IO stream.
    pub fn is_io(&self) -> bool {
        self.as_io().is_some()
    }

    /// Returns true if this error was caused by prematurely reaching the end of the input data.
    pub fn is_eof(&self) -> bool {
        if let ErrorKind::UnexpectedEof = self.inner.kind {
            true
        } else {
            false
        }
    }

    /// Returns the underlying error if it was caused by a failure to read or write bytes on an IO
    /// stream.
    pub fn as_io(&self) -> Option<&io::Error> {
        if let ErrorKind::Io(err) = &self.inner.kind {
            Some(err)
        } else {
            None
        }
    }

    /// Returns the underlying error if it was caused by a failure to read or write bytes on an IO
    /// stream or `self` if it was not.
    pub fn into_io(self) -> Result<io::Error, Self> {
        if let ErrorKind::Io(err) = self.inner.kind {
            Ok(err)
        } else {
            Err(self)
        }
    }
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match &self.inner.kind {
            ErrorKind::Io(err) => Some(err),
            _ => None,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if let Some(position) = &self.inner.file_position {
            write!(f, "{:?} ({})", &self.inner.kind, position)
        } else {
            fmt::Debug::fmt(&self.inner.kind, f)
        }
    }
}

impl fmt::Display for FilePosition {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            FilePosition::LineColumn(line, column) => {
                write!(f, "line {}, column {}", line, column)
            }
            FilePosition::Offset(offset) => {
                write!(f, "offset {}", offset)
            }
        }
    }
}

impl ErrorKind {
    pub fn with_byte_offset(self, offset: u64) -> Error {
        self.with_position(FilePosition::Offset(offset))
    }

    pub fn with_position(self, pos: FilePosition) -> Error {
        Error {
            inner: Box::new(ErrorImpl {
                kind: self,
                file_position: Some(pos),
            }),
        }
    }

    pub fn without_position(self) -> Error {
        Error {
            inner: Box::new(ErrorImpl {
                kind: self,
                file_position: None,
            }),
        }
    }
}

impl EventKind {
    pub fn of_event(event: &Event) -> EventKind {
        match event {
            Event::StartArray(_) => EventKind::StartArray,
            Event::StartDictionary(_) => EventKind::StartDictionary,
            Event::EndCollection => EventKind::EndCollection,
            Event::Boolean(_) => EventKind::Boolean,
            Event::Data(_) => EventKind::Data,
            Event::Date(_) => EventKind::Date,
            Event::Integer(_) => EventKind::Integer,
            Event::Real(_) => EventKind::Real,
            Event::String(_) => EventKind::String,
            Event::Uid(_) => EventKind::Uid,
        }
    }
}

impl fmt::Display for EventKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            EventKind::StartArray => "StartArray",
            EventKind::StartDictionary => "StartDictionary",
            EventKind::EndCollection => "EndCollection",
            EventKind::Boolean => "Boolean",
            EventKind::Data => "Data",
            EventKind::Date => "Date",
            EventKind::Integer => "Integer",
            EventKind::Real => "Real",
            EventKind::String => "String",
            EventKind::Uid => "Uid",
            EventKind::ValueOrStartCollection => "value or start collection",
            EventKind::DictionaryKeyOrEndCollection => "dictionary key or end collection",
        }
        .fmt(f)
    }
}

pub(crate) fn from_io_without_position(err: io::Error) -> Error {
    ErrorKind::Io(err).without_position()
}

pub(crate) fn unexpected_event_type(expected: EventKind, found: &Event) -> Error {
    let found = EventKind::of_event(&found);
    ErrorKind::UnexpectedEventType { expected, found }.without_position()
}
