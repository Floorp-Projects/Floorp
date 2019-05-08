extern crate tempfile;

pub mod preferences;
pub mod prefreader;
pub mod profile;

#[cfg(test)]
mod test {
    //    use std::fs::File;
    //    use profile::Profile;
    use crate::preferences::Pref;
    use crate::prefreader::{parse, serialize, tokenize};
    use crate::prefreader::{Position, PrefToken};
    use std::collections::BTreeMap;
    use std::error::Error;
    use std::io::Cursor;
    use std::str;

    #[test]
    fn tokenize_simple() {
        let prefs = "  user_pref ( 'example.pref.string', 'value' )   ;\n \
                     pref(\"example.pref.int\", -123); sticky_pref('example.pref.bool',false);";

        let p = Position::new();

        let expected = vec![
            PrefToken::UserPrefFunction(p),
            PrefToken::Paren('(', p),
            PrefToken::String("example.pref.string".into(), p),
            PrefToken::Comma(p),
            PrefToken::String("value".into(), p),
            PrefToken::Paren(')', p),
            PrefToken::Semicolon(p),
            PrefToken::PrefFunction(p),
            PrefToken::Paren('(', p),
            PrefToken::String("example.pref.int".into(), p),
            PrefToken::Comma(p),
            PrefToken::Int(-123, p),
            PrefToken::Paren(')', p),
            PrefToken::Semicolon(p),
            PrefToken::StickyPrefFunction(p),
            PrefToken::Paren('(', p),
            PrefToken::String("example.pref.bool".into(), p),
            PrefToken::Comma(p),
            PrefToken::Bool(false, p),
            PrefToken::Paren(')', p),
            PrefToken::Semicolon(p),
        ];

        tokenize_test(prefs, &expected);
    }

    #[test]
    fn tokenize_comments() {
        let prefs = "# bash style comment\n /*block comment*/ user_pref/*block comment*/(/*block \
                     comment*/ 'example.pref.string'  /*block comment*/,/*block comment*/ \
                     'value'/*block comment*/ )// line comment";

        let p = Position::new();

        let expected = vec![
            PrefToken::CommentBashLine(" bash style comment".into(), p),
            PrefToken::CommentBlock("block comment".into(), p),
            PrefToken::UserPrefFunction(p),
            PrefToken::CommentBlock("block comment".into(), p),
            PrefToken::Paren('(', p),
            PrefToken::CommentBlock("block comment".into(), p),
            PrefToken::String("example.pref.string".into(), p),
            PrefToken::CommentBlock("block comment".into(), p),
            PrefToken::Comma(p),
            PrefToken::CommentBlock("block comment".into(), p),
            PrefToken::String("value".into(), p),
            PrefToken::CommentBlock("block comment".into(), p),
            PrefToken::Paren(')', p),
            PrefToken::CommentLine(" line comment".into(), p),
        ];

        tokenize_test(prefs, &expected);
    }

    #[test]
    fn tokenize_escapes() {
        let prefs = r#"user_pref('example\x20pref', "\u0020\u2603\uD800\uDC96\"\'\n\r\\\w)"#;

        let p = Position::new();

        let expected = vec![
            PrefToken::UserPrefFunction(p),
            PrefToken::Paren('(', p),
            PrefToken::String("example pref".into(), p),
            PrefToken::Comma(p),
            PrefToken::String(" ☃𐂖\"'\n\r\\\\w".into(), p),
            PrefToken::Paren(')', p),
        ];

        tokenize_test(prefs, &expected);
    }

