use nom::{IResult, Parser};

pub(crate) fn sp(input: &str) -> IResult<&str, &str> {
    nom::combinator::recognize(nom::multi::many0(nom::branch::alt((
        // ignores line comments
        nom::combinator::value(
            (),
            nom::sequence::tuple((
                nom::bytes::complete::tag("//"),
                nom::bytes::complete::take_until("\n"),
                nom::bytes::complete::tag("\n"),
            )),
        ),
        // ignores whitespace
        nom::combinator::value((), nom::character::complete::multispace1),
        // ignores block comments
        nom::combinator::value(
            (),
            nom::sequence::tuple((
                nom::bytes::complete::tag("/*"),
                nom::bytes::complete::take_until("*/"),
                nom::bytes::complete::tag("*/"),
            )),
        ),
    ))))(input)
}

/// ws also ignores line & block comments
pub(crate) fn ws<'a, F>(inner: F) -> impl FnMut(&'a str) -> IResult<&str, &str>
where
    F: Parser<&'a str, &'a str, nom::error::Error<&'a str>>,
{
    nom::sequence::delimited(sp, inner, sp)
}
