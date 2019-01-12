use lexer::dfa::interpret::interpret;
use lexer::dfa::{self, DFAConstructionError, NFAIndex, Precedence, DFA};
use lexer::re;

pub fn dfa(inputs: &[(&str, Precedence)]) -> Result<DFA, DFAConstructionError> {
    let regexs: Result<Vec<_>, _> = inputs.iter().map(|&(s, _)| re::parse_regex(s)).collect();
    let regexs = match regexs {
        Ok(rs) => rs,
        Err(_) => panic!("unexpected parse error"),
    };
    let precedences: Vec<_> = inputs.iter().map(|&(_, p)| p).collect();
    dfa::build_dfa(&regexs, &precedences)
}

const P1: Precedence = Precedence(1);
const P0: Precedence = Precedence(0);

#[test]
fn tokenizer() {
    let dfa = dfa(&[
        /* 0 */ (r#"class"#, P1),
        /* 1 */ (r#"[a-zA-Z_][a-zA-Z0-9_]*"#, P0),
        /* 2 */ (r#"[0-9]+"#, P0),
        /* 3 */ (r#" +"#, P0),
        /* 4 */ (r#">>"#, P0),
        /* 5 */ (r#">"#, P0),
    ])
    .unwrap();

    assert_eq!(interpret(&dfa, "class Foo"), Some((NFAIndex(0), "class")));
    assert_eq!(interpret(&dfa, "classz Foo"), Some((NFAIndex(1), "classz")));
    assert_eq!(interpret(&dfa, "123"), Some((NFAIndex(2), "123")));
    assert_eq!(interpret(&dfa, "  classz Foo"), Some((NFAIndex(3), "  ")));
    assert_eq!(interpret(&dfa, ">"), Some((NFAIndex(5), ">")));
    assert_eq!(interpret(&dfa, ">>"), Some((NFAIndex(4), ">>")));
}

#[test]
fn ambiguous_regex() {
    // here the keyword and the regex have same precedence, so we have
    // an ambiguity
    assert!(dfa(&[(r#"class"#, P0), (r#"[a-zA-Z_][a-zA-Z0-9_]*"#, P0)]).is_err());
}

#[test]
fn issue_32() {
    assert!(dfa(&[(r#"."#, P0)]).is_ok());
}

#[test]
fn issue_35() {
    assert!(dfa(&[(r#".*"#, P0), (r"[-+]?[0-9]*\.?[0-9]+", P0)]).is_err());
}

#[test]
fn alternatives() {
    let dfa = dfa(&[(r#"abc|abd"#, P0)]).unwrap();
    assert_eq!(interpret(&dfa, "abc"), Some((NFAIndex(0), "abc")));
    assert_eq!(interpret(&dfa, "abd"), Some((NFAIndex(0), "abd")));
    assert_eq!(interpret(&dfa, "123"), None);
}

#[test]
fn alternatives_extension() {
    let dfa = dfa(&[(r#"abc|abcd"#, P0)]).unwrap();
    assert_eq!(interpret(&dfa, "abc"), Some((NFAIndex(0), "abc")));
    assert_eq!(interpret(&dfa, "abcd"), Some((NFAIndex(0), "abcd")));
    assert_eq!(interpret(&dfa, "123"), None);
}

#[test]
fn alternatives_contraction() {
    let dfa = dfa(&[(r#"abcd|abc"#, P0)]).unwrap();
    assert_eq!(interpret(&dfa, "abc"), Some((NFAIndex(0), "abc")));
    assert_eq!(interpret(&dfa, "abcd"), Some((NFAIndex(0), "abcd")));
    assert_eq!(interpret(&dfa, "123"), None);
}
