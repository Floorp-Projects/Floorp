extern crate toml;
extern crate serde;

use serde::de::Deserialize;

#[test]
fn main() {
    let s = "
        [a] foo = 1
        [[b]] foo = 1
    ";
    assert!(s.parse::<toml::Value>().is_err());

    let mut d = toml::de::Deserializer::new(s);
    d.set_require_newline_after_table(false);
    let value = toml::Value::deserialize(&mut d).unwrap();
    assert_eq!(value["a"]["foo"].as_integer(), Some(1));
    assert_eq!(value["b"][0]["foo"].as_integer(), Some(1));
}
