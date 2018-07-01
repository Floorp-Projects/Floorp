extern crate proc_macro2;

use std::str::{self, FromStr};

use proc_macro2::{Literal, Span, Term, TokenStream, TokenTree};

#[test]
fn symbols() {
    assert_eq!(Term::new("foo", Span::call_site()).as_str(), "foo");
    assert_eq!(Term::new("bar", Span::call_site()).as_str(), "bar");
}

#[test]
fn literals() {
    assert_eq!(Literal::string("foo").to_string(), "\"foo\"");
    assert_eq!(Literal::string("\"").to_string(), "\"\\\"\"");
    assert_eq!(Literal::f32_unsuffixed(10.0).to_string(), "10.0");
}

#[test]
fn roundtrip() {
    fn roundtrip(p: &str) {
        println!("parse: {}", p);
        let s = p.parse::<TokenStream>().unwrap().to_string();
        println!("first: {}", s);
        let s2 = s.to_string().parse::<TokenStream>().unwrap().to_string();
        assert_eq!(s, s2);
    }
    roundtrip("a");
    roundtrip("<<");
    roundtrip("<<=");
    roundtrip(
        "
        1
        1.0
        1f32
        2f64
        1usize
        4isize
        4e10
        1_000
        1_0i32
        8u8
        9
        0
        0xffffffffffffffffffffffffffffffff
    ",
    );
    roundtrip("'a");
    roundtrip("'static");
    roundtrip("'\\u{10__FFFF}'");
    roundtrip("\"\\u{10_F0FF__}foo\\u{1_0_0_0__}\"");
}

#[test]
fn fail() {
    fn fail(p: &str) {
        if p.parse::<TokenStream>().is_ok() {
            panic!("should have failed to parse: {}", p);
        }
    }
    fail("1x");
    fail("1u80");
    fail("1f320");
    fail("' static");
    fail("'mut");
    fail("r#1");
    fail("r#_");
}

#[cfg(procmacro2_semver_exempt)]
#[test]
fn span_test() {
    use proc_macro2::TokenTree;

    fn check_spans(p: &str, mut lines: &[(usize, usize, usize, usize)]) {
        let ts = p.parse::<TokenStream>().unwrap();
        check_spans_internal(ts, &mut lines);
    }

    fn check_spans_internal(ts: TokenStream, lines: &mut &[(usize, usize, usize, usize)]) {
        for i in ts {
            if let Some((&(sline, scol, eline, ecol), rest)) = lines.split_first() {
                *lines = rest;

                let start = i.span().start();
                assert_eq!(start.line, sline, "sline did not match for {}", i);
                assert_eq!(start.column, scol, "scol did not match for {}", i);

                let end = i.span().end();
                assert_eq!(end.line, eline, "eline did not match for {}", i);
                assert_eq!(end.column, ecol, "ecol did not match for {}", i);

                match i {
                    TokenTree::Group(ref g) => {
                        check_spans_internal(g.stream().clone(), lines);
                    }
                    _ => {}
                }
            }
        }
    }

    check_spans(
        "\
/// This is a document comment
testing 123
{
  testing 234
}",
        &[
            (1, 0, 1, 30),  // #
            (1, 0, 1, 30),  // [ ... ]
            (1, 0, 1, 30),  // doc
            (1, 0, 1, 30),  // =
            (1, 0, 1, 30),  // "This is..."
            (2, 0, 2, 7),   // testing
            (2, 8, 2, 11),  // 123
            (3, 0, 5, 1),   // { ... }
            (4, 2, 4, 9),   // testing
            (4, 10, 4, 13), // 234
        ],
    );
}

#[cfg(procmacro2_semver_exempt)]
#[cfg(not(feature = "nightly"))]
#[test]
fn default_span() {
    let start = Span::call_site().start();
    assert_eq!(start.line, 1);
    assert_eq!(start.column, 0);
    let end = Span::call_site().end();
    assert_eq!(end.line, 1);
    assert_eq!(end.column, 0);
    let source_file = Span::call_site().source_file();
    assert_eq!(source_file.path().to_string(), "<unspecified>");
    assert!(!source_file.is_real());
}

