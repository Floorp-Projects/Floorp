use std::collections::HashMap;

use serde::{Deserialize, Serialize};

#[derive(Debug, PartialEq, Deserialize, Serialize)]
struct UnitStruct;

#[derive(Debug, PartialEq, Deserialize, Serialize)]
struct NewType(f32);

#[derive(Debug, PartialEq, Deserialize, Serialize)]
struct TupleStruct(UnitStruct, i8);

#[derive(Debug, PartialEq, Eq, Hash, Deserialize, Serialize)]
struct Key(u32);

#[derive(Debug, PartialEq, Deserialize, Serialize)]
struct Struct {
    tuple: ((), NewType, TupleStruct),
    vec: Vec<Option<UnitStruct>>,
    map: HashMap<Key, i32>,
    deep_vec: HashMap<Key, Vec<()>>,
    deep_map: HashMap<Key, HashMap<Key, i32>>,
}

#[test]
fn empty_sets_arrays() {
    let value = Struct {
        tuple: ((), NewType(0.5), TupleStruct(UnitStruct, -5)),
        vec: vec![],
        map: vec![].into_iter().collect(),
        deep_vec: vec![(Key(0), vec![])].into_iter().collect(),
        deep_map: vec![(Key(0), vec![].into_iter().collect())]
            .into_iter()
            .collect(),
    };

    let pretty = ron::ser::PrettyConfig::new()
        .enumerate_arrays(true)
        .new_line("\n".to_string());
    let serial = ron::ser::to_string_pretty(&value, pretty).unwrap();

    println!("Serialized: {}", serial);

    assert_eq!(
        "(
    tuple: ((), (0.5), ((), -5)),
    vec: [],
    map: {},
    deep_vec: {
        (0): [],
    },
    deep_map: {
        (0): {},
    },
)",
        serial
    );

    let deserial = ron::de::from_str(&serial);

    assert_eq!(Ok(value), deserial);
}
