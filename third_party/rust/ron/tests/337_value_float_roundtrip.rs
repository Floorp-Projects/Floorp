#[test]
fn roundtrip_value_float_with_decimals() {
    let v: ron::Value = ron::from_str("1.0").unwrap();

    assert_eq!(v, ron::Value::Number(1.0_f64.into()));

    let ser = ron::ser::to_string(&v).unwrap();

    let roundtrip = ron::from_str(&ser).unwrap();

    assert_eq!(v, roundtrip);
}

#[test]
#[allow(clippy::float_cmp)]
fn roundtrip_value_float_into() {
    let v: ron::Value = ron::from_str("1.0").unwrap();
    assert_eq!(v, ron::Value::Number(1.0_f64.into()));

    let ser = ron::ser::to_string(&v).unwrap();

    let f1: f64 = ron::from_str(&ser).unwrap();
    assert_eq!(f1, 1.0_f64);

    let roundtrip: ron::Value = ron::from_str(&ser).unwrap();

    let f2: f64 = roundtrip.into_rust().unwrap();
    assert_eq!(f2, 1.0_f64);
}
