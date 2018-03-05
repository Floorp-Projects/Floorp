extern crate toml;
extern crate serde;
extern crate serde_json;

use toml::{Value as Toml, to_string_pretty};
use serde::ser::Serialize;
use serde_json::Value as Json;

fn to_json(toml: toml::Value) -> Json {
    fn doit(s: &str, json: Json) -> Json {
        let mut map = serde_json::Map::new();
        map.insert("type".to_string(), Json::String(s.to_string()));
        map.insert("value".to_string(), json);
        Json::Object(map)
    }

    match toml {
        Toml::String(s) => doit("string", Json::String(s)),
        Toml::Integer(i) => doit("integer", Json::String(i.to_string())),
        Toml::Float(f) => doit("float", Json::String({
            let s = format!("{:.15}", f);
            let s = format!("{}", s.trim_right_matches('0'));
            if s.ends_with('.') {format!("{}0", s)} else {s}
        })),
        Toml::Boolean(b) => doit("bool", Json::String(format!("{}", b))),
        Toml::Datetime(s) => doit("datetime", Json::String(s.to_string())),
        Toml::Array(arr) => {
            let is_table = match arr.first() {
                Some(&Toml::Table(..)) => true,
                _ => false,
            };
            let json = Json::Array(arr.into_iter().map(to_json).collect());
            if is_table {json} else {doit("array", json)}
        }
        Toml::Table(table) => {
            let mut map = serde_json::Map::new();
            for (k, v) in table {
                map.insert(k, to_json(v));
            }
            Json::Object(map)
        }
    }
}

fn run_pretty(toml: Toml) {
    // Assert toml == json
    println!("### pretty round trip parse.");
    
    // standard pretty
    let toml_raw = to_string_pretty(&toml).expect("to string");
    let toml2 = toml_raw.parse().expect("from string");
    assert_eq!(toml, toml2);

    // pretty with indent 2
    let mut result = String::with_capacity(128);
    {
        let mut serializer = toml::Serializer::pretty(&mut result);
        serializer.pretty_array_indent(2);
        toml.serialize(&mut serializer).expect("to string");
    }
    assert_eq!(toml, result.parse().expect("from str"));
    result.clear();
    {
        let mut serializer = toml::Serializer::new(&mut result);
        serializer.pretty_array_trailing_comma(false);
        toml.serialize(&mut serializer).expect("to string");
    }
    assert_eq!(toml, result.parse().expect("from str"));
    result.clear();
    {
        let mut serializer = toml::Serializer::pretty(&mut result);
        serializer.pretty_string(false);
        toml.serialize(&mut serializer).expect("to string");
        assert_eq!(toml, toml2);
    }
    assert_eq!(toml, result.parse().expect("from str"));
    result.clear();
    {
        let mut serializer = toml::Serializer::pretty(&mut result);
        serializer.pretty_array(false);
        toml.serialize(&mut serializer).expect("to string");
        assert_eq!(toml, toml2);
    }
    assert_eq!(toml, result.parse().expect("from str"));
}

fn run(toml_raw: &str, json_raw: &str) {
    println!("parsing:\n{}", toml_raw);
    let toml: Toml = toml_raw.parse().unwrap();
    let json: Json = json_raw.parse().unwrap();

    // Assert toml == json
    let toml_json = to_json(toml.clone());
    assert!(json == toml_json,
            "expected\n{}\ngot\n{}\n",
            serde_json::to_string_pretty(&json).unwrap(),
            serde_json::to_string_pretty(&toml_json).unwrap());

    // Assert round trip
    println!("round trip parse: {}", toml);
    let toml2 = toml.to_string().parse().unwrap();
    assert_eq!(toml, toml2);
    run_pretty(toml);
}

macro_rules! test( ($name:ident, $toml:expr, $json:expr) => (
    #[test]
    fn $name() { run($toml, $json); }
) );

test!(array_empty,
       include_str!("valid/array-empty.toml"),
       include_str!("valid/array-empty.json"));
test!(array_nospaces,
       include_str!("valid/array-nospaces.toml"),
       include_str!("valid/array-nospaces.json"));
test!(arrays_hetergeneous,
       include_str!("valid/arrays-hetergeneous.toml"),
       include_str!("valid/arrays-hetergeneous.json"));
test!(arrays,
       include_str!("valid/arrays.toml"),
       include_str!("valid/arrays.json"));
test!(arrays_nested,
       include_str!("valid/arrays-nested.toml"),
       include_str!("valid/arrays-nested.json"));
test!(empty,
       include_str!("valid/empty.toml"),
       include_str!("valid/empty.json"));
test!(bool,
       include_str!("valid/bool.toml"),
       include_str!("valid/bool.json"));
test!(datetime,
       include_str!("valid/datetime.toml"),
       include_str!("valid/datetime.json"));
