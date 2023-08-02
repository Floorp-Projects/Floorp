//! Parsing of [Rust `fmt` syntax][0].
//!
//! [0]: std::fmt#syntax

use std::{convert::identity, iter};

use unicode_xid::UnicodeXID as XID;

/// Output of the [`format_string`] parser.
#[derive(Debug, Clone, Eq, PartialEq)]
pub(crate) struct FormatString<'a> {
    pub(crate) formats: Vec<Format<'a>>,
}

/// Output of the [`format`] parser.
///
/// [`format`]: fn@format
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub(crate) struct Format<'a> {
    pub(crate) arg: Option<Argument<'a>>,
    pub(crate) spec: Option<FormatSpec<'a>>,
}

/// Output of the [`format_spec`] parser.
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub(crate) struct FormatSpec<'a> {
    pub(crate) width: Option<Width<'a>>,
    pub(crate) precision: Option<Precision<'a>>,
    pub(crate) ty: Type,
}

/// Output of the [`argument`] parser.
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub(crate) enum Argument<'a> {
    Integer(usize),
    Identifier(&'a str),
}

/// Output of the [`precision`] parser.
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub(crate) enum Precision<'a> {
    Count(Count<'a>),
    Star,
}

/// Output of the [`count`] parser.
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub(crate) enum Count<'a> {
    Integer(usize),
    Parameter(Parameter<'a>),
}

/// Output of the [`type_`] parser. See [formatting traits][1] for more info.
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#formatting-traits
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub(crate) enum Type {
    Display,
    Debug,
    LowerDebug,
    UpperDebug,
    Octal,
    LowerHex,
    UpperHex,
    Pointer,
    Binary,
    LowerExp,
    UpperExp,
}

impl Type {
    /// Returns trait name of this [`Type`].
    pub(crate) fn trait_name(&self) -> &'static str {
        match self {
            Type::Display => "Display",
            Type::Debug | Type::LowerDebug | Type::UpperDebug => "Debug",
            Type::Octal => "Octal",
            Type::LowerHex => "LowerHex",
            Type::UpperHex => "UpperHex",
            Type::Pointer => "Pointer",
            Type::Binary => "Binary",
            Type::LowerExp => "LowerExp",
            Type::UpperExp => "UpperExp",
        }
    }
}

/// Type alias for the [`FormatSpec::width`].
type Width<'a> = Count<'a>;

/// Output of the [`maybe_format`] parser.
type MaybeFormat<'a> = Option<Format<'a>>;

/// Output of the [`identifier`] parser.
type Identifier<'a> = &'a str;

/// Output of the [`parameter`] parser.
type Parameter<'a> = Argument<'a>;

/// [`str`] left to parse.
///
/// [`str`]: prim@str
type LeftToParse<'a> = &'a str;

/// Parses a `format_string` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`format_string`]` := `[`text`]` [`[`maybe_format text`]`] *`
///
/// # Example
///
/// ```text
/// Hello
/// Hello, {}!
/// {:?}
/// Hello {people}!
/// {} {}
/// {:04}
/// {par:-^#.0$?}
/// ```
///
/// # Return value
///
/// - [`Some`] in case of successful parse.
/// - [`None`] otherwise (not all characters are consumed by underlying
///   parsers).
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
pub(crate) fn format_string(input: &str) -> Option<FormatString<'_>> {
    let (mut input, _) = optional_result(text)(input);

    let formats = iter::repeat(())
        .scan(&mut input, |input, _| {
            let (curr, format) =
                alt(&mut [&mut maybe_format, &mut map(text, |(i, _)| (i, None))])(
                    input,
                )?;
            **input = curr;
            Some(format)
        })
        .flatten()
        .collect();

    // Should consume all tokens for a successful parse.
    input.is_empty().then_some(FormatString { formats })
}

/// Parses a `maybe_format` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`maybe_format`]` := '{' '{' | '}' '}' | `[`format`]
///
/// # Example
///
/// ```text
/// {{
/// }}
/// {:04}
/// {:#?}
/// {par:-^#.0$?}
/// ```
///
/// [`format`]: fn@format
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn maybe_format(input: &str) -> Option<(LeftToParse<'_>, MaybeFormat<'_>)> {
    alt(&mut [
        &mut map(str("{{"), |i| (i, None)),
        &mut map(str("}}"), |i| (i, None)),
        &mut map(format, |(i, format)| (i, Some(format))),
    ])(input)
}

