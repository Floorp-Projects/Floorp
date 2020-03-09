use crate::stack_value_generated::AstError;
use crate::DeclarationKind;
use crate::Token;
use std::{convert::Infallible, error::Error, fmt};

#[derive(Debug)]
pub enum ParseError {
    // Lexical errors
    IllegalCharacter(char),
    InvalidEscapeSequence,
    UnterminatedString,
    UnterminatedRegExp,
    UnterminatedMultiLineComment,
    LexerError,
    NoLineTerminatorHereExpectedToken,
    ParserCannotUnpackToken,

    // Generic syntax errors
    NotImplemented(&'static str),
    SyntaxError(Token),
    UnexpectedEnd,
    InvalidAssignmentTarget,
    InvalidParameter,
    InvalidIdentifier(String, usize),
    AstError(String),

    // Destructuring errors
    ArrayPatternWithNonFinalRest,
    ArrayBindingPatternWithInvalidRest,
    ObjectPatternWithMethod,
    ObjectPatternWithNonFinalRest,
    ObjectBindingPatternWithInvalidRest,

    // 14.8 Async arrow function definitions
    ArrowHeadInvalid,
    ArrowParametersWithNonFinalRest,

    DuplicateBinding(String, DeclarationKind, usize, DeclarationKind, usize),
    DuplicateExport(String, usize, usize),
    MissingExport(String, usize),

    // Annex B. FunctionDeclarations in IfStatement Statement Clauses
    // https://tc39.es/ecma262/#sec-functiondeclarations-in-ifstatement-statement-clauses
    FunctionDeclInSingleStatement,
    LabelledFunctionDeclInSingleStatement,
}

impl ParseError {
    pub fn message(&self) -> String {
        match self {
            ParseError::IllegalCharacter(c) => format!("illegal character: {:?}", c),
            ParseError::InvalidEscapeSequence => format!("invalid escape sequence"),
            ParseError::UnterminatedString => format!("unterminated string literal"),
            ParseError::UnterminatedRegExp => format!("unterminated regexp literal"),
            ParseError::UnterminatedMultiLineComment => format!("unterminated multiline comment"),
            ParseError::LexerError => format!("lexical error"),
            ParseError::NoLineTerminatorHereExpectedToken => format!(
                "no-line-terminator-here expects a token"
            ),
            ParseError::ParserCannotUnpackToken => format!("cannot unpack token"),
            ParseError::NotImplemented(message) => format!("not implemented: {}", message),
            ParseError::SyntaxError(token) => format!("syntax error on: {:?}", token),
            ParseError::UnexpectedEnd => format!("unexpected end of input"),
            ParseError::InvalidAssignmentTarget => format!("invalid left-hand side of assignment"),
            ParseError::InvalidParameter => format!("invalid parameter"),
            ParseError::InvalidIdentifier(name, _) => {
                format!("invalid identifier {}", name)
            }
            ParseError::AstError(ast_error) => format!("{}", ast_error),
            ParseError::ArrayPatternWithNonFinalRest => {
                format!("array patterns can have a rest element (`...x`) only at the end")
            }
            ParseError::ArrayBindingPatternWithInvalidRest => format!(
                "the expression after `...` in this array pattern must be a single identifier"
            ),
            ParseError::ObjectPatternWithMethod => format!("object patterns can't have methods"),
            ParseError::ObjectPatternWithNonFinalRest => {
                format!("object patterns can have a rest element (`...x`) only at the end")
            }
            ParseError::ObjectBindingPatternWithInvalidRest => format!(
                "the expression after `...` in this object pattern must be a single identifier"
            ),
            ParseError::ArrowHeadInvalid => format!(
                "unexpected `=>` after function call (parentheses around the arrow function may help)"
            ),
            ParseError::ArrowParametersWithNonFinalRest => format!(
                "arrow function parameters can have a rest element (`...x`) only at the end"
            ),
            ParseError::DuplicateBinding(name, kind1, _, kind2, _) => format!(
                "redeclaration of {} '{}' with {}",
                kind1.to_str(),
                name,
                kind2.to_str(),
            ),
            ParseError::DuplicateExport(name, _, _) => format!(
                "duplicate export name '{}'",
                name,
            ),
            ParseError::MissingExport(name, _) => format!(
                "local binding for export '{}' not found",
                name,
            ),
            ParseError::FunctionDeclInSingleStatement => format!(
                "function declarations can't appear in single-statement context"
            ),
            ParseError::LabelledFunctionDeclInSingleStatement => format!(
                "functions can only be labelled inside blocks"
            ),
        }
    }
}

impl PartialEq for ParseError {
    fn eq(&self, other: &ParseError) -> bool {
        format!("{:?}", self) == format!("{:?}", other)
    }
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.message())
    }
}

impl From<Infallible> for ParseError {
    fn from(err: Infallible) -> ParseError {
        match err {}
    }
}

impl From<AstError> for ParseError {
    fn from(err: AstError) -> ParseError {
        ParseError::AstError(err)
    }
}

impl Error for ParseError {}

pub type Result<T> = std::result::Result<T, ParseError>;
