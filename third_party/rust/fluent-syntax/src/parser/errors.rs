use std::fmt::{self, Display, Formatter};

#[derive(Debug, PartialEq, Clone)]
pub struct ParserError {
    pub pos: (usize, usize),
    pub slice: Option<(usize, usize)>,
    pub kind: ErrorKind,
}

impl std::error::Error for ParserError {}

impl Display for ParserError {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.kind, f)
    }
}

macro_rules! error {
    ($kind:expr, $start:expr) => {{
        Err(ParserError {
            pos: ($start, $start + 1),
            slice: None,
            kind: $kind,
        })
    }};
    ($kind:expr, $start:expr, $end:expr) => {{
        Err(ParserError {
            pos: ($start, $end),
            slice: None,
            kind: $kind,
        })
    }};
}

#[derive(Debug, PartialEq, Clone)]
pub enum ErrorKind {
    Generic,
    ExpectedEntry,
    ExpectedToken(char),
    ExpectedCharRange { range: String },
    ExpectedMessageField { entry_id: String },
    ExpectedTermField { entry_id: String },
    ForbiddenWhitespace,
    ForbiddenCallee,
    ForbiddenKey,
    MissingDefaultVariant,
    MissingVariants,
    MissingValue,
    MissingVariantKey,
    MissingLiteral,
    MultipleDefaultVariants,
    MessageReferenceAsSelector,
    TermReferenceAsSelector,
    MessageAttributeAsSelector,
    TermAttributeAsPlaceable,
    UnterminatedStringExpression,
    PositionalArgumentFollowsNamed,
    DuplicatedNamedArgument(String),
    ForbiddenVariantAccessor,
    UnknownEscapeSequence(String),
    InvalidUnicodeEscapeSequence(String),
    UnbalancedClosingBrace,
    ExpectedInlineExpression,
    ExpectedSimpleExpressionAsSelector,
}

impl Display for ErrorKind {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Generic => write!(f, "An error occurred"),
            Self::ExpectedEntry => write!(f, "Expected an entry"),
            Self::ExpectedToken(letter) => {
                write!(f, "Expected a token starting with \"{}\"", letter)
            }
            Self::ExpectedCharRange { range } => write!(f, "Expected one of \"{}\"", range),
            Self::ExpectedMessageField { entry_id } => {
                write!(f, "Expected a message field for \"{}\"", entry_id)
            }
            Self::ExpectedTermField { entry_id } => {
                write!(f, "Expected a term field for \"{}\"", entry_id)
            }
            Self::ForbiddenWhitespace => write!(f, "Whitespace is not allowed here"),
            Self::ForbiddenCallee => write!(f, "Callee is not allowed here"),
            Self::ForbiddenKey => write!(f, "Key is not allowed here"),
            Self::MissingDefaultVariant => {
                write!(f, "The select expression must have a default variant")
            }
            Self::MissingVariants => {
                write!(f, "The select expression must have one or more variants")
            }
            Self::MissingValue => write!(f, "Expected a value"),
            Self::MissingVariantKey => write!(f, "Expected a variant key"),
            Self::MissingLiteral => write!(f, "Expected a literal"),
            Self::MultipleDefaultVariants => {
                write!(f, "A select expression can only have one default variant",)
            }
            Self::MessageReferenceAsSelector => {
                write!(f, "Message references can't be used as a selector")
            }
            Self::TermReferenceAsSelector => {
                write!(f, "Term references can't be used as a selector")
            }
            Self::MessageAttributeAsSelector => {
                write!(f, "Message attributes can't be used as a selector")
            }
            Self::TermAttributeAsPlaceable => {
                write!(f, "Term attributes can't be used as a placeable")
            }
            Self::UnterminatedStringExpression => write!(f, "Unterminated string expression"),
            Self::PositionalArgumentFollowsNamed => {
                write!(f, "Positional arguments must come before named arguments",)
            }
            Self::DuplicatedNamedArgument(name) => {
                write!(f, "The \"{}\" argument appears twice", name)
            }
            Self::ForbiddenVariantAccessor => write!(f, "Forbidden variant accessor"),
            Self::UnknownEscapeSequence(seq) => write!(f, "Unknown escape sequence, \"{}\"", seq),
            Self::InvalidUnicodeEscapeSequence(seq) => {
                write!(f, "Invalid unicode escape sequence, \"{}\"", seq)
            }
            Self::UnbalancedClosingBrace => write!(f, "Unbalanced closing brace"),
            Self::ExpectedInlineExpression => write!(f, "Expected an inline expression"),
            Self::ExpectedSimpleExpressionAsSelector => {
                write!(f, "Expected a simple expression as selector")
            }
        }
    }
}
