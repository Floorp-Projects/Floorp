/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

fn main() {
    println!("cargo:rustc-link-lib=shell32");
    println!("cargo:rustc-link-lib=shlwapi");
    println!("cargo:rustc-link-lib=rpcrt4");
    println!(
        "cargo:rustc-link-search={}/toolkit/mozapps/update/common",
        env!("MOZ_TOPOBJDIR")
    );
}
