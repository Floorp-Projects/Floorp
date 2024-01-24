/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Work around the fact that `sqlcipher` might get enabled by a cargo feature
//! another crate in the workspace needs, without setting up nss.  (This is a
//! gross hack).
fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    // If NSS_DIR isn't set, we don't really care, ignore the Err case.
    let _ = nss_build_common::link_nss();

    uniffi::generate_scaffolding("./src/webext-storage.udl").unwrap();
}
