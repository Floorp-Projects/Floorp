ast_types! {
    /// Represents an integer value
    #[derive(Copy)]
    enum IntegerLit<'a> {
        /// Parses `-?[1-9][0-9]*`
        #[derive(Copy)]
        Dec(struct DecLit<'a>(
            &'a str = ws!(recognize!(do_parse!(
                opt!(char!('-')) >>
                one_of!("123456789") >>
                take_while!(|c: char| c.is_ascii_digit()) >>
                (())
            ))),
        )),
        /// Parses `-?0[Xx][0-9A-Fa-f]+)`
        #[derive(Copy)]
        Hex(struct HexLit<'a>(
            &'a str = ws!(recognize!(do_parse!(
                opt!(char!('-')) >>
                char!('0') >>
                alt!(char!('x') | char!('X')) >>
                take_while!(|c: char| c.is_ascii_hexdigit()) >>
                (())
            ))),
        )),
        /// Parses `-?0[0-7]*`
        #[derive(Copy)]
        Oct(struct OctLit<'a>(
            &'a str = ws!(recognize!(do_parse!(
                opt!(char!('-')) >>
                char!('0') >>
                take_while!(|c| '0' <= c && c <= '7') >>
                (())
            ))),
        )),
    }

    /// Represents a string value
    ///
    /// Follow `/"[^"]*"/`
    #[derive(Copy)]
    struct StringLit<'a>(
        &'a str = ws!(do_parse!(
            char!('"') >>
            s: take_while!(|c| c != '"') >>
            char!('"') >>
            (s)
        )),
    )

    /// Represents a default literal value. Ex: `34|34.23|"value"|[ ]|true|false|null`
    #[derive(Copy)]
    enum DefaultValue<'a> {
        Boolean(BooleanLit),
        /// Represents `[ ]`
        #[derive(Copy, Default)]
        EmptyArray(struct EmptyArrayLit {
            open_bracket: term!(OpenBracket),
            close_bracket: term!(CloseBracket),
        }),
        /// Represents `{ }`
        #[derive(Copy, Default)]
        EmptyDictionary(struct EmptyDictionaryLit {
            open_brace: term!(OpenBrace),
            close_brace: term!(CloseBrace),
        }),
        Float(FloatLit<'a>),
        Integer(IntegerLit<'a>),
        Null(term!(null)),
        String(StringLit<'a>),
    }

    /// Represents `true`, `false`, `34.23`, `null`, `56`, ...
    #[derive(Copy)]
    enum ConstValue<'a> {
        Boolean(BooleanLit),
        Float(FloatLit<'a>),
        Integer(IntegerLit<'a>),
        Null(term!(null)),
    }

    /// Represents either `true` or `false`
    #[derive(Copy)]
    struct BooleanLit(
        bool = alt!(
            weedle!(term!(true)) => {|_| true} |
            weedle!(term!(false)) => {|_| false}
        ),
    )

    /// Represents a floating point value, `NaN`, `Infinity`, '+Infinity`
    #[derive(Copy)]
    enum FloatLit<'a> {
        /// Parses `/-?(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)([Ee][+-]?[0-9]+)?|[0-9]+[Ee][+-]?[0-9]+)/`
        #[derive(Copy)]
        Value(struct FloatValueLit<'a>(
            &'a str = ws!(recognize!(do_parse!(
                opt!(char!('-')) >>
                alt!(
                    do_parse!(
                        // (?:[0-9]+\.[0-9]*|[0-9]*\.[0-9]+)
                        alt!(
                            do_parse!(
                                take_while1!(|c: char| c.is_ascii_digit()) >>
                                char!('.') >>
                                take_while!(|c: char| c.is_ascii_digit()) >>
                                (())
                            )
                            |
                            do_parse!(
                                take_while!(|c: char| c.is_ascii_digit()) >>
                                char!('.') >>
                                take_while1!(|c: char| c.is_ascii_digit()) >>
                                (())
                            )
                        ) >>
                        // (?:[Ee][+-]?[0-9]+)?
                        opt!(do_parse!(
                            alt!(char!('e') | char!('E')) >>
                            opt!(alt!(char!('+') | char!('-'))) >>
                            take_while1!(|c: char| c.is_ascii_digit()) >>
                            (())
                        )) >>
                        (())
                    )
                    |
                    // [0-9]+[Ee][+-]?[0-9]+
                    do_parse!(
                        take_while1!(|c: char| c.is_ascii_digit()) >>
                        alt!(char!('e') | char!('E')) >>
                        opt!(alt!(char!('+') | char!('-'))) >>
                        take_while1!(|c: char| c.is_ascii_digit()) >>
                        (())
                    )
                ) >>
                (())
            ))),
        )),
        NegInfinity(term!(-Infinity)),
        Infinity(term!(Infinity)),
        NaN(term!(NaN)),
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::term::*;
    use crate::Parse;

    test!(should_parse_integer { "45" =>
        "";
        IntegerLit => IntegerLit::Dec(DecLit("45"))
    });

    test!(should_parse_integer_surrounding_with_spaces { "  123123  " =>
        "";
        IntegerLit => IntegerLit::Dec(DecLit("123123"))
    });

    test!(should_parse_integer_preceeding_others { "3453 string" =>
        "string";
        IntegerLit => IntegerLit::Dec(DecLit("3453"))
    });

    test!(should_parse_neg_integer { "-435" =>
        "";
        IntegerLit => IntegerLit::Dec(DecLit("-435"))
    });

    test!(should_parse_hex_number { "0X08" =>
        "";
        IntegerLit => IntegerLit::Hex(HexLit("0X08"))
    });

    test!(should_parse_hex_large_number { "0xA" =>
        "";
        IntegerLit => IntegerLit::Hex(HexLit("0xA"))
    });

    test!(should_parse_zero { "0" =>
        "";
        IntegerLit => IntegerLit::Oct(OctLit("0"))
    });

    test!(should_parse_oct_number { "-07561" =>
        "";
        IntegerLit => IntegerLit::Oct(OctLit("-07561"))
    });

    test!(should_parse_float { "45.434" =>
        "";
        FloatLit => FloatLit::Value(FloatValueLit("45.434"))
    });

    test!(should_parse_float_surrounding_with_spaces { "  2345.2345  " =>
        "";
        FloatLit => FloatLit::Value(FloatValueLit("2345.2345"))
    });

    test!(should_parse_float_preceeding_others { "3453.32334 string" =>
        "string";
        FloatLit => FloatLit::Value(FloatValueLit("3453.32334"))
    });

    test!(should_parse_neg_float { "-435.3435" =>
        "";
        FloatLit => FloatLit::Value(FloatValueLit("-435.3435"))
    });

    test!(should_parse_float_exp { "5.3434e23" =>
        "";
        FloatLit => FloatLit::Value(FloatValueLit("5.3434e23"))
    });

    test!(should_parse_float_exp_with_decimal { "3e23" =>
        "";
        FloatLit => FloatLit::Value(FloatValueLit("3e23"))
    });

    test!(should_parse_neg_infinity { "-Infinity" =>
        "";
        FloatLit => FloatLit::NegInfinity(term!(-Infinity))
    });

    test!(should_parse_infinity { "Infinity" =>
        "";
        FloatLit => FloatLit::Infinity(term!(Infinity))
    });

    test!(should_parse_string { r#""this is a string""# =>
        "";
        StringLit => StringLit("this is a string")
    });

    test!(should_parse_string_surround_with_spaces { r#"  "this is a string"  "# =>
        "";
        StringLit => StringLit("this is a string")
    });

    test!(should_parse_string_followed_by_string { r#" "this is first"  "this is second" "# =>
        r#""this is second" "#;
        StringLit => StringLit("this is first")
    });

    test!(should_parse_string_with_spaces { r#"  "  this is a string  "  "# =>
        "";
        StringLit => StringLit("  this is a string  ")
    });

    test!(should_parse_string_with_comment { r#"  "// this is still a string"
     "# =>
        "";
        StringLit => StringLit("// this is still a string")
    });

    test!(should_parse_string_with_multiline_comment { r#"  "/*"  "*/"  "# =>
        r#""*/"  "#;
        StringLit => StringLit("/*")
    });

    test!(should_parse_null { "null" =>
        "";
        Null => Null
    });

    test!(should_parse_empty_array { "[]" =>
        "";
        EmptyArrayLit => Default::default()
    });

    test!(should_parse_bool_true { "true" =>
        "";
        BooleanLit => BooleanLit(true)
    });

    test!(should_parse_bool_false { "false" =>
        "";
        BooleanLit => BooleanLit(false)
    });
}
