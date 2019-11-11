extern crate autocfg;

fn main() {
    autocfg::rerun_path(file!());

    let ac = autocfg::new();
    ac.emit_has_type("i128");
}
