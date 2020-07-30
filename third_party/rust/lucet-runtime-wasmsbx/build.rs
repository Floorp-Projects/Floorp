use cc;

fn main() {
    build_c_api_tests();
}

fn build_c_api_tests() {
    cc::Build::new()
        .file("tests/c_api.c")
        .include("include")
        .compile("lucet_runtime_c_api_tests");
}
