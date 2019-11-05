
use std::env;
use std::io::Write;
use std::process::{Command, Stdio};

fn main() {
    // we need to output *some* file to opt out of the default
    println!("cargo:rerun-if-changed=build.rs");

    detect_maybe_uninit();
}

fn detect_maybe_uninit() {
    let has_stable_maybe_uninit = probe(&stable_maybe_uninit());
    if has_stable_maybe_uninit {
        println!("cargo:rustc-cfg=has_stable_maybe_uninit");
        return;
    }
    let has_unstable_union_with_md = probe(&maybe_uninit_code(true));
    if has_unstable_union_with_md {
        println!("cargo:rustc-cfg=has_manually_drop_in_union");
        println!("cargo:rustc-cfg=has_union_feature");
    }
}

// To guard against changes in this currently unstable feature, use
// a detection tests instead of a Rustc version and/or date test.
fn stable_maybe_uninit() -> String {
    let code = "
    #![allow(warnings)]
    use std::mem::MaybeUninit;

    fn main() { }
    ";
    code.to_string()
}

// To guard against changes in this currently unstable feature, use
// a detection tests instead of a Rustc version and/or date test.
fn maybe_uninit_code(use_feature: bool) -> String {
    let feature = if use_feature { "#![feature(untagged_unions)]" } else { "" };

    let code = "
    #![allow(warnings)]
    use std::mem::ManuallyDrop;

    #[derive(Copy)]
    pub union MaybeUninit<T> {
        empty: (),
        value: ManuallyDrop<T>,
    }

    impl<T> Clone for MaybeUninit<T> where T: Copy
    {
        fn clone(&self) -> Self { *self }
    }

    fn main() {
        let value1 = MaybeUninit::<[i32; 3]> { empty: () };
        let value2 = MaybeUninit { value: ManuallyDrop::new([1, 2, 3]) };
    }
    ";


    [feature, code].concat()
}

/// Test if a code snippet can be compiled
fn probe(code: &str) -> bool {
    let rustc = env::var_os("RUSTC").unwrap_or_else(|| "rustc".into());
    let out_dir = env::var_os("OUT_DIR").expect("environment variable OUT_DIR");

    let mut child = Command::new(rustc)
        .arg("--out-dir")
        .arg(out_dir)
        .arg("--emit=obj")
        .arg("-")
        .stdin(Stdio::piped())
        .spawn()
        .expect("rustc probe");

    child
        .stdin
        .as_mut()
        .expect("rustc stdin")
        .write_all(code.as_bytes())
        .expect("write rustc stdin");

    child.wait().expect("rustc probe").success()
}