/// Parses a `format` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`format`]` := '{' [`[`argument`]`] [':' `[`format_spec`]`] '}'`
///
/// # Example
///
/// ```text
/// {par}
/// {:#?}
/// {par:-^#.0$?}
/// ```
///
/// [`format`]: fn@format
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn format(input: &str) -> Option<(LeftToParse<'_>, Format<'_>)> {
    let input = char('{')(input)?;

    let (input, arg) = optional_result(argument)(input);

    let (input, spec) = map_or_else(
        char(':'),
        |i| Some((i, None)),
        map(format_spec, |(i, s)| (i, Some(s))),
    )(input)?;

    let input = char('}')(input)?;

    Some((input, Format { arg, spec }))
}

/// Parses an `argument` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`argument`]` := `[`integer`]` | `[`identifier`]
///
/// # Example
///
/// ```text
/// 0
/// ident
/// Минск
/// ```
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn argument(input: &str) -> Option<(LeftToParse<'_>, Argument)> {
    alt(&mut [
        &mut map(identifier, |(i, ident)| (i, Argument::Identifier(ident))),
        &mut map(integer, |(i, int)| (i, Argument::Integer(int))),
    ])(input)
}

/// Parses a `format_spec` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`format_spec`]` := [[fill]align][sign]['#']['0'][width]`
///                     `['.' `[`precision`]`]`[`type`]
///
/// # Example
///
/// ```text
/// ^
/// <^
/// ->+#0width$.precision$x?
/// ```
///
/// [`type`]: type_
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn format_spec(input: &str) -> Option<(LeftToParse<'_>, FormatSpec<'_>)> {
    let input = unwrap_or_else(
        alt(&mut [
            &mut try_seq(&mut [&mut any_char, &mut one_of("<^>")]),
            &mut one_of("<^>"),
        ]),
        identity,
    )(input);

    let input = seq(&mut [
        &mut optional(one_of("+-")),
        &mut optional(char('#')),
        &mut optional(try_seq(&mut [
            &mut char('0'),
            &mut lookahead(check_char(|c| !matches!(c, '$'))),
        ])),
    ])(input);

    let (input, width) = optional_result(count)(input);

    let (input, precision) = map_or_else(
        char('.'),
        |i| Some((i, None)),
        map(precision, |(i, p)| (i, Some(p))),
    )(input)?;

    let (input, ty) = type_(input)?;

    Some((
        input,
        FormatSpec {
            width,
            precision,
            ty,
        },
    ))
}

/// Parses a `precision` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`precision`]` := `[`count`]` | '*'`
///
/// # Example
///
/// ```text
/// 0
/// 42$
/// par$
/// *
/// ```
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn precision(input: &str) -> Option<(LeftToParse<'_>, Precision<'_>)> {
    alt(&mut [
        &mut map(count, |(i, c)| (i, Precision::Count(c))),
        &mut map(char('*'), |i| (i, Precision::Star)),
    ])(input)
}

/// Parses a `type` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`type`]` := '' | '?' | 'x?' | 'X?' | identifier`
///
/// # Example
///
/// All possible [`Type`]s.
///
/// ```text
/// ?
/// x?
/// X?
/// o
/// x
/// X
/// p
/// b
/// e
/// E
/// ```
///
/// [`type`]: type_
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn type_(input: &str) -> Option<(&str, Type)> {
    alt(&mut [
        &mut map(str("x?"), |i| (i, Type::LowerDebug)),
        &mut map(str("X?"), |i| (i, Type::UpperDebug)),
        &mut map(char('?'), |i| (i, Type::Debug)),
        &mut map(char('o'), |i| (i, Type::Octal)),
        &mut map(char('x'), |i| (i, Type::LowerHex)),
        &mut map(char('X'), |i| (i, Type::UpperHex)),
        &mut map(char('p'), |i| (i, Type::Pointer)),
        &mut map(char('b'), |i| (i, Type::Binary)),
        &mut map(char('e'), |i| (i, Type::LowerExp)),
        &mut map(char('E'), |i| (i, Type::UpperExp)),
        &mut map(lookahead(char('}')), |i| (i, Type::Display)),
    ])(input)
}

