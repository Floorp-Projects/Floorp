#[test]
fn ui() {
    if !version_check::is_nightly().unwrap() {
        return;
    }

    let mut config = compiletest::Config {
        mode: compiletest::common::Mode::Ui,
        src_base: std::path::PathBuf::from("tests/ui"),
        target_rustcflags: Some(String::from(
            "\
             --edition=2018 \
             -Z unstable-options \
             --extern serde_repr \
             ",
        )),
        build_base: std::path::PathBuf::from("target/ui"),
        ..Default::default()
    };

    config.link_deps();
    config.clean_rmeta();

    compiletest::run_tests(&config);
}
