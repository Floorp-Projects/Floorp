#[derive(Debug, PartialEq)]
pub struct ParserError {
    pub pos: (usize, usize),
    pub slice: Option<(usize, usize)>,
    pub kind: ErrorKind,
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

#[derive(Debug, PartialEq)]
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
