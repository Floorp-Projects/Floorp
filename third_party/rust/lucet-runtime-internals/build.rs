use cc;

fn main() {
    cc::Build::new()
        .file("src/context/context_asm.S")
        .compile("context_context_asm");
    cc::Build::new()
        .file("src/instance/siginfo_ext.c")
        .compile("instance_siginfo_ext");

    // TODO: this should only be built for tests, but Cargo doesn't
    // currently let you specify different build.rs options for tests:
    // <https://github.com/rust-lang/cargo/issues/1581>
    cc::Build::new()
        .file("src/context/tests/c_child.c")
        .compile("context_tests_c_child");
}
