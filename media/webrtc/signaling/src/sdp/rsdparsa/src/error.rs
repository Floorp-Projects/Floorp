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
                "{}: {}",
                INTERNAL_ERROR_MESSAGE_UNKNOWN_ADDRESS_TYPE, unknown
            ),
            SdpParserInternalError::AddressTypeMismatch { found, expected } => write!(
                f,
                "{}: {}, {}",
                INTERNAL_ERROR_MESSAGE_ADDRESS_TYPE_MISMATCH, found, expected
            ),
            SdpParserInternalError::Generic(ref message) => write!(f, "Parsing error: {}", message),
            SdpParserInternalError::Unsupported(ref message) => {
                write!(f, "Unsupported parsing error: {}", message)
            }
            SdpParserInternalError::Integer(ref error) => {
                write!(f, "Integer parsing error: {}", error)
            }
            SdpParserInternalError::Float(ref error) => write!(f, "Float parsing error: {}", error),
            SdpParserInternalError::Domain(ref error) => {
                write!(f, "Domain name parsing error: {}", error)
            }
            SdpParserInternalError::IpAddress(ref error) => {
                write!(f, "IP address parsing error: {}", error)
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
            match *self {
                SdpParserError::Sequence { .. } => 3,
                _ => 4,
            },
        )?;
        match *self {
            SdpParserError::Line {
                ref error,
                ref line,
                ..
            } => {
                state.serialize_field("type", "Line")?;
                state.serialize_field("message", &format!("{}", error))?;
                state.serialize_field("line", &line)?
            }
            SdpParserError::Unsupported {
                ref error,
                ref line,
                ..
            } => {
                state.serialize_field("type", "Unsupported")?;
                state.serialize_field("message", &format!("{}", error))?;
                state.serialize_field("line", &line)?
            }
            SdpParserError::Sequence { ref message, .. } => {
                state.serialize_field("type", "Sequence")?;
                state.serialize_field("message", &message)?;
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
        match *self {
            SdpParserError::Line {
                ref error,
                ref line,
                ref line_number,
            } => write!(
                f,
                "Line error: {} in line({}): {}",
                error, line_number, line
            ),
            SdpParserError::Unsupported {
                ref error,
                ref line,
                ref line_number,
            } => write!(
                f,
                "Unsupported: {} in line({}): {}",
                error, line_number, line
            ),
            SdpParserError::Sequence {
                ref message,
                ref line_number,
            } => write!(f, "Sequence error in line({}): {}", line_number, message),
        }
    }
}

impl Error for SdpParserError {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match *self {
            SdpParserError::Line { ref error, .. }
            | SdpParserError::Unsupported { ref error, .. } => Some(error),
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
mod tests {
    use super::*;
    use address::Address;
    use std::str::FromStr;
    #[test]
    fn test_sdp_parser_internal_error_unknown_address_type() {
        let error = SdpParserInternalError::UnknownAddressType("foo".to_string());
        assert_eq!(
            format!("{}", error),
            format!("{}: {}", INTERNAL_ERROR_MESSAGE_UNKNOWN_ADDRESS_TYPE, "foo")
        );
        assert!(error.source().is_none());
    }
    #[test]
    fn test_sdp_parser_internal_error_address_type_mismatch() {
        let error = SdpParserInternalError::AddressTypeMismatch {
            found: AddressType::IpV4,
            expected: AddressType::IpV6,
        };
        assert_eq!(
            format!("{}", error),
            format!(
                "{}: {}, {}",
                INTERNAL_ERROR_MESSAGE_ADDRESS_TYPE_MISMATCH,
                AddressType::IpV4,
                AddressType::IpV6
            )
        );
        assert!(error.source().is_none());
    }

    #[test]
    fn test_sdp_parser_internal_error_generic() {
        let generic = SdpParserInternalError::Generic("generic message".to_string());
        assert_eq!(format!("{}", generic), "Parsing error: generic message");
        assert!(generic.source().is_none());
    }

    #[test]
    fn test_sdp_parser_internal_error_unsupported() {
        let unsupported =
            SdpParserInternalError::Unsupported("unsupported internal message".to_string());
        assert_eq!(
            format!("{}", unsupported),
            "Unsupported parsing error: unsupported internal message"
        );
        assert!(unsupported.source().is_none());
    }

    #[test]
    fn test_sdp_parser_internal_error_integer() {
        let v = "12a";
        let integer = v.parse::<u64>();
        assert!(integer.is_err());
        let int_err = SdpParserInternalError::Integer(integer.err().unwrap());
        assert_eq!(
            format!("{}", int_err),
            "Integer parsing error: invalid digit found in string"
        );
        assert!(!int_err.source().is_none());
    }

    #[test]
    fn test_sdp_parser_internal_error_float() {
        let v = "12.2a";
        let float = v.parse::<f32>();
        assert!(float.is_err());
        let int_err = SdpParserInternalError::Float(float.err().unwrap());
        assert_eq!(
            format!("{}", int_err),
            "Float parsing error: invalid float literal"
        );
        assert!(!int_err.source().is_none());
    }

    #[test]
    fn test_sdp_parser_internal_error_address() {
        let v = "127.0.0.500";
        let addr_err = Address::from_str(v).err().unwrap();
        assert_eq!(
            format!("{}", addr_err),
            "Domain name parsing error: invalid IPv4 address"
        );
        assert!(!addr_err.source().is_none());
    }

    #[test]
    fn test_sdp_parser_error_line() {
        let line1 = SdpParserError::Line {
            error: SdpParserInternalError::Generic("test message".to_string()),
            line: "test line".to_string(),
            line_number: 13,
        };
        assert_eq!(
            format!("{}", line1),
            "Line error: Parsing error: test message in line(13): test line"
        );
        assert!(line1.source().is_some());
    }

    #[test]
    fn test_sdp_parser_error_unsupported() {
        let unsupported1 = SdpParserError::Unsupported {
            error: SdpParserInternalError::Generic("unsupported value".to_string()),
            line: "unsupported line".to_string(),
            line_number: 21,
        };
        assert_eq!(
            format!("{}", unsupported1),
            "Unsupported: Parsing error: unsupported value in line(21): unsupported line"
        );
        assert!(unsupported1.source().is_some());
    }

    #[test]
    fn test_sdp_parser_error_sequence() {
        let sequence1 = SdpParserError::Sequence {
            message: "sequence message".to_string(),
            line_number: 42,
        };
        assert_eq!(
            format!("{}", sequence1),
            "Sequence error in line(42): sequence message"
        );
        assert!(sequence1.source().is_none());
    }
}
