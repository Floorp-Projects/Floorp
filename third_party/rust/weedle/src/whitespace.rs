use crate::IResult;

pub(crate) fn sp(input: &str) -> IResult<&str, &str> {
    recognize!(
        input,
        many0!(alt!(
            // ignores line comments
            do_parse!(tag!("//") >> take_until!("\n") >> char!('\n') >> (()))
            |
            // ignores whitespace
            map!(take_while1!(|c| c == '\t' || c == '\n' || c == '\r' || c == ' '), |_| ())
            |
            // ignores block comments
            do_parse!(tag!("/*") >> take_until!("*/") >> tag!("*/") >> (()))
        ))
    )
}

/// ws! also ignores line & block comments
macro_rules! ws (
    ($i:expr, $($args:tt)*) => ({
        use $crate::whitespace::sp;

        do_parse!($i,
            sp >>
            s: $($args)* >>
            sp >>
            (s)
        )
    });
);
