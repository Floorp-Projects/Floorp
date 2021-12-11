fn main() {
    println!("cargo:rerun-if-changed=");

    let cargo_manifest_dir = std::env::var_os("CARGO_MANIFEST_DIR").unwrap();
    let include_dir = std::path::PathBuf::from(cargo_manifest_dir).join("include");
    println!("cargo:include-dir={}", include_dir.display());
}
