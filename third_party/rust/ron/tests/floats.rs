use ron::{
    de::from_str,
    ser::{to_string_pretty, PrettyConfig},
};

#[test]
fn test_inf_and_nan() {
    assert_eq!(from_str("inf"), Ok(std::f64::INFINITY));
    assert_eq!(from_str("-inf"), Ok(std::f64::NEG_INFINITY));
    assert_eq!(from_str::<f64>("NaN").map(|n| n.is_nan()), Ok(true))
}

#[test]
fn decimal_floats() {
    let pretty = PrettyConfig::new().decimal_floats(false);
    let without_decimal = to_string_pretty(&1.0, pretty).unwrap();
    assert_eq!(without_decimal, "1");

    let pretty = PrettyConfig::new().decimal_floats(false);
    let without_decimal = to_string_pretty(&1.1, pretty).unwrap();
    assert_eq!(without_decimal, "1.1");

    let pretty = PrettyConfig::new().decimal_floats(true);
    let with_decimal = to_string_pretty(&1.0, pretty).unwrap();
    assert_eq!(with_decimal, "1.0");

    let pretty = PrettyConfig::new().decimal_floats(true);
    let with_decimal = to_string_pretty(&1.1, pretty).unwrap();
    assert_eq!(with_decimal, "1.1");
    let with_pretty = to_string_pretty(&1.0, PrettyConfig::new()).unwrap();
    assert_eq!(with_pretty, "1.0");

    let tiny_pretty = to_string_pretty(&0.00000000000000005, PrettyConfig::new()).unwrap();
    assert_eq!(tiny_pretty, "0.00000000000000005");
}
