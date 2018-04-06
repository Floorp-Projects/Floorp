extern crate string_cache_codegen;

use std::env;
use std::path::Path;

fn main() {
    string_cache_codegen::AtomType::new("atom::tests::TestAtom", "test_atom!")
        .atoms(&[
            "a", "b", "address", "area", "body", "font-weight", "br", "html", "head", "id",
        ])
        .write_to_file(&Path::new(&env::var("OUT_DIR").unwrap()).join("test_atom.rs"))
        .unwrap()
}
