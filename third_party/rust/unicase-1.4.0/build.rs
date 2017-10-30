extern crate rustc_version as rustc;

fn main() {
    if rustc::version_matches(">= 1.5") {
        println!("cargo:rustc-cfg=iter_cmp");
    }
}
