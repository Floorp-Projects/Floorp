use crate::resolve::ResolverError;
use fluent_syntax::parser::ParserError;

#[derive(Debug, PartialEq)]
pub enum FluentError {
    Overriding { kind: &'static str, id: String },
    ParserError(ParserError),
    ResolverError(ResolverError),
}

impl From<ResolverError> for FluentError {
    fn from(error: ResolverError) -> Self {
        FluentError::ResolverError(error)
    }
}

impl From<ParserError> for FluentError {
    fn from(error: ParserError) -> Self {
        FluentError::ParserError(error)
    }
}
