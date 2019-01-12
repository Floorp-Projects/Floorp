use std::env;
use std::ffi::OsString;
use std::process::Command;

fn main() {
    let rustc = env::var_os("RUSTC").unwrap_or(OsString::from("rustc"));
    let output = Command::new(&rustc)
        .arg("--version")
        .output()
        .unwrap()
        .stdout;
    let version = String::from_utf8(output).unwrap();

    // If we're using nightly Rust, then we can enable vector optimizations.
    // Note that these aren't actually activated unless the `unstable` feature
    // is enabled.
    //
    // We also don't activate these if we've explicitly disabled auto
    // optimizations. Disabling auto optimizations is intended for use in
    // tests, so that we can reliably test fallback implementations.
    if env::var_os("CARGO_CFG_REGEX_DISABLE_AUTO_OPTIMIZATIONS").is_none() {
        if version.contains("nightly") {
            println!("cargo:rustc-cfg=regex_runtime_teddy_ssse3");
            println!("cargo:rustc-cfg=regex_runtime_teddy_avx2");
        }
    }
}
