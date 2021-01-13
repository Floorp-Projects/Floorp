use crate::resolver::ResolverError;
use fluent_syntax::parser::ParserError;
use std::error::Error;

#[derive(Debug, PartialEq, Clone)]
pub enum EntryKind {
    Message,
    Term,
    Function,
}

impl std::fmt::Display for EntryKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Message => f.write_str("message"),
            Self::Term => f.write_str("term"),
            Self::Function => f.write_str("function"),
        }
    }
}

#[derive(Debug, PartialEq, Clone)]
pub enum FluentError {
    Overriding { kind: EntryKind, id: String },
    ParserError(ParserError),
    ResolverError(ResolverError),
}

impl std::fmt::Display for FluentError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Overriding { kind, id } => {
                write!(f, "Attempt to override an existing {}: \"{}\".", kind, id)
            }
            Self::ParserError(err) => write!(f, "Parser error: {}", err),
            Self::ResolverError(err) => write!(f, "Resolver error: {}", err),
        }
    }
}

impl Error for FluentError {}

impl From<ResolverError> for FluentError {
    fn from(error: ResolverError) -> Self {
        Self::ResolverError(error)
    }
}

impl From<ParserError> for FluentError {
    fn from(error: ParserError) -> Self {
        Self::ParserError(error)
    }
}