#[cfg(procmacro2_semver_exempt)]
#[test]
fn span_join() {
    let source1 = "aaa\nbbb"
        .parse::<TokenStream>()
        .unwrap()
        .into_iter()
        .collect::<Vec<_>>();
    let source2 = "ccc\nddd"
        .parse::<TokenStream>()
        .unwrap()
        .into_iter()
        .collect::<Vec<_>>();

    assert!(source1[0].span().source_file() != source2[0].span().source_file());
    assert_eq!(
        source1[0].span().source_file(),
        source1[1].span().source_file()
    );

    let joined1 = source1[0].span().join(source1[1].span());
    let joined2 = source1[0].span().join(source2[0].span());
    assert!(joined1.is_some());
    assert!(joined2.is_none());

    let start = joined1.unwrap().start();
    let end = joined1.unwrap().end();
    assert_eq!(start.line, 1);
    assert_eq!(start.column, 0);
    assert_eq!(end.line, 2);
    assert_eq!(end.column, 3);

    assert_eq!(
        joined1.unwrap().source_file(),
        source1[0].span().source_file()
    );
}

#[test]
fn no_panic() {
    let s = str::from_utf8(b"b\'\xc2\x86  \x00\x00\x00^\"").unwrap();
    assert!(s.parse::<proc_macro2::TokenStream>().is_err());
}

#[test]
fn tricky_doc_comment() {
    let stream = "/**/".parse::<proc_macro2::TokenStream>().unwrap();
    let tokens = stream.into_iter().collect::<Vec<_>>();
    assert!(tokens.is_empty(), "not empty -- {:?}", tokens);

    let stream = "/// doc".parse::<proc_macro2::TokenStream>().unwrap();
    let tokens = stream.into_iter().collect::<Vec<_>>();
    assert!(tokens.len() == 2, "not length 2 -- {:?}", tokens);
    match tokens[0] {
        proc_macro2::TokenTree::Op(ref tt) => assert_eq!(tt.op(), '#'),
        _ => panic!("wrong token {:?}", tokens[0]),
    }
    let mut tokens = match tokens[1] {
        proc_macro2::TokenTree::Group(ref tt) => {
            assert_eq!(tt.delimiter(), proc_macro2::Delimiter::Bracket);
            tt.stream().into_iter()
        }
        _ => panic!("wrong token {:?}", tokens[0]),
    };

    match tokens.next().unwrap() {
        proc_macro2::TokenTree::Term(ref tt) => assert_eq!(tt.as_str(), "doc"),
        t => panic!("wrong token {:?}", t),
    }
    match tokens.next().unwrap() {
        proc_macro2::TokenTree::Op(ref tt) => assert_eq!(tt.op(), '='),
        t => panic!("wrong token {:?}", t),
    }
    match tokens.next().unwrap() {
        proc_macro2::TokenTree::Literal(ref tt) => {
            assert_eq!(tt.to_string(), "\" doc\"");
        }
        t => panic!("wrong token {:?}", t),
    }
    assert!(tokens.next().is_none());

    let stream = "//! doc".parse::<proc_macro2::TokenStream>().unwrap();
    let tokens = stream.into_iter().collect::<Vec<_>>();
    assert!(tokens.len() == 3, "not length 3 -- {:?}", tokens);
}

#[test]
fn raw_identifier() {
    let mut tts = TokenStream::from_str("r#dyn").unwrap().into_iter();
    match tts.next().unwrap() {
        TokenTree::Term(raw) => assert_eq!("r#dyn", raw.as_str()),
        wrong => panic!("wrong token {:?}", wrong),
    }
    assert!(tts.next().is_none());
}