    fn tokenize_test(prefs: &str, expected: &[PrefToken]) {
        println!("{}\n", prefs);

        for (e, a) in expected.iter().zip(tokenize(prefs.as_bytes())) {
            let success = match (e, &a) {
                (&PrefToken::PrefFunction(_), &PrefToken::PrefFunction(_)) => true,
                (&PrefToken::UserPrefFunction(_), &PrefToken::UserPrefFunction(_)) => true,
                (&PrefToken::StickyPrefFunction(_), &PrefToken::StickyPrefFunction(_)) => true,
                (
                    &PrefToken::CommentBlock(ref data_e, _),
                    &PrefToken::CommentBlock(ref data_a, _),
                ) => data_e == data_a,
                (
                    &PrefToken::CommentLine(ref data_e, _),
                    &PrefToken::CommentLine(ref data_a, _),
                ) => data_e == data_a,
                (
                    &PrefToken::CommentBashLine(ref data_e, _),
                    &PrefToken::CommentBashLine(ref data_a, _),
                ) => data_e == data_a,
                (&PrefToken::Paren(data_e, _), &PrefToken::Paren(data_a, _)) => data_e == data_a,
                (&PrefToken::Semicolon(_), &PrefToken::Semicolon(_)) => true,
                (&PrefToken::Comma(_), &PrefToken::Comma(_)) => true,
                (&PrefToken::String(ref data_e, _), &PrefToken::String(ref data_a, _)) => {
                    data_e == data_a
                }
                (&PrefToken::Int(data_e, _), &PrefToken::Int(data_a, _)) => data_e == data_a,
                (&PrefToken::Bool(data_e, _), &PrefToken::Bool(data_a, _)) => data_e == data_a,
                (&PrefToken::Error(data_e, _), &PrefToken::Error(data_a, _)) => data_e == data_a,
                (_, _) => false,
            };
            if !success {
                println!("Expected {:?}, got {:?}", e, a);
            }
            assert!(success);
        }
    }

    #[test]
    fn parse_simple() {
        let input = "  user_pref /* block comment */ ( 'example.pref.string', 'value' )   ;\n \
                     pref(\"example.pref.int\", -123); sticky_pref('example.pref.bool',false)";

        let mut expected: BTreeMap<String, Pref> = BTreeMap::new();
        expected.insert("example.pref.string".into(), Pref::new("value"));
        expected.insert("example.pref.int".into(), Pref::new(-123));
        expected.insert("example.pref.bool".into(), Pref::new_sticky(false));

        parse_test(input, expected);
    }

    #[test]
    fn parse_escape() {
        let input = r#"user_pref('example\\pref\"string', 'val\x20ue' )"#;

        let mut expected: BTreeMap<String, Pref> = BTreeMap::new();
        expected.insert("example\\pref\"string".into(), Pref::new("val ue"));

        parse_test(input, expected);
    }

    fn parse_test(input: &str, expected: BTreeMap<String, Pref>) {
        match parse(input.as_bytes()) {
            Ok(ref actual) => {
                println!("Expected:\n{:?}\nActual\n{:?}", expected, actual);
                assert_eq!(actual, &expected);
            }
            Err(e) => {
                println!("{}", e.description());
                assert!(false)
            }
        }
    }

    #[test]
    fn serialize_simple() {
        let input = "  user_pref /* block comment */ ( 'example.pref.string', 'value' )   ;\n \
                     pref(\"example.pref.int\", -123); sticky_pref('example.pref.bool',false)";
        let expected = "sticky_pref(\"example.pref.bool\", false);
user_pref(\"example.pref.int\", -123);
user_pref(\"example.pref.string\", \"value\");\n";

        serialize_test(input, expected);
    }

    #[test]
    fn serialize_quotes() {
        let input = r#"user_pref('example\\with"quotes"', '"Value"')"#;
        let expected = r#"user_pref("example\\with\"quotes\"", "\"Value\"");
"#;

        serialize_test(input, expected);
    }

    fn serialize_test(input: &str, expected: &str) {
        let buf = Vec::with_capacity(expected.len());
        let mut out = Cursor::new(buf);
        serialize(&parse(input.as_bytes()).unwrap(), &mut out).unwrap();
        let data = out.into_inner();
        let actual = str::from_utf8(&*data).unwrap();
        println!("Expected:\n{:?}\nActual\n{:?}", expected, actual);
        assert_eq!(actual, expected);
    }
}
