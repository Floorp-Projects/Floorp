use rustc_version::{version_meta, Channel, Version};

fn main() {
    println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
    let target = std::env::var("TARGET")
        .expect("TARGET environment variable not defined");
    if target.contains("neon") {
        println!("cargo:rustc-cfg=libcore_neon");
    }
    let ver_meta = version_meta().unwrap();
    let old_const_generics =
        if ver_meta.semver < Version::parse("1.56.0-alpha").unwrap() {
            true
        } else if ver_meta.semver >= Version::parse("1.57.0-alpha").unwrap() {
            false
        } else {
            match ver_meta.channel {
                Channel::Stable | Channel::Beta => false,
                Channel::Nightly | Channel::Dev
                    if ver_meta
                        .commit_date
                        .as_deref()
                        .map(|d| d < "2021-08-31")
                        .unwrap_or(false) =>
                {
                    true
                }
                _ => false,
            }
        };
    if old_const_generics {
        println!("cargo:rustc-cfg=const_generics");
    }
}
