#[macro_use]
extern crate lazy_static;
extern crate metadeps;
extern crate pkg_config;

use std::env;
use std::sync::Mutex;

lazy_static! {
    static ref LOCK: Mutex<()> = Mutex::new(());
}

fn toml(path: &str) -> metadeps::Result<std::collections::HashMap<String, pkg_config::Library>> {
    let _l = LOCK.lock();
    env::set_var("PKG_CONFIG_PATH", &env::current_dir().unwrap().join("tests"));
    env::set_var("CARGO_MANIFEST_DIR", &env::current_dir().unwrap().join("tests").join(path));
    env::set_var("CARGO_FEATURE_TEST_FEATURE", "");
    metadeps::probe()
}

#[test]
fn good() {
    let libraries = toml("toml-good").unwrap();
    let testlib = libraries.get("testlib").unwrap();
    assert_eq!(testlib.version, "1.2.3");
    let testdata = libraries.get("testdata").unwrap();
    assert_eq!(testdata.version, "4.5.6");
    assert!(libraries.get("testmore").is_none());
}

fn toml_err(path: &str, err_starts_with: &str) {
    let err = toml(path).unwrap_err();
    if !err.description().starts_with(err_starts_with) {
        panic!("Expected error to start with: {:?}\nGot error: {:?}", err_starts_with, err);
    }
}

#[test]
fn missing_file() {
    toml_err("toml-missing-file", "Error opening");
}

#[test]
fn missing_key() {
    toml_err("toml-missing-key", "No package.metadata.pkg-config in");
}

#[test]
fn not_table() {
    toml_err("toml-not-table", "package.metadata.pkg-config not a table in");
}

#[test]
fn version_missing() {
    toml_err("toml-version-missing", "No version in package.metadata.pkg-config.testlib");
}

#[test]
fn version_not_string() {
    toml_err("toml-version-not-string", "package.metadata.pkg-config.testlib not a string or table");
}

#[test]
fn version_in_table_not_string() {
    toml_err("toml-version-in-table-not-string", "Unexpected key package.metadata.pkg-config.testlib.version type integer");
}

#[test]
fn feature_not_string() {
    toml_err("toml-feature-not-string", "Unexpected key package.metadata.pkg-config.testlib.feature type integer");
}

#[test]
fn unexpected_key() {
    toml_err("toml-unexpected-key", "Unexpected key package.metadata.pkg-config.testlib.color type string");
}
