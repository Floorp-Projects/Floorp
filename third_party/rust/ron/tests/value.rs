use ron::value::{Number, Value};
use serde::Serialize;
use std::collections::BTreeMap;

#[test]
fn bool() {
    assert_eq!(Value::from_str("true"), Ok(Value::Bool(true)));
    assert_eq!(Value::from_str("false"), Ok(Value::Bool(false)));
}

#[test]
fn char() {
    assert_eq!(Value::from_str("'a'"), Ok(Value::Char('a')));
}

#[test]
fn map() {
    let mut map = BTreeMap::new();
    map.insert(Value::Char('a'), Value::Number(Number::new(1f64)));
    map.insert(Value::Char('b'), Value::Number(Number::new(2f64)));
    assert_eq!(Value::from_str("{ 'a': 1, 'b': 2 }"), Ok(Value::Map(map)));
}

#[test]
fn number() {
    assert_eq!(Value::from_str("42"), Ok(Value::Number(Number::new(42f64))));
    assert_eq!(
        Value::from_str("3.1415"),
        Ok(Value::Number(Number::new(3.1415f64)))
    );
}

#[test]
fn option() {
    let opt = Some(Box::new(Value::Char('c')));
    assert_eq!(Value::from_str("Some('c')"), Ok(Value::Option(opt)));
}

#[test]
fn string() {
    let normal = "\"String\"";
    assert_eq!(Value::from_str(normal), Ok(Value::String("String".into())));

    let raw = "r\"Raw String\"";
    assert_eq!(Value::from_str(raw), Ok(Value::String("Raw String".into())));

    let raw_hashes = "r#\"Raw String\"#";
    assert_eq!(
        Value::from_str(raw_hashes),
        Ok(Value::String("Raw String".into()))
    );

    let raw_escaped = "r##\"Contains \"#\"##";
    assert_eq!(
        Value::from_str(raw_escaped),
        Ok(Value::String("Contains \"#".into()))
    );

    let raw_multi_line = "r\"Multi\nLine\"";
    assert_eq!(
        Value::from_str(raw_multi_line),
        Ok(Value::String("Multi\nLine".into()))
    );
}

#[test]
fn seq() {
    let seq = vec![
        Value::Number(Number::new(1f64)),
        Value::Number(Number::new(2f64)),
    ];
    assert_eq!(Value::from_str("[1, 2]"), Ok(Value::Seq(seq)));
}

#[test]
fn unit() {
    use ron::de::{Error, ParseError, Position};

    assert_eq!(Value::from_str("()"), Ok(Value::Unit));
    assert_eq!(Value::from_str("Foo"), Ok(Value::Unit));

    assert_eq!(
        Value::from_str(""),
        Err(Error::Parser(ParseError::Eof, Position { col: 1, line: 1 }))
    );
}

#[derive(Serialize)]
struct Scene(Option<(u32, u32)>);

#[derive(Serialize)]
struct Scene2 {
    foo: Option<(u32, u32)>,
}

#[test]
fn roundtrip() {
    use ron::{de::from_str, ser::to_string};

    {
        let s = to_string(&Scene2 {
            foo: Some((122, 13)),
        })
        .unwrap();
        println!("{}", s);
        let scene: Value = from_str(&s).unwrap();
        println!("{:?}", scene);
    }
    {
        let s = to_string(&Scene(Some((13, 122)))).unwrap();
        println!("{}", s);
        let scene: Value = from_str(&s).unwrap();
        println!("{:?}", scene);
    }
}
