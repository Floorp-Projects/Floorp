extern crate target_build_utils;
use target_build_utils::TargetInfo;
use std::io::Write;
use std::env;

fn main(){
    let target = TargetInfo::new().expect("could not get target info");
    match target.target_os() {
        "linux" | "android" => println!("cargo:rustc-link-lib=dl"),
        "freebsd" | "dragonfly" => println!("cargo:rustc-link-lib=c"),
        // netbsd claims dl* will be available to any dynamically linked binary, but I haven’t
        // found any libraries that have to be linked to on other platforms.
        // What happens if the executable is not linked up dynamically?
        "openbsd" | "bitrig" | "netbsd" | "macos" | "ios" => {}
        // dependencies come with winapi
        "windows" => {}
        tos => {
            writeln!(::std::io::stderr(),
                     "Building for an unknown target_os=`{}`!\nPlease report an issue ",
                     tos).expect("could not report the error");
            ::std::process::exit(0xfc);
        }
    }
    maybe_test_helpers();
}

fn maybe_test_helpers() {
    if env::var("OPT_LEVEL").ok().and_then(|v| v.parse().ok()).unwrap_or(0u64) != 0 {
        // certainly not for testing, just skip.
        return;
    }
    let mut outpath = if let Some(od) = env::var_os("OUT_DIR") { od } else { return };
    let target = if let Some(t) = env::var_os("TARGET") { t } else { return };
    let rustc = env::var_os("RUSTC").unwrap_or_else(|| { "rustc".into() });
    outpath.push("/libtest_helpers.dll"); // extension for windows required, POSIX does not care.
    let _ = ::std::process::Command::new(rustc)
        .arg("src/test_helpers.rs")
        .arg("-o")
        .arg(outpath)
        .arg("-O")
        .arg("--target")
        .arg(target)
        .output();
    // Ignore the failures here. We do not want failures of this thing to inhibit people from
    // building and using the library. Might make it hard to debug why tests fail in case this
    // library does not get built, though.
}
