extern crate pkg_config;
#[macro_use]
extern crate lazy_static;

use pkg_config::Error;
use std::env;
use std::sync::Mutex;
use std::path::PathBuf;

lazy_static! {
    static ref LOCK: Mutex<()> = Mutex::new(());
}

fn reset() {
    for (k, _) in env::vars() {
        if k.contains("DYNAMIC") ||
           k.contains("STATIC") ||
           k.contains("PKG_CONFIG_ALLOW_CROSS") ||
           k.contains("FOO_NO_PKG_CONFIG") {
            env::remove_var(&k);
        }
    }
    env::remove_var("TARGET");
    env::remove_var("HOST");
    env::set_var("PKG_CONFIG_PATH", &env::current_dir().unwrap().join("tests"));
}

fn find(name: &str) -> Result<pkg_config::Library, Error> {
    pkg_config::probe_library(name)
}

#[test]
fn cross_disabled() {
    let _g = LOCK.lock();
    reset();
    env::set_var("TARGET", "foo");
    env::set_var("HOST", "bar");
    match find("foo") {
        Err(Error::CrossCompilation) => {},
        x => panic!("Error::CrossCompilation expected, found `{:?}`", x),
    }
}

#[test]
fn cross_enabled() {
    let _g = LOCK.lock();
    reset();
    env::set_var("TARGET", "foo");
    env::set_var("HOST", "bar");
    env::set_var("PKG_CONFIG_ALLOW_CROSS", "1");
    find("foo").unwrap();
}

#[test]
fn package_disabled() {
    let _g = LOCK.lock();
    reset();
    env::set_var("FOO_NO_PKG_CONFIG", "1");
    match find("foo") {
        Err(Error::EnvNoPkgConfig(name)) => {
            assert_eq!(name, "FOO_NO_PKG_CONFIG")
        }
        x => panic!("Error::EnvNoPkgConfig expected, found `{:?}`", x),
    }
}

#[test]
fn output_ok() {
    let _g = LOCK.lock();
    reset();
    let lib = find("foo").unwrap();
    assert!(lib.libs.contains(&"gcc".to_string()));
    assert!(lib.libs.contains(&"coregrind-amd64-linux".to_string()));
    assert!(lib.link_paths.contains(&PathBuf::from("/usr/lib/valgrind")));
}

#[test]
fn framework() {
    let _g = LOCK.lock();
    reset();
    let lib = find("framework").unwrap();
    assert!(lib.frameworks.contains(&"foo".to_string()));
    assert!(lib.framework_paths.contains(&PathBuf::from("/usr/lib")));
}

#[test]
fn get_variable() {
    let _g = LOCK.lock();
    reset();
    let prefix = pkg_config::get_variable("foo", "prefix").unwrap();
    assert_eq!(prefix, "/usr");
}

#[test]
fn version() {
    let _g = LOCK.lock();
    reset();
    assert_eq!(&find("foo").unwrap().version[..], "3.10.0.SVN");
}
