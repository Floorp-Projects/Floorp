extern crate rustc_version;

fn main() {
    let vers = rustc_version::version().unwrap();
    if vers.major == 1 && vers.minor >= 26 {
        println!("cargo:rustc-cfg=stable_i128")
    }
}
