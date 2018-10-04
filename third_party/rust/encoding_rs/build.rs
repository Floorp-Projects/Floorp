fn main() {
    #[cfg(feature = "simd-accel")]
    println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
}
