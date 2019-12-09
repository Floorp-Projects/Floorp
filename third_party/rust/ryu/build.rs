use std::env;
use std::process::Command;
use std::str::{self, FromStr};

// The rustc-cfg strings below are *not* public API. Please let us know by
// opening a GitHub issue if your build environment requires some way to enable
// these cfgs other than by executing our build script.
fn main() {
    let minor = match rustc_minor_version() {
        Some(minor) => minor,
        None => return,
    };

    let target = env::var("TARGET").unwrap();
    let emscripten = target == "asmjs-unknown-emscripten" || target == "wasm32-unknown-emscripten";

    // 128-bit integers stabilized in Rust 1.26:
    // https://blog.rust-lang.org/2018/05/10/Rust-1.26.html
    //
    // Disabled on Emscripten targets as Emscripten doesn't
    // currently support integers larger than 64 bits.
    if minor >= 26 && !emscripten {
        println!("cargo:rustc-cfg=integer128");
    }

    // #[must_use] on functions stabilized in Rust 1.27:
    // https://blog.rust-lang.org/2018/06/21/Rust-1.27.html
    if minor >= 27 {
        println!("cargo:rustc-cfg=must_use_return");
    }

    // MaybeUninit<T> stabilized in Rust 1.36:
    // https://blog.rust-lang.org/2019/07/04/Rust-1.36.0.html
    if minor >= 36 {
        println!("cargo:rustc-cfg=maybe_uninit");
    }
}

fn rustc_minor_version() -> Option<u32> {
    let rustc = match env::var_os("RUSTC") {
        Some(rustc) => rustc,
        None => return None,
    };

    let output = match Command::new(rustc).arg("--version").output() {
        Ok(output) => output,
        Err(_) => return None,
    };

    let version = match str::from_utf8(&output.stdout) {
        Ok(version) => version,
        Err(_) => return None,
    };

    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return None;
    }

    let next = match pieces.next() {
        Some(next) => next,
        None => return None,
    };

    u32::from_str(next).ok()
}
