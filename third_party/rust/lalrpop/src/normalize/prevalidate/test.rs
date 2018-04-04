use parser;
use test_util;

fn check_err(expected_err: &str, grammar: &str, span: &str) {
    let parsed_grammar = parser::parse_grammar(&grammar).unwrap();
    let err = super::validate(&parsed_grammar).unwrap_err();
    test_util::check_norm_err(expected_err, span, err);
}

#[test]
fn named_symbols() {
    check_err(
        r#"named symbols \(like `"Num"`\) require a custom action"#,
        r#"grammar; Term = { <n:"Num"> };"#,
        r#"                     ~~~~~    "#,
    );
}

#[test]
fn bad_assoc_type() {
    check_err(
        r#"associated type `Foo` not recognized"#,
        r#"grammar; extern { type Foo = i32; enum Tok { } }"#,
        r#"                       ~~~                      "#,
    );
}

#[test]
fn dup_assoc_type() {
    check_err(
        r#"associated type `Location` already specified"#,
        r#"grammar; extern { type Location = i32; type Location = u32; enum Tok { } }"#,
        r#"                                            ~~~~~~~~                      "#,
    );
}

#[test]
fn lookahead_without_loc_type() {
    check_err(
        r#"lookahead/lookbehind require you to declare the type of a location"#,
        r#"grammar; extern { enum Tok { } } Foo = @L;"#,
        r#"                                       ~~ "#,
    );
}

#[test]
fn multiple_extern_token() {
    check_err(
        r#"multiple extern definitions are not permitted"#,
        r#"grammar; extern { enum Tok { } } extern { enum Tok { } }"#,
        r#"                                 ~~~~~~                 "#,
    );
}

#[test]
fn unrecognized_annotation() {
    check_err(
        r#"unrecognized annotation `foo`"#,
        r#"grammar; #[foo] Term = ();"#,
        r#"           ~~~            "#,
    );
}

#[test]
fn duplicate_annotation() {
    check_err(
        r#"duplicate annotation `inline`"#,
        r#"grammar; #[inline] #[inline] Term = ();"#,
        r#"                     ~~~~~~            "#,
    );
}

#[test]
fn pub_inline_annotation() {
    check_err(
        r#"public items cannot be marked #\[inline\]"#,
        r#"grammar; #[inline] pub Term = ();"#,
        r#"           ~~~~~~            "#,
    );
}

#[test]
fn multiple_match_token() {
    check_err(
        r#"multiple match definitions are not permitted"#,
        r#"grammar; match { _ } match { _ }"#,
        r#"                     ~~~~~      "#,
    );
}

#[test]
fn match_after_extern_token() {
    check_err(
        r#"match and extern \(with custom tokens\) definitions are mutually exclusive"#,
        r#"grammar; extern { enum Tok { } } match { _ }"#,
        r#"                                 ~~~~~      "#,
    );
}

#[test]
fn extern_after_match_token() {
    check_err(
        r#"extern \(with custom tokens\) and match definitions are mutually exclusive"#,
        r#"grammar; match { _ } extern { enum Tok { } }"#,
        r#"                     ~~~~~~                 "#,
    );
}

#[test]
fn match_catch_all_first_of_last() {
    check_err(
        r#"Catch all must be final item"#,
        r#"grammar; match { _, "abc" }"#,
        r#"                 ~         "#,
    );
}

#[test]
fn match_catch_all_last_of_first() {
    check_err(
        r#"Catch all must be final item"#,
        r#"grammar; match { "abc", _ } else { "foo" }"#,
        r#"                        ~                 "#,
    );
}

#[test]
fn expandable_expression_requires_named_variables() {
    check_err(
        r#"Using `<>` between curly braces \(e.g., `\{<>\}`\) only works when your parsed values have been given names \(e.g., `<x:Foo>`, not just `<Foo>`\)"#,
        r#"grammar; Term = { <A> => Foo {<>} };"#,
        r#"                  ~~~~~~~~~~~~~~~~  "#);
}

#[test]
fn mixing_names_and_anonymous_values() {
    check_err(
        r#"anonymous symbols like this one cannot be combined with named symbols like `b:B`"#,
        r#"grammar; Term = { <A> <b:B> => Alien: Eighth passanger of Nostromo};"#,
        r#"                  ~~~                                               "#,
    );
}

#[test]
fn public_macros() {
    check_err(
        r#"macros cannot be marked public"#,
        r#"grammar; pub Comma<T> = (T ",")* T?;"#,
        r#"             ~~~~~~~~               "#,
    );
}
