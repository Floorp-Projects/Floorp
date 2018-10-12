use std::fmt;

/// An enum of all possible tokens allowed by the
/// [WebIDL grammar](https://heycam.github.io/webidl/#idl-grammar) A token in this case is a
/// terminal, either a static string or regular expression based token. Note that not all possible
/// simplifications are made such as converting the `True` and `False` tokens to actual booleans.
/// This choice was made to be as consistent as possible with the WebIDL grammar.
#[allow(missing_docs)]
#[derive(Clone, Debug, PartialEq)]
pub enum Token {
    // Keywords
    Any,
    ArrayBuffer,
    Attribute,
    Boolean,
    Byte,
    ByteString,
    Callback,
    Const,
    DataView,
    Deleter,
    Dictionary,
    DOMString,
    Double,
    Enum,
    Error,
    False,
    Float,
    Float32Array,
    Float64Array,
    FrozenArray,
    Getter,
    Implements,
    Includes,
    Inherit,
    Int16Array,
    Int32Array,
    Int8Array,
    Interface,
    Iterable,
    LegacyCaller,
    Long,
    Maplike,
    Mixin,
    Namespace,
    NaN,
    NegativeInfinity,
    Null,
    Object,
    Octet,
    Optional,
    Or,
    Partial,
    PositiveInfinity,
    Promise,
    ReadOnly,
    Record,
    Required,
    Sequence,
    Setlike,
    Setter,
    Short,
    Static,
    Stringifier,
    Symbol,
    True,
    Typedef,
    USVString,
    Uint16Array,
    Uint32Array,
    Uint8Array,
    Uint8ClampedArray,
    Unrestricted,
    Unsigned,
    Void,

    // Regular expressions
    FloatLiteral(f64),
    Identifier(String),
    IntegerLiteral(i64),
    OtherLiteral(char),
    StringLiteral(String),

    // Symbols
    Colon,
    Comma,
    Ellipsis,
    Equals,
    GreaterThan,
    Hyphen,
    LeftBrace,
    LeftBracket,
    LeftParenthesis,
    LessThan,
    Period,
    QuestionMark,
    RightBrace,
    RightBracket,
    RightParenthesis,
    Semicolon,
}

impl fmt::Display for Token {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}
