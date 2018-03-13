extern crate toml;

use std::f64;

#[test]
fn test_invalid_float_encode() {
    fn bad(value: toml::Value) {
        assert!(toml::to_string(&value).is_err());
    }

    bad(toml::Value::Float(f64::INFINITY));
    bad(toml::Value::Float(f64::NEG_INFINITY));
    bad(toml::Value::Float(f64::NAN));
}
