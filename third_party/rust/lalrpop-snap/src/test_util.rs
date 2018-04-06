use diff;
use grammar::parse_tree as pt;
use grammar::repr as r;
use normalize::NormError;
use regex::Regex;
use std::fmt::{Debug, Error, Formatter};

thread_local! {
    static SPAN: Regex =
        Regex::new(r"Span\([0-9 ,]*\)").unwrap()
}

struct ExpectedDebug<'a>(&'a str);

impl<'a> Debug for ExpectedDebug<'a> {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        write!(fmt, "{}", self.0)
    }
}

pub fn expect_debug<D: Debug>(actual: D, expected: &str) {
    compare(
        ExpectedDebug(&format!("{:#?}", actual)),
        ExpectedDebug(expected),
    )
}

pub fn compare<D: Debug, E: Debug>(actual: D, expected: E) {
    let actual_s = format!("{:?}", actual);
    let expected_s = format!("{:?}", expected);

    SPAN.with(|span| {
        let actual_s = span.replace_all(&actual_s, "Span(..)");
        let expected_s = span.replace_all(&expected_s, "Span(..)");
        if actual_s != expected_s {
            let actual_s = format!("{:#?}", actual);
            let expected_s = format!("{:#?}", expected);

            for diff in diff::lines(&actual_s, &expected_s) {
                match diff {
                    diff::Result::Right(r) => println!("- {}", r),
                    diff::Result::Left(l) => println!("+ {}", l),
                    diff::Result::Both(l, _) => println!("  {}", l),
                }
            }

            assert!(false);
        }
    });
}

pub fn normalized_grammar(s: &str) -> r::Grammar {
    ::normalize::normalize_without_validating(::parser::parse_grammar(s).unwrap()).unwrap()
}

pub fn check_norm_err(expected_err: &str, span: &str, err: NormError) {
    let expected_err = Regex::new(expected_err).unwrap();
    let start_index = span.find("~").unwrap();
    let end_index = span.rfind("~").unwrap() + 1;
    assert!(start_index <= end_index);
    assert_eq!(err.span, pt::Span(start_index, end_index));
    assert!(
        expected_err.is_match(&err.message),
        "unexpected error text `{}`, which did not match regular expression `{}`",
        err.message,
        expected_err
    );
}
