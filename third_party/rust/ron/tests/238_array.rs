use ron::{
    error::{Error, Position, SpannedError},
    value::{Number, Value},
};

#[test]
fn test_array() {
    let array: [i32; 3] = [1, 2, 3];

    let ser = ron::to_string(&array).unwrap();
    assert_eq!(ser, "(1,2,3)");

    let de: [i32; 3] = ron::from_str(&ser).unwrap();
    assert_eq!(de, array);

    let value: Value = ron::from_str(&ser).unwrap();
    assert_eq!(
        value,
        Value::Seq(vec![
            Value::Number(Number::from(1)),
            Value::Number(Number::from(2)),
            Value::Number(Number::from(3)),
        ])
    );

    let ser = ron::to_string(&value).unwrap();
    assert_eq!(ser, "[1,2,3]");

    let de: [i32; 3] = value.into_rust().unwrap();
    assert_eq!(de, array);

    // FIXME: fails and hence arrays do not roundtrip
    let de: SpannedError = ron::from_str::<[i32; 3]>(&ser).unwrap_err();
    assert_eq!(
        de,
        SpannedError {
            code: Error::ExpectedStructLike,
            position: Position { line: 1, col: 1 },
        }
    );

    let value: Value = ron::from_str(&ser).unwrap();
    assert_eq!(
        value,
        Value::Seq(vec![
            Value::Number(Number::from(1)),
            Value::Number(Number::from(2)),
            Value::Number(Number::from(3)),
        ])
    );
}
