use crate::literal::DefaultValue;
use crate::{term, IResult, Parse};

pub(crate) fn is_alphanum_underscore_dash(token: char) -> bool {
    nom::AsChar::is_alphanum(token) || matches!(token, '_' | '-')
}

fn marker<S>(i: &str) -> IResult<&str, S>
where
    S: ::std::default::Default,
{
    Ok((i, S::default()))
}

impl<'a, T: Parse<'a>> Parse<'a> for Option<T> {
    parser!(nom::combinator::opt(weedle!(T)));
}

impl<'a, T: Parse<'a>> Parse<'a> for Box<T> {
    parser!(nom::combinator::map(weedle!(T), Box::new));
}

/// Parses `item1 item2 item3...`
impl<'a, T: Parse<'a>> Parse<'a> for Vec<T> {
    parser!(nom::multi::many0(T::parse));
}

impl<'a, T: Parse<'a>, U: Parse<'a>> Parse<'a> for (T, U) {
    parser!(nom::sequence::tuple((T::parse, U::parse)));
}

impl<'a, T: Parse<'a>, U: Parse<'a>, V: Parse<'a>> Parse<'a> for (T, U, V) {
    parser!(nom::sequence::tuple((T::parse, U::parse, V::parse)));
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
        list: Vec<T> = nom::multi::separated_list0(weedle!(S), weedle!(T)),
        separator: S = marker,
    }

    /// Parses `item1, item2, item3, ...`
    struct PunctuatedNonEmpty<T, S> where [T: Parse<'a>, S: Parse<'a> + ::std::default::Default] {
        list: Vec<T> = nom::sequence::terminated(
            nom::multi::separated_list1(weedle!(S), weedle!(T)),
            nom::combinator::opt(weedle!(S))
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
        &'a str = crate::whitespace::ws(nom::sequence::preceded(
            nom::combinator::opt(nom::character::complete::char('_')),
            nom::combinator::recognize(nom::sequence::tuple((
                nom::bytes::complete::take_while1(nom::AsChar::is_alphanum),
                nom::bytes::complete::take_while(is_alphanum_underscore_dash),
            )))
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

    test!(should_parse_identifier_preceding_others { "hello  note" =>
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
