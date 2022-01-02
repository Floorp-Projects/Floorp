extern crate cc;

use std::env;

fn main() {
    let target = env::var("TARGET").unwrap();
    let target_os = target.split('-').nth(2).unwrap();

    if target_os == "windows" {
        let mut builder = cc::Build::new();
        builder.file("c/windows.c");
        builder.compile("info");
    }
}