test!(example,
       include_str!("valid/example.toml"),
       include_str!("valid/example.json"));
test!(float,
       include_str!("valid/float.toml"),
       include_str!("valid/float.json"));
test!(implicit_and_explicit_after,
       include_str!("valid/implicit-and-explicit-after.toml"),
       include_str!("valid/implicit-and-explicit-after.json"));
test!(implicit_and_explicit_before,
       include_str!("valid/implicit-and-explicit-before.toml"),
       include_str!("valid/implicit-and-explicit-before.json"));
test!(implicit_groups,
       include_str!("valid/implicit-groups.toml"),
       include_str!("valid/implicit-groups.json"));
test!(integer,
       include_str!("valid/integer.toml"),
       include_str!("valid/integer.json"));
test!(key_equals_nospace,
       include_str!("valid/key-equals-nospace.toml"),
       include_str!("valid/key-equals-nospace.json"));
test!(key_space,
       include_str!("valid/key-space.toml"),
       include_str!("valid/key-space.json"));
test!(key_special_chars,
       include_str!("valid/key-special-chars.toml"),
       include_str!("valid/key-special-chars.json"));
test!(key_with_pound,
       include_str!("valid/key-with-pound.toml"),
       include_str!("valid/key-with-pound.json"));
test!(long_float,
       include_str!("valid/long-float.toml"),
       include_str!("valid/long-float.json"));
test!(long_integer,
       include_str!("valid/long-integer.toml"),
       include_str!("valid/long-integer.json"));
test!(multiline_string,
       include_str!("valid/multiline-string.toml"),
       include_str!("valid/multiline-string.json"));
test!(raw_multiline_string,
       include_str!("valid/raw-multiline-string.toml"),
       include_str!("valid/raw-multiline-string.json"));
test!(raw_string,
       include_str!("valid/raw-string.toml"),
       include_str!("valid/raw-string.json"));
test!(string_empty,
       include_str!("valid/string-empty.toml"),
       include_str!("valid/string-empty.json"));
test!(string_escapes,
       include_str!("valid/string-escapes.toml"),
       include_str!("valid/string-escapes.json"));
test!(string_simple,
       include_str!("valid/string-simple.toml"),
       include_str!("valid/string-simple.json"));
test!(string_with_pound,
       include_str!("valid/string-with-pound.toml"),
       include_str!("valid/string-with-pound.json"));
test!(table_array_implicit,
       include_str!("valid/table-array-implicit.toml"),
       include_str!("valid/table-array-implicit.json"));
test!(table_array_many,
       include_str!("valid/table-array-many.toml"),
       include_str!("valid/table-array-many.json"));
test!(table_array_nest,
       include_str!("valid/table-array-nest.toml"),
       include_str!("valid/table-array-nest.json"));
test!(table_array_one,
       include_str!("valid/table-array-one.toml"),
       include_str!("valid/table-array-one.json"));
test!(table_empty,
       include_str!("valid/table-empty.toml"),
       include_str!("valid/table-empty.json"));
test!(table_sub_empty,
       include_str!("valid/table-sub-empty.toml"),
       include_str!("valid/table-sub-empty.json"));
test!(table_multi_empty,
       include_str!("valid/table-multi-empty.toml"),
       include_str!("valid/table-multi-empty.json"));
test!(table_whitespace,
       include_str!("valid/table-whitespace.toml"),
       include_str!("valid/table-whitespace.json"));
test!(table_with_pound,
       include_str!("valid/table-with-pound.toml"),
       include_str!("valid/table-with-pound.json"));
test!(unicode_escape,
       include_str!("valid/unicode-escape.toml"),
       include_str!("valid/unicode-escape.json"));
test!(unicode_literal,
       include_str!("valid/unicode-literal.toml"),
       include_str!("valid/unicode-literal.json"));
test!(hard_example,
       include_str!("valid/hard_example.toml"),
       include_str!("valid/hard_example.json"));
test!(example2,
       include_str!("valid/example2.toml"),
       include_str!("valid/example2.json"));
test!(example3,
       include_str!("valid/example-v0.3.0.toml"),
       include_str!("valid/example-v0.3.0.json"));
test!(example4,
       include_str!("valid/example-v0.4.0.toml"),
       include_str!("valid/example-v0.4.0.json"));
test!(example_bom,
       include_str!("valid/example-bom.toml"),
       include_str!("valid/example.json"));

test!(datetime_truncate,
      include_str!("valid/datetime-truncate.toml"),
      include_str!("valid/datetime-truncate.json"));
test!(key_quote_newline,
      include_str!("valid/key-quote-newline.toml"),
      include_str!("valid/key-quote-newline.json"));
test!(table_array_nest_no_keys,
      include_str!("valid/table-array-nest-no-keys.toml"),
      include_str!("valid/table-array-nest-no-keys.json"));
