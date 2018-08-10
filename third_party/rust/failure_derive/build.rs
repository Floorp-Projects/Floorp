use std::env;
use std::process::Command;
use std::str;
use std::str::FromStr;

fn main() {
    if rustc_has_dyn_trait() {
        println!("cargo:rustc-cfg=has_dyn_trait");
    }
}

fn rustc_has_dyn_trait() -> bool {
    let rustc = match env::var_os("RUSTC") {
        Some(rustc) => rustc,
        None => return false,
    };

    let output = match Command::new(rustc).arg("--version").output() {
        Ok(output) => output,
        Err(_) => return false,
    };

    let version = match str::from_utf8(&output.stdout) {
        Ok(version) => version,
        Err(_) => return false,
    };

    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return true;
    }

    let next = match pieces.next() {
        Some(next) => next,
        None => return false,
    };

    u32::from_str(next).unwrap_or(0) >= 27
}
