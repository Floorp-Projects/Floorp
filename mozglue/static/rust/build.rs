/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
}
