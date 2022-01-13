/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use rustc_version::{version, Version};
use std::env;
use std::path::PathBuf;

fn main() {
    let dist_path = {
        let path = PathBuf::from(env::var_os("MOZ_DIST").unwrap());
        if !path.is_absolute() || !path.is_dir() {
            panic!(
                "MOZ_DIST must be an absolute directory, was: {}",
                path.display()
            );
        }
        path
    };
    let topobjdir = {
        let path = PathBuf::from(env::var_os("MOZ_TOPOBJDIR").unwrap());
        if !path.is_absolute() || !path.is_dir() {
            panic!(
                "MOZ_TOPOBJDIR must be an absolute directory, was: {}",
                path.display()
            );
        }
        path
    };
    let mut build = cc::Build::new();
    build.cpp(true);
    // For js-confdefs.h, see wrappers.cpp.
    build.include(topobjdir.join("js").join("src"));
    build.include(dist_path.join("include"));
    build.define("MOZ_HAS_MOZGLUE", None);
    build.file("wrappers.cpp");
    build.compile("wrappers");
    println!("cargo:rerun-if-changed=wrappers.cpp");

    let ver = version().unwrap();
    let max_oom_hook_version = Version::parse("1.60.0-alpha").unwrap();

    if ver < max_oom_hook_version {
        println!("cargo:rustc-cfg=feature=\"oom_with_hook\"");
    } else if std::env::var("MOZ_AUTOMATION").is_ok() {
        panic!("Builds on automation must use a version of rust for which we know how to hook OOM: want < {}, have {}",
               max_oom_hook_version, ver);
    }
}
