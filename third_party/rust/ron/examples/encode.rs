use std::{collections::HashMap, iter::FromIterator};

use ron::ser::{to_string_pretty, PrettyConfig};
use serde::Serialize;

#[derive(Serialize)]
struct Config {
    float: (f32, f64),
    tuple: TupleStruct,
    map: HashMap<u8, char>,
    nested: Nested,
    var: Variant,
    array: Vec<()>,
}

#[derive(Serialize)]
struct TupleStruct((), bool);

#[derive(Serialize)]
enum Variant {
    A(u8, &'static str),
}

#[derive(Serialize)]
struct Nested {
    a: String,
    b: char,
}

fn main() {
    let data = Config {
        float: (2.18, -1.1),
        tuple: TupleStruct((), false),
        map: HashMap::from_iter(vec![(0, '1'), (1, '2'), (3, '5'), (8, '1')]),
        nested: Nested {
            a: "Hello from \"RON\"".to_string(),
            b: 'b',
        },
        var: Variant::A(!0, ""),
        array: vec![(); 3],
    };

    let pretty = PrettyConfig::new()
        .depth_limit(2)
        .separate_tuple_members(true)
        .enumerate_arrays(true);
    let s = to_string_pretty(&data, pretty).expect("Serialization failed");

    println!("{}", s);
}
