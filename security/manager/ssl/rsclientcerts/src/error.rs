/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fmt;

/// Helper macro to create an Error that knows which file and line it occurred
/// on. Can optionally have some extra information as a String.
#[macro_export]
macro_rules! error_here {
    ($error_type:expr) => {
        Error::new($error_type, file!(), line!(), None)
    };
    ($error_type:expr, $info:expr) => {
        Error::new($error_type, file!(), line!(), Some($info))
    };
}

/// Error type for identifying errors in this crate. Use the error_here! macro
/// to instantiate.
#[derive(Debug)]
pub struct Error {
    typ: ErrorType,
    file: &'static str,
    line: u32,
    info: Option<String>,
}

impl Error {
    pub fn new(typ: ErrorType, file: &'static str, line: u32, info: Option<String>) -> Error {
        Error {
            typ,
            file,
            line,
            info,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if let Some(info) = &self.info {
            write!(f, "{} at {}:{} ({})", self.typ, self.file, self.line, info)
        } else {
            write!(f, "{} at {}:{}", self.typ, self.file, self.line)
        }
    }
}

impl Clone for Error {
    fn clone(&self) -> Self {
        Error {
            typ: self.typ,
            file: self.file,
            line: self.line,
            info: self.info.as_ref().cloned(),
        }
    }

    fn clone_from(&mut self, source: &Self) {
        self.typ = source.typ;
        self.file = source.file;
        self.line = source.line;
        self.info = source.info.as_ref().cloned();
    }
}

#[derive(Copy, Clone, Debug)]
pub enum ErrorType {
    /// An error in an external library or resource.
    ExternalError,
    /// Unexpected extra input (e.g. in an ASN.1 encoding).
    ExtraInput,
    /// Invalid argument.
    InvalidArgument,
    /// Invalid data input.
    InvalidInput,
    /// An internal library failure (e.g. an expected invariant failed).
    LibraryFailure,
    /// Truncated input (e.g. in an ASN.1 encoding).
    TruncatedInput,
    /// Unsupported input.
    UnsupportedInput,
    /// A given value could not be represented in the type used for it.
    ValueTooLarge,
}

impl fmt::Display for ErrorType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let error_type_str = match self {
            ErrorType::ExternalError => "ExternalError",
            ErrorType::ExtraInput => "ExtraInput",
            ErrorType::InvalidArgument => "InvalidArgument",
            ErrorType::InvalidInput => "InvalidInput",
            ErrorType::LibraryFailure => "LibraryFailure",
            ErrorType::TruncatedInput => "TruncatedInput",
            ErrorType::UnsupportedInput => "UnsupportedInput",
            ErrorType::ValueTooLarge => "ValueTooLarge",
        };
        write!(f, "{}", error_type_str)
    }
}
