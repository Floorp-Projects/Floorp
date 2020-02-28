use crate::parser_tables_generated::TerminalId;
use ast::SourceLocation;

/// An ECMAScript input token. The lexer discards input matching *WhiteSpace*,
/// *LineTerminator*, and *Comment*. The remaining input elements are called
/// tokens, and they're fed to the parser.
///
/// Tokens match the goal terminals of the ECMAScript lexical grammar; see
/// <https://tc39.es/ecma262/#sec-ecmascript-language-lexical-grammar>.
#[derive(Clone, Debug, PartialEq)]
pub struct Token<'a> {
    /// Token type.
    pub terminal_id: TerminalId,

    /// Offset of this token, in bytes, within the source buffer.
    pub loc: SourceLocation,

    /// True if this token is the first token on a source line. This is true at
    /// the start of a script or module and after each LineTerminatorSequence.
    /// It is unaffected by single-line `/* delimited */` comments.
    ///
    /// *LineContinuation*s (a backslash followed by a newline) also don't
    /// affect this, since they can only happen inside strings, not between
    /// tokens.
    ///
    /// For a `TerminalId::End` token, this is false, regardless of whether
    /// there was a newline at the end of the file.
    pub is_on_new_line: bool,

    /// Data about the token. The exact meaning of this field depends on the
    /// `terminal_id`.
    ///
    /// For names and keyword tokens, this is just the token as it appears in
    /// the source. Same goes for *BooleanLiteral*, *NumericLiteral*, and
    /// *RegularExpressionLiteral* tokens.
    ///
    /// For a string literal, the string characters, after decoding
    /// *EscapeSequence*s and removing *LineContinuation*s (the SV of the
    /// literal, in standardese).
    ///
    /// For all other tokens (including template literal parts), the content is
    /// unspecified for now. TODO.
    pub value: Option<&'a str>,
}

impl Token<'_> {
    pub fn basic_token(terminal_id: TerminalId, loc: SourceLocation) -> Self {
        Self {
            terminal_id,
            loc,
            is_on_new_line: false,
            value: None,
        }
    }
}
