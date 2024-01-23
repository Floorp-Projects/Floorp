/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(feature = "serialize")]
use serde::ser::{Serialize, SerializeStruct, Serializer};
use std::error;
use std::error::Error;
use std::fmt;
extern crate url;
use address::AddressType;
use std::num::ParseFloatError;
use std::num::ParseIntError;

#[derive(Debug, Clone)]
pub enum SdpParserInternalError {
    UnknownAddressType(String),
    AddressTypeMismatch {
        found: AddressType,
        expected: AddressType,
    },
    Generic(String),
    Unsupported(String),
    Integer(ParseIntError),
    Float(ParseFloatError),
    Domain(url::ParseError),
    IpAddress(std::net::AddrParseError),
}

const INTERNAL_ERROR_MESSAGE_UNKNOWN_ADDRESS_TYPE: &str = "Unknown address type";
const INTERNAL_ERROR_MESSAGE_ADDRESS_TYPE_MISMATCH: &str =
    "Address is of a different type(1) than declared(2)";

impl fmt::Display for SdpParserInternalError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SdpParserInternalError::UnknownAddressType(ref unknown) => write!(
                f,
                "{INTERNAL_ERROR_MESSAGE_UNKNOWN_ADDRESS_TYPE}: {unknown}"
            ),
            SdpParserInternalError::AddressTypeMismatch { found, expected } => write!(
                f,
                "{INTERNAL_ERROR_MESSAGE_ADDRESS_TYPE_MISMATCH}: {found}, {expected}"
            ),
            SdpParserInternalError::Generic(ref message) => write!(f, "Parsing error: {message}"),
            SdpParserInternalError::Unsupported(ref message) => {
                write!(f, "Unsupported parsing error: {message}")
            }
            SdpParserInternalError::Integer(ref error) => {
                write!(f, "Integer parsing error: {error}")
            }
            SdpParserInternalError::Float(ref error) => write!(f, "Float parsing error: {error}"),
            SdpParserInternalError::Domain(ref error) => {
                write!(f, "Domain name parsing error: {error}")
            }
            SdpParserInternalError::IpAddress(ref error) => {
                write!(f, "IP address parsing error: {error}")
            }
        }
    }
}

impl Error for SdpParserInternalError {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match *self {
            SdpParserInternalError::Integer(ref error) => Some(error),
            SdpParserInternalError::Float(ref error) => Some(error),
            SdpParserInternalError::Domain(ref error) => Some(error),
            SdpParserInternalError::IpAddress(ref error) => Some(error),
            // Can't tell much more about our internal errors
            _ => None,
        }
    }
}

#[derive(Debug, Clone)]
pub enum SdpParserError {
    Line {
        error: SdpParserInternalError,
        line: String,
        line_number: usize,
    },
    Unsupported {
        error: SdpParserInternalError,
        line: String,
        line_number: usize,
    },
    Sequence {
        message: String,
        line_number: usize,
    },
}

#[cfg(feature = "serialize")]
impl Serialize for SdpParserError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut state = serializer.serialize_struct(
            "error",
            match self {
                SdpParserError::Sequence { .. } => 3,
                _ => 4,
            },
        )?;
        match self {
            SdpParserError::Line { error, line, .. } => {
                state.serialize_field("type", "Line")?;
                state.serialize_field("message", &format!("{error}"))?;
                state.serialize_field("line", line)?
            }
            SdpParserError::Unsupported { error, line, .. } => {
                state.serialize_field("type", "Unsupported")?;
                state.serialize_field("message", &format!("{error}"))?;
                state.serialize_field("line", line)?
            }
            SdpParserError::Sequence { message, .. } => {
                state.serialize_field("type", "Sequence")?;
                state.serialize_field("message", message)?;
            }
        };
        state.serialize_field(
            "line_number",
            &match *self {
                SdpParserError::Line { line_number, .. } => line_number,
                SdpParserError::Unsupported { line_number, .. } => line_number,
                SdpParserError::Sequence { line_number, .. } => line_number,
            },
        )?;
        state.end()
    }
}

impl fmt::Display for SdpParserError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            SdpParserError::Line {
                error,
                line,
                line_number,
            } => write!(f, "Line error: {error} in line({line_number}): {line}"),
            SdpParserError::Unsupported {
                error,
                line,
                line_number,
            } => write!(f, "Unsupported: {error} in line({line_number}): {line}",),
            SdpParserError::Sequence {
                message,
                line_number,
            } => write!(f, "Sequence error in line({line_number}): {message}"),
        }
    }
}

impl Error for SdpParserError {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            SdpParserError::Line { error, .. } | SdpParserError::Unsupported { error, .. } => {
                Some(error)
            }
            // Can't tell much more about our internal errors
            _ => None,
        }
    }
}

impl From<ParseIntError> for SdpParserInternalError {
    fn from(err: ParseIntError) -> SdpParserInternalError {
        SdpParserInternalError::Integer(err)
    }
}

impl From<url::ParseError> for SdpParserInternalError {
    fn from(err: url::ParseError) -> SdpParserInternalError {
        SdpParserInternalError::Domain(err)
    }
}

impl From<std::net::AddrParseError> for SdpParserInternalError {
    fn from(err: std::net::AddrParseError) -> SdpParserInternalError {
        SdpParserInternalError::IpAddress(err)
    }
}

impl From<ParseFloatError> for SdpParserInternalError {
    fn from(err: ParseFloatError) -> SdpParserInternalError {
        SdpParserInternalError::Float(err)
    }
}

#[cfg(test)]
#[path = "./error_tests.rs"]
mod tests;
