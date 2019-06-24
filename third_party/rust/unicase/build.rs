extern crate version_check as rustc;

fn main() {
    if rustc::is_min_version("1.5.0").map(|(is_min, _)| is_min).unwrap_or(true) {
        println!("cargo:rustc-cfg=__unicase__iter_cmp");
    }

    if rustc::is_min_version("1.13.0").map(|(is_min, _)| is_min).unwrap_or(true) {
        println!("cargo:rustc-cfg=__unicase__default_hasher");
    }

    if rustc::is_min_version("1.31.0").map(|(is_min, _)| is_min).unwrap_or(true) {
        println!("cargo:rustc-cfg=__unicase__const_fns");
    }
}
