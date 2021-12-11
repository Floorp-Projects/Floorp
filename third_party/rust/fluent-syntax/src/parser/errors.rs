use std::ops::Range;
use thiserror::Error;

/// Error containing information about an error encountered by the Fluent Parser.
///
/// Errors in Fluent Parser are non-fatal, and the syntax has been
/// designed to allow for strong recovery.
///
/// In result [`ParserError`] is designed to point at the slice of
/// the input that is most likely to be a complete fragment from after
/// the end of a valid entry, to the start of the next valid entry, with
/// the invalid syntax in the middle.
///
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
/// key1 = Value 1
///
/// g@Rb@ge = #2y ds
///
/// key2 = Value 2
///
/// "#;
///
/// let (resource, errors) = parser::parse_runtime(ftl)
///     .expect_err("Resource should contain errors.");
///
/// assert_eq!(
///     errors,
///     vec![
///         parser::ParserError {
///             pos: 18..19,
///             slice: Some(17..35),
///             kind: parser::ErrorKind::ExpectedToken('=')
///         }
///     ]
/// );
///
/// assert_eq!(
///     resource.body[0],
///     ast::Entry::Message(
///         ast::Message {
///             id: ast::Identifier {
///                 name: "key1"
///             },
///             value: Some(ast::Pattern {
///                 elements: vec![
///                     ast::PatternElement::TextElement {
///                         value: "Value 1"
///                     },
///                 ]
///             }),
///             attributes: vec![],
///             comment: None,
///         }
///     ),
/// );
///
/// assert_eq!(
///     resource.body[1],
///     ast::Entry::Junk {
///         content: "g@Rb@ge = #2y ds\n\n"
///     }
/// );
///
/// assert_eq!(
///     resource.body[2],
///     ast::Entry::Message(
///         ast::Message {
///             id: ast::Identifier {
///                 name: "key2"
///             },
///             value: Some(ast::Pattern {
///                 elements: vec![
///                     ast::PatternElement::TextElement {
///                         value: "Value 2"
///                     },
///                 ]
///             }),
///             attributes: vec![],
///             comment: None,
///         }
///     ),
/// );
/// ```
///
/// The information contained in the `ParserError` should allow the tooling
/// to display rich contextual annotations of the error slice, using
/// crates such as `annotate-snippers`.
#[derive(Error, Debug, PartialEq, Clone)]
#[error("{}", self.kind)]
pub struct ParserError {
    /// Precise location of where the parser encountered the error.
    pub pos: Range<usize>,
    /// Slice of the input from the end of the last valid entry to the beginning
    /// of the next valid entry with the invalid syntax in the middle.
    pub slice: Option<Range<usize>>,
    /// The type of the error that the parser encountered.
    pub kind: ErrorKind,
}

macro_rules! error {
    ($kind:expr, $start:expr) => {{
        Err(ParserError {
            pos: $start..$start + 1,
            slice: None,
            kind: $kind,
        })
    }};
    ($kind:expr, $start:expr, $end:expr) => {{
        Err(ParserError {
            pos: $start..$end,
            slice: None,
            kind: $kind,
        })
    }};
}

/// Kind of an error associated with the [`ParserError`].
#[derive(Error, Debug, PartialEq, Clone)]
pub enum ErrorKind {
    #[error("Expected a token starting with \"{0}\"")]
    ExpectedToken(char),
    #[error("Expected one of \"{range}\"")]
    ExpectedCharRange { range: String },
    #[error("Expected a message field for \"{entry_id}\"")]
    ExpectedMessageField { entry_id: String },
    #[error("Expected a term field for \"{entry_id}\"")]
    ExpectedTermField { entry_id: String },
    #[error("Callee is not allowed here")]
    ForbiddenCallee,
    #[error("The select expression must have a default variant")]
    MissingDefaultVariant,
    #[error("Expected a value")]
    MissingValue,
    #[error("A select expression can only have one default variant")]
    MultipleDefaultVariants,
    #[error("Message references can't be used as a selector")]
    MessageReferenceAsSelector,
    #[error("Term references can't be used as a selector")]
    TermReferenceAsSelector,
    #[error("Message attributes can't be used as a selector")]
    MessageAttributeAsSelector,
    #[error("Term attributes can't be used as a selector")]
    TermAttributeAsPlaceable,
    #[error("Unterminated string literal")]
    UnterminatedStringLiteral,
    #[error("Positional arguments must come before named arguments")]
    PositionalArgumentFollowsNamed,
    #[error("The \"{0}\" argument appears twice")]
    DuplicatedNamedArgument(String),
    #[error("Unknown escape sequence")]
    UnknownEscapeSequence(String),
    #[error("Invalid unicode escape sequence, \"{0}\"")]
    InvalidUnicodeEscapeSequence(String),
    #[error("Unbalanced closing brace")]
    UnbalancedClosingBrace,
    #[error("Expected an inline expression")]
    ExpectedInlineExpression,
    #[error("Expected a simple expression as selector")]
    ExpectedSimpleExpressionAsSelector,
    #[error("Expected a string or number literal")]
    ExpectedLiteral,
}
