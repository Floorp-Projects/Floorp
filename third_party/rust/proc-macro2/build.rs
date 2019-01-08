use std::env;
use std::process::Command;
use std::str;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let target = env::var("TARGET").unwrap();

    let minor = match rustc_minor_version() {
        Some(n) => n,
        None => return,
    };

    if minor >= 26 {
        println!("cargo:rustc-cfg=u128");
    }

    if !enable_use_proc_macro(&target) {
        return;
    }
    println!("cargo:rustc-cfg=use_proc_macro");

    // Rust 1.29 stabilized the necessary APIs in the `proc_macro` crate
    if (minor >= 29 && !cfg!(procmacro2_semver_exempt)) || cfg!(feature = "nightly") {
        println!("cargo:rustc-cfg=wrap_proc_macro");

        if cfg!(procmacro2_semver_exempt) {
            println!("cargo:rustc-cfg=super_unstable");
            // https://github.com/alexcrichton/proc-macro2/issues/147
            println!("cargo:rustc-cfg=procmacro2_semver_exempt");
        }
    }

    if minor == 29 {
        println!("cargo:rustc-cfg=slow_extend");
    }
}

fn enable_use_proc_macro(target: &str) -> bool {
    // wasm targets don't have the `proc_macro` crate, disable this feature.
    if target.contains("wasm32") {
        return false;
    }

    // Otherwise, only enable it if our feature is actually enabled.
    cfg!(feature = "proc-macro")
}

fn rustc_minor_version() -> Option<u32> {
    macro_rules! otry {
        ($e:expr) => {
            match $e {
                Some(e) => e,
                None => return None,
            }
        };
    }
    let rustc = otry!(env::var_os("RUSTC"));
    let output = otry!(Command::new(rustc).arg("--version").output().ok());
    let version = otry!(str::from_utf8(&output.stdout).ok());
    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return None;
    }
    otry!(pieces.next()).parse().ok()
}
