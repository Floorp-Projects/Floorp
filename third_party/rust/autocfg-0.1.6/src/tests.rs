use super::AutoCfg;

impl AutoCfg {
    fn core_std(&self, path: &str) -> String {
        let krate = if self.no_std { "core" } else { "std" };
        format!("{}::{}", krate, path)
    }
}

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
    let add = ac.core_std("ops::Add");
    let add_rhs = ac.core_std("ops::Add<i32>");
    let add_rhs_output = ac.core_std("ops::Add<i32, Output = i32>");
    assert!(ac.probe_path(&add));
    assert!(ac.probe_trait(&add));
    assert!(ac.probe_trait(&add_rhs));
    assert!(ac.probe_trait(&add_rhs_output));
    assert!(ac.probe_type(&add_rhs_output));
}

#[test]
fn probe_as_ref() {
    let ac = AutoCfg::with_dir("target").unwrap();
    let as_ref = ac.core_std("convert::AsRef");
    let as_ref_str = ac.core_std("convert::AsRef<str>");
    assert!(ac.probe_path(&as_ref));
    assert!(ac.probe_trait(&as_ref_str));
    assert!(ac.probe_type(&as_ref_str));
}

#[test]
fn probe_i128() {
    let ac = AutoCfg::with_dir("target").unwrap();
    let missing = !ac.probe_rustc_version(1, 26);
    let i128_path = ac.core_std("i128");
    assert!(missing ^ ac.probe_path(&i128_path));
    assert!(missing ^ ac.probe_type("i128"));
}

#[test]
fn probe_sum() {
    let ac = AutoCfg::with_dir("target").unwrap();
    let missing = !ac.probe_rustc_version(1, 12);
    let sum = ac.core_std("iter::Sum");
    let sum_i32 = ac.core_std("iter::Sum<i32>");
    assert!(missing ^ ac.probe_path(&sum));
    assert!(missing ^ ac.probe_trait(&sum));
    assert!(missing ^ ac.probe_trait(&sum_i32));
    assert!(missing ^ ac.probe_type(&sum_i32));
}

#[test]
fn probe_std() {
    let ac = AutoCfg::with_dir("target").unwrap();
    assert_eq!(ac.probe_sysroot_crate("std"), !ac.no_std);
}

#[test]
fn probe_alloc() {
    let ac = AutoCfg::with_dir("target").unwrap();
    let missing = !ac.probe_rustc_version(1, 36);
    assert!(missing ^ ac.probe_sysroot_crate("alloc"));
}

#[test]
fn probe_bad_sysroot_crate() {
    let ac = AutoCfg::with_dir("target").unwrap();
    assert!(!ac.probe_sysroot_crate("doesnt_exist"));
}

#[test]
fn probe_no_std() {
    let ac = AutoCfg::with_dir("target").unwrap();
    assert!(ac.probe_type("i32"));
    assert!(ac.probe_type("[i32]"));
    assert_eq!(ac.probe_type("Vec<i32>"), !ac.no_std);
}
