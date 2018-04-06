extern crate lalrpop;

fn main() {
    println!("cargo:rerun-if-changed=src/parser/grammar.lalrpop");
    lalrpop::Configuration::new()
        .use_cargo_dir_conventions()
        .process()
        .unwrap();
}
