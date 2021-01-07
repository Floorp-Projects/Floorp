extern crate rustc_version;
use rustc_version::{version, Version};

fn main() {
    // Assert we haven't travelled back in time
    assert!(version().unwrap().major >= 1);

    // Check for a minimum version
    if version().unwrap() >= Version::parse("1.36.0").unwrap() {
        println!("cargo:rustc-cfg=memoffset_maybe_uninit");
    }
}
