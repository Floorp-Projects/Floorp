use crate::config::Syntax;
use crate::parser::{Expr, Node, Whitespace, Ws};

fn check_ws_split(s: &str, res: &(&str, &str, &str)) {
    match super::split_ws_parts(s) {
        Node::Lit(lws, s, rws) => {
            assert_eq!(lws, res.0);
            assert_eq!(s, res.1);
            assert_eq!(rws, res.2);
        }
        _ => {
            panic!("fail");
        }
    }
}

#[test]
fn test_ws_splitter() {
    check_ws_split("", &("", "", ""));
    check_ws_split("a", &("", "a", ""));
    check_ws_split("\ta", &("\t", "a", ""));
    check_ws_split("b\n", &("", "b", "\n"));
    check_ws_split(" \t\r\n", &(" \t\r\n", "", ""));
}

#[test]
#[should_panic]
fn test_invalid_block() {
    super::parse("{% extend \"blah\" %}", &Syntax::default()).unwrap();
}

#[test]
fn test_parse_filter() {
    use Expr::*;
    let syntax = Syntax::default();
    assert_eq!(
        super::parse("{{ strvar|e }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Filter("e", vec![Var("strvar")]),)],
    );
    assert_eq!(
        super::parse("{{ 2|abs }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Filter("abs", vec![NumLit("2")]),)],
    );
    assert_eq!(
        super::parse("{{ -2|abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Filter("abs", vec![Unary("-", NumLit("2").into())]),
        )],
    );
    assert_eq!(
        super::parse("{{ (1 - 2)|abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Filter(
                "abs",
                vec![Group(
                    BinOp("-", NumLit("1").into(), NumLit("2").into()).into()
                )]
            ),
        )],
    );
}

#[test]
fn test_parse_numbers() {
    let syntax = Syntax::default();
    assert_eq!(
        super::parse("{{ 2 }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::NumLit("2"),)],
    );
    assert_eq!(
        super::parse("{{ 2.5 }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::NumLit("2.5"),)],
    );
}

#[test]
fn test_parse_var() {
    let s = Syntax::default();

    assert_eq!(
        super::parse("{{ foo }}", &s).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::Var("foo"))],
    );
    assert_eq!(
        super::parse("{{ foo_bar }}", &s).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::Var("foo_bar"))],
    );

    assert_eq!(
        super::parse("{{ none }}", &s).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::Var("none"))],
    );
}

#[test]
fn test_parse_const() {
    let s = Syntax::default();

    assert_eq!(
        super::parse("{{ FOO }}", &s).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::Path(vec!["FOO"]))],
    );
    assert_eq!(
        super::parse("{{ FOO_BAR }}", &s).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::Path(vec!["FOO_BAR"]))],
    );

    assert_eq!(
        super::parse("{{ NONE }}", &s).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::Path(vec!["NONE"]))],
    );
}

#[test]
fn test_parse_path() {
    let s = Syntax::default();

    assert_eq!(
        super::parse("{{ None }}", &s).unwrap(),
        vec![Node::Expr(Ws(None, None), Expr::Path(vec!["None"]))],
    );
    assert_eq!(
        super::parse("{{ Some(123) }}", &s).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Call(
                Box::new(Expr::Path(vec!["Some"])),
                vec![Expr::NumLit("123")]
            ),
        )],
    );

    assert_eq!(
        super::parse("{{ Ok(123) }}", &s).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Call(Box::new(Expr::Path(vec!["Ok"])), vec![Expr::NumLit("123")]),
        )],
    );
    assert_eq!(
        super::parse("{{ Err(123) }}", &s).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Call(Box::new(Expr::Path(vec!["Err"])), vec![Expr::NumLit("123")]),
        )],
    );
}

#[test]
fn test_parse_var_call() {
    assert_eq!(
        super::parse("{{ function(\"123\", 3) }}", &Syntax::default()).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Call(
                Box::new(Expr::Var("function")),
                vec![Expr::StrLit("123"), Expr::NumLit("3")]
            ),
        )],
    );
}

#[test]
fn test_parse_path_call() {
    let s = Syntax::default();

    assert_eq!(
        super::parse("{{ Option::None }}", &s).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Path(vec!["Option", "None"])
        )],
    );
    assert_eq!(
        super::parse("{{ Option::Some(123) }}", &s).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Call(
                Box::new(Expr::Path(vec!["Option", "Some"])),
                vec![Expr::NumLit("123")],
            ),
        )],
    );

    assert_eq!(
        super::parse("{{ self::function(\"123\", 3) }}", &s).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Call(
                Box::new(Expr::Path(vec!["self", "function"])),
                vec![Expr::StrLit("123"), Expr::NumLit("3")],
            ),
        )],
    );
}

#[test]
fn test_parse_root_path() {
    let syntax = Syntax::default();
    assert_eq!(
        super::parse("{{ std::string::String::new() }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Call(
                Box::new(Expr::Path(vec!["std", "string", "String", "new"])),
                vec![]
            ),
        )],
    );
    assert_eq!(
        super::parse("{{ ::std::string::String::new() }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Expr::Call(
                Box::new(Expr::Path(vec!["", "std", "string", "String", "new"])),
                vec![]
            ),
        )],
    );
}

