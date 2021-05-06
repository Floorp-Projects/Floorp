/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate rustc_version;

use rustc_version::{version, Version};

fn main() {
    let ver = version().unwrap();
    let mut bootstrap = false;
    let max_oom_hook_version = Version::parse("1.53.0-alpha").unwrap();

    if ver >= Version::parse("1.28.0-alpha").unwrap() && ver < max_oom_hook_version {
        println!("cargo:rustc-cfg=feature=\"oom_with_hook\"");
        bootstrap = true;
    } else if std::env::var("MOZ_AUTOMATION").is_ok() {
        panic!("Builds on automation must use a version of rust for which we know how to hook OOM: want < {}, have {}",
               max_oom_hook_version, ver);
    }

    // This is a rather awful thing to do, but we're only doing it on
    // versions of rustc that are not going to change the unstable APIs
    // we use from under us, all being already released or beta.
    if bootstrap && ver < Version::parse("1.50.0").unwrap() {
        println!("cargo:rustc-env=RUSTC_BOOTSTRAP=1");
    }
}
