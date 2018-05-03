fn main() {
    // This is a rather awful thing to do, but we're only doing it on
    // versions of rustc, >= 1.24 < 1.27, that are not going to change
    // the unstable APIs we use from under us (1.26 being a beta as of
    // writing, and close to release).
    #[cfg(feature = "oom_with_global_alloc")]
    println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
}
