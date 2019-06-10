use attribute::ExtendedAttributeList;
use common::{Braced, Generics, Identifier, Punctuated};
use term;
use Parse;

/// Parses a union of types
pub type UnionType<'a> = Braced<Punctuated<UnionMemberType<'a>, term!(or)>>;

ast_types! {
    /// Parses either single type or a union type
    enum Type<'a> {
        /// Parses one of the single types
        Single(enum SingleType<'a> {
            Any(term!(any)),
            NonAny(NonAnyType<'a>),
        }),
        Union(MayBeNull<UnionType<'a>>),
    }

    // Parses any single non-any type
    enum NonAnyType<'a> {
        Promise(PromiseType<'a>),
        Integer(MayBeNull<IntegerType>),
        FloatingPoint(MayBeNull<FloatingPointType>),
        Boolean(MayBeNull<term!(boolean)>),
        Byte(MayBeNull<term!(byte)>),
        Octet(MayBeNull<term!(octet)>),
        ByteString(MayBeNull<term!(ByteString)>),
        DOMString(MayBeNull<term!(DOMString)>),
        USVString(MayBeNull<term!(USVString)>),
        Sequence(MayBeNull<SequenceType<'a>>),
        Object(MayBeNull<term!(object)>),
        Symbol(MayBeNull<term!(symbol)>),
        Error(MayBeNull<term!(Error)>),
        ArrayBuffer(MayBeNull<term!(ArrayBuffer)>),
        DataView(MayBeNull<term!(DataView)>),
        Int8Array(MayBeNull<term!(Int8Array)>),
        Int16Array(MayBeNull<term!(Int16Array)>),
        Int32Array(MayBeNull<term!(Int32Array)>),
        Uint8Array(MayBeNull<term!(Uint8Array)>),
        Uint16Array(MayBeNull<term!(Uint16Array)>),
        Uint32Array(MayBeNull<term!(Uint32Array)>),
        Uint8ClampedArray(MayBeNull<term!(Uint8ClampedArray)>),
        Float32Array(MayBeNull<term!(Float32Array)>),
        Float64Array(MayBeNull<term!(Float64Array)>),
        ArrayBufferView(MayBeNull<term!(ArrayBufferView)>),
        BufferSource(MayBeNull<term!(BufferSource)>),
        FrozenArrayType(MayBeNull<FrozenArrayType<'a>>),
        RecordType(MayBeNull<RecordType<'a>>),
        Identifier(MayBeNull<Identifier<'a>>),
    }

    /// Parses `sequence<Type>`
    struct SequenceType<'a> {
        sequence: term!(sequence),
        generics: Generics<Box<Type<'a>>>,
    }

    /// Parses `FrozenArray<Type>`
    struct FrozenArrayType<'a> {
        frozen_array: term!(FrozenArray),
        generics: Generics<Box<Type<'a>>>,
    }

    /// Parses a nullable type. Ex: `object | object??`
    ///
    /// `??` means an actual ? not an optional requirement
    #[derive(Copy)]
    struct MayBeNull<T> where [T: Parse<'a>] {
        type_: T,
        q_mark: Option<term::QMark>,
    }

    /// Parses a `Promise<Type|void>` type
    struct PromiseType<'a> {
        promise: term!(Promise),
        generics: Generics<Box<ReturnType<'a>>>,
    }

    /// Parses `unsigned? short|long|(long long)`
    #[derive(Copy)]
    enum IntegerType {
        /// Parses `unsigned? long long`
        #[derive(Copy)]
        LongLong(struct LongLongType {
            unsigned: Option<term!(unsigned)>,
            long_long: (term!(long), term!(long)),
        }),
        /// Parses `unsigned? long`
        #[derive(Copy)]
        Long(struct LongType {
            unsigned: Option<term!(unsigned)>,
            long: term!(long),
        }),
        /// Parses `unsigned? short`
        #[derive(Copy)]
        Short(struct ShortType {
            unsigned: Option<term!(unsigned)>,
            short: term!(short),
        }),
    }

    /// Parses `unrestricted? float|double`
    #[derive(Copy)]
    enum FloatingPointType {
        /// Parses `unrestricted? float`
        #[derive(Copy)]
        Float(struct FloatType {
            unrestricted: Option<term!(unrestricted)>,
            float: term!(float),
        }),
        /// Parses `unrestricted? double`
        #[derive(Copy)]
        Double(struct DoubleType {
            unrestricted: Option<term!(unrestricted)>,
            double: term!(double),
        }),
    }

    /// Parses `record<StringType, Type>`
    struct RecordType<'a> {
        record: term!(record),
        generics: Generics<(StringType, term!(,), Box<Type<'a>>)>,
    }

    /// Parses one of the string types `ByteString|DOMString|USVString`
    #[derive(Copy)]
    enum StringType {
        Byte(term!(ByteString)),
        DOM(term!(DOMString)),
        USV(term!(USVString)),
    }

    /// Parses one of the member of a union type
    enum UnionMemberType<'a> {
        Single(NonAnyType<'a>),
        Union(MayBeNull<UnionType<'a>>),
    }

    /// Parses a const type
    enum ConstType<'a> {
        Integer(MayBeNull<IntegerType>),
        FloatingPoint(MayBeNull<FloatingPointType>),
        Boolean(MayBeNull<term!(boolean)>),
        Byte(MayBeNull<term!(byte)>),
        Octet(MayBeNull<term!(octet)>),
        Identifier(MayBeNull<Identifier<'a>>),
    }

    /// Parses the return type which may be `void` or any given Type
    enum ReturnType<'a> {
        Void(term!(void)),
        Type(Type<'a>),
    }

    /// Parses `[attributes]? type`
    struct AttributedType<'a> {
        attributes: Option<ExtendedAttributeList<'a>>,
        type_: Type<'a>,
    }
}

#[cfg(test)]
mod test {
    use super::*;

    test!(should_parse_may_be_null { "short" =>
        "";
        MayBeNull<::types::IntegerType>;
        q_mark.is_none();
    });

    test!(should_parse_nullable { "short?" =>
        "";
        MayBeNull<::types::IntegerType>;
        q_mark.is_some();
    });

    test_variants!(
        ReturnType {
            Void == "void",
            Type == "any",
        }
    );

    test_variants!(
        ConstType {
            Integer == "short",
            FloatingPoint == "float",
            Boolean == "boolean",
            Byte == "byte",
            Octet == "octet",
            Identifier == "name",
        }
    );

    test_variants!(
        NonAnyType {
            Promise == "Promise<long>",
            Integer == "long",
            FloatingPoint == "float",
            Boolean == "boolean",
            Byte == "byte",
            Octet == "octet",
            ByteString == "ByteString",
            DOMString == "DOMString",
            USVString == "USVString",
            Sequence == "sequence<short>",
            Object == "object",
            Symbol == "symbol",
            Error == "Error",
            ArrayBuffer == "ArrayBuffer",
            DataView == "DataView",
            Int8Array == "Int8Array",
            Int16Array == "Int16Array",
            Int32Array == "Int32Array",
            Uint8Array == "Uint8Array",
            Uint16Array == "Uint16Array",
            Uint32Array == "Uint32Array",
            Uint8ClampedArray == "Uint8ClampedArray",
            Float32Array == "Float32Array",
            Float64Array == "Float64Array",
            ArrayBufferView == "ArrayBufferView",
            BufferSource == "BufferSource",
            FrozenArrayType == "FrozenArray<short>",
            RecordType == "record<DOMString, short>",
            Identifier == "mango"
        }
    );

    test_variants!(
        UnionMemberType {
            Single == "byte",
            Union == "(byte or byte)"
        }
    );

    test_variants!(
        StringType {
            DOM == "DOMString",
            USV == "USVString",
            Byte == "ByteString"
        }
    );

    test!(should_parse_record_type { "record<DOMString, short>" =>
        "";
        RecordType;
    });

    test!(should_parse_double_type { "double" =>
        "";
        DoubleType;
    });

    test!(should_parse_float_type { "float" =>
        "";
        FloatType;
    });

    test_variants!(
        FloatingPointType {
            Float == "float",
            Double == "double"
        }
    );

    test!(should_parse_long_long_type { "long long" =>
        "";
        LongLongType;
    });

    test!(should_parse_long_type { "long" =>
        "";
        LongType;
    });

    test!(should_parse_short_type { "short" =>
        "";
        ShortType;
    });

    test_variants!(
        IntegerType {
            Short == "short",
            Long == "long",
            LongLong == "long long"
        }
    );

    test!(should_parse_promise_type { "Promise<short>" =>
        "";
        PromiseType;
    });

    test!(should_parse_frozen_array_type { "FrozenArray<short>" =>
        "";
        FrozenArrayType;
    });

    test!(should_parse_sequence_type { "sequence<short>" =>
        "";
        SequenceType;
    });

    test_variants!(
        SingleType {
            Any == "any",
            NonAny == "Promise<short>",
        }
    );

    test_variants!(
        Type {
            Single == "short",
            Union == "(short or float)"
        }
    );

    test!(should_parse_attributed_type { "[Named] short" =>
        "";
        AttributedType;
        attributes.is_some();
    });

    test!(should_parse_type_as_identifier { "DOMStringMap" =>
        // if type is not parsed as identifier, it is parsed as `DOMString` and 'Map' is left
        "";
        ::types::Type;
    });
}