#[test]
fn change_delimiters_parse_filter() {
    let syntax = Syntax {
        expr_start: "{=",
        expr_end: "=}",
        ..Syntax::default()
    };

    super::parse("{= strvar|e =}", &syntax).unwrap();
}

#[test]
fn test_precedence() {
    use Expr::*;
    let syntax = Syntax::default();
    assert_eq!(
        super::parse("{{ a + b == c }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "==",
                BinOp("+", Var("a").into(), Var("b").into()).into(),
                Var("c").into(),
            )
        )],
    );
    assert_eq!(
        super::parse("{{ a + b * c - d / e }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "-",
                BinOp(
                    "+",
                    Var("a").into(),
                    BinOp("*", Var("b").into(), Var("c").into()).into(),
                )
                .into(),
                BinOp("/", Var("d").into(), Var("e").into()).into(),
            )
        )],
    );
    assert_eq!(
        super::parse("{{ a * (b + c) / -d }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "/",
                BinOp(
                    "*",
                    Var("a").into(),
                    Group(BinOp("+", Var("b").into(), Var("c").into()).into()).into()
                )
                .into(),
                Unary("-", Var("d").into()).into()
            )
        )],
    );
    assert_eq!(
        super::parse("{{ a || b && c || d && e }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "||",
                BinOp(
                    "||",
                    Var("a").into(),
                    BinOp("&&", Var("b").into(), Var("c").into()).into(),
                )
                .into(),
                BinOp("&&", Var("d").into(), Var("e").into()).into(),
            )
        )],
    );
}

#[test]
fn test_associativity() {
    use Expr::*;
    let syntax = Syntax::default();
    assert_eq!(
        super::parse("{{ a + b + c }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "+",
                BinOp("+", Var("a").into(), Var("b").into()).into(),
                Var("c").into()
            )
        )],
    );
    assert_eq!(
        super::parse("{{ a * b * c }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "*",
                BinOp("*", Var("a").into(), Var("b").into()).into(),
                Var("c").into()
            )
        )],
    );
    assert_eq!(
        super::parse("{{ a && b && c }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "&&",
                BinOp("&&", Var("a").into(), Var("b").into()).into(),
                Var("c").into()
            )
        )],
    );
    assert_eq!(
        super::parse("{{ a + b - c + d }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "+",
                BinOp(
                    "-",
                    BinOp("+", Var("a").into(), Var("b").into()).into(),
                    Var("c").into()
                )
                .into(),
                Var("d").into()
            )
        )],
    );
    assert_eq!(
        super::parse("{{ a == b != c > d > e == f }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "==",
                BinOp(
                    ">",
                    BinOp(
                        ">",
                        BinOp(
                            "!=",
                            BinOp("==", Var("a").into(), Var("b").into()).into(),
                            Var("c").into()
                        )
                        .into(),
                        Var("d").into()
                    )
                    .into(),
                    Var("e").into()
                )
                .into(),
                Var("f").into()
            )
        )],
    );
}

#[test]
fn test_odd_calls() {
    use Expr::*;
    let syntax = Syntax::default();
    assert_eq!(
        super::parse("{{ a[b](c) }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Call(
                Box::new(Index(Box::new(Var("a")), Box::new(Var("b")))),
                vec![Var("c")],
            ),
        )],
    );
    assert_eq!(
        super::parse("{{ (a + b)(c) }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Call(
                Box::new(Group(Box::new(BinOp(
                    "+",
                    Box::new(Var("a")),
                    Box::new(Var("b"))
                )))),
                vec![Var("c")],
            ),
        )],
    );
    assert_eq!(
        super::parse("{{ a + b(c) }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "+",
                Box::new(Var("a")),
                Box::new(Call(Box::new(Var("b")), vec![Var("c")])),
            ),
        )],
    );
    assert_eq!(
        super::parse("{{ (-a)(b) }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Call(
                Box::new(Group(Box::new(Unary("-", Box::new(Var("a")))))),
                vec![Var("b")],
            ),
        )],
    );
    assert_eq!(
        super::parse("{{ -a(b) }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Unary("-", Box::new(Call(Box::new(Var("a")), vec![Var("b")])),),
        )],
    );
}

