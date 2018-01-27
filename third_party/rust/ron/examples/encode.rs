extern crate ron;
#[macro_use]
extern crate serde;

use std::collections::HashMap;
use std::default::Default;
use std::fs::File;

use ron::ser::{PrettyConfig, to_string_pretty};

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
    use std::io::Write;
    use std::iter::FromIterator;

    let mut file = File::create("config.ron").expect("Failed to create file");

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

    let pretty = PrettyConfig {
        separate_tuple_members: true,
        enumerate_arrays: true,
        ..PrettyConfig::default()
    };
    let s = to_string_pretty(&data, pretty).expect("Serialization failed");

    file.write(s.as_bytes()).expect("Failed to write data to file");
}
