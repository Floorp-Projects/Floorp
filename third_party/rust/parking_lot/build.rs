use rustc_version::{version, Version};

fn main() {
    if version().unwrap() >= Version::parse("1.34.0").unwrap() {
        println!("cargo:rustc-cfg=has_sized_atomics");
        println!("cargo:rustc-cfg=has_checked_instant");
    }
}
