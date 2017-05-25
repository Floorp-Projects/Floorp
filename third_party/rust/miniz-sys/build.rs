extern crate gcc;

use std::env;

fn main() {
    gcc::compile_library("libminiz.a", &["miniz.c"]);
    println!("cargo:root={}", env::var("OUT_DIR").unwrap());
}
