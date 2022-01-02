fn main() {
    cc::Build::new().file("wrapper.c").compile("wrapper");
}
