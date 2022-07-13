macro_rules! generate_terms {
    ($( $(#[$attr:meta])* $typ:ident => $tok:expr ),*) => {
        $(
            $(#[$attr])*
            #[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
            pub struct $typ;

            impl<'a> $crate::Parse<'a> for $typ {
                parser!(do_parse!(
                    ws!(tag!($tok)) >>
                    ($typ)
                ));
            }
        )*
    };
}

macro_rules! ident_tag (
    ($i:expr, $tok:expr) => (
        {
            match tag!($i, $tok) {
                Err(e) => Err(e),
                Ok((i, o)) => {
                    use nom::{character::is_alphanumeric, Err as NomErr, error::ErrorKind};
                    let mut res = Ok((i, o));
                    if let Some(&c) = i.as_bytes().first() {
                        if is_alphanumeric(c) || c == b'_' || c == b'-' {
                            res = Err(NomErr::Error(($i, ErrorKind::Tag)));
                        }
                    }
                    res
                },
            }
        }
    )
);

macro_rules! generate_terms_for_names {
    ($( $(#[$attr:meta])* $typ:ident => $tok:expr,)*) => {
        $(
            $(#[$attr])*
            #[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
            pub struct $typ;

            impl<'a> $crate::Parse<'a> for $typ {
                parser!(do_parse!(
                    ws!(ident_tag!($tok)) >>
                    ($typ)
                ));
            }
        )*
    };
}

generate_terms! {
    /// Represents the terminal symbol `(`
    OpenParen => "(",

    /// Represents the terminal symbol `)`
    CloseParen => ")",

    /// Represents the terminal symbol `[`
    OpenBracket => "[",

    /// Represents the terminal symbol `]`
    CloseBracket => "]",

    /// Represents the terminal symbol `{`
    OpenBrace => "{",

    /// Represents the terminal symbol `}`
    CloseBrace => "}",

    /// Represents the terminal symbol `,`
    Comma => ",",

    /// Represents the terminal symbol `-`
    Minus => "-",

    /// Represents the terminal symbol `.`
    Dot => ".",

    /// Represents the terminal symbol `...`
    Ellipsis => "...",

    /// Represents the terminal symbol `:`
    Colon => ":",

    /// Represents the terminal symbol `;`
    SemiColon => ";",

    /// Represents the terminal symbol `<`
    LessThan => "<",

    /// Represents the terminal symbol `=`
    Assign => "=",

    /// Represents the terminal symbol `>`
    GreaterThan => ">",

    /// Represents the terminal symbol `?`
    QMark => "?"
}

generate_terms_for_names! {
    /// Represents the terminal symbol `or`
    Or => "or",

    /// Represents the terminal symbol `optional`
    Optional => "optional",

    /// Represents the terminal symbol `async`
    Async => "async",

    /// Represents the terminal symbol `attribute`
    Attribute => "attribute",

    /// Represents the terminal symbol `callback`
    Callback => "callback",

    /// Represents the terminal symbol `const`
    Const => "const",

    /// Represents the terminal symbol `deleter`
    Deleter => "deleter",

    /// Represents the terminal symbol `dictionary`
    Dictionary => "dictionary",

    /// Represents the terminal symbol `enum`
    Enum => "enum",

    /// Represents the terminal symbol `getter`
    Getter => "getter",

    /// Represents the terminal symbol `includes`
    Includes => "includes",

    /// Represents the terminal symbol `inherit`
    Inherit => "inherit",

    /// Represents the terminal symbol `interface`
    Interface => "interface",

    /// Represents the terminal symbol `iterable`
    Iterable => "iterable",

    /// Represents the terminal symbol `maplike`
    Maplike => "maplike",

    /// Represents the terminal symbol `namespace`
    Namespace => "namespace",

    /// Represents the terminal symbol `partial`
    Partial => "partial",

    /// Represents the terminal symbol `required`
    Required => "required",

    /// Represents the terminal symbol `setlike`
    Setlike => "setlike",

    /// Represents the terminal symbol `setter`
    Setter => "setter",

    /// Represents the terminal symbol `static`
    Static => "static",

    /// Represents the terminal symbol `stringifier`
    Stringifier => "stringifier",

    /// Represents the terminal symbol `typedef`
    Typedef => "typedef",

    /// Represents the terminal symbol `unrestricted`
    Unrestricted => "unrestricted",

    /// Represents the terminal symbol `symbol`
    Symbol => "symbol",

    /// Represents the terminal symbol `Infinity`
    NegInfinity => "-Infinity",

    /// Represents the terminal symbol `ByteString`
    ByteString => "ByteString",

    /// Represents the terminal symbol `DOMString`
    DOMString => "DOMString",

    /// Represents the terminal symbol `FrozenArray`
    FrozenArray => "FrozenArray",

    /// Represents the terminal symbol `Infinity`
    Infinity => "Infinity",

    /// Represents the terminal symbol `NaN`
    NaN => "NaN",

    /// Represents the terminal symbol `USVString`
    USVString => "USVString",

    /// Represents the terminal symbol `any`
    Any => "any",

    /// Represents the terminal symbol `boolean`
    Boolean => "boolean",

    /// Represents the terminal symbol `byte`
    Byte => "byte",

    /// Represents the terminal symbol `double`
    Double => "double",

    /// Represents the terminal symbol `false`
    False => "false",

    /// Represents the terminal symbol `float`
    Float => "float",

    /// Represents the terminal symbol `long`
    Long => "long",

    /// Represents the terminal symbol `null`
    Null => "null",

    /// Represents the terminal symbol `object`
    Object => "object",

    /// Represents the terminal symbol `octet`
    Octet => "octet",

    /// Represents the terminal symbol `sequence`
    Sequence => "sequence",

    /// Represents the terminal symbol `short`
    Short => "short",

    /// Represents the terminal symbol `true`
    True => "true",

    /// Represents the terminal symbol `unsigned`
    Unsigned => "unsigned",

    /// Represents the terminal symbol `undefined`
    Undefined => "undefined",

    /// Represents the terminal symbol `record`
    Record => "record",

    /// Represents the terminal symbol `ArrayBuffer`
    ArrayBuffer => "ArrayBuffer",

    /// Represents the terminal symbol `DataView`
    DataView => "DataView",

    /// Represents the terminal symbol `Int8Array`
    Int8Array => "Int8Array",

    /// Represents the terminal symbol `Int16Array`
    Int16Array => "Int16Array",

    /// Represents the terminal symbol `Int32Array`
    Int32Array => "Int32Array",

    /// Represents the terminal symbol `Uint8Array`
    Uint8Array => "Uint8Array",

    /// Represents the terminal symbol `Uint16Array`
    Uint16Array => "Uint16Array",

    /// Represents the terminal symbol `Uint32Array`
    Uint32Array => "Uint32Array",

    /// Represents the terminal symbol `Uint8ClampedArray`
    Uint8ClampedArray => "Uint8ClampedArray",

    /// Represents the terminal symbol `Float32Array`
    Float32Array => "Float32Array",

    /// Represents the terminal symbol `Float64Array`
    Float64Array => "Float64Array",

    /// Represents the terminal symbol `ArrayBufferView`
    ArrayBufferView => "ArrayBufferView",

    /// Represents the terminal symbol `BufferSource
    BufferSource => "BufferSource",

    /// Represents the terminal symbol `Promise`
    Promise => "Promise",

    /// Represents the terminal symbol `Error`
    Error => "Error",

    /// Represents the terminal symbol `readonly`
    ReadOnly => "readonly",

    /// Represents the terminal symbol `mixin`
    Mixin => "mixin",

    /// Represents the terminal symbol `implements`
    Implements => "implements",

    /// Represents the terminal symbol `legacycaller`
    LegacyCaller => "legacycaller",

    /// Represents the terminal symbol `constructor`
    Constructor => "constructor",
}

#[macro_export]
macro_rules! term {
    (OpenParen) => {
        $crate::term::OpenParen
    };
    (CloseParen) => {
        $crate::term::CloseParen
    };
    (OpenBracket) => {
        $crate::term::OpenBracket
    };
    (CloseBracket) => {
        $crate::term::CloseBracket
    };
    (OpenBrace) => {
        $crate::term::OpenBrace
    };
    (CloseBrace) => {
        $crate::term::CloseBrace
    };
    (,) => {
        $crate::term::Comma
    };
    (-) => {
        $crate::term::Minus
    };
    (.) => {
        $crate::term::Dot
    };
    (...) => {
        $crate::term::Ellipsis
    };
    (:) => {
        $crate::term::Colon
    };
    (;) => {
        $crate::term::SemiColon
    };
    (<) => {
        $crate::term::LessThan
    };
    (=) => {
        $crate::term::Assign
    };
    (>) => {
        $crate::term::GreaterThan
    };
    (?) => {
        $crate::term::QMark
    };
    (or) => {
        $crate::term::Or
    };
    (optional) => {
        $crate::term::Optional
    };
    (async) => {
        $crate::term::Async
    };
    (attribute) => {
        $crate::term::Attribute
    };
    (callback) => {
        $crate::term::Callback
    };
    (const) => {
        $crate::term::Const
    };
    (deleter) => {
        $crate::term::Deleter
    };
    (dictionary) => {
        $crate::term::Dictionary
    };
    (enum) => {
        $crate::term::Enum
    };
    (getter) => {
        $crate::term::Getter
    };
    (includes) => {
        $crate::term::Includes
    };
    (inherit) => {
        $crate::term::Inherit
    };
    (interface) => {
        $crate::term::Interface
    };
    (iterable) => {
        $crate::term::Iterable
    };
    (maplike) => {
        $crate::term::Maplike
    };
    (namespace) => {
        $crate::term::Namespace
    };
    (partial) => {
        $crate::term::Partial
    };
    (required) => {
        $crate::term::Required
    };
    (setlike) => {
        $crate::term::Setlike
    };
    (setter) => {
        $crate::term::Setter
    };
    (static) => {
        $crate::term::Static
    };
    (stringifier) => {
        $crate::term::Stringifier
    };
    (typedef) => {
        $crate::term::Typedef
    };
    (unrestricted) => {
        $crate::term::Unrestricted
    };
    (symbol) => {
        $crate::term::Symbol
    };
    (- Infinity) => {
        $crate::term::NegInfinity
    };
    (ByteString) => {
        $crate::term::ByteString
    };
    (DOMString) => {
        $crate::term::DOMString
    };
    (FrozenArray) => {
        $crate::term::FrozenArray
    };
    (Infinity) => {
        $crate::term::Infinity
    };
    (NaN) => {
        $crate::term::NaN
    };
    (USVString) => {
        $crate::term::USVString
    };
    (any) => {
        $crate::term::Any
    };
    (boolean) => {
        $crate::term::Boolean
    };
    (byte) => {
        $crate::term::Byte
    };
    (double) => {
        $crate::term::Double
    };
    (false) => {
        $crate::term::False
    };
    (float) => {
        $crate::term::Float
    };
    (long) => {
        $crate::term::Long
    };
    (null) => {
        $crate::term::Null
    };
    (object) => {
        $crate::term::Object
    };
    (octet) => {
        $crate::term::Octet
    };
    (sequence) => {
        $crate::term::Sequence
    };
    (short) => {
        $crate::term::Short
    };
    (true) => {
        $crate::term::True
    };
    (unsigned) => {
        $crate::term::Unsigned
    };
    (undefined) => {
        $crate::term::Undefined
    };
    (record) => {
        $crate::term::Record
    };
    (ArrayBuffer) => {
        $crate::term::ArrayBuffer
    };
    (DataView) => {
        $crate::term::DataView
    };
    (Int8Array) => {
        $crate::term::Int8Array
    };
    (Int16Array) => {
        $crate::term::Int16Array
    };
    (Int32Array) => {
        $crate::term::Int32Array
    };
    (Uint8Array) => {
        $crate::term::Uint8Array
    };
    (Uint16Array) => {
        $crate::term::Uint16Array
    };
    (Uint32Array) => {
        $crate::term::Uint32Array
    };
    (Uint8ClampedArray) => {
        $crate::term::Uint8ClampedArray
    };
    (Float32Array) => {
        $crate::term::Float32Array
    };
    (Float64Array) => {
        $crate::term::Float64Array
    };
    (ArrayBufferView) => {
        $crate::term::ArrayBufferView
    };
    (BufferSource) => {
        $crate::term::BufferSource
    };
    (Promise) => {
        $crate::term::Promise
    };
    (Error) => {
        $crate::term::Error
    };
    (readonly) => {
        $crate::term::ReadOnly
    };
    (mixin) => {
        $crate::term::Mixin
    };
    (implements) => {
        $crate::term::Implements
    };
    (legacycaller) => {
        $crate::term::LegacyCaller
    };
    (constructor) => {
        $crate::term::Constructor
    };
}

#[cfg(test)]
mod test {
    macro_rules! generate_tests {
        ($($m:ident, $typ:ident, $string:expr;)*) => {
            $(
                mod $m {
                    use super::super::$typ;
                    use crate::Parse;

                    #[test]
                    fn should_parse() {
                        let (rem, parsed) = $typ::parse(concat!($string)).unwrap();
                        assert_eq!(rem, "");
                        assert_eq!(parsed, $typ);
                    }

                    #[test]
                    fn should_parse_with_preceding_spaces() {
                        let (rem, parsed) = $typ::parse(concat!("  ", $string)).unwrap();
                        assert_eq!(rem, "");
                        assert_eq!(parsed, $typ);
                    }

                    #[test]
                    fn should_parse_with_succeeding_spaces() {
                        let (rem, parsed) = $typ::parse(concat!($string, "  ")).unwrap();
                        assert_eq!(rem, "");
                        assert_eq!(parsed, $typ);
                    }

                    #[test]
                    fn should_parse_with_surrounding_spaces() {
                        let (rem, parsed) = $typ::parse(concat!("  ", $string, "  ")).unwrap();
                        assert_eq!(rem, "");
                        assert_eq!(parsed, $typ);
                    }

                    #[test]
                    fn should_parse_if_anything_next() {
                        let (rem, parsed) = $typ::parse(concat!($string, "  anything")).unwrap();
                        assert_eq!(rem, "anything");
                        assert_eq!(parsed, $typ);
                    }
                }
            )*
        };
    }

    generate_tests![
        openparen, OpenParen, "(";
        closeparen, CloseParen, ")";
        openbracket, OpenBracket, "[";
        closebracket, CloseBracket, "]";
        openbrace, OpenBrace, "{";
        closebrace, CloseBrace, "}";
        comma, Comma, ",";
        minus, Minus, "-";
        dot, Dot, ".";
        ellipsis, Ellipsis, "...";
        colon, Colon, ":";
        semicolon, SemiColon, ";";
        lessthan, LessThan, "<";
        assign, Assign, "=";
        greaterthan, GreaterThan, ">";
        qmark, QMark, "?";
        or, Or, "or";
        optional, Optional, "optional";
        async_, Async, "async";
        attribute, Attribute, "attribute";
        callback, Callback, "callback";
        const_, Const, "const";
        deleter, Deleter, "deleter";
        dictionary, Dictionary, "dictionary";
        enum_, Enum, "enum";
        getter, Getter, "getter";
        includes, Includes, "includes";
        inherit, Inherit, "inherit";
        interface, Interface, "interface";
        iterable, Iterable, "iterable";
        maplike, Maplike, "maplike";
        namespace, Namespace, "namespace";
        partial, Partial, "partial";
        required, Required, "required";
        setlike, Setlike, "setlike";
        setter, Setter, "setter";
        static_, Static, "static";
        stringifier, Stringifier, "stringifier";
        typedef, Typedef, "typedef";
        unrestricted, Unrestricted, "unrestricted";
        symbol, Symbol, "symbol";
        neginfinity, NegInfinity, "-Infinity";
        bytestring, ByteString, "ByteString";
        domstring, DOMString, "DOMString";
        frozenarray, FrozenArray, "FrozenArray";
        infinity, Infinity, "Infinity";
        nan, NaN, "NaN";
        usvstring, USVString, "USVString";
        any, Any, "any";
        boolean, Boolean, "boolean";
        byte, Byte, "byte";
        double, Double, "double";
        false_, False, "false";
        float, Float, "float";
        long, Long, "long";
        null, Null, "null";
        object, Object, "object";
        octet, Octet, "octet";
        sequence, Sequence, "sequence";
        short, Short, "short";
        true_, True, "true";
        unsigned, Unsigned, "unsigned";
        undefined, Undefined, "undefined";
        record, Record, "record";
        arraybuffer, ArrayBuffer, "ArrayBuffer";
        dataview, DataView, "DataView";
        int8array, Int8Array, "Int8Array";
        int16array, Int16Array, "Int16Array";
        int32array, Int32Array, "Int32Array";
        uint8array, Uint8Array, "Uint8Array";
        uint16array, Uint16Array, "Uint16Array";
        uint32array, Uint32Array, "Uint32Array";
        uint8clampedarray, Uint8ClampedArray, "Uint8ClampedArray";
        float32array, Float32Array, "Float32Array";
        float64array, Float64Array, "Float64Array";
        promise, Promise, "Promise";
        error, Error, "Error";
        implements, Implements, "implements";
        legacycaller, LegacyCaller, "legacycaller";
        constructor, Constructor, "constructor";
    ];
}
