fn main() {
	println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
    let target = std::env::var("TARGET")
        .expect("TARGET environment variable not defined");
    if target.contains("neon") {
        println!("cargo:rustc-cfg=libcore_neon");
    }
}
