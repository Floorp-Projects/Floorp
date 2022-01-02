use super::{constants::ConstantSolvingError, token::TokenValue};
use crate::Span;
use pp_rs::token::PreprocessorError;
use std::borrow::Cow;
use thiserror::Error;

fn join_with_comma(list: &[ExpectedToken]) -> String {
    let mut string = "".to_string();
    for (i, val) in list.iter().enumerate() {
        string.push_str(&val.to_string());
        match i {
            i if i == list.len() - 1 => {}
            i if i == list.len() - 2 => string.push_str(" or "),
            _ => string.push_str(", "),
        }
    }
    string
}

/// One of the expected tokens returned in [`InvalidToken`](ErrorKind::InvalidToken)
#[derive(Debug, PartialEq)]
pub enum ExpectedToken {
    /// A specific token was expected
    Token(TokenValue),
    /// A type was expected
    TypeName,
    /// An identifier was expected
    Identifier,
    /// An integer literal was expected
    IntLiteral,
    /// A float literal was expected
    FloatLiteral,
    /// A boolean literal was expected
    BoolLiteral,
    /// The end of file was expected
    Eof,
}
impl From<TokenValue> for ExpectedToken {
    fn from(token: TokenValue) -> Self {
        ExpectedToken::Token(token)
    }
}
impl std::fmt::Display for ExpectedToken {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match *self {
            ExpectedToken::Token(ref token) => write!(f, "{:?}", token),
            ExpectedToken::TypeName => write!(f, "a type"),
            ExpectedToken::Identifier => write!(f, "identifier"),
            ExpectedToken::IntLiteral => write!(f, "integer literal"),
            ExpectedToken::FloatLiteral => write!(f, "float literal"),
            ExpectedToken::BoolLiteral => write!(f, "bool literal"),
            ExpectedToken::Eof => write!(f, "end of file"),
        }
    }
}

/// Information about the cause of an error
#[derive(Debug, Error)]
#[cfg_attr(test, derive(PartialEq))]
pub enum ErrorKind {
    /// Whilst parsing as encountered an unexpected EOF.
    #[error("Unexpected end of file")]
    EndOfFile,
    /// The shader specified an unsupported or invalid profile.
    #[error("Invalid profile: {0}")]
    InvalidProfile(String),
    /// The shader requested an unsupported or invalid version.
    #[error("Invalid version: {0}")]
    InvalidVersion(u64),
    /// Whilst parsing an unexpected token was encountered.
    ///
    /// A list of expected tokens is also returned.
    #[error("Expected {}, found {0:?}", join_with_comma(.1))]
    InvalidToken(TokenValue, Vec<ExpectedToken>),
    /// A specific feature is not yet implemented.
    ///
    /// To help prioritize work please open an issue in the github issue tracker
    /// if none exist already or react to the already existing one.
    #[error("Not implemented: {0}")]
    NotImplemented(&'static str),
    /// A reference to a variable that wasn't declared was used.
    #[error("Unknown variable: {0}")]
    UnknownVariable(String),
    /// A reference to a type that wasn't declared was used.
    #[error("Unknown type: {0}")]
    UnknownType(String),
    /// A reference to a non existant member of a type was made.
    #[error("Unknown field: {0}")]
    UnknownField(String),
    /// An unknown layout qualifier was used.
    ///
    /// If the qualifier does exist please open an issue in the github issue tracker
    /// if none exist already or react to the already existing one to help
    /// prioritize work.
    #[error("Unknown layout qualifier: {0}")]
    UnknownLayoutQualifier(String),
    /// A variable with the same name already exists in the current scope.
    #[cfg(feature = "glsl-validate")]
    #[error("Variable already declared: {0}")]
    VariableAlreadyDeclared(String),
    /// A semantic error was detected in the shader.
    #[error("{0}")]
    SemanticError(Cow<'static, str>),
    /// An error was returned by the preprocessor.
    #[error("{0:?}")]
    PreprocessorError(PreprocessorError),
}

impl From<ConstantSolvingError> for ErrorKind {
    fn from(err: ConstantSolvingError) -> Self {
        ErrorKind::SemanticError(err.to_string().into())
    }
}

/// Error returned during shader parsing
#[derive(Debug, Error)]
#[error("{kind}")]
#[cfg_attr(test, derive(PartialEq))]
pub struct Error {
    /// Holds the information about the error itself.
    pub kind: ErrorKind,
    /// Holds information about the range of the source code where the error happened.
    pub meta: Span,
}
