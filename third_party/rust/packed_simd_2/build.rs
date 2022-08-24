use rustc_version::{version, Version};

fn main() {
    let target = std::env::var("TARGET").expect("TARGET environment variable not defined");
    if target.contains("neon") {
        println!("cargo:rustc-cfg=libcore_neon");
    }
    if version().unwrap() < Version::parse("1.61.0-alpha").unwrap() {
        println!("cargo:rustc-cfg=aarch64_target_feature");
    }
}
