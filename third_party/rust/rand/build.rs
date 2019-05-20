extern crate autocfg;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let ac = autocfg::new();
    ac.emit_rustc_version(1, 25);
    ac.emit_rustc_version(1, 26);
    ac.emit_rustc_version(1, 27);
}
