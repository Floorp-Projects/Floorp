use super::parser::ParserError;
use std::error::Error;
use std::fmt::{self, Display};

/// Enum with errors that can be returned by LanguageIdentifier.
#[derive(Debug, PartialEq)]
pub enum LanguageIdentifierError {
    /// An unknown error - currently covers all-but parser errors.
    Unknown,
    /// A parser error.
    ParserError(ParserError),
}

impl From<ParserError> for LanguageIdentifierError {
    fn from(error: ParserError) -> LanguageIdentifierError {
        LanguageIdentifierError::ParserError(error)
    }
}

impl Error for LanguageIdentifierError {}

impl Display for LanguageIdentifierError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            LanguageIdentifierError::Unknown => write!(f, "Unknown error"),
            LanguageIdentifierError::ParserError(p) => write!(f, "Parser error: {}", p),
        }
    }
}
