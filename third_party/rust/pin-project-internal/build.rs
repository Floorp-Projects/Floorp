use std::{env, process::Command, str};

// The rustc-cfg strings below are *not* public API. Please let us know by
// opening a GitHub issue if your build environment requires some way to enable
// these cfgs other than by executing our build script.
fn main() {
    let minor = match rustc_minor_version() {
        Some(minor) => minor,
        None => return,
    };

    // Underscore const names requires Rust 1.37:
    // https://github.com/rust-lang/rust/pull/61347
    if minor >= 37 {
        println!("cargo:rustc-cfg=underscore_consts");
    }

    // #[deprecated] on proc-macro requires Rust 1.40:
    // https://github.com/rust-lang/rust/pull/65666
    if minor >= 40 {
        println!("cargo:rustc-cfg=deprecated_proc_macro");
    }
}

fn rustc_minor_version() -> Option<u32> {
    let rustc = env::var_os("RUSTC")?;
    let output = Command::new(rustc).arg("--version").output().ok()?;
    let version = str::from_utf8(&output.stdout).ok()?;
    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return None;
    }
    pieces.next()?.parse().ok()
}
