extern crate yaml_rust;

use yaml_rust::{Yaml, YamlEmitter, YamlLoader};

fn test_round_trip(original: &Yaml) {
    let mut out = String::new();
    YamlEmitter::new(&mut out).dump(original).unwrap();
    let documents = YamlLoader::load_from_str(&out).unwrap();
    assert_eq!(documents.len(), 1);
    assert_eq!(documents[0], *original);
}

#[test]
fn test_escape_character() {
    let y = Yaml::String("\x1b".to_owned());
    test_round_trip(&y);
}

#[test]
fn test_colon_in_string() {
    let y = Yaml::String("x: %".to_owned());
    test_round_trip(&y);
}
