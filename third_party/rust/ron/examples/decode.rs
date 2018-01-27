extern crate ron;
#[macro_use]
extern crate serde;

use std::collections::HashMap;

use ron::de::from_str;

#[derive(Debug, Deserialize)]
struct Config {
    boolean: bool,
    float: f32,
    map: HashMap<u8, char>,
    nested: Nested,
    tuple: (u32, u32),
}

#[derive(Debug, Deserialize)]
struct Nested {
    a: String,
    b: char,
}

const CONFIG: &str = "(
    boolean: true,
    float: 8.2,
    map: {
        1: '1',
        2: '4',
        3: '9',
        4: '1',
        5: '2',
        6: '3',
    },
    nested: Nested(
        a: \"Decode me!\",
        b: 'z',
    ),
    tuple: (3, 7),
)";

fn main() {
    let config: Config = match from_str(CONFIG) {
        Ok(x) => x,
        Err(e) => {
            println!("Failed to load config: {}", e);

            ::std::process::exit(1);
        },
    };

    println!("Config: {:?}", &config);
}
