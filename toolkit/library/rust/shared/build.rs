extern crate rustc_version;

use rustc_version::{version, Version};

fn main() {
    let ver = version().unwrap();
    let mut bootstrap = false;

    if ver >= Version::parse("1.24.0").unwrap() && ver < Version::parse("1.27.0").unwrap() {
        println!("cargo:rustc-cfg=feature=\"oom_with_global_alloc\"");
        bootstrap = true;
    } else if ver >= Version::parse("1.28.0-alpha").unwrap() && ver < Version::parse("1.29.0").unwrap() {
        println!("cargo:rustc-cfg=feature=\"oom_with_hook\"");
        bootstrap = true;
    } else if std::env::var("MOZ_AUTOMATION").is_ok() {
        panic!("Builds on automation must use a version of rust that supports OOM hooking")
    }

    // This is a rather awful thing to do, but we're only doing it on
    // versions of rustc that are not going to change the unstable APIs
    // we use from under us, all being already released or beta.
    if bootstrap {
        println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
    }
}
