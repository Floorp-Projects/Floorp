use std::error;
use std::fmt::{self, Debug, Display};
use std::io;
use std::result;
use std::str;
use std::string;

use yaml_rust::emitter;
use yaml_rust::scanner::{self, Marker, ScanError};

use serde::{de, ser};

use path::Path;
use private;

/// This type represents all possible errors that can occur when serializing or
/// deserializing YAML data.
pub struct Error(Box<ErrorImpl>);

/// Alias for a `Result` with the error type `serde_yaml::Error`.
pub type Result<T> = result::Result<T, Error>;

/// This type represents all possible errors that can occur when serializing or
/// deserializing a value using YAML.
#[derive(Debug)]
pub enum ErrorImpl {
    Message(String, Option<Pos>),

    Emit(emitter::EmitError),
    Scan(scanner::ScanError),
    Io(io::Error),
    Utf8(str::Utf8Error),
    FromUtf8(string::FromUtf8Error),

    EndOfStream,
    MoreThanOneDocument,
    RecursionLimitExceeded,
}

#[derive(Debug)]
pub struct Pos {
    marker: Marker,
    path: String,
}

/// This type represents the location that an error occured.
#[derive(Debug)]
pub struct Location {
    index: usize,
    line: usize,
    column: usize,
}

impl Location {
    /// The byte index of the error
    pub fn index(&self) -> usize {
        self.index
    }

    /// The line of the error
    pub fn line(&self) -> usize {
        self.line
    }

    /// The column of the error
    pub fn column(&self) -> usize {
        self.column
    }

    // This is to keep decoupled with the yaml crate
    #[doc(hidden)]
    fn from_marker(marker: &Marker) -> Self {
        Location {
            // `col` returned from the `yaml` crate is 0-indexed but all error messages add + 1 to this value
            column: marker.col() + 1,
            index: marker.index(),
            line: marker.line(),
        }
    }
}

impl Error {
    /// Returns the Location from the error if one exists.
    ///
    /// Not all types of errors have a location so this can return `None`.
    ///
    /// # Examples
    ///
    /// ```edition2018
    /// # use serde_yaml::{Value, Error};
    /// #
    /// // The `@` character as the first character makes this invalid yaml
    /// let invalid_yaml: Result<Value, Error> = serde_yaml::from_str("@invalid_yaml");
    ///
    /// let location = invalid_yaml.unwrap_err().location().unwrap();
    ///
    /// assert_eq!(location.line(), 1);
    /// assert_eq!(location.column(), 1);
    /// ```
    pub fn location(&self) -> Option<Location> {
        match *self.0 {
            ErrorImpl::Message(_, Some(ref pos)) => Some(Location::from_marker(&pos.marker)),
            ErrorImpl::Scan(ref scan) => Some(Location::from_marker(scan.marker())),
            _ => None,
        }
    }
}

impl private {
    pub fn error_end_of_stream() -> Error {
        Error(Box::new(ErrorImpl::EndOfStream))
    }

    pub fn error_more_than_one_document() -> Error {
        Error(Box::new(ErrorImpl::MoreThanOneDocument))
    }

    pub fn error_io(err: io::Error) -> Error {
        Error(Box::new(ErrorImpl::Io(err)))
    }

    pub fn error_emitter(err: emitter::EmitError) -> Error {
        Error(Box::new(ErrorImpl::Emit(err)))
    }

    pub fn error_scanner(err: scanner::ScanError) -> Error {
        Error(Box::new(ErrorImpl::Scan(err)))
    }

    pub fn error_str_utf8(err: str::Utf8Error) -> Error {
        Error(Box::new(ErrorImpl::Utf8(err)))
    }

    pub fn error_string_utf8(err: string::FromUtf8Error) -> Error {
        Error(Box::new(ErrorImpl::FromUtf8(err)))
    }

    pub fn error_recursion_limit_exceeded() -> Error {
        Error(Box::new(ErrorImpl::RecursionLimitExceeded))
    }

