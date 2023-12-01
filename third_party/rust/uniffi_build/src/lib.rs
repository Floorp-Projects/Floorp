/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{Context, Result};
use camino::Utf8Path;
use std::env;

/// Generate the rust "scaffolding" required to build a uniffi component.
///
/// Given the path to an UDL file, this function will call the `uniffi-bindgen`
/// command-line tool to generate the `pub extern "C"` functions and other supporting
/// code required to expose the defined interface from Rust. The expectation is that
/// this will be called from a crate's build script, and the resulting file will
/// be `include!()`ed into the build.
///
/// This function will attempt to locate and parse a corresponding Cargo.toml to
/// determine the crate name which will host this UDL.
///
/// Given an UDL file named `example.udl`, the generated scaffolding will be written
/// into a file named `example.uniffi.rs` in the `$OUT_DIR` directory.
pub fn generate_scaffolding(udl_file: impl AsRef<Utf8Path>) -> Result<()> {
    let udl_file = udl_file.as_ref();
    println!("cargo:rerun-if-changed={udl_file}");
    println!("cargo:rerun-if-env-changed=UNIFFI_TESTS_DISABLE_EXTENSIONS");
    let out_dir = env::var("OUT_DIR").context("$OUT_DIR missing?!")?;
    uniffi_bindgen::generate_component_scaffolding(udl_file, Some(out_dir.as_ref()), false)
}

/// Like generate_scaffolding, but uses the specified crate_name instead of locating and parsing
/// Cargo.toml.
pub fn generate_scaffolding_for_crate(
    udl_file: impl AsRef<Utf8Path>,
    crate_name: &str,
) -> Result<()> {
    let udl_file = udl_file.as_ref();

    println!("cargo:rerun-if-changed={udl_file}");
    // The UNIFFI_TESTS_DISABLE_EXTENSIONS variable disables some bindings, but it is evaluated
    // at *build* time, so we need to rebuild when it changes.
    println!("cargo:rerun-if-env-changed=UNIFFI_TESTS_DISABLE_EXTENSIONS");
    // Why don't we just depend on uniffi-bindgen and call the public functions?
    // Calling the command line helps making sure that the generated swift/Kotlin/whatever
    // bindings were generated with the same version of uniffi as the Rust scaffolding code.
    let out_dir = env::var("OUT_DIR").context("$OUT_DIR missing?!")?;
    uniffi_bindgen::generate_component_scaffolding_for_crate(
        udl_file,
        crate_name,
        Some(out_dir.as_ref()),
        false,
    )
}