/// Parses a `count` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`count`]` := `[`parameter`]` | `[`integer`]
///
/// # Example
///
/// ```text
/// 0
/// 42$
/// par$
/// ```
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn count(input: &str) -> Option<(LeftToParse<'_>, Count<'_>)> {
    alt(&mut [
        &mut map(parameter, |(i, p)| (i, Count::Parameter(p))),
        &mut map(integer, |(i, int)| (i, Count::Integer(int))),
    ])(input)
}

/// Parses a `parameter` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// [`parameter`]` := `[`argument`]` '$'`
///
/// # Example
///
/// ```text
/// 42$
/// par$
/// ```
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn parameter(input: &str) -> Option<(LeftToParse<'_>, Parameter<'_>)> {
    and_then(argument, |(i, arg)| map(char('$'), |i| (i, arg))(i))(input)
}

/// Parses an `identifier` as defined in the [grammar spec][1].
///
/// # Grammar
///
/// `IDENTIFIER_OR_KEYWORD : XID_Start XID_Continue* | _ XID_Continue+`
///
/// See [rust reference][2] for more info.
///
/// # Example
///
/// ```text
/// identifier
/// Минск
/// ```
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
/// [2]: https://doc.rust-lang.org/reference/identifiers.html
fn identifier(input: &str) -> Option<(LeftToParse<'_>, Identifier<'_>)> {
    map(
        alt(&mut [
            &mut map(
                check_char(XID::is_xid_start),
                take_while0(check_char(XID::is_xid_continue)),
            ),
            &mut and_then(char('_'), take_while1(check_char(XID::is_xid_continue))),
        ]),
        |(i, _)| (i, &input[..(input.as_bytes().len() - i.as_bytes().len())]),
    )(input)
}

/// Parses an `integer` as defined in the [grammar spec][1].
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn integer(input: &str) -> Option<(LeftToParse<'_>, usize)> {
    and_then(
        take_while1(check_char(|c| c.is_ascii_digit())),
        |(i, int)| int.parse().ok().map(|int| (i, int)),
    )(input)
}

/// Parses a `text` as defined in the [grammar spec][1].
///
/// [1]: https://doc.rust-lang.org/stable/std/fmt/index.html#syntax
fn text(input: &str) -> Option<(LeftToParse<'_>, &str)> {
    take_until1(any_char, one_of("{}"))(input)
}

type Parser<'p> = &'p mut dyn FnMut(&str) -> &str;

/// Applies non-failing parsers in sequence.
fn seq<'p>(parsers: &'p mut [Parser<'p>]) -> impl FnMut(&str) -> LeftToParse<'_> + 'p {
    move |input| parsers.iter_mut().fold(input, |i, p| (**p)(i))
}

type FallibleParser<'p> = &'p mut dyn FnMut(&str) -> Option<&str>;

/// Tries to apply parsers in sequence. Returns [`None`] in case one of them
/// returned [`None`].
fn try_seq<'p>(
    parsers: &'p mut [FallibleParser<'p>],
) -> impl FnMut(&str) -> Option<LeftToParse<'_>> + 'p {
    move |input| parsers.iter_mut().try_fold(input, |i, p| (**p)(i))
}