    pub fn fix_marker(mut error: Error, marker: Marker, path: Path) -> Error {
        if let ErrorImpl::Message(_, ref mut none @ None) = *error.0.as_mut() {
            *none = Some(Pos {
                marker: marker,
                path: path.to_string(),
            });
        }
        error
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        match *self.0 {
            ErrorImpl::Message(ref msg, _) => msg,
            ErrorImpl::Emit(_) => "emit error",
            ErrorImpl::Scan(_) => "scan error",
            ErrorImpl::Io(ref err) => err.description(),
            ErrorImpl::Utf8(ref err) => err.description(),
            ErrorImpl::FromUtf8(ref err) => err.description(),
            ErrorImpl::EndOfStream => "EOF while parsing a value",
            ErrorImpl::MoreThanOneDocument => {
                "deserializing from YAML containing more than one document is not supported"
            }
            ErrorImpl::RecursionLimitExceeded => "recursion limit exceeded",
        }
    }

    fn cause(&self) -> Option<&error::Error> {
        match *self.0 {
            ErrorImpl::Scan(ref err) => Some(err),
            ErrorImpl::Io(ref err) => Some(err),
            ErrorImpl::Utf8(ref err) => Some(err),
            ErrorImpl::FromUtf8(ref err) => Some(err),
            _ => None,
        }
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self.0 {
            ErrorImpl::Message(ref msg, None) => Display::fmt(msg, f),
            ErrorImpl::Message(ref msg, Some(Pos { marker, ref path })) => {
                if path == "." {
                    write!(f, "{}", ScanError::new(marker, msg))
                } else {
                    write!(f, "{}: {}", path, ScanError::new(marker, msg))
                }
            }
            ErrorImpl::Emit(emitter::EmitError::FmtError(_)) => f.write_str("yaml-rust fmt error"),
            ErrorImpl::Emit(emitter::EmitError::BadHashmapKey) => f.write_str("bad hash map key"),
            ErrorImpl::Scan(ref err) => Display::fmt(err, f),
            ErrorImpl::Io(ref err) => Display::fmt(err, f),
            ErrorImpl::Utf8(ref err) => Display::fmt(err, f),
            ErrorImpl::FromUtf8(ref err) => Display::fmt(err, f),
            ErrorImpl::EndOfStream => f.write_str("EOF while parsing a value"),
            ErrorImpl::MoreThanOneDocument => f.write_str(
                "deserializing from YAML containing more than one document is not supported",
            ),
            ErrorImpl::RecursionLimitExceeded => f.write_str("recursion limit exceeded"),
        }
    }
}

// Remove two layers of verbosity from the debug representation. Humans often
// end up seeing this representation because it is what unwrap() shows.
impl Debug for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        match *self.0 {
            ErrorImpl::Message(ref msg, ref pos) => formatter
                .debug_tuple("Message")
                .field(msg)
                .field(pos)
                .finish(),
            ErrorImpl::Emit(ref emit) => formatter.debug_tuple("Emit").field(emit).finish(),
            ErrorImpl::Scan(ref scan) => formatter.debug_tuple("Scan").field(scan).finish(),
            ErrorImpl::Io(ref io) => formatter.debug_tuple("Io").field(io).finish(),
            ErrorImpl::Utf8(ref utf8) => formatter.debug_tuple("Utf8").field(utf8).finish(),
            ErrorImpl::FromUtf8(ref from_utf8) => {
                formatter.debug_tuple("FromUtf8").field(from_utf8).finish()
            }
            ErrorImpl::EndOfStream => formatter.debug_tuple("EndOfStream").finish(),
            ErrorImpl::MoreThanOneDocument => formatter.debug_tuple("MoreThanOneDocument").finish(),
            ErrorImpl::RecursionLimitExceeded => {
                formatter.debug_tuple("RecursionLimitExceeded").finish()
            }
        }
    }
}

impl ser::Error for Error {
    fn custom<T: Display>(msg: T) -> Self {
        Error(Box::new(ErrorImpl::Message(msg.to_string(), None)))
    }
}

impl de::Error for Error {
    fn custom<T: Display>(msg: T) -> Self {
        Error(Box::new(ErrorImpl::Message(msg.to_string(), None)))
    }
}
