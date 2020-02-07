/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(target_os = "windows")]
use bindgen;

#[cfg(target_os = "windows")]
use std::env;
#[cfg(target_os = "windows")]
use std::path::PathBuf;

fn main() {
    #[cfg(target_os = "windows")]
    {
        let bindings = bindgen::Builder::default()
            .header("src/wrapper-windows.h")
            .whitelist_function("NCryptSignHash")
            .generate()
            .expect("Unable to generate bindings");
        let out_path = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR unset?"));
        bindings
            .write_to_file(out_path.join("bindings.rs"))
            .expect("Couldn't write bindings");
    }
}
