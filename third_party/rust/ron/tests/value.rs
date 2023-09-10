use std::f64;

use ron::{
    error::Error,
    value::{Map, Number, Value},
};
use serde::Serialize;

#[test]
fn bool() {
    assert_eq!("true".parse(), Ok(Value::Bool(true)));
    assert_eq!("false".parse(), Ok(Value::Bool(false)));
}

#[test]
fn char() {
    assert_eq!("'a'".parse(), Ok(Value::Char('a')));
}

#[test]
fn map() {
    let mut map = Map::new();
    map.insert(Value::Char('a'), Value::Number(Number::new(1)));
    map.insert(Value::Char('b'), Value::Number(Number::new(2f64)));
    assert_eq!("{ 'a': 1, 'b': 2.0 }".parse(), Ok(Value::Map(map)));
}

#[test]
fn number() {
    assert_eq!("42".parse(), Ok(Value::Number(Number::new(42))));
    assert_eq!(
        "3.141592653589793".parse(),
        Ok(Value::Number(Number::new(f64::consts::PI)))
    );
}

#[test]
fn option() {
    let opt = Some(Box::new(Value::Char('c')));
    assert_eq!("Some('c')".parse(), Ok(Value::Option(opt)));
}

#[test]
fn string() {
    let normal = "\"String\"";
    assert_eq!(normal.parse(), Ok(Value::String("String".into())));

    let raw = "r\"Raw String\"";
    assert_eq!(raw.parse(), Ok(Value::String("Raw String".into())));

    let raw_hashes = "r#\"Raw String\"#";
    assert_eq!(raw_hashes.parse(), Ok(Value::String("Raw String".into())));

    let raw_escaped = "r##\"Contains \"#\"##";
    assert_eq!(
        raw_escaped.parse(),
        Ok(Value::String("Contains \"#".into()))
    );

    let raw_multi_line = "r\"Multi\nLine\"";
    assert_eq!(
        raw_multi_line.parse(),
        Ok(Value::String("Multi\nLine".into()))
    );
}

#[test]
fn seq() {
    let seq = vec![
        Value::Number(Number::new(1)),
        Value::Number(Number::new(2f64)),
    ];
    assert_eq!("[1, 2.0]".parse(), Ok(Value::Seq(seq)));

    let err = Value::Seq(vec![Value::Number(Number::new(1))])
        .into_rust::<[i32; 2]>()
        .unwrap_err();

    assert_eq!(
        err,
        Error::ExpectedDifferentLength {
            expected: String::from("an array of length 2"),
            found: 1,
        }
    );

    let err = Value::Seq(vec![
        Value::Number(Number::new(1)),
        Value::Number(Number::new(2)),
        Value::Number(Number::new(3)),
    ])
    .into_rust::<[i32; 2]>()
    .unwrap_err();

    assert_eq!(
        err,
        Error::ExpectedDifferentLength {
            expected: String::from("a sequence of length 2"),
            found: 3,
        }
    );
}

#[test]
fn unit() {
    use ron::error::{Error, Position, SpannedError};

    assert_eq!("()".parse(), Ok(Value::Unit));
    assert_eq!("Foo".parse(), Ok(Value::Unit));

    assert_eq!(
        "".parse::<Value>(),
        Err(SpannedError {
            code: Error::Eof,
            position: Position { col: 1, line: 1 }
        })
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

#[test]
fn map_roundtrip_338() {
    // https://github.com/ron-rs/ron/issues/338

    let v: Value = ron::from_str("{}").unwrap();
    println!("{:?}", v);

    let ser = ron::to_string(&v).unwrap();
    println!("{:?}", ser);

    let roundtrip = ron::from_str(&ser).unwrap();
    println!("{:?}", roundtrip);

    assert_eq!(v, roundtrip);
}
