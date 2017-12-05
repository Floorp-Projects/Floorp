#![cfg(feature="compiletest")]
extern crate compiletest_rs as compiletest;

fn run_mode(mode: &'static str) {
    let mut config = compiletest::Config::default();
    config.mode = mode.parse().expect("Invalid mode");
    config.src_base = ["tests", mode].iter().collect();
    config.target_rustcflags = Some("-L target/debug/ -L target/debug/deps/".to_owned());

    config.verbose = true;

    compiletest::run_tests(&config);
}

#[test]
fn compile_test() {
    run_mode("compile-fail");
}
