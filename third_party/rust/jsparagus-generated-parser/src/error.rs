use crate::stack_value_generated::AstError;
use crate::DeclarationKind;
use crate::Token;
use std::{convert::Infallible, error::Error, fmt};

#[derive(Debug)]
pub enum ParseError<'alloc> {
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
    InvalidIdentifier(&'alloc str, usize),
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

    DuplicateBinding(&'alloc str, DeclarationKind, usize, DeclarationKind, usize),
    DuplicateExport(&'alloc str, usize, usize),
    MissingExport(&'alloc str, usize),

    // Labelled Statement Errors
    DuplicateLabel,
    BadContinue,
    ToughBreak,
    LabelNotFound,

    // Annex B. FunctionDeclarations in IfStatement Statement Clauses
    // https://tc39.es/ecma262/#sec-functiondeclarations-in-ifstatement-statement-clauses
    FunctionDeclInSingleStatement,
    LabelledFunctionDeclInSingleStatement,
}

impl<'alloc> ParseError<'alloc> {
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
            ParseError::DuplicateLabel => format!(
                "duplicate label"
            ),
            ParseError::BadContinue => format!(
                "continue must be inside loop"
            ),
            ParseError::ToughBreak => format!(
                "unlabeled break must be inside loop or switch"
            ),
            ParseError::LabelNotFound => format!(
                "label not found"
            ),
        }
    }
}

impl<'alloc> PartialEq for ParseError<'alloc> {
    fn eq(&self, other: &ParseError) -> bool {
        format!("{:?}", self) == format!("{:?}", other)
    }
}

impl<'alloc> fmt::Display for ParseError<'alloc> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.message())
    }
}

impl<'alloc> From<Infallible> for ParseError<'alloc> {
    fn from(err: Infallible) -> ParseError<'alloc> {
        match err {}
    }
}

impl<'alloc> From<AstError> for ParseError<'alloc> {
    fn from(err: AstError) -> ParseError<'alloc> {
        ParseError::AstError(err)
    }
}

impl<'alloc> Error for ParseError<'alloc> {}

pub type Result<'alloc, T> = std::result::Result<T, ParseError<'alloc>>;
