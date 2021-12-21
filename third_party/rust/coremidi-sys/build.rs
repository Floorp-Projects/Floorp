fn main() {
    if std::env::var("TARGET").expect("cannot read TARGET environment variable").contains("apple") {
        println!("cargo:rustc-link-lib=framework=CoreMIDI");
    }
}