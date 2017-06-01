use std::env::var;
use std::process::Command;
use std::str;

fn main() {
    let verbose = Command::new(var("RUSTC").unwrap_or("rustc".into()))
        .arg("--version")
        .arg("--verbose")
        .output()
        .unwrap()
        .stdout;
    let verbose = str::from_utf8(&verbose).unwrap();
    let mut commit_date = None;
    let mut release = None;
    for line in verbose.lines() {
        let mut parts = line.split(':');
        match parts.next().unwrap().trim() {
            "commit-date" => commit_date = Some(parts.next().unwrap().trim()),
            "release" => release = Some(parts.next().unwrap().trim()),
            _ => {}
        }
    }
    let version = release.unwrap().split('-').next().unwrap();;
    let mut version_components = version.split('.').map(|s| s.parse::<u32>().unwrap());
    let version = (
        version_components.next().unwrap(),
        version_components.next().unwrap(),
        version_components.next().unwrap(),
        // "unknown" sorts after "2016-02-14", which is what we want to defaut to unprefixed
        // https://github.com/servo/heapsize/pull/44#issuecomment-187935883
        commit_date.unwrap()
    );
    assert_eq!(version_components.next(), None);
    if version < (1, 8, 0, "2016-02-14") {
        println!("cargo:rustc-cfg=prefixed_jemalloc");
    }
}