/// Returns first successful parser or [`None`] in case all of them returned
/// [`None`].
fn alt<'p, 'i, T: 'i>(
    parsers: &'p mut [&'p mut dyn FnMut(&'i str) -> Option<T>],
) -> impl FnMut(&'i str) -> Option<T> + 'p {
    move |input| parsers.iter_mut().find_map(|p| (**p)(input))
}

/// Maps output of the parser in case it returned [`Some`].
fn map<'i, I: 'i, O: 'i>(
    mut parser: impl FnMut(&'i str) -> Option<I>,
    mut f: impl FnMut(I) -> O,
) -> impl FnMut(&'i str) -> Option<O> {
    move |input| parser(input).map(&mut f)
}

/// Maps output of the parser in case it returned [`Some`] or uses `default`.
fn map_or_else<'i, I: 'i, O: 'i>(
    mut parser: impl FnMut(&'i str) -> Option<I>,
    mut default: impl FnMut(&'i str) -> O,
    mut f: impl FnMut(I) -> O,
) -> impl FnMut(&'i str) -> O {
    move |input| parser(input).map_or_else(|| default(input), &mut f)
}

/// Returns the contained [`Some`] value or computes it from a closure.
fn unwrap_or_else<'i, O: 'i>(
    mut parser: impl FnMut(&'i str) -> Option<O>,
    mut f: impl FnMut(&'i str) -> O,
) -> impl FnMut(&'i str) -> O {
    move |input| parser(input).unwrap_or_else(|| f(input))
}

/// Returns [`None`] if the parser returned is [`None`], otherwise calls `f`
/// with the wrapped value and returns the result.
fn and_then<'i, I: 'i, O: 'i>(
    mut parser: impl FnMut(&'i str) -> Option<I>,
    mut f: impl FnMut(I) -> Option<O>,
) -> impl FnMut(&'i str) -> Option<O> {
    move |input| parser(input).and_then(&mut f)
}

/// Checks whether `parser` is successful while not advancing the original
/// `input`.
fn lookahead(
    mut parser: impl FnMut(&str) -> Option<&str>,
) -> impl FnMut(&str) -> Option<LeftToParse<'_>> {
    move |input| map(&mut parser, |_| input)(input)
}

/// Makes underlying `parser` optional by returning the original `input` in case
/// it returned [`None`].
fn optional(
    mut parser: impl FnMut(&str) -> Option<&str>,
) -> impl FnMut(&str) -> LeftToParse<'_> {
    move |input: &str| parser(input).unwrap_or(input)
}

/// Makes underlying `parser` optional by returning the original `input` and
/// [`None`] in case it returned [`None`].
fn optional_result<'i, T: 'i>(
    mut parser: impl FnMut(&'i str) -> Option<(&'i str, T)>,
) -> impl FnMut(&'i str) -> (LeftToParse<'i>, Option<T>) {
    move |input: &str| {
        map_or_else(&mut parser, |i| (i, None), |(i, c)| (i, Some(c)))(input)
    }
}

/// Parses while `parser` is successful. Never fails.
fn take_while0(
    mut parser: impl FnMut(&str) -> Option<&str>,
) -> impl FnMut(&str) -> (LeftToParse<'_>, &str) {
    move |input| {
        let mut cur = input;
        while let Some(step) = parser(cur) {
            cur = step;
        }
        (
            cur,
            &input[..(input.as_bytes().len() - cur.as_bytes().len())],
        )
    }
}

/// Parses while `parser` is successful. Returns [`None`] in case `parser` never
/// succeeded.
fn take_while1(
    mut parser: impl FnMut(&str) -> Option<&str>,
) -> impl FnMut(&str) -> Option<(LeftToParse<'_>, &str)> {
    move |input| {
        let mut cur = parser(input)?;
        while let Some(step) = parser(cur) {
            cur = step;
        }
        Some((
            cur,
            &input[..(input.as_bytes().len() - cur.as_bytes().len())],
        ))
    }
}

/// Parses with `basic` while `until` returns [`None`]. Returns [`None`] in case
/// `until` succeeded initially or `basic` never succeeded. Doesn't consume
/// [`char`]s parsed by `until`.
///
/// [`char`]: fn@char
fn take_until1(
    mut basic: impl FnMut(&str) -> Option<&str>,
    mut until: impl FnMut(&str) -> Option<&str>,
) -> impl FnMut(&str) -> Option<(LeftToParse<'_>, &str)> {
    move |input: &str| {
        if until(input).is_some() {
            return None;
        }
        let mut cur = basic(input)?;
        loop {
            if until(cur).is_some() {
                break;
            }
            let Some(b) = basic(cur) else {
                break;
            };
            cur = b;
        }

        Some((
            cur,
            &input[..(input.as_bytes().len() - cur.as_bytes().len())],
        ))
    }
}

/// Checks whether `input` starts with `s`.
fn str(s: &str) -> impl FnMut(&str) -> Option<LeftToParse<'_>> + '_ {
    move |input| input.starts_with(s).then(|| &input[s.as_bytes().len()..])
}

/// Checks whether `input` starts with `c`.
fn char(c: char) -> impl FnMut(&str) -> Option<LeftToParse<'_>> {
    move |input| input.starts_with(c).then(|| &input[c.len_utf8()..])
}

/// Checks whether first [`char`] suits `check`.
///
/// [`char`]: fn@char
fn check_char(
    mut check: impl FnMut(char) -> bool,
) -> impl FnMut(&str) -> Option<LeftToParse<'_>> {
    move |input| {
        input
            .chars()
            .next()
            .and_then(|c| check(c).then(|| &input[c.len_utf8()..]))
    }
}

