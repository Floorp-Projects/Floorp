extern crate autocfg;

fn main() {
    let ac = autocfg::new();

    // Check for a minimum version for a few features
    if ac.probe_rustc_version(1, 31) {
        println!("cargo:rustc-cfg=allow_clippy");
    }
    if ac.probe_rustc_version(1, 36) {
        println!("cargo:rustc-cfg=maybe_uninit");
    }
    if ac.probe_rustc_version(1, 40) {
        println!("cargo:rustc-cfg=doctests");
    }
}
