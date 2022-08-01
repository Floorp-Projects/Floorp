use crate::literal::DefaultValue;
use crate::term;
use crate::Parse;

impl<'a, T: Parse<'a>> Parse<'a> for Option<T> {
    parser!(opt!(weedle!(T)));
}

impl<'a, T: Parse<'a>> Parse<'a> for Box<T> {
    parser!(do_parse!(inner: weedle!(T) >> (Box::new(inner))));
}

/// Parses `item1 item2 item3...`
impl<'a, T: Parse<'a>> Parse<'a> for Vec<T> {
    parser!(many0!(weedle!(T)));
}

impl<'a, T: Parse<'a>, U: Parse<'a>> Parse<'a> for (T, U) {
    parser!(do_parse!(t: weedle!(T) >> u: weedle!(U) >> ((t, u))));
}

impl<'a, T: Parse<'a>, U: Parse<'a>, V: Parse<'a>> Parse<'a> for (T, U, V) {
    parser!(do_parse!(
        t: weedle!(T) >> u: weedle!(U) >> v: weedle!(V) >> ((t, u, v))
    ));
}

ast_types! {
    /// Parses `( body )`
    #[derive(Copy, Default)]
    struct Parenthesized<T> where [T: Parse<'a>] {
        open_paren: term::OpenParen,
        body: T,
        close_paren: term::CloseParen,
    }

    /// Parses `[ body ]`
    #[derive(Copy, Default)]
    struct Bracketed<T> where [T: Parse<'a>] {
        open_bracket: term::OpenBracket,
        body: T,
        close_bracket: term::CloseBracket,
    }

    /// Parses `{ body }`
    #[derive(Copy, Default)]
    struct Braced<T> where [T: Parse<'a>] {
        open_brace: term::OpenBrace,
        body: T,
        close_brace: term::CloseBrace,
    }

    /// Parses `< body >`
    #[derive(Copy, Default)]
    struct Generics<T> where [T: Parse<'a>] {
        open_angle: term::LessThan,
        body: T,
        close_angle: term::GreaterThan,
    }

    /// Parses `(item1, item2, item3,...)?`
    struct Punctuated<T, S> where [T: Parse<'a>, S: Parse<'a> + ::std::default::Default] {
        list: Vec<T> = separated_list0!(weedle!(S), weedle!(T)),
        separator: S = marker,
    }

    /// Parses `item1, item2, item3, ...`
    struct PunctuatedNonEmpty<T, S> where [T: Parse<'a>, S: Parse<'a> + ::std::default::Default] {
        list: Vec<T> = terminated!(
            separated_list1!(weedle!(S), weedle!(T)),
            opt!(weedle!(S))
        ),
        separator: S = marker,
    }

    /// Represents an identifier
    ///
    /// Follows `/_?[A-Za-z][0-9A-Z_a-z-]*/`
    #[derive(Copy)]
    struct Identifier<'a>(
        // See https://heycam.github.io/webidl/#idl-names for why the leading
        // underscore is trimmed
        &'a str = ws!(do_parse!(
            opt!(char!('_')) >>
            id: recognize!(do_parse!(
                take_while1!(|c: char| c.is_ascii_alphabetic()) >>
                take_while!(|c: char| c.is_ascii_alphanumeric() || c == '_' || c == '-') >>
                (())
            )) >>
            (id)
        )),
    )

    /// Parses rhs of an assignment expression. Ex: `= 45`
    #[derive(Copy)]
    struct Default<'a> {
        assign: term!(=),
        value: DefaultValue<'a>,
    }
}

#[cfg(test)]
mod test {
    use super::*;

    test!(should_parse_optional_present { "one" =>
        "";
        Option<Identifier>;
        is_some();
    });

    test!(should_parse_optional_not_present { "" =>
        "";
        Option<Identifier>;
        is_none();
    });

    test!(should_parse_boxed { "one" =>
        "";
        Box<Identifier>;
    });

    test!(should_parse_vec { "one two three" =>
        "";
        Vec<Identifier>;
        len() == 3;
    });

    test!(should_parse_parenthesized { "( one )" =>
        "";
        Parenthesized<Identifier>;
        body.0 == "one";
    });

    test!(should_parse_bracketed { "[ one ]" =>
        "";
        Bracketed<Identifier>;
        body.0 == "one";
    });

    test!(should_parse_braced { "{ one }" =>
        "";
        Braced<Identifier>;
        body.0 == "one";
    });

    test!(should_parse_generics { "<one>" =>
        "";
        Generics<Identifier>;
        body.0 == "one";
    });

    test!(should_parse_generics_two { "<one, two>" =>
        "";
        Generics<(Identifier, term!(,), Identifier)> =>
            Generics {
                open_angle: term!(<),
                body: (Identifier("one"), term!(,), Identifier("two")),
                close_angle: term!(>),
            }
    });

    test!(should_parse_comma_separated_values { "one, two, three" =>
        "";
        Punctuated<Identifier, term!(,)>;
        list.len() == 3;
    });

    test!(err should_not_parse_comma_separated_values_empty { "" =>
        PunctuatedNonEmpty<Identifier, term!(,)>
    });

    test!(should_parse_identifier { "hello" =>
        "";
        Identifier;
        0 == "hello";
    });

    test!(should_parse_numbered_identifier { "hello5" =>
        "";
        Identifier;
        0 == "hello5";
    });

    test!(should_parse_underscored_identifier { "_hello_" =>
        "";
        Identifier;
        0 == "hello_";
    });

    test!(should_parse_identifier_surrounding_with_spaces { "  hello  " =>
        "";
        Identifier;
        0 == "hello";
    });

    test!(should_parse_identifier_preceeding_others { "hello  note" =>
        "note";
        Identifier;
        0 == "hello";
    });

    test!(should_parse_identifier_attached_to_symbol { "hello=" =>
        "=";
        Identifier;
        0 == "hello";
    });
}
