use rustc_version::{version, Version};

fn main() {
    println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
    let target = std::env::var("TARGET")
        .expect("TARGET environment variable not defined");
    if target.contains("neon") {
        println!("cargo:rustc-cfg=libcore_neon");
    }
    let ver = version().unwrap();
    if ver < Version::parse("1.56.0-alpha").unwrap() {
        println!("cargo:rustc-cfg=const_generics");
    }
}
