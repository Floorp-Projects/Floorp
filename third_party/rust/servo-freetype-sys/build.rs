/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate cmake;
extern crate pkg_config;

use cmake::Config;
use std::env;

fn main() {
    if !env::var("TARGET").unwrap().contains("eabi") &&
        pkg_config::Config::new().atleast_version("18.5.12").find("freetype2").is_ok() {
        return
    }

    let dst = Config::new("freetype2")
        .define("WITH_BZip2", "OFF")
        .define("WITH_HarfBuzz", "OFF")
        .define("WITH_PNG", "OFF")
        .define("WITH_ZLIB", "OFF")
        .profile("Release")
        .build();
    let out_dir = env::var("OUT_DIR").unwrap();
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=static=freetype");
    println!("cargo:outdir={}", out_dir);
}