/// Checks whether first [`char`] of input is present in `chars`.
///
/// [`char`]: fn@char
fn one_of(chars: &str) -> impl FnMut(&str) -> Option<LeftToParse<'_>> + '_ {
    move |input: &str| chars.chars().find_map(|c| char(c)(input))
}

/// Parses any [`char`].
///
/// [`char`]: fn@char
fn any_char(input: &str) -> Option<LeftToParse<'_>> {
    input.chars().next().map(|c| &input[c.len_utf8()..])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn text() {
        assert_eq!(format_string(""), Some(FormatString { formats: vec![] }));
        assert_eq!(
            format_string("test"),
            Some(FormatString { formats: vec![] }),
        );
        assert_eq!(
            format_string("Минск"),
            Some(FormatString { formats: vec![] }),
        );
        assert_eq!(format_string("🦀"), Some(FormatString { formats: vec![] }));
    }

    #[test]
    fn argument() {
        assert_eq!(
            format_string("{}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: None,
                }],
            }),
        );
        assert_eq!(
            format_string("{0}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: Some(Argument::Integer(0)),
                    spec: None,
                }],
            }),
        );
        assert_eq!(
            format_string("{par}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: Some(Argument::Identifier("par")),
                    spec: None,
                }],
            }),
        );
        assert_eq!(
            format_string("{Минск}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: Some(Argument::Identifier("Минск")),
                    spec: None,
                }],
            }),
        );
    }

    #[test]
    fn spec() {
        assert_eq!(
            format_string("{:}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:^}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:-<}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{: <}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:^<}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:+}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:^<-}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:#}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:+#}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:-<#}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:^<-#}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:0}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:#0}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:-0}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:^<0}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:^<+#0}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:1}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: Some(Count::Integer(1)),
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:1$}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: Some(Count::Parameter(Argument::Integer(1))),
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:par$}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: Some(Count::Parameter(Argument::Identifier("par"))),
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:-^-#0Минск$}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: Some(Count::Parameter(Argument::Identifier("Минск"))),
                        precision: None,
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:.*}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: Some(Precision::Star),
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:.0}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: Some(Precision::Count(Count::Integer(0))),
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:.0$}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: Some(Precision::Count(Count::Parameter(
                            Argument::Integer(0),
                        ))),
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:.par$}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: Some(Precision::Count(Count::Parameter(
                            Argument::Identifier("par"),
                        ))),
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{: >+#2$.par$}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: Some(Count::Parameter(Argument::Integer(2))),
                        precision: Some(Precision::Count(Count::Parameter(
                            Argument::Identifier("par"),
                        ))),
                        ty: Type::Display,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:x?}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::LowerDebug,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{:E}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: None,
                        precision: None,
                        ty: Type::UpperExp,
                    }),
                }],
            }),
        );
        assert_eq!(
            format_string("{: >+#par$.par$X?}"),
            Some(FormatString {
                formats: vec![Format {
                    arg: None,
                    spec: Some(FormatSpec {
                        width: Some(Count::Parameter(Argument::Identifier("par"))),
                        precision: Some(Precision::Count(Count::Parameter(
                            Argument::Identifier("par"),
                        ))),
                        ty: Type::UpperDebug,
                    }),
                }],
            }),
        );
    }

    #[test]
    fn full() {
        assert_eq!(
            format_string("prefix{{{0:#?}postfix{par:-^par$.a$}}}"),
            Some(FormatString {
                formats: vec![
                    Format {
                        arg: Some(Argument::Integer(0)),
                        spec: Some(FormatSpec {
                            width: None,
                            precision: None,
                            ty: Type::Debug,
                        }),
                    },
                    Format {
                        arg: Some(Argument::Identifier("par")),
                        spec: Some(FormatSpec {
                            width: Some(Count::Parameter(Argument::Identifier("par"))),
                            precision: Some(Precision::Count(Count::Parameter(
                                Argument::Identifier("a"),
                            ))),
                            ty: Type::Display,
                        }),
                    },
                ],
            }),
        );
    }

    #[test]
    fn error() {
        assert_eq!(format_string("{"), None);
        assert_eq!(format_string("}"), None);
        assert_eq!(format_string("{{}"), None);
        assert_eq!(format_string("{:x?"), None);
        assert_eq!(format_string("{:.}"), None);
        assert_eq!(format_string("{:q}"), None);
        assert_eq!(format_string("{:par}"), None);
        assert_eq!(format_string("{⚙️}"), None);
    }
}
