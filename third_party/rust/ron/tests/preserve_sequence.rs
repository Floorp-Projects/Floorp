use ron::{
    de::from_str,
    ser::{to_string_pretty, PrettyConfig},
};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

#[derive(Debug, Deserialize, Serialize)]
struct Config {
    boolean: bool,
    float: f32,
    map: BTreeMap<u8, char>,
    nested: Nested,
    tuple: (u32, u32),
}

#[derive(Debug, Deserialize, Serialize)]
struct Nested {
    a: String,
    b: char,
}

fn read_original(source: &str) -> String {
    source.to_string().replace("\r\n", "\n")
}

fn make_roundtrip(source: &str) -> String {
    let config: Config = from_str(source).unwrap();
    let pretty = PrettyConfig::new()
        .depth_limit(3)
        .separate_tuple_members(true)
        .enumerate_arrays(true)
        .new_line("\n".into());
    to_string_pretty(&config, pretty).expect("Serialization failed")
}

#[test]
fn test_sequence_ex1() {
    let file = include_str!("preserve_sequence_ex1.ron");
    assert_eq!(read_original(file), make_roundtrip(file));
}

#[test]
fn test_sequence_ex2() {
    let file = include_str!("preserve_sequence_ex2.ron");
    assert_eq!(read_original(file), make_roundtrip(file));
}
