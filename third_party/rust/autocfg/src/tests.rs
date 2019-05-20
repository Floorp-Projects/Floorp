use super::AutoCfg;

#[test]
fn autocfg_version() {
    let ac = AutoCfg::with_dir("target").unwrap();
    println!("version: {:?}", ac.rustc_version);
    assert!(ac.probe_rustc_version(1, 0));
}

#[test]
fn version_cmp() {
    use super::version::Version;
    let v123 = Version::new(1, 2, 3);

    assert!(Version::new(1, 0, 0) < v123);
    assert!(Version::new(1, 2, 2) < v123);
    assert!(Version::new(1, 2, 3) == v123);
    assert!(Version::new(1, 2, 4) > v123);
    assert!(Version::new(1, 10, 0) > v123);
    assert!(Version::new(2, 0, 0) > v123);
}

#[test]
fn probe_add() {
    let ac = AutoCfg::with_dir("target").unwrap();
    assert!(ac.probe_path("std::ops::Add"));
    assert!(ac.probe_trait("std::ops::Add"));
    assert!(ac.probe_trait("std::ops::Add<i32>"));
    assert!(ac.probe_trait("std::ops::Add<i32, Output = i32>"));
    assert!(ac.probe_type("std::ops::Add<i32, Output = i32>"));
}

#[test]
fn probe_as_ref() {
    let ac = AutoCfg::with_dir("target").unwrap();
    assert!(ac.probe_path("std::convert::AsRef"));
    assert!(ac.probe_trait("std::convert::AsRef<str>"));
    assert!(ac.probe_type("std::convert::AsRef<str>"));
}

#[test]
fn probe_i128() {
    let ac = AutoCfg::with_dir("target").unwrap();
    let missing = !ac.probe_rustc_version(1, 26);
    assert!(missing ^ ac.probe_path("std::i128"));
    assert!(missing ^ ac.probe_type("i128"));
}

#[test]
fn probe_sum() {
    let ac = AutoCfg::with_dir("target").unwrap();
    let missing = !ac.probe_rustc_version(1, 12);
    assert!(missing ^ ac.probe_path("std::iter::Sum"));
    assert!(missing ^ ac.probe_trait("std::iter::Sum"));
    assert!(missing ^ ac.probe_trait("std::iter::Sum<i32>"));
    assert!(missing ^ ac.probe_type("std::iter::Sum<i32>"));
}