#[test]
fn test_parse_comments() {
    let s = &Syntax::default();

    assert_eq!(
        super::parse("{##}", s).unwrap(),
        vec![Node::Comment(Ws(None, None))],
    );
    assert_eq!(
        super::parse("{#- #}", s).unwrap(),
        vec![Node::Comment(Ws(Some(Whitespace::Suppress), None))],
    );
    assert_eq!(
        super::parse("{# -#}", s).unwrap(),
        vec![Node::Comment(Ws(None, Some(Whitespace::Suppress)))],
    );
    assert_eq!(
        super::parse("{#--#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Suppress),
            Some(Whitespace::Suppress)
        ))],
    );
    assert_eq!(
        super::parse("{#- foo\n bar -#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Suppress),
            Some(Whitespace::Suppress)
        ))],
    );
    assert_eq!(
        super::parse("{#- foo\n {#- bar\n -#} baz -#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Suppress),
            Some(Whitespace::Suppress)
        ))],
    );
    assert_eq!(
        super::parse("{#+ #}", s).unwrap(),
        vec![Node::Comment(Ws(Some(Whitespace::Preserve), None))],
    );
    assert_eq!(
        super::parse("{# +#}", s).unwrap(),
        vec![Node::Comment(Ws(None, Some(Whitespace::Preserve)))],
    );
    assert_eq!(
        super::parse("{#++#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Preserve),
            Some(Whitespace::Preserve)
        ))],
    );
    assert_eq!(
        super::parse("{#+ foo\n bar +#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Preserve),
            Some(Whitespace::Preserve)
        ))],
    );
    assert_eq!(
        super::parse("{#+ foo\n {#+ bar\n +#} baz -+#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Preserve),
            Some(Whitespace::Preserve)
        ))],
    );
    assert_eq!(
        super::parse("{#~ #}", s).unwrap(),
        vec![Node::Comment(Ws(Some(Whitespace::Minimize), None))],
    );
    assert_eq!(
        super::parse("{# ~#}", s).unwrap(),
        vec![Node::Comment(Ws(None, Some(Whitespace::Minimize)))],
    );
    assert_eq!(
        super::parse("{#~~#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Minimize),
            Some(Whitespace::Minimize)
        ))],
    );
    assert_eq!(
        super::parse("{#~ foo\n bar ~#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Minimize),
            Some(Whitespace::Minimize)
        ))],
    );
    assert_eq!(
        super::parse("{#~ foo\n {#~ bar\n ~#} baz -~#}", s).unwrap(),
        vec![Node::Comment(Ws(
            Some(Whitespace::Minimize),
            Some(Whitespace::Minimize)
        ))],
    );

    assert_eq!(
        super::parse("{# foo {# bar #} {# {# baz #} qux #} #}", s).unwrap(),
        vec![Node::Comment(Ws(None, None))],
    );
}

#[test]
fn test_parse_tuple() {
    use super::Expr::*;
    let syntax = Syntax::default();
    assert_eq!(
        super::parse("{{ () }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Tuple(vec![]),)],
    );
    assert_eq!(
        super::parse("{{ (1) }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Group(Box::new(NumLit("1"))),)],
    );
    assert_eq!(
        super::parse("{{ (1,) }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Tuple(vec![NumLit("1")]),)],
    );
    assert_eq!(
        super::parse("{{ (1, ) }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Tuple(vec![NumLit("1")]),)],
    );
    assert_eq!(
        super::parse("{{ (1 ,) }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Tuple(vec![NumLit("1")]),)],
    );
    assert_eq!(
        super::parse("{{ (1 , ) }}", &syntax).unwrap(),
        vec![Node::Expr(Ws(None, None), Tuple(vec![NumLit("1")]),)],
    );
    assert_eq!(
        super::parse("{{ (1, 2) }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Tuple(vec![NumLit("1"), NumLit("2")]),
        )],
    );
    assert_eq!(
        super::parse("{{ (1, 2,) }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Tuple(vec![NumLit("1"), NumLit("2")]),
        )],
    );
    assert_eq!(
        super::parse("{{ (1, 2, 3) }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Tuple(vec![NumLit("1"), NumLit("2"), NumLit("3")]),
        )],
    );
    assert_eq!(
        super::parse("{{ ()|abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Filter("abs", vec![Tuple(vec![])]),
        )],
    );
    assert_eq!(
        super::parse("{{ () | abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp("|", Box::new(Tuple(vec![])), Box::new(Var("abs"))),
        )],
    );
    assert_eq!(
        super::parse("{{ (1)|abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Filter("abs", vec![Group(Box::new(NumLit("1")))]),
        )],
    );
    assert_eq!(
        super::parse("{{ (1) | abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "|",
                Box::new(Group(Box::new(NumLit("1")))),
                Box::new(Var("abs"))
            ),
        )],
    );
    assert_eq!(
        super::parse("{{ (1,)|abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Filter("abs", vec![Tuple(vec![NumLit("1")])]),
        )],
    );
    assert_eq!(
        super::parse("{{ (1,) | abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "|",
                Box::new(Tuple(vec![NumLit("1")])),
                Box::new(Var("abs"))
            ),
        )],
    );
    assert_eq!(
        super::parse("{{ (1, 2)|abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            Filter("abs", vec![Tuple(vec![NumLit("1"), NumLit("2")])]),
        )],
    );
    assert_eq!(
        super::parse("{{ (1, 2) | abs }}", &syntax).unwrap(),
        vec![Node::Expr(
            Ws(None, None),
            BinOp(
                "|",
                Box::new(Tuple(vec![NumLit("1"), NumLit("2")])),
                Box::new(Var("abs"))
            ),
        )],
    );
}

#[test]
fn test_missing_space_after_kw() {
    let syntax = Syntax::default();
    let err = super::parse("{%leta=b%}", &syntax).unwrap_err();
    assert!(matches!(
        &*err.msg,
        "unable to parse template:\n\n\"{%leta=b%}\""
    ));
}
