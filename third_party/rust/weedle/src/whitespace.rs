use {CompleteStr, IResult};

pub fn sp(input: CompleteStr) -> IResult<CompleteStr, CompleteStr> {
    recognize!(
        input,
        many0!(
            alt!(
                do_parse!(tag!("//") >> take_until!("\n") >> char!('\n') >> (()))
                |
                map!(
                    take_while1!(|c| c == '\t' || c == '\n' || c == '\r' || c == ' '),
                    |_| ()
                )
                |
                do_parse!(
                    tag!("/*") >>
                    take_until!("*/") >>
                    tag!("*/") >>
                    (())
                )
            )
        )
    )
}

/// ws! also ignores line & block comments
macro_rules! ws (
    ($i:expr, $($args:tt)*) => ({
        use $crate::whitespace::sp;
        use $crate::nom::Convert;
        use $crate::nom::Err;
        use $crate::nom::lib::std::result::Result::*;

        match sep!($i, sp, $($args)*) {
            Err(e) => Err(e),
            Ok((i1, o)) => {
                match (sp)(i1) {
                    Err(e) => Err(Err::convert(e)),
                    Ok((i2, _)) => Ok((i2, o))
                }
            }
        }
    });
);
